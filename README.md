# img2ascii

You give it a picture.
It prints the picture using text characters (ASCII) in your terminal.
It can also keep colors (if you want).

There are **two ways** to use it:
- **GUI app**: clicky-clicky window (`zenscii-gui`)
- **CLI app**: terminal command (`zenscii`)

## Step 0: Install the stuff (Arch)

Copy/paste this:

```bash
sudo pacman -S --needed cmake gcc imagemagick ffmpeg
```

If you want the GUI too, also install Qt:

```bash
sudo pacman -S --needed qt6-base
```

## Step 1: Build it

Copy/paste this:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

After building you will have:
- `./build/zenscii` (the terminal program)
- `./build/zenscii-gui` (the GUI program, **only if** Qt6 is installed)

## Step 2A: Use the GUI (easy mode)

Run:

```bash
./build/zenscii-gui
```

Then:
- Click **Open…**
- Move the **Width** slider/spinbox until it looks good
- Use **Zoom** (buttons, field, or `Ctrl + mouse wheel`) to zoom out/in in the preview area
- Pick **Color**:
  - `ansi24` = best colors (most terminals)
  - `ansi256` = okay colors (old terminals)
  - `none` = no colors (plain ASCII)
- Optional:
  - **background** = also color the background behind each character
  - **invert** = flip dark/light
- Click **Save preview…** if you want a PNG/JPG output of what you see
- Click **Save B&W .txt…** to export a plain monochrome ASCII text file
- Optional (video):
  - Click **Open video…** to load and render frames
  - Click **Preview video** to play/pause inside the app before exporting
  - Click **Save ASCII MP4…** to export the MP4
  - Set **Video FPS** and **Max frames** (0 = whole video)
  - Output MP4 is rendered from sampled frames using your current GUI settings (set **Color = none** for B&W)

## Step 2B: Use the CLI (terminal mode)

### Important: input format

The CLI reads **PPM** images (`P3` or `P6`).
Your PNG/JPG/WebP is NOT PPM, so you convert it using ImageMagick like this:

```bash
magick input.png ppm:- | ./build/zenscii -
```

That’s it. (Yes, it’s weird. It works.)

### Common commands (copy/paste)

**Color (best):**

```bash
magick input.png ppm:- | ./build/zenscii - --width 160 --color ansi24
```

**No color (plain ASCII):**

```bash
magick input.jpg ppm:- | ./build/zenscii - --width 160 --color none
```

**256-color + background:**

```bash
magick input.webp ppm:- | ./build/zenscii - --width 120 --color ansi256 --bg
```

## If it looks wrong (quick fixes)

- **Too tall / too squished**: change `--aspect`

```bash
magick input.png ppm:- | ./build/zenscii - --width 160 --aspect 0.45
```

- **Colors are annoying**: turn them off

```bash
magick input.png ppm:- | ./build/zenscii - --color none
```

- **Still getting colors somehow**: force-disable with `NO_COLOR`

```bash
NO_COLOR=1 magick input.png ppm:- | ./build/zenscii - --width 160
```


