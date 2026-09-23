# ImageEditor

Copyright 2026, Forschungszentrum Jülich GmbH

Authors: Hartmut Mohlberg, Daniel Krötz<br>
Institute of Neuroscience and Medicine (INM-1), Forschungszentrum Jülich GmbH

---

A versatile Qt6-based image processing tool with JSON-history support, designed to run on Debian Linux and macOS. It supports both a Graphical User Interface and a headless batch mode for server environments.

## Features

- **Interactive image manipulation tools**: Paint, lasso cut, polygon selection, layer transform (move, rotate, scale, mirror, perspective warp, cage warp). All operations are fully documented and saved in a JSON project file for later replay.
- **Batch Processing**: Apply JSON transformation histories via CLI without a GUI.
- **JSON Project Support**: Load and apply transformation histories from JSON files.
- **Offscreen Optimized**: Suitable for headless servers using the Qt `offscreen` platform plugin.
- **BigTIFF / Pyramid TIFF Viewer**: Tile-based, pan/zoomable viewer for very large TIFF and BigTIFF files stored as image pyramids (multi-resolution IFDs). Supports all standard TIFF compression codecs via libtiff. Requires `libtiff` ≥ 4.0.
- **HDF5 Image Viewer** *(optional)*: Tile-based viewer for large HDF5 image datasets with built-in pyramid support (`/pyramid/00`–`/pyramid/N`). Reads RGB and grayscale datasets with chunk-based tile loading. Requires `libhdf5`.
- **Color LUT / Color Table**: A toolbar color-table selector applies lookup tables (LUT) to the image view and all active layers simultaneously. Includes general-purpose LUTs (Jet, Viridis, Plasma, Inferno, Hot, Cold, Copper) as well as histology-specific LUTs (Nissl, Myelin).
- **Configurable appearance**: Control point size/color, grid color, cage warp color, lasso color, handle color, and cursor appearance are all adjustable in the Config dialog.
- **Persistent window geometry**: Window size and position are saved between sessions.
- **Dock widgets**: Layer list and undo-history docks can be opened at startup via a CLI flag or a Config dialog option.
- **Download progress**: A progress dialog is shown when images or project files are fetched from the internet.

---

## Prerequisites

### Toolchain Requirements

| Requirement | Minimum version | Notes |
| :--- | :--- | :--- |
| **CMake** | 3.22 | |
| **C++ compiler** | C++17 | GCC 9+, Clang 10+, or MSVC 2019+ |
| **Qt6** | 6.5 (recommended 6.6+) | Components: Core, Gui, Widgets, OpenGL, OpenGLWidgets, Svg, Network |
| **libtiff** | 4.0 | Required for BigTIFF/Pyramid TIFF viewer. `TIFFIsBigTIFF` must be present (libtiff < 4.0 is rejected). libtiff ≥ 4.5 additionally enables exact version display in the About dialog. |
| **libhdf5** | any recent | *Optional.* Required only for the HDF5 image viewer (C interface). |

### Qt6 Availability by Linux Distribution

The ImageEditor application requires **Qt6**. Below is a list of Linux distributions that provide native support for Qt6 through their official package repositories (`apt`).

#### Supported Ubuntu Versions
| Ubuntu Version | Release Name | Qt6 Support | Native Repo Version |
| :--- | :--- | :--- | :--- |
| **Ubuntu 26.04** | Resolute Raccoon | ✅ Supported | Qt 6.10.x |
| **Ubuntu 24.10** | Oracular Oriole | ✅ Supported | Qt 6.6.x |
| **Ubuntu 24.04 LTS** | Noble Numbat | ❌ No | Qt 6.4.x |
| **Ubuntu 22.04 LTS** | Jammy Jellyfish | ❌ No | Qt 6.2.4 |
| **Ubuntu 20.04 LTS** | Focal Fossa | ❌ No | (Qt 5 only) |

