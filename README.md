# Zenscii

Convert images and videos to ASCII art in real time - right from terminal or with a GUI , a lightweight tool written in C++ with a qt-6 based gui. 

---

## Features

-  Image → ASCII conversion  
-  Real-time video → ASCII streaming  
-  GUI application (Qt-based)  
-  CLI tool for scripting and pipelines  
-  Multiple color modes (ANSI, grayscale, etc.)  


---


## Installation (Arch Linux) - AUR
```bash
yay -S zenscii
```


---


## Build from Source 

*(If you're on Arch Linux, it's recommended to install via AUR instead)*

Dependecies:
```bash
sudo pacman -S --needed cmake gcc imagemagick ffmpeg
```

If you want the GUI too, also install Qt:

```bash
sudo pacman -S --needed qt6-base
```

Installation:
```bash
git clone https://github.com/Just-Utkarsh/Zenscii.git
cd Zenscii

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run:
```bash
./build/zenscii        # CLI
./build/zenscii-gui    # GUI
```

---

## Usage

**CLI:**

IMG -> ASCII
```bash
magick input.png ppm:- | zenscii --width 160 --color ansi24
#type zenscii in terminal for more options 
```

VIDEO -> ASCII
```bash
ffmpeg -hide_banner -loglevel error -i input.mp4 \-vf "fps=45,scale=320:-1" \ -f image2pipe -vcodec ppm - | \ zenscii --stream --fps 45 --width 160 --color none
```

**GUI**

1.IMG -> ASCII
  - open image
  - select the amount of width you want (more width = more pixels)
  - select color formatting (none - B&W , ANSI24 , ANSI256)
  - change the zoom level if it goes out of bound
  - click save preview to save in the image format
  - click save as .txt to save B&W in form of txt file , can also save in the form of image format.


2.Video -> ASCII
  - In the video row , pre-set the video settings you want to convert
  - select the amount of width you want (more width = more pixels)
  - select FPS (prefrabely 30-45)
  - select color formatting (none - B&W , ANSI24 , ANSI256)
  - load in the video using the open video button , conversion begins
  - press preview video to preview the video
  - save it in mp4 format

---





