#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <thread>
#include <chrono>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

struct Options {
  std::string input_path;
  int width = 120;
  double aspect = 0.5; 
  std::string chars = " .:-=+*#%@";
  enum class ColorMode { None, Ansi256, Ansi24 } color = ColorMode::Ansi24;
  bool background = false;
  bool invert = false;
  bool truecolor_if_supported = true;
  bool stream = false;
  double stream_fps = 10.0;
  int stream_max_frames = 0;  // 0 = until EOF
  bool stream_clear = true;
};
struct Image {
  int w = 0;
  int h = 0;
  std::vector<uint8_t> rgb;  
};
std::optional<int> ParseInt(std::string_view s) {
  int v = 0;
  auto* b = s.data();
  auto* e = s.data() + s.size();
  auto r = std::from_chars(b, e, v);
  if (r.ec != std::errc{} || r.ptr != e) return std::nullopt;
  return v;
}
std::optional<double> ParseDouble(std::string_view s) {//cgange in future
  std::string tmp(s);
  char* end = nullptr;
  const double v = std::strtod(tmp.c_str(), &end);
  if (!end || *end != '\0') return std::nullopt;
  return v;
}
bool IsTruthyEnv(std::string_view v) {
  std::string s(v);
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return (s == "1" || s == "true" || s == "yes" || s == "on");
}
Options::ColorMode DetectColorMode(const Options& opt) {
  if (opt.color == Options::ColorMode::None) return Options::ColorMode::None;
  const char* no_color = std::getenv("NO_COLOR");
  if (no_color && *no_color) return Options::ColorMode::None;
  const char* colorterm = std::getenv("COLORTERM");
  const char* term = std::getenv("TERM");
  const std::string ct = colorterm ? std::string(colorterm) : "";
  const std::string t = term ? std::string(term) : "";
  const bool ct_truecolor = (ct.find("truecolor") != std::string::npos) || (ct.find("24bit") != std::string::npos);
  const bool term_256 = (t.find("256color") != std::string::npos);
  if (opt.color == Options::ColorMode::Ansi24) {
    if (opt.truecolor_if_supported && (ct_truecolor || term_256 || !t.empty())) {
      return Options::ColorMode::Ansi24;
    }
    return Options::ColorMode::Ansi256;
  }
  return Options::ColorMode::Ansi256;
}
int Clamp255(int v) { return std::max(0, std::min(255, v)); }

