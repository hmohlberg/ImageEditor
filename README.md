# ImageEditor

A versatile Qt6-based image processing tool with JSON-history support, designed to run on Debian Linux and macOS. It supports both a Graphical User Interface and a headless batch mode for server environments.

## Features
- Batch Processing: Run transformations via CLI without a GUI.
- JSON Project Support: Load and apply transformation histories from JSON files.
- Offscreen Optimized: Perfect for headless servers using the 'minimal' platform plugin.
- High-Res Ready: Automatically handles large image allocations (>128MB).
- Vulkan Support: Optional hardware acceleration for rendering.

---

## Prerequisites

### Linux (Debian 12 / Bookworm)
```bash
sudo apt update
sudo apt install qt6-base-dev qt6-base-dev-tools cmake g++ build-essential libvulkan-dev

```

### macOS

```bash
brew install qt cmake

```

*Note: If CMake fails to find Qt, run: export CMAKE_PREFIX_PATH=$(brew --prefix qt)*

---

## Build Process

1. Enter the project directory:
cd /path/to/your/ImageEditor
2. Create a build folder:
mkdir build && cd build
3. Configure and Compile:
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

---

### MS Windows (not yet tested)
1. **Install Qt6**: Download the open-source installer from [qt.io](https://www.qt.io/download). Ensure **CMake** and a compiler (like **MinGW** or **MSVC**) are selected during installation.
2. **Setup Environment**: Add the Qt `bin` folder to your System PATH (e.g., `C:\Qt\6.x.x\mingw_64\bin`).

**Build via Command Line (PowerShell/CMD):**
```powershell
# Create build directory
mkdir build
cd build

# Configure (replace the path to your Qt installation if not in PATH)
cmake .. -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2019_64"

# Compile
cmake --build . --config Release
```
---

## Usage & CLI Options

To run the editor without a GUI (e.g., via SSH on your Debian server), use the --batch flag combined with the -platform minimal Qt option.

### Command Line Arguments

| Option | Description |
| --- | --- |
| -f, --file <file> | Path to the input image file. |
| --project <json> | Path to an input JSON-project file (history). |
| -o, --output <file> | Path to the output image file. |
| --batch | Run in batch mode (no GUI). |
| --vulkan | Enable hardware accelerated Vulkan rendering. |
| --history | Print history of last calls to stdout. |
| -v, --version | Displays version information. |
| -h, --help | Displays help options. |

### Examples

**Convert an image to grayscale (Offscreen):**

```bash
./ImageEditor -platform minimal --batch -f photo.jpg -o output.png

```

**Apply a JSON transformation project:**

```bash
./ImageEditor -platform minimal --batch --project task.json -o result.png

```

---

## Technical Notes

### Cross-Platform Compatibility

The codebase automatically handles the transition from mirrored() (older Qt6) to flipped() (Qt 6.7+) to ensure warning-free compilation:

```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    image = image.flipped(Qt::Vertical);
#else
    image = image.mirrored(false, true);
#endif

```

### Headless Execution

When running on a server without an X11/Wayland session, always append -platform minimal to prevent QPA connection errors.

```

---

