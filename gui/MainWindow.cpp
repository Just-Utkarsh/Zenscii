#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QProgressDialog>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QString>
#include <QTextStream>
#include <QToolButton>
#include <QTimer>
#include <QWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

double luma01(const QColor& c) {
  // Rec. 709
  const double y = 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF();
  return std::clamp(y, 0.0, 1.0);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Zenscii");

  auto* root = new QWidget(this);
  auto* layout = new QGridLayout(root);

  path_ = new QLineEdit(root);
  path_->setPlaceholderText("Open an image…");
  path_->setReadOnly(true);

  openBtn_ = new QPushButton("Open…", root);
  saveBtn_ = new QPushButton("Save preview…", root);
  saveTextBtn_ = new QPushButton("Save B&W .txt…", root);
  saveBtn_->setEnabled(false);
  saveTextBtn_->setEnabled(false);

  width_ = new QSpinBox(root);
  width_->setRange(10, 400);
  width_->setValue(120);
  width_->setSuffix(" chars");

  aspect_ = new QDoubleSpinBox(root);
  aspect_->setRange(0.10, 2.50);
  aspect_->setSingleStep(0.05);
  aspect_->setValue(0.50);

  zoom_ = new QDoubleSpinBox(root);
  zoom_->setRange(0.10, 4.00);
  zoom_->setSingleStep(0.10);
  zoom_->setDecimals(2);
  zoom_->setValue(1.00);
  zoom_->setSuffix("×");

  auto* zoomOutBtn = new QToolButton(root);
  zoomOutBtn->setText("−");
  auto* zoomInBtn = new QToolButton(root);
  zoomInBtn->setText("+");
  auto* zoomResetBtn = new QToolButton(root);
  zoomResetBtn->setText("1:1");

  chars_ = new QLineEdit(root);
  chars_->setText(" .:-=+*#%@");

  color_ = new QComboBox(root);
  color_->addItem("none", static_cast<int>(ColorMode::None));
  color_->addItem("ansi256", static_cast<int>(ColorMode::Ansi256));
  color_->addItem("ansi24", static_cast<int>(ColorMode::Ansi24));
  color_->setCurrentIndex(2);

  bg_ = new QCheckBox("background", root);
  invert_ = new QCheckBox("invert", root);

  auto* previewLabel = new QLabel(root);
  previewLabel->setText("Open an image to preview ASCII art.");
  previewLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  previewLabel->setBackgroundRole(QPalette::Base);
  previewLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  previewLabel->setScaledContents(false);
  preview_ = previewLabel;

  auto* scroll = new QScrollArea(root);
  scroll->setWidget(previewLabel);
  // Important: keep the preview widget at its natural size so wide renders
  // stay intact and scrollbars work as expected.
  scroll->setWidgetResizable(false);
  scroll_ = scroll;

  int r = 0;
  layout->addWidget(path_, r, 0, 1, 4);
  layout->addWidget(openBtn_, r, 4, 1, 1);
  layout->addWidget(saveBtn_, r, 5, 1, 1);

  r++;
  layout->addWidget(saveTextBtn_, r, 4, 1, 2);

  r++;
  layout->addWidget(new QLabel("Width", root), r, 0);
  layout->addWidget(width_, r, 1);
  layout->addWidget(new QLabel("Aspect", root), r, 2);
  layout->addWidget(aspect_, r, 3);
  layout->addWidget(new QLabel("Zoom", root), r, 4);
  auto* zoomRow = new QWidget(root);
  auto* zoomLayout = new QGridLayout(zoomRow);
  zoomLayout->setContentsMargins(0, 0, 0, 0);
  zoomLayout->addWidget(zoomOutBtn, 0, 0);
  zoomLayout->addWidget(zoom_, 0, 1);
  zoomLayout->addWidget(zoomInBtn, 0, 2);
  zoomLayout->addWidget(zoomResetBtn, 0, 3);
  layout->addWidget(zoomRow, r, 5);

  r++;
  layout->addWidget(new QLabel("Chars", root), r, 0);
  layout->addWidget(chars_, r, 1, 1, 3);
  layout->addWidget(bg_, r, 4);
  layout->addWidget(invert_, r, 5);

  r++;
  layout->addWidget(new QLabel("Color", root), r, 0);
  layout->addWidget(color_, r, 1, 1, 2);
  layout->addWidget(new QLabel("Tip", root), r, 3);
  layout->addWidget(new QLabel("Ctrl + mouse wheel to zoom", root), r, 4, 1, 2);

  r++;
  // Video conversion controls
  videoFps_ = new QDoubleSpinBox(root);
  videoFps_->setRange(1.0, 60.0);
  videoFps_->setSingleStep(1.0);
  videoFps_->setValue(10.0);
  videoFps_->setSuffix(" fps");

  videoMaxFrames_ = new QSpinBox(root);
  videoMaxFrames_->setRange(0, 1000000);
  videoMaxFrames_->setValue(0);
  videoMaxFrames_->setToolTip("0 = use whole video");

  openVideoBtn_ = new QPushButton("Open video…", root);
  previewVideoBtn_ = new QPushButton("Preview video", root);
  saveVideoBtn_ = new QPushButton("Save ASCII MP4…", root);
  previewVideoBtn_->setEnabled(false);
  saveVideoBtn_->setEnabled(false);

  auto* videoRow = new QWidget(root);
  auto* videoLayout = new QGridLayout(videoRow);
  videoLayout->setContentsMargins(0, 0, 0, 0);
  videoLayout->addWidget(new QLabel("Video FPS", root), 0, 0);
  videoLayout->addWidget(videoFps_, 0, 1);
  videoLayout->addWidget(new QLabel("Max frames", root), 0, 2);
  videoLayout->addWidget(videoMaxFrames_, 0, 3);
  videoLayout->addWidget(openVideoBtn_, 0, 4);
  videoLayout->addWidget(previewVideoBtn_, 0, 5);
  videoLayout->addWidget(saveVideoBtn_, 0, 6);

  layout->addWidget(videoRow, r, 0, 1, 7);

  r++;
  layout->addWidget(scroll, r, 0, 1, 7);

  setCentralWidget(root);
  setStatusBar(new QStatusBar(this));
  setStatus("Ready.");

  connect(openBtn_, &QPushButton::clicked, this, &MainWindow::onOpenImage);
  connect(saveBtn_, &QPushButton::clicked, this, &MainWindow::onSavePreview);
  connect(saveTextBtn_, &QPushButton::clicked, this, &MainWindow::onSaveText);

  connect(width_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
  connect(aspect_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
  connect(chars_, &QLineEdit::textChanged, this, &MainWindow::onSettingsChanged);
  connect(color_, &QComboBox::currentIndexChanged, this, &MainWindow::onSettingsChanged);
  connect(bg_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
  connect(invert_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);

  connect(zoom_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onZoomChanged);
  connect(zoomOutBtn, &QToolButton::clicked, this, [this]() {
    zoom_->setValue(clampd(zoom_->value() - zoom_->singleStep(), zoom_->minimum(), zoom_->maximum()));
  });
  connect(zoomInBtn, &QToolButton::clicked, this, [this]() {
    zoom_->setValue(clampd(zoom_->value() + zoom_->singleStep(), zoom_->minimum(), zoom_->maximum()));
  });
  connect(zoomResetBtn, &QToolButton::clicked, this, [this]() { zoom_->setValue(1.0); });

  // Ctrl+wheel zoom on the preview area.
  scroll_->viewport()->installEventFilter(this);

  connect(openVideoBtn_, &QPushButton::clicked, this, &MainWindow::onOpenVideo);
  connect(previewVideoBtn_, &QPushButton::clicked, this, &MainWindow::onToggleVideoPreview);
  connect(saveVideoBtn_, &QPushButton::clicked, this, &MainWindow::onSaveAsciiVideoMp4);

  videoTimer_ = new QTimer(this);
  videoTimer_->setTimerType(Qt::PreciseTimer);
  connect(videoTimer_, &QTimer::timeout, this, &MainWindow::onVideoTick);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
  if (scroll_ && obj == scroll_->viewport() && event->type() == QEvent::Wheel) {
    auto* we = static_cast<QWheelEvent*>(event);
    if (we->modifiers().testFlag(Qt::ControlModifier)) {
      // One "step" is typically 120 units.
      const int steps = (we->angleDelta().y() != 0) ? (we->angleDelta().y() / 120) : 0;
      if (steps != 0) {
        const double next = zoom_->value() + steps * zoom_->singleStep();
        zoom_->setValue(clampd(next, zoom_->minimum(), zoom_->maximum()));
      }
      we->accept();
      return true;
    }
  }
  return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setStatus(const QString& s) {
  if (statusBar()) statusBar()->showMessage(s, 5000);
}

void MainWindow::onOpenImage() {
  const QString path = QFileDialog::getOpenFileName(
      this,
      "Open image",
      QString(),
      "Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.ppm *.pgm *.pbm);;All files (*)");
  if (path.isEmpty()) return;

  QString err;
  QImage img = loadImageAny(path, &err);
  if (img.isNull()) {
    setStatus("Failed to load image: " + err);
    return;
  }

  // Normalize to RGB for rendering.
  src_ = img.convertToFormat(QImage::Format_RGB888);
  path_->setText(path);
  recomputePreview();
}

void MainWindow::onSavePreview() {
  if (previewImg_.isNull()) return;
  const QString out = QFileDialog::getSaveFileName(
      this, "Save preview", "ascii_preview.png", "PNG (*.png);;JPEG (*.jpg *.jpeg)");
  if (out.isEmpty()) return;
  if (!previewImg_.save(out)) {
    setStatus("Failed to save preview.");
    return;
  }
  setStatus("Saved: " + out);
}

void MainWindow::onSaveText() {
  if (src_.isNull()) return;
  const QString out = QFileDialog::getSaveFileName(
      this, "Save B&W ASCII text", "ascii_bw.txt", "Text files (*.txt);;All files (*)");
  if (out.isEmpty()) return;

  QFile file(out);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    setStatus("Failed to save text output.");
    return;
  }

  QTextStream ts(&file);
  ts << renderAsciiText(src_);
  if (ts.status() != QTextStream::Ok) {
    setStatus("Failed to write text output.");
    return;
  }
  setStatus("Saved B&W text: " + out);
}

void MainWindow::onOpenVideo() {
  const QString in = QFileDialog::getOpenFileName(
      this,
      "Open video",
      QString(),
      "Videos (*.mp4 *.mkv *.webm *.mov *.avi);;All files (*)");
  if (in.isEmpty()) return;

  // Check ffmpeg availability.
  {
    QProcess p;
    p.setProgram("ffmpeg");
    p.setArguments({"-version"});
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();
    if (!p.waitForFinished(3000) || p.exitCode() != 0) {
      QMessageBox::critical(this, "Missing dependency", "ffmpeg not found in PATH.");
      return;
    }
  }

  // Stop any prior preview.
  videoPlaying_ = false;
  videoTimer_->stop();
  previewVideoBtn_->setText("Preview video");

  videoFrames_.clear();
  videoFrameIdx_ = 0;
  videoPath_ = in;
  videoFpsActual_ = videoFps_ ? videoFps_->value() : 10.0;

  const double fps = videoFpsActual_;
  const int maxFrames = videoMaxFrames_ ? videoMaxFrames_->value() : 0;

  setStatus("Extracting frames with ffmpeg…");

  QTemporaryDir tmp;
  if (!tmp.isValid()) {
    QMessageBox::critical(this, "Temp dir error", "Failed to create temporary directory.");
    return;
  }

  const QString framesDir = tmp.path() + "/frames";
  QDir().mkpath(framesDir);

  // Extract frames.
  {
    QStringList args;
    args << "-hide_banner" << "-loglevel" << "error";
    args << "-y";
    args << "-i" << in;
    args << "-vf" << QString("fps=%1").arg(fps, 0, 'f', 3);
    if (maxFrames > 0) args << "-frames:v" << QString::number(maxFrames);
    args << (framesDir + "/frame_%06d.png");

    QProcess proc;
    proc.setProgram("ffmpeg");
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    proc.waitForFinished(-1);
    if (proc.exitCode() != 0) {
      QMessageBox::critical(this, "ffmpeg error", "ffmpeg failed while extracting frames.");
      return;
    }
  }

  QDir d(framesDir);
  const auto frameFiles = d.entryList(QStringList() << "frame_*.png", QDir::Files, QDir::Name);
  if (frameFiles.empty()) {
    QMessageBox::critical(this, "No frames", "No frames were extracted from the video.");
    return;
  }

  QProgressDialog progress("Rendering ASCII video frames…", "Cancel", 0, frameFiles.size(), this);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);

  videoFrames_.reserve(frameFiles.size());
  for (int i = 0; i < frameFiles.size(); ++i) {
    if (progress.wasCanceled()) break;
    progress.setValue(i);

    const QString framePath = framesDir + "/" + frameFiles[i];
    QImage frame(framePath);
    if (frame.isNull()) continue;
    frame = frame.convertToFormat(QImage::Format_RGB888);

    QImage ascii = renderAsciiImage(frame);
    videoFrames_.push_back(ascii);
  }

  if (progress.wasCanceled() || videoFrames_.isEmpty()) {
    videoFrames_.clear();
    setStatus("Video load cancelled.");
    previewVideoBtn_->setEnabled(false);
    saveVideoBtn_->setEnabled(false);
    return;
  }

  previewVideoBtn_->setEnabled(true);
  saveVideoBtn_->setEnabled(true);
  setStatus(QString("Loaded %1 ASCII frames.").arg(videoFrames_.size()));

  showDisplayImage(videoFrames_.front());
}

void MainWindow::onToggleVideoPreview() {
  if (videoFrames_.isEmpty()) return;
  videoPlaying_ = !videoPlaying_;
  if (videoPlaying_) {
    previewVideoBtn_->setText("Pause preview");
    const int intervalMs = std::max(1, static_cast<int>(std::lround(1000.0 / std::max(1.0, videoFpsActual_))));
    videoTimer_->start(intervalMs);
  } else {
    previewVideoBtn_->setText("Preview video");
    videoTimer_->stop();
  }
}

void MainWindow::onVideoTick() {
  if (!videoPlaying_ || videoFrames_.isEmpty()) return;
  videoFrameIdx_ = (videoFrameIdx_ + 1) % videoFrames_.size();
  showDisplayImage(videoFrames_[videoFrameIdx_]);
}

void MainWindow::onSaveAsciiVideoMp4() {
  if (videoFrames_.isEmpty()) return;

  const QString out = QFileDialog::getSaveFileName(
      this,
      "Save ASCII video (MP4)",
      "zenscii_video.mp4",
      "MP4 (*.mp4);;All files (*)");
  if (out.isEmpty()) return;

  // Stop preview while exporting for consistent UX.
  videoPlaying_ = false;
  videoTimer_->stop();
  previewVideoBtn_->setText("Preview video");

  auto padToEven = [](const QImage& img) -> QImage {
    const int w = img.width();
    const int h = img.height();
    const int w2 = (w % 2 == 0) ? w : (w + 1);
    const int h2 = (h % 2 == 0) ? h : (h + 1);
    if (w2 == w && h2 == h) return img;
    QImage out(w2, h2, QImage::Format_ARGB32);
    out.fill(Qt::black);
    QPainter p(&out);
    p.drawImage(0, 0, img);
    return out;
  };

  QTemporaryDir tmp;
  if (!tmp.isValid()) {
    QMessageBox::critical(this, "Temp dir error", "Failed to create temporary directory.");
    return;
  }
  const QString asciiDir = tmp.path() + "/ascii";
  QDir().mkpath(asciiDir);

  QProgressDialog progress("Writing frames…", "Cancel", 0, videoFrames_.size(), this);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);

  for (int i = 0; i < videoFrames_.size(); ++i) {
    if (progress.wasCanceled()) break;
    progress.setValue(i);
    QImage img = padToEven(videoFrames_[i]);
    const QString outFrame = asciiDir + QString("/frame_%1.png").arg(i + 1, 6, 10, QChar('0'));
    img.save(outFrame);
  }
  if (progress.wasCanceled()) {
    setStatus("Export cancelled.");
    return;
  }

  setStatus("Encoding MP4 with ffmpeg…");
  {
    QStringList args;
    args << "-hide_banner" << "-loglevel" << "error";
    args << "-y";
    args << "-framerate" << QString::number(videoFpsActual_, 'f', 3);
    args << "-i" << (asciiDir + "/frame_%06d.png");
    args << "-c:v" << "libx264";
    args << "-pix_fmt" << "yuv420p";
    args << "-movflags" << "+faststart";
    args << out;

    QProcess proc;
    proc.setProgram("ffmpeg");
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    proc.waitForFinished(-1);
    if (proc.exitCode() != 0) {
      QMessageBox::critical(this, "ffmpeg error", "ffmpeg failed while encoding MP4.");
      return;
    }
  }

  setStatus("Saved ASCII video: " + out);
}

void MainWindow::onSettingsChanged() {
  if (src_.isNull()) return;
  recomputePreview();
}

void MainWindow::onZoomChanged() {
  if (displayImg_.isNull()) return;
  updatePreviewPixmap();
}

QImage MainWindow::loadImageAny(const QString& path, QString* err) const {
  QImageReader reader(path);
  reader.setAutoTransform(true);
  const QImage img = reader.read();
  if (img.isNull() && err) *err = reader.errorString();
  return img;
}

QChar MainWindow::rampChar(double t01) const {
  QString ramp = chars_->text();
  if (ramp.isEmpty()) ramp = " ";

  t01 = std::clamp(t01, 0.0, 1.0);
  if (invert_->isChecked()) t01 = 1.0 - t01;

  const int n = ramp.size();
  const int idx = clampi(static_cast<int>(std::lround(t01 * (n - 1))), 0, n - 1);
  return ramp.at(idx);
}

QImage MainWindow::renderAsciiImage(const QImage& src) const {
  const int outW = std::max(1, width_->value());
  const double aspect = aspect_->value();

  const int srcW = src.width();
  const int srcH = src.height();
  const double scale = static_cast<double>(outW) / static_cast<double>(srcW);
  const int outH = std::max(1, static_cast<int>(std::lround(srcH * scale * aspect)));

  // We'll draw glyphs; pick a monospace font and compute cell size.
  QFont font;
  font.setStyleHint(QFont::Monospace);
  font.setFamily(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
  font.setPointSize(12);

  QFontMetrics fm(font);
  const int cellW = std::max(1, fm.horizontalAdvance('M'));
  const int cellH = std::max(1, fm.height());
  const int baseline = fm.ascent();

  QImage canvas(outW * cellW, outH * cellH, QImage::Format_ARGB32);
  canvas.fill(Qt::black);

  QPainter p(&canvas);
  p.setFont(font);
  p.setRenderHint(QPainter::TextAntialiasing, true);

  // Box-filter average color from source for each output cell.
  for (int y = 0; y < outH; ++y) {
    const int y0 = static_cast<int>(std::floor((y * 1.0 * srcH) / outH));
    const int y1 = static_cast<int>(std::floor(((y + 1.0) * srcH) / outH)) - 1;
    for (int x = 0; x < outW; ++x) {
      const int x0 = static_cast<int>(std::floor((x * 1.0 * srcW) / outW));
      const int x1 = static_cast<int>(std::floor(((x + 1.0) * srcW) / outW)) - 1;

      qint64 sr = 0, sg = 0, sb = 0, n = 0;
      for (int yy = std::max(0, y0); yy <= std::min(srcH - 1, y1); ++yy) {
        const uchar* row = src.constScanLine(yy);
        for (int xx = std::max(0, x0); xx <= std::min(srcW - 1, x1); ++xx) {
          const int idx = xx * 3;
          sr += row[idx + 0];
          sg += row[idx + 1];
          sb += row[idx + 2];
          ++n;
        }
      }
      if (n == 0) n = 1;
      const QColor c(static_cast<int>(sr / n), static_cast<int>(sg / n), static_cast<int>(sb / n));

      const QChar ch = rampChar(luma01(c));

      const QRect cell(x * cellW, y * cellH, cellW, cellH);

      const int mode = color_->currentData().toInt();
      const bool colorEnabled = (mode != static_cast<int>(ColorMode::None));
      if (bg_->isChecked() && colorEnabled) {
        p.fillRect(cell, c);
      }

      if (colorEnabled) {
        p.setPen(c);
      } else {
        p.setPen(Qt::white);
      }

      p.drawText(cell.left(), cell.top() + baseline, QString(ch));
    }
  }

  return canvas;
}

QString MainWindow::renderAsciiText(const QImage& src) const {
  const int outW = std::max(1, width_->value());
  const double aspect = aspect_->value();
  const int srcW = src.width();
  const int srcH = src.height();
  const double scale = static_cast<double>(outW) / static_cast<double>(srcW);
  const int outH = std::max(1, static_cast<int>(std::lround(srcH * scale * aspect)));

  QString out;
  out.reserve(outW * outH + outH);
  for (int y = 0; y < outH; ++y) {
    const int y0 = static_cast<int>(std::floor((y * 1.0 * srcH) / outH));
    const int y1 = static_cast<int>(std::floor(((y + 1.0) * srcH) / outH)) - 1;
    for (int x = 0; x < outW; ++x) {
      const int x0 = static_cast<int>(std::floor((x * 1.0 * srcW) / outW));
      const int x1 = static_cast<int>(std::floor(((x + 1.0) * srcW) / outW)) - 1;

      qint64 sr = 0, sg = 0, sb = 0, n = 0;
      for (int yy = std::max(0, y0); yy <= std::min(srcH - 1, y1); ++yy) {
        const uchar* row = src.constScanLine(yy);
        for (int xx = std::max(0, x0); xx <= std::min(srcW - 1, x1); ++xx) {
          const int idx = xx * 3;
          sr += row[idx + 0];
          sg += row[idx + 1];
          sb += row[idx + 2];
          ++n;
        }
      }
      if (n == 0) n = 1;
      const QColor c(static_cast<int>(sr / n), static_cast<int>(sg / n), static_cast<int>(sb / n));
      out.append(rampChar(luma01(c)));
    }
    out.append('\n');
  }
  return out;
}

void MainWindow::updatePreviewPixmap() {
  if (displayImg_.isNull()) return;

  const double z = zoom_ ? zoom_->value() : 1.0;
  const int w = std::max(1, static_cast<int>(std::lround(displayImg_.width() * z)));
  const int h = std::max(1, static_cast<int>(std::lround(displayImg_.height() * z)));

  const QPixmap pm = QPixmap::fromImage(displayImg_).scaled(
      w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  preview_->setPixmap(pm);
  preview_->resize(pm.size());
}

void MainWindow::showDisplayImage(const QImage& img) {
  displayImg_ = img;
  updatePreviewPixmap();
}

void MainWindow::recomputePreview() {
  if (src_.isNull()) return;
  previewImg_ = renderAsciiImage(src_);
  showDisplayImage(previewImg_);
  saveBtn_->setEnabled(!previewImg_.isNull());
  saveTextBtn_->setEnabled(!previewImg_.isNull());
  setStatus("Rendered " + QString::number(width_->value()) + "x…");
}