uint8_t Luma(uint8_t r, uint8_t g, uint8_t b) {
  const double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  return static_cast<uint8_t>(std::lround(std::clamp(y, 0.0, 255.0)));
}
int Ansi256IndexFromRgb(int r, int g, int b) {
  auto to6 = [](int c) -> int { return static_cast<int>(std::lround((c / 255.0) * 5.0)); };
  const int rr = to6(r);
  const int gg = to6(g);
  const int bb = to6(b);
  return 16 + 36 * rr + 6 * gg + bb;
}
void PrintUsage(std::ostream& os) {
  os << "zenscii - convert images to ASCII art (optionally colored)\n"
     << "Input format: PPM (P3/P6). Use ImageMagick to convert PNG/JPG/WebP.\n\n"
     << "Usage:\n"
     << "  zenscii <image.ppm|-> [options]\n\n"
     << "Options:\n"
     << "  --width <n>           Output width in characters (default 120)\n"
     << "  --aspect <f>          Height multiplier (default 0.5)\n"
     << "  --chars <string>      Dark->light ramp (default \" .:-=+*#%@\")\n"
     << "  --invert              Invert brightness mapping\n"
     << "  --color <mode>        none | ansi256 | ansi24 (default ansi24)\n"
     << "  --bg                  Also color the background (uses same pixel)\n"
     << "  --stream              Read multiple PPM frames and animate in terminal\n"
     << "  --fps <n>             Stream FPS (default 10)\n"
     << "  --max-frames <n>       Stop after N frames (default 0 = until EOF)\n"
     << "  --no-clear            Don't clear screen between frames (stream mode)\n"
     << "  --help                Show this help\n\n"
     << "Examples:\n"
     << "  zenscii photo.ppm --width 160 --color ansi24\n"
     << "  magick photo.png ppm:- | zenscii - --width 160 --color ansi24\n"
     << "  magick photo.jpg ppm:- | zenscii - --width 120 --color none\n"
     << "  magick icon.webp ppm:- | zenscii - --width 80 --color ansi256 --bg\n";
}
std::optional<Options> ParseArgs(int argc, char** argv) {
  Options opt;
  std::vector<std::string_view> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

  if (args.empty()) return std::nullopt;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string_view a = args[i];
    if (a == "--help" || a == "-h") {
      return std::nullopt;
    }
    if (a == "-" || (!a.empty() && a[0] != '-')) {
      if (!opt.input_path.empty()) {
        std::cerr << "Error: multiple input paths provided.\n";
        return std::nullopt;
      }
      opt.input_path = std::string(a);
      continue;
    }
    auto require_value = [&](std::string_view flag) -> std::optional<std::string_view> {
      if (i + 1 >= args.size()) {
        std::cerr << "Error: missing value for " << flag << "\n";
        return std::nullopt;
      }
      ++i;
      return args[i];
    };
    if (a == "--width") {
      auto v = require_value(a);
      if (!v) return std::nullopt;
      auto n = ParseInt(*v);
      if (!n || *n <= 0 || *n > 2000) {
        std::cerr << "Error: invalid --width\n";
        return std::nullopt;
      }
      opt.width = *n;
    } else if (a == "--stream") {
      opt.stream = true;
    } else if (a == "--fps") {
      auto v = require_value(a);
      if (!v) return std::nullopt;
      auto f = ParseDouble(*v);
      if (!f || !std::isfinite(*f) || *f <= 0.0 || *f > 240.0) {
        std::cerr << "Error: invalid --fps\n";
        return std::nullopt;
      }
      opt.stream_fps = *f;
    } else if (a == "--max-frames") {
      auto v = require_value(a);
      if (!v) return std::nullopt;
      auto n = ParseInt(*v);
      if (!n || *n < 0) {
        std::cerr << "Error: invalid --max-frames\n";
        return std::nullopt;
      }
      opt.stream_max_frames = *n;
    } else if (a == "--no-clear") {
      opt.stream_clear = false;
    } else if (a == "--aspect") {
      auto v = require_value(a);
      if (!v) return std::nullopt;
      auto f = ParseDouble(*v);
      if (!f || !std::isfinite(*f) || *f <= 0.0 || *f > 5.0) {
        std::cerr << "Error: invalid --aspect\n";
        return std::nullopt;
      }
      opt.aspect = *f;
    } else if (a == "--chars") {
      auto v = require_value(a);
      if (!v) return std::nullopt;
      if (v->empty()) {
        std::cerr << "Error: --chars cannot be empty\n";
        return std::nullopt;
      }
      opt.chars = std::string(*v);
    } else if (a == "--color") {
      auto v = require_value(a);
      if (!v) return std::nullopt;
      if (*v == "none") opt.color = Options::ColorMode::None;
      else if (*v == "ansi256") opt.color = Options::ColorMode::Ansi256;
      else if (*v == "ansi24") opt.color = Options::ColorMode::Ansi24;
      else {
        std::cerr << "Error: invalid --color (use none|ansi256|ansi24)\n";
        return std::nullopt;
      }
    } else if (a == "--bg") {
      opt.background = true;
    } else if (a == "--invert") {
      opt.invert = true;
    } else {
      std::cerr << "Error: unknown option " << a << "\n";
      return std::nullopt;
    }
  }

  if (opt.input_path.empty()) return std::nullopt;
  const char* no_color = std::getenv("NO_COLOR");
  if (no_color && *no_color) opt.color = Options::ColorMode::None;

  return opt;
}
std::string_view RampChar(const std::string& ramp, double t01, bool invert) {
  if (ramp.empty()) return " ";
  t01 = std::clamp(t01, 0.0, 1.0);
  if (invert) t01 = 1.0 - t01;

  const double idx_f = t01 * static_cast<double>(ramp.size() - 1);
  const size_t idx = static_cast<size_t>(std::lround(idx_f));
  return std::string_view(&ramp[idx], 1);
}
void SkipWsAndComments(std::istream& is) {
  while (true) {
    while (true) {
      int c = is.peek();
      if (c == EOF) return;
      if (!std::isspace(static_cast<unsigned char>(c))) break;
      is.get();
    }
    int c = is.peek();
    if (c == '#') {
      std::string dummy;
      std::getline(is, dummy);
      continue;
    }
    return;
  }
}
std::optional<std::string> ReadToken(std::istream& is) {
  SkipWsAndComments(is);
  std::string tok;
  while (true) {
    int c = is.peek();
    if (c == EOF) break;
    if (std::isspace(static_cast<unsigned char>(c)) || c == '#') break;
    tok.push_back(static_cast<char>(is.get()));
  }
  if (tok.empty()) return std::nullopt;
  return tok;
}
std::optional<Image> ReadPPM(std::istream& is, std::string* err) {
  auto magic = ReadToken(is);
  if (!magic) {
    if (err) *err = "PPM parse error: missing magic";
    return std::nullopt;
  }
  const bool is_p3 = (*magic == "P3");
  const bool is_p6 = (*magic == "P6");
  if (!is_p3 && !is_p6) {
    if (err) *err = "PPM parse error: expected P3 or P6";
    return std::nullopt;
  }
  auto w_tok = ReadToken(is);
  auto h_tok = ReadToken(is);
  auto mv_tok = ReadToken(is);
  if (!w_tok || !h_tok || !mv_tok) {
    if (err) *err = "PPM parse error: missing header values";
    return std::nullopt;
  }
  auto w = ParseInt(*w_tok);
  auto h = ParseInt(*h_tok);
  auto mv = ParseInt(*mv_tok);
  if (!w || !h || !mv || *w <= 0 || *h <= 0 || *mv <= 0 || *mv > 255) {
    if (err) *err = "PPM parse error: invalid width/height/maxval (maxval must be 1..255)";
    return std::nullopt;
  }
  Image img;
  img.w = *w;
  img.h = *h;
  img.rgb.resize(static_cast<size_t>(img.w) * static_cast<size_t>(img.h) * 3u);
  if (is_p6) {
    int c = is.peek();
    if (c != EOF && std::isspace(static_cast<unsigned char>(c))) is.get();

    const size_t need = static_cast<size_t>(img.w) * static_cast<size_t>(img.h) * 3u;
    is.read(reinterpret_cast<char*>(img.rgb.data()), static_cast<std::streamsize>(need));
    if (static_cast<size_t>(is.gcount()) != need) {
      if (err) *err = "PPM parse error: truncated pixel data (P6)";
      return std::nullopt;
    }
    return img;
  }
  const int maxv = *mv;
  for (size_t i = 0; i < img.rgb.size(); ++i) {
    auto t = ReadToken(is);
    if (!t) {
      if (err) *err = "PPM parse error: truncated pixel data (P3)";
      return std::nullopt;
    }
    auto v = ParseInt(*t);
    if (!v || *v < 0 || *v > maxv) {
      if (err) *err = "PPM parse error: invalid pixel value (P3)";
      return std::nullopt;
    }
    const int scaled = static_cast<int>(std::lround((*v / static_cast<double>(maxv)) * 255.0));
    img.rgb[i] = static_cast<uint8_t>(Clamp255(scaled));
  }
  return img;
}
std::tuple<uint8_t, uint8_t, uint8_t> AvgRgbInBox(
    const Image& img, int x0, int y0, int x1, int y1) {
  x0 = std::max(0, std::min(img.w - 1, x0));
  x1 = std::max(0, std::min(img.w - 1, x1));
  y0 = std::max(0, std::min(img.h - 1, y0));
  y1 = std::max(0, std::min(img.h - 1, y1));
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);
  uint64_t sr = 0, sg = 0, sb = 0;
  uint64_t n = 0;
  for (int y = y0; y <= y1; ++y) {
    const size_t row = static_cast<size_t>(y) * static_cast<size_t>(img.w) * 3u;
    for (int x = x0; x <= x1; ++x) {
      const size_t idx = row + static_cast<size_t>(x) * 3u;
      sr += img.rgb[idx + 0];
      sg += img.rgb[idx + 1];
      sb += img.rgb[idx + 2];
      ++n;
    }
  }
  if (n == 0) return {0, 0, 0};
  const uint8_t r = static_cast<uint8_t>(sr / n);
  const uint8_t g = static_cast<uint8_t>(sg / n);
  const uint8_t b = static_cast<uint8_t>(sb / n);
  return {r, g, b};
}
}  
int main(int argc, char** argv) {
  const auto opt_maybe = ParseArgs(argc, argv);
  if (!opt_maybe) {
    PrintUsage(std::cout);
    return 2;
  }
  const Options opt = *opt_maybe;
  const Options::ColorMode color_mode = DetectColorMode(opt);

  auto render_one = [&](const Image& img) {
    const int src_w = img.w;
    const int src_h = img.h;
    const int out_w = std::max(1, opt.width);
    const double scale = static_cast<double>(out_w) / static_cast<double>(src_w);
    const int out_h = std::max(1, static_cast<int>(std::lround(src_h * scale * opt.aspect)));

    const auto reset_all = []() { std::cout << "\x1b[0m"; };
    const auto reset_line = [&]() {
      std::cout << "\x1b[0m";
      std::cout << "\n";
    };
    const bool color_enabled = (color_mode != Options::ColorMode::None);

    for (int y = 0; y < out_h; ++y) {
      const int y0 = static_cast<int>(std::floor((y * 1.0 * src_h) / out_h));
      const int y1 = static_cast<int>(std::floor(((y + 1.0) * src_h) / out_h)) - 1;
      for (int x = 0; x < out_w; ++x) {
        const int x0 = static_cast<int>(std::floor((x * 1.0 * src_w) / out_w));
        const int x1 = static_cast<int>(std::floor(((x + 1.0) * src_w) / out_w)) - 1;
        const auto [r, g, b] = AvgRgbInBox(img, x0, y0, x1, y1);
        const uint8_t y8 = Luma(r, g, b);
        const double t = static_cast<double>(y8) / 255.0;
        const std::string_view ch = RampChar(opt.chars, t, opt.invert);
        if (color_enabled) {
          if (color_mode == Options::ColorMode::Ansi24) {
            if (opt.background) {
              std::cout << "\x1b[48;2;" << static_cast<int>(r) << ";" << static_cast<int>(g) << ";"
                        << static_cast<int>(b) << "m";
            }
            std::cout << "\x1b[38;2;" << static_cast<int>(r) << ";" << static_cast<int>(g) << ";"
                      << static_cast<int>(b) << "m";
          } else {
            const int idx = Ansi256IndexFromRgb(r, g, b);
            if (opt.background) std::cout << "\x1b[48;5;" << idx << "m";
            std::cout << "\x1b[38;5;" << idx << "m";
          }
        }
        std::cout << ch;
      }
      reset_line();
    }
    reset_all();
  };

  if (opt.stream) {
    if (opt.input_path != "-") {
      std::cerr << "Error: --stream requires input path '-'\n";
      return 1;
    }

    const auto frame_delay = std::chrono::duration<double>(1.0 / std::max(1e-6, opt.stream_fps));
    int frames = 0;

    std::cout << "\x1b[?25l";  // hide cursor
    while (true) {
      SkipWsAndComments(std::cin);
      if (std::cin.peek() == EOF) break;

      std::string err;
      auto img_maybe = ReadPPM(std::cin, &err);
      if (!img_maybe) {
        if (std::cin.eof()) break;
        std::cout << "\x1b[0m\x1b[?25h\n";
        std::cerr << "Error: failed to read PPM frame: " << (err.empty() ? "unknown error" : err) << "\n";
        return 1;
      }

      if (opt.stream_clear) std::cout << "\x1b[H\x1b[2J";
      else std::cout << "\x1b[H";

      render_one(*img_maybe);
      std::cout << std::flush;

      ++frames;
      if (opt.stream_max_frames > 0 && frames >= opt.stream_max_frames) break;
      std::this_thread::sleep_for(frame_delay);
    }
    std::cout << "\x1b[0m\x1b[?25h\n";
    return 0;
  }

  std::optional<Image> img_maybe;
  std::string err;
  if (opt.input_path == "-") {
    img_maybe = ReadPPM(std::cin, &err);
  } else {
    std::ifstream in(opt.input_path, std::ios::binary);
    if (!in) {
      std::cerr << "Error: failed to open file: " << opt.input_path << "\n";
      return 1;
    }
    img_maybe = ReadPPM(in, &err);
  }
  if (!img_maybe) {
    std::cerr << "Error: failed to read PPM: " << (err.empty() ? "unknown error" : err) << "\n";
    return 1;
  }

  render_one(*img_maybe);
  return 0;
}

