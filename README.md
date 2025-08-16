# YabokuPlayer

YabokuPlayer is a cross-platform audio player in C++ using [Dear ImGui](https://github.com/ocornut/imgui) + OpenGL, PortAudio.

---

## Features
- Displaying lyrics (Genius API).
- Displaying artist biography (Last.fm API).
- Displaying album cover (Must be include in the audio file).
- Equalizer and Boost Bass, Boost Hight modes.
- Cross-platform operation (Linux, Windows, macOS).
- Playlists.

---

## Dependencies

Require for install:

- **CMake** (>= 3.10)
- **Compiler with C++17 support** (GCC, Clang or MSVC)
- **PortAudio** (`portaudio-2.0`)
- **Python 3** (to get lyrics in `scripts/`, `need requests and beautifulsoup4 lib for python`)
- **pkg-config**
- **GLFW** (part of `external/glfw`)
- **Dear ImGui** (part of `external/imgui`)
- **tinyfiledialogs** (part of `external/tinyfiledialogs`)
- **nlohmann/json** (part of `external/json`)
- **stb_image** (in `include/`)

---

## Building

### Linux
```bash
# Cloning the repository
git clone --recursive https://github.com/Yabokua/Yaboku-Player.git
cd YabokuPlayer

# Installing dependencies
sudo apt install build-essential cmake pkg-config portaudio19-dev python3

# Building
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Running:
```bash
./yaboku_player
```

Installing on the system:
```bash
sudo make install
```
After that, you can run it anywhere with the command:
```bash
yaboku_player
```

---

### Windows (MinGW or MSVC)

1. Install **CMake**, **PortAudio**, **Python 3**, **taglib**, **libcurl**, **vcpkg**.
2. In the terminal:
```powershell
git clone --recursive https://github.com/yourusername/YabokuPlayer.git
cd YabokuPlayer
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```
For MSVC:
```powershell
cmake .. -G "Visual Studio 17 2022"
```
Open `.sln` in Visual Studio and build the project.

---

### macOS
```bash
# Cloning
git clone --recursive https://github.com/Yabokua/Yaboku-Player.git
cd YabokuPlayer
cd YabokuPlayer

# Installing dependencies
brew install cmake pkg-config portaudio python taglib ffmpeg

if "command not found: brew", install Homebrew:
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Building
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.logicalcpu)
```

Running:
```bash
./yaboku_player
```

Installing:
```bash
sudo make install
```

---

## Running

After building, you can run the player with the command:
```bash
./yaboku_player
```
Or, if installed:
```bash
yaboku_player
```
Or find in your directory / apps.

Important: next to the executable file there should be a `resources/` folder containing icons, fonts and configs.

---

## Project structure
```
YabokuPlayer/
├── build/ # Build results
├── config/ # Configurations
├── external/ # Third-party libraries
├── include/ # Header files
├── resources/ # Icons, fonts, desktop file
├── scripts/ # Python scripts
├── src/ # Source code
└── temp/ # Temporary files
```

---

GitHub: https://github.com/Yabokua?tab=repositories
LinkedIn: https://www.linkedin.com/in/vladislav-boiev

## License
The project is distributed under the MIT license.