#### Supported Debian Versions
| Debian Version | Release Name | Qt6 Support | Native Repo Version |
| :--- | :--- | :--- | :--- |
| **Debian 13** | Trixie | ✅ Testing | Qt 6.8.x |
| **Debian 12** | Bookworm | ❌ No | Qt 6.4.2 |
| **Debian 11** | Bullseye | ❌ No | (Qt 5 only) |

> **Note for older systems:** If you are using an unsupported version (like Ubuntu 20.04), you must install Qt6 manually via the Qt Online Installer or use a containerized environment (Docker).

---

### Installation on Linux

```bash
# Update package lists
sudo apt update

# Build tools (GCC, make) and CMake ≥ 3.22
sudo apt install build-essential cmake

# Qt6 — all required components
# qt6-base-dev  → Core, Gui, Widgets, Network, OpenGL, OpenGLWidgets
# qt6-svg-dev   → Svg
sudo apt install qt6-base-dev qt6-svg-dev

# OpenGL development headers (needed by Qt6 OpenGL / OpenGLWidgets)
sudo apt install libgl-dev libegl-dev

# XKB — required by Qt6 on X11 / XCB (Linux only)
sudo apt install libxkbcommon-dev libxkbcommon-x11-dev

# libtiff ≥ 4.0 — required for BigTIFF / Pyramid TIFF viewer
sudo apt install libtiff-dev

# Optional: HDF5 C library — required for HDF5 image viewer
sudo apt install libhdf5-dev
```

> **libtiff version:** `libtiff-dev` on Debian 12+ and Ubuntu 22.04+ provides libtiff 4.x, which satisfies the ≥ 4.0 requirement. Run `dpkg -s libtiff-dev | grep Version` to verify.

> **HDF5:** If `libhdf5-dev` is not installed, CMake will print a notice and the HDF5 viewer will simply not be compiled. All other functionality remains unaffected.

> **qt6-declarative-dev** is *not* required for the default build. Install it only if you need Qt Quick / QML support.

### macOS

```bash
# CMake
brew install cmake

# Qt6 — all required components (Core, Gui, Widgets, OpenGL, Svg, Network, …)
brew install qt

# libtiff ≥ 4.0 — required for BigTIFF / Pyramid TIFF viewer
brew install libtiff

# Optional: HDF5 — required for HDF5 image viewer
brew install hdf5
```

> **Qt not found by CMake?** Run `export CMAKE_PREFIX_PATH=$(brew --prefix qt)` before invoking cmake, or add it to your shell profile.

> **libtiff version:** Homebrew installs the latest libtiff (currently 4.6.x), which satisfies all requirements including the ≥ 4.5 version-display feature.

### Windows with WSL and Debian

* Create a directory where ImageEditor should be installed, open the Powershell there (Open the folder → Right-click → Open in terminal) and enter `wsl --install Debian`
* Wait for Download and Installation to finish — at the end you will be asked to create a username and password.
* After picking a user and password combination the Linux environment will autostart. Now run the following commands:
  ```bash
  sudo apt-get update
  sudo apt-get install qt6-base-dev qt6-declarative-dev qt6-svg-dev git cmake
  sudo apt-get install libtiff-dev libxkbcommon-dev libxkbcommon-x11-dev
  sudo apt-get install libhdf5-dev   # optional, for HDF5 viewer
  sudo git clone https://github.com/hmohlberg/ImageEditor.git
  ```
* Leave the Linux environment by entering `exit`
* To run ImageEditor open the file in `ImageEditor/bin/windows/wsl.bat`

### Windows with WSL and Ubuntu 24.04

```bash
wsl --install Ubuntu-24.04
wsl --set-default Ubuntu-24.04

sudo apt update
sudo apt install qt6-svg-dev qt6-base-dev qt6-declarative-dev
sudo apt install libgles2 libgles2-mesa-dev libegl1-mesa-dev
sudo apt install libtiff-dev libxkbcommon-dev libxkbcommon-x11-dev
sudo apt install libhdf5-dev   # optional, for HDF5 viewer
```

