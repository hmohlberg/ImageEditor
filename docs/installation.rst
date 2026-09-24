Installation
============

Prerequisites
-------------

.. list-table::
   :header-rows: 1
   :widths: 25 20 55

   * - Requirement
     - Minimum version
     - Notes
   * - CMake
     - 3.22
     -
   * - C++ compiler
     - C++17
     - GCC 9+, Clang 10+, or MSVC 2019+
   * - Qt6
     - 6.5 (6.6+ recommended)
     - Components: Core, Gui, Widgets, OpenGL, OpenGLWidgets, Svg, Network
   * - libtiff
     - 4.0
     - Required for BigTIFF/Pyramid TIFF viewer. ``TIFFIsBigTIFF`` must be present.
   * - libhdf5
     - any recent
     - *Optional.* Required only for the HDF5 image viewer (C interface).

Qt6 Availability
~~~~~~~~~~~~~~~~

.. list-table:: Ubuntu
   :header-rows: 1
   :widths: 30 25 20 25

   * - Version
     - Release Name
     - Qt6 Support
     - Native Repo Version
   * - Ubuntu 26.04
     - Resolute Raccoon
     - ✓
     - Qt 6.10.x
   * - Ubuntu 24.10
     - Oracular Oriole
     - ✓
     - Qt 6.6.x
   * - Ubuntu 24.04 LTS
     - Noble Numbat
     - ✗
     - Qt 6.4.x
   * - Ubuntu 22.04 LTS
     - Jammy Jellyfish
     - ✗
     - Qt 6.2.4

.. list-table:: Debian
   :header-rows: 1
   :widths: 30 25 20 25

   * - Version
     - Release Name
     - Qt6 Support
     - Native Repo Version
   * - Debian 13
     - Trixie
     - ✓ (testing)
     - Qt 6.8.x
   * - Debian 12
     - Bookworm
     - ✗
     - Qt 6.4.2

.. note::
   For older systems without native Qt6 packages, use the Qt Online Installer
   or a containerised environment.

Linux
-----

.. code-block:: bash

   sudo apt update

   # Build tools and CMake
   sudo apt install build-essential cmake

   # Qt6 — all required components
   sudo apt install qt6-base-dev qt6-svg-dev

   # OpenGL headers
   sudo apt install libgl-dev libegl-dev

   # XKB (required by Qt6 on X11/XCB)
   sudo apt install libxkbcommon-dev libxkbcommon-x11-dev

   # libtiff ≥ 4.0 (required for BigTIFF viewer)
   sudo apt install libtiff-dev

   # Optional: HDF5 (required for HDF5 viewer)
   sudo apt install libhdf5-dev

macOS
-----

.. code-block:: bash

   brew install cmake

   # Qt6 — Core, Gui, Widgets, OpenGL, Svg, Network
   brew install qt

   # libtiff ≥ 4.0
   brew install libtiff

   # Optional: HDF5
   brew install hdf5

.. tip::
   If CMake cannot find Qt, run::

       export CMAKE_PREFIX_PATH=$(brew --prefix qt)

   or add it to your shell profile.

Windows — WSL / Debian
-----------------------

Open PowerShell in the target directory and run::

    wsl --install Debian

Then inside the WSL session:

.. code-block:: bash

   sudo apt-get update
   sudo apt-get install qt6-base-dev qt6-svg-dev git cmake
   sudo apt-get install libtiff-dev libxkbcommon-dev libxkbcommon-x11-dev
   sudo apt-get install libhdf5-dev   # optional
   sudo git clone https://github.com/hmohlberg/ImageEditor.git

To run ImageEditor from Windows, open ``ImageEditor/bin/windows/wsl.bat``.

Windows — WSL / Ubuntu 24.04
------------------------------

.. code-block:: bash

   wsl --install Ubuntu-24.04
   wsl --set-default Ubuntu-24.04

   sudo apt update
   sudo apt install qt6-svg-dev qt6-base-dev
   sudo apt install libgles2 libgles2-mesa-dev libegl1-mesa-dev
   sudo apt install libtiff-dev libxkbcommon-dev libxkbcommon-x11-dev
   sudo apt install libhdf5-dev   # optional

Add to ``~/.bashrc`` for WSL rendering:

.. code-block:: bash

   export MESA_LOADER_DRIVER_OVERRIDE=d3d12
   export GALLIUM_DRIVER=d3d12

Build
-----

.. code-block:: bash

   cd /path/to/ImageEditor
   mkdir build && cd build
   cmake ..
   make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

CMake Options
~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - Option
     - Default
     - Description
   * - ``WITH_TIFF``
     - ``ON``
     - Enable BigTIFF/Pyramid TIFF support via libtiff.
   * - ``WITH_HDF5``
     - ``ON``
     - Enable HDF5 image viewer support.

Example — build without HDF5:

.. code-block:: bash

   cmake -DWITH_HDF5=OFF ..
