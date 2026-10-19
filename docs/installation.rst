Pre-built Binaries
==================


.. note::

   **The pre-built binaries are distributed as part of this Git repository.**
   They are **not** separate downloads — clone or pull the repository to obtain them.
   The `bin/` subdirectory is always kept in sync with the latest release.


Executables 
-----------


Ready-to-run executables for version **1.0.2** (architecture-specific) are located in the `bin/` subdirectory.
**Clone the repository once** to get everything you need:

.. code-block:: bash

   git clone https://github.com/hmohlberg/ImageEditor.git
   cd ImageEditor


.. list-table::
   :header-rows: 1

   * - Platform
     - Architecture
     - Directory
     - Notes
   * - **Debian / Ubuntu**
     - x86_64
     - `bin/debian/`
     - Built on Debian 13 (Trixie) with Qt 6.8.
   * - **macOS**
     - Apple Silicon (ARM64)
     - `bin/macos/`
     - Requires macOS 13 (Ventura) or later.
   * - **Windows (WSL)**
     - x86_64
     - `bin/windows/`
     - Run `wsl.bat` to launch ImageEditor inside WSL from Windows Explorer.


.. note::

   **Note:** When launched from the command line without arguments, ImageEditor runs in batch mode and prints help information to the console. Pass `--gui` to open the graphical window.


**Debian / Ubuntu:**

.. code-block:: bash

   cd bin/debian
   ./ImageEditor --gui


**macOS:**

.. code-block:: bash

   bin/macos/ImageEditor --gui


**Windows (via WSL):** double-click `bin/windows/wsl.bat` — the launcher passes `--gui` automatically. WSL must be installed first — see Windows with WSL and Debian.


Installation Packages (download)
--------------------------------


Installer packages for **macOS** (`.dmg`) and **Debian/Ubuntu** (`.deb`) can be downloaded from the
**`GitHub Releases page <https://github.com/hmohlberg/ImageEditor/releases/latest>`_**.

.. list-table::
   :header-rows: 1

   * - Package
     - Platform
     - Notes
   * - `ImageEditor-1.0.2.dmg`
     - macOS 13+
     - Drag **ImageEditor.app** to Applications, then launch from there.
   * - `imageeditor_1.0.2_amd64.deb`
     - Debian 13 / Ubuntu 24.10+
     - `sudo dpkg -i imageeditor_*.deb && sudo apt-get install -f`


