# ImageEditor

Copyright 2026, Forschungszentrum Jülich GmbH

Authors: Hartmut Mohlberg, Daniel Krötz <br>
Institute of Neuroscience and Medicine (INM-1), Forschungszentrum Jülich GmbH

---

A versatile Qt6-based image processing tool with JSON-history support, designed to run on Debian Linux and macOS. It supports both a Graphical User Interface and a headless batch mode for server environments.

## Features
- Basic interactive image manipulation tools: Provides a few image manipulation tools, the use of which can be fully documented and saved in a JSON project file for later use.
- Batch Processing: Run transformations via CLI without a GUI.
- JSON Project Support: Load and apply transformation histories from JSON files.
- Offscreen Optimized: Perfect for headless servers using the 'minimal' platform plugin.
- High-Res Ready: Automatically handles large image allocations (>128MB).
- Vulkan Support: Optional hardware acceleration for rendering.

---

## Prerequisites

### System Requirements & Qt6 Compatibility

The ImageEditor application requires **Qt6**. Below is a list of Linux distributions that provide native support for Qt6 through their official package repositories (`apt`).

### Supported Ubuntu Versions
| Ubuntu Version | Release Name | Qt6 Support | Native Repo Version |
| :--- | :--- | :--- | :--- |
| **Ubuntu 24.10** | Oracular Oriole | ✅ Supported | Qt 6.6.x |
| **Ubuntu 24.04 LTS**| Noble Numbat | ✅ Supported | Qt 6.4.x |
| **Ubuntu 22.04 LTS**| Jammy Jellyfish | ✅ Supported [^1] | Qt 6.2.4 |
| **Ubuntu 20.04 LTS**| Focal Fossa | ❌ No | (Qt 5 only) |

[^1]: This version of QT seems to be missing some functionality used in the precompiled version

### Supported Debian Versions
| Debian Version | Release Name | Qt6 Support | Native Repo Version |
| :--- | :--- | :--- | :--- |
| **Debian 13** | Trixie | ✅ Testing | Qt 6.6 / 6.7 |
| **Debian 12** | Bookworm | ✅ Stable | Qt 6.4.2 |
| **Debian 11** | Bullseye | ❌ No | (Qt 5 only) |

---

### Installation on Linux
To set up the development environment on a supported Debian or Ubuntu system, run the following commands:

```bash
# Update package lists
sudo apt update

# Install Qt6 base development files
sudo apt install qt6-base-dev qt6-declarative-dev qt6-svg-dev

# Optional: Install build tools if not present
sudo apt install build-essential cmake
```

Note for older systems: If you are using an unsupported version (like Ubuntu 20.04), you must install Qt6 manually via the Qt Online Installer or use a containerized environment (Docker).

### macOS

```bash
brew install qt cmake

```

*Note: If CMake fails to find Qt, run: export CMAKE_PREFIX_PATH=$(brew --prefix qt)*

### Windows with WSL

* Create a directory where ImageEditor should be installed, open the Powershell there (Open the folder --> Rightclick --> Open in terminal) and enter `wsl --install Debian`
* Wait for Download and Installation to finish, at the end you will be asked to create a username and password. Can be the same as Windows or different.
* After picking a user and password combination the Linux environment will autostart. Now run the following commands:
  *  `sudo apt-get update`
  *  `sudo apt-get install qt6-base-dev qt6-declarative-dev qt6-svg-dev git`
  *  `sudo git clone https://github.com/hmohlberg/ImageEditor.git`
* Leave the Linux environment by entering `exit`
* To run ImageEditor open the file in ImageEditor/bin/windows/wsl.bat
 
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

To run the editor without a GUI (e.g., via SSH on your Debian server), use the --batch flag.

### Command Line Arguments

| Option | Description |
| --- | --- |
| -f, --file <file> | Path to the input image file. |
| --project <json> | Path to an input JSON-project file (history). |
| --class <file> | Path to input image class file. |
| -o, --output <file> | Path to the output image file. |
| --config <file> | Path to config file. |
| --batch | Run in batch mode (no GUI). |
| --vulkan | Enable hardware accelerated Vulkan rendering. |
| --history | Print history of last calls to stdout. |
| --force | Overwrite existing output file. |
| -v, --version | Displays version information. |
| -h, --help | Displays help options. |

### Examples

**Open an image with a project file:**

```bash
./ImageEditor --file filename.png --project project.json 

```

**Apply a JSON transformation project (Offscreen):**

```bash
./ImageEditor --batch --project task.json -o result.png

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
---

## Acknowledgements
This work was funded by Helmholtz Association’s Initiative and Networking Fund through the Helmholtz International BigBrain Analytics and Learning Laboratory (HIBALL) under the Helmholtz International Lab grant agreement InterLabs-0015, the European Union’s Horizon Europe Programme under the Specific Grant Agreement No. 101147319 (EBRAINS 2.0 Project) and No. 945539 (Human Brain Project SGA3).

This project was developed in frame of the [BigBrainProject](https://bigbrainproject.org/index.html) and significantly contributed to the [Julich Brain Atlas](https://julich-brain-atlas.de/).



| | | | |
|----|----|----|----|
|<img height="60" alt="image" src="https://github.com/user-attachments/assets/60deb09b-f10e-480b-9cc8-fc31e5453ded" /> | <img height="60" alt="image" src="https://github.com/user-attachments/assets/44f16600-b2f2-44ff-ae9c-8d2f9bae27ed" /> | <img height="100" alt="image" src="https://github.com/user-attachments/assets/bb0d4599-10ff-49d7-9163-6bc655d4a842" /> | <img height="100" alt="image" src="https://github.com/user-attachments/assets/cd8a7520-1b51-453b-b36c-e209c32bb25d" /> |