These environment variables are necessary for WSL rendering and can be saved in `.bashrc`:

```bash
export MESA_LOADER_DRIVER_OVERRIDE=d3d12
export GALLIUM_DRIVER=d3d12
```

---

## Build Process

```bash
# Enter the project directory
cd /path/to/ImageEditor

# Create a build folder and configure
mkdir build && cd build
cmake ..

# Compile
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### CMake Options

| Option | Default | Description |
| :--- | :--- | :--- |
| `WITH_TIFF` | `ON` | Enable BigTIFF/Pyramid TIFF support via libtiff. |
| `WITH_HDF5` | `ON` | Enable HDF5 image viewer support. |

Example — build without HDF5:
```bash
cmake -DWITH_HDF5=OFF ..
```

---

### MS Windows (not yet tested)

1. **Install Qt6**: Download the open-source installer from [qt.io](https://www.qt.io/download). Ensure **CMake** and a compiler (like **MinGW** or **MSVC**) are selected during installation.
2. **Setup Environment**: Add the Qt `bin` folder to your System PATH (e.g., `C:\Qt\6.x.x\mingw_64\bin`).

**Build via Command Line (PowerShell/CMD):**
```powershell
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2019_64"
cmake --build . --config Release
```

---

## Usage & CLI Options

To run the editor without a GUI (e.g., via SSH on a server), use the `--batch` flag or simply provide `--output`.

### Command Line Arguments

| Option | Description |
| :--- | :--- |
| `-f, --file <file>` | Path to input image file, HTTP/HTTPS URL, or `.list` file. Supported formats: `png`, `jpg`, `bmp`, `tif`/`tiff` (including BigTIFF pyramids), `h5`/`hdf5`. |
| `--project <json>` | Path to input JSON project file. Use `none` to suppress project loading. |
| `-o, --output <file>` | Path to output image file. Providing this option automatically enables batch mode. |
| `--class <file>` | Path to input image class file. |
| `--config <file>` | Path to a config file. |
| `--batch` | Run in batch mode (no GUI). |
| `--gui` | Force GUI mode even when no input is provided. |
| `--docks` | Show layer and undo-history docks at startup. |
| `--scale <factor>` | Coordinate scale factor between the project file resolution and the BigTIFF resolution (default: `20`, i.e. project at 20 µm, BigTIFF at 1 µm). Also selects the pyramid level for raster export from a BigTIFF. |
| `--force` | Overwrite an existing output file. |
| `--save-json <file>` | In batch mode, save a loaded project file in the latest format version. |
| `--save-intermediate <path>` | In batch mode, save an image after each processing step. |
| `--concatenate` | Concatenate image transformations in batch mode. |
| `--alpha-masking` | Force alpha channel mask processing. |
| `--skip-validation` | Skip all validation checks and force loading of the input image. |
| `--history [n]` | Print the history of the last calls to stdout. Optional: limit to last `n` entries. |
| `--debug` | Enable debug output to stdout. |
| `--verbose` | Enable verbose output to stdout. |
| `--version, -v` | Print version and build information. |
| `--about` | Print version, authors, and license information. |
| `-h, --help` | Display help. |

### Examples

**Open a standard image with a project file:**
```bash
./ImageEditor --file filename.png --project project.json
```

**Open a large BigTIFF pyramid image:**
```bash
./ImageEditor --file large_image.tif
```

**Open a large HDF5 pyramid image:**
```bash
./ImageEditor --file brain_section.h5
```

**Open with docks visible at startup:**
```bash
./ImageEditor --file image.png --docks
```

**Apply a JSON project in batch mode (no GUI):**
```bash
./ImageEditor --file image.png --project task.json --output result.png
```

**Export a BigTIFF pyramid at project resolution (scale factor 20) to PNG:**
```bash
./ImageEditor --file large_image.tif --project none --output result.png --scale 20
```

**Apply a project to a BigTIFF and write a new BigTIFF:**
```bash
./ImageEditor --file large_image.tif --project task.json --output result.tif --scale 20
```

---

## Technical Notes

### BigTIFF / Pyramid TIFF Viewer

Large TIFF and BigTIFF files stored as multi-resolution pyramids (multiple IFDs sorted by decreasing resolution) are opened in a dedicated viewer that never loads the full image into memory. Tiles are read on demand via libtiff (`TIFFReadRGBATile`) and cached in an LRU tile cache. Pan and zoom are smooth at any resolution level because the viewer automatically selects the best pyramid level for the current zoom factor.

- Requires libtiff ≥ 4.0 (`libtiff-dev` on Linux, `brew install libtiff` on macOS).
- All TIFF compression codecs supported by libtiff work (Deflate, LZW, JPEG, uncompressed, …).
- Both tiled and strip-based TIFFs are supported.
- The color LUT toolbar applies to the BigTIFF viewer in real time.

### BigTIFF Batch Processing

In batch mode, BigTIFF input is handled without loading the full image into RAM:

| Input | Output | Behaviour |
| :--- | :--- | :--- |
| BigTIFF | `.tif` / `.tiff` | Tile-based pipeline: project operations are applied per-tile at every pyramid level; output is a new BigTIFF pyramid. |
| BigTIFF | `.png` / other raster | The pyramid level closest to `finest_width / scale` is decoded in full via `TIFFReadRGBAImage`, an optional project is applied in memory via `ImageProcessor`, and the result is saved as a raster image. |

The `--scale` factor (default 20) controls which pyramid level is selected for raster export and how project-space coordinates map to BigTIFF pixel space.

### HDF5 Image Viewer

HDF5 files containing image datasets are opened in a dedicated viewer using the same tile-based approach. The viewer expects the file to contain a `/pyramid/00` dataset (the full-resolution image) and optional downscaled levels at `/pyramid/01`, `/pyramid/02`, … with a factor-of-4 downscale per level. If no pyramid group is present, a single `/Image` dataset is used. Datasets must be 2D (grayscale) or 3D (rows × cols × channels) with `uint8` data type.

- HDF5 support is **optional**: CMake detects `libhdf5` automatically. If not found, a notice is printed and the rest of the application compiles unchanged.
- Can be disabled explicitly with `-DWITH_HDF5=OFF`.
- Requires libhdf5 (`libhdf5-dev` on Linux, `brew install hdf5` on macOS).
- Chunk dimensions from the HDF5 dataset are used as the tile size (typically 2048 × 2048).
- For non-identity color LUTs, RGB images are converted to luminance first, then the LUT is applied.

---

## Acknowledgements

This work was funded by Helmholtz Association's Initiative and Networking Fund through the Helmholtz International BigBrain Analytics and Learning Laboratory (HIBALL) under the Helmholtz International Lab grant agreement InterLabs-0015, the European Union's Horizon Europe Programme under the Specific Grant Agreement No. 101147319 (EBRAINS 2.0 Project) and No. 945539 (Human Brain Project SGA3).

This project was developed in the frame of the [BigBrainProject](https://bigbrainproject.org/index.html) and significantly contributed to the [Julich Brain Atlas](https://julich-brain-atlas.de/).

| | | | |
|----|----|----|----|
|<img height="60" alt="image" src="https://github.com/user-attachments/assets/60deb09b-f10e-480b-9cc8-fc31e5453ded" /> | <img height="60" alt="image" src="https://github.com/user-attachments/assets/44f16600-b2f2-44ff-ae9c-8d2f9bae27ed" /> | <img height="100" alt="image" src="https://github.com/user-attachments/assets/bb0d4599-10ff-49d7-9163-6bc655d4a842" /> | <img height="100" alt="image" src="https://github.com/user-attachments/assets/cd8a7520-1b51-453b-b36c-e209c32bb25d" /> |