.. note::

   If you prefer to build from source instead, follow the instructions in the [Building from Source](#building-from-source) section below.


macOS Gatekeeper warning
~~~~~~~~~~~~~~~~~~~~~~~~


Because the app is not yet signed with an Apple Developer certificate, macOS may show:

.. note::

   *"Apple could not verify that ImageEditor is free of malware."*


To open it anyway, **right-click** the app icon → **Open** → **Open**. After that first confirmation the app launches normally without further warnings.

Alternatively, remove the quarantine flag via Terminal (requires administrator password):

.. code-block:: bash

   sudo xattr -dr com.apple.quarantine /Applications/ImageEditor.app



Updating ImageEditor
====================


Because the binaries are part of this repository, updating ImageEditor is simply a matter of pulling the latest commits.

Linux and macOS
---------------


Open a terminal in the ImageEditor directory and run:

.. code-block:: bash

   git pull


The updated binary is immediately available in `bin/debian/` or `bin/macos/` — no reinstallation required.

Windows (via WSL)
-----------------


On Windows the update is handled automatically by the launcher.
Double-clicking `bin/windows/wsl.bat` will:

1. Open the WSL environment.
2. Run `git pull` inside the repository to fetch the latest binary and project files.
3. Launch ImageEditor with `--gui` automatically.

No manual steps are needed — every time you start the application via `wsl.bat` it checks for and applies updates before launching.

.. note::

   **Prerequisite:** The initial clone must have been performed inside the WSL environment as described in [Windows with WSL and Debian](#windows-with-wsl-and-debian). If the clone was done outside WSL, the `git pull` step in the launcher will have no effect.



Building from Source
====================


Prerequisites
-------------


Toolchain Requirements
~~~~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1

   * - Requirement
     - Minimum version
     - Notes
   * - **CMake**
     - 3.22
     - 
   * - **C++ compiler**
     - C++17
     - GCC 9+, Clang 10+, or MSVC 2019+
   * - **Qt6**
     - 6.5 (recommended 6.6+)
     - Components: Core, Gui, Widgets, OpenGL, OpenGLWidgets, Svg, Network
   * - **libtiff**
     - 4.0
     - Required for BigTIFF/Pyramid TIFF viewer. `TIFFIsBigTIFF` must be present (libtiff < 4.0 is rejected). libtiff ≥ 4.5 additionally enables exact version display in the About dialog.
   * - **libhdf5**
     - any recent
     - *Optional.* Required only for the HDF5 image viewer (C interface).
   * - **onnxruntime**
     - any recent
     - *Optional.* Required only for AI inpainting with the LaMa model.


Qt6 Availability by Linux Distribution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The ImageEditor application requires **Qt6**. Below is a list of Linux distributions that provide native support for Qt6 through their official package repositories (`apt`).

Supported Ubuntu Versions
^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Ubuntu Version
     - Release Name
     - Qt6 Support
     - Native Repo Version
   * - **Ubuntu 26.04**
     - Resolute Raccoon
     - ✅ Supported
     - Qt 6.10.x
   * - **Ubuntu 24.10**
     - Oracular Oriole
     - ✅ Supported
     - Qt 6.6.x
   * - **Ubuntu 24.04 LTS**
     - Noble Numbat
     - ❌ No
     - Qt 6.4.x
   * - **Ubuntu 22.04 LTS**
     - Jammy Jellyfish
     - ❌ No
     - Qt 6.2.4
   * - **Ubuntu 20.04 LTS**
     - Focal Fossa
     - ❌ No
     - (Qt 5 only)


Supported Debian Versions
^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Debian Version
     - Release Name
     - Qt6 Support
     - Native Repo Version
   * - **Debian 13**
     - Trixie
     - ✅ Testing
     - Qt 6.8.x
   * - **Debian 12**
     - Bookworm
     - ❌ No
     - Qt 6.4.2
   * - **Debian 11**
     - Bullseye
     - ❌ No
     - (Qt 5 only)


.. note::

   **Note for older systems:** If you are using an unsupported version (like Ubuntu 20.04), you must install Qt6 manually via the Qt Online Installer or use a containerized environment (Docker).



Installation on Linux
~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

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
   
   # Optional: onnxruntime — required for AI inpainting with the LaMa model
   # No official Debian/Ubuntu package exists; install from the GitHub releases page:
   # https://github.com/microsoft/onnxruntime/releases
   # Download the Linux x64 tgz, then:
   #   sudo tar -xzf onnxruntime-linux-x64-*.tgz -C /usr/local --strip-components=1


.. note::

   **libtiff version:** `libtiff-dev` on Debian 12+ and Ubuntu 22.04+ provides libtiff 4.x, which satisfies the ≥ 4.0 requirement. Run `dpkg -s libtiff-dev | grep Version` to verify.


.. note::

   **HDF5:** If `libhdf5-dev` is not installed, CMake will print a notice and the HDF5 viewer will simply not be compiled. All other functionality remains unaffected.


.. note::

   **qt6-declarative-dev** is *not* required for the default build. Install it only if you need Qt Quick / QML support.


macOS
~~~~~


.. code-block:: bash

   # CMake
   brew install cmake
   
   # Qt6 — all required components (Core, Gui, Widgets, OpenGL, Svg, Network, …)
   brew install qt
   
   # libtiff ≥ 4.0 — required for BigTIFF / Pyramid TIFF viewer
   brew install libtiff
   
   # Optional: HDF5 — required for HDF5 image viewer
   brew install hdf5
   
   # Optional: onnxruntime — required for AI inpainting with the LaMa model
   brew install onnxruntime


.. note::

   **Qt not found by CMake?** Run `export CMAKE_PREFIX_PATH=$(brew --prefix qt)` before invoking cmake, or add it to your shell profile.


.. note::

   **libtiff version:** Homebrew installs the latest libtiff (currently 4.6.x), which satisfies all requirements including the ≥ 4.5 version-display feature.


Windows with WSL and Debian
~~~~~~~~~~~~~~~~~~~~~~~~~~~


- Create a directory where ImageEditor should be installed, open the Powershell there (Open the folder → Right-click → Open in terminal) and enter `wsl --install Debian`
- Wait for Download and Installation to finish — at the end you will be asked to create a username and password.
- After picking a user and password combination the Linux environment will autostart. Now run the following commands:
  ```bash
  sudo apt-get update
  sudo apt-get install qt6-base-dev qt6-declarative-dev qt6-svg-dev git cmake
  sudo apt-get install libtiff-dev libxkbcommon-dev libxkbcommon-x11-dev
  sudo apt-get install libhdf5-dev   # optional, for HDF5 viewer
  git clone https://github.com/hmohlberg/ImageEditor.git
  ```
- Leave the Linux environment by entering `exit`
- To run ImageEditor open the file in `ImageEditor/bin/windows/wsl.bat`

Windows with WSL and Ubuntu 24.04
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

   wsl --install Ubuntu-24.04
   wsl --set-default Ubuntu-24.04
   
   sudo apt update
   sudo apt install qt6-svg-dev qt6-base-dev qt6-declarative-dev
   sudo apt install libgles2 libgles2-mesa-dev libegl1-mesa-dev
   sudo apt install libtiff-dev libxkbcommon-dev libxkbcommon-x11-dev
   sudo apt install libhdf5-dev   # optional, for HDF5 viewer


These environment variables are necessary for WSL rendering and can be saved in `.bashrc`:

.. code-block:: bash

   export MESA_LOADER_DRIVER_OVERRIDE=d3d12
   export GALLIUM_DRIVER=d3d12



Build Process
-------------


.. code-block:: bash

   # Enter the project directory
   cd /path/to/ImageEditor
   
   # Create a build folder and configure
   mkdir build && cd build
   cmake ..
   
   # Compile
   make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)


CMake Options
-------------


.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Description
   * - `WITH_TIFF`
     - `ON`
     - Enable BigTIFF/Pyramid TIFF support via libtiff.
   * - `WITH_HDF5`
     - `ON`
     - Enable HDF5 image viewer support.
   * - `BUILD_MACOS_BUNDLE`
     - `OFF`
     - Build a self-contained macOS `.app` bundle (macOS only). Used by `scripts/create_dmg.sh` to produce a distributable DMG.


Example — build without HDF5:
.. code-block:: bash

   cmake -DWITH_HDF5=OFF ..


Example — build a macOS app bundle:
.. code-block:: bash

   cmake -DBUILD_MACOS_BUNDLE=ON ..
   make -j$(sysctl -n hw.ncpu)


Debian Package (.deb)
---------------------


After a successful build, a `.deb` installer can be generated with CPack:

.. code-block:: bash

   cd build
   cpack


This produces a file named `imageeditor_1.0.2_amd64.deb` (architecture is detected automatically via `dpkg --print-architecture`).

Install the package with:
.. code-block:: bash

   sudo dpkg -i imageeditor_1.0.2_amd64.deb
   sudo apt-get install -f   # resolve any missing runtime dependencies


The package installs the `ImageEditor` binary to `/usr/local/bin/` and registers a `.desktop` entry so it appears in the application menu.


MS Windows (not yet tested)
---------------------------


1. **Install Qt6**: Download the open-source installer from `qt.io <https://www.qt.io/download>`_. Ensure **CMake** and a compiler (like **MinGW** or **MSVC**) are selected during installation.
2. **Setup Environment**: Add the Qt `bin` folder to your System PATH (e.g., `C:\Qt\6.x.x\mingw_64\bin`).

**Build via Command Line (PowerShell/CMD):**
.. code-block:: powershell

   mkdir build
   cd build
   cmake .. -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2019_64"
   cmake --build . --config Release
