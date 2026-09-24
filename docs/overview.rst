Overview
========

ImageEditor is a Qt C++ application for multi-layer image editing and
polygon-based annotation.

Key modules
-----------

``core/``
   Global configuration (:class:`EditorStyle`), system interface, and logging.

``gui/``
   Main window, image view, and tool bars.

``layer/``
   Layer model (:class:`LayerItem`), mask layers, editable polygons and lassos.

``undo/``
   Command objects for Qt's undo/redo framework.

``util/``
   Shared widget utilities and helper functions.

``web/``
   Tile server integration for large-image (BigTIFF) display.

Getting started
---------------

Build with CMake and Qt 6::

   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   ./build/ImageEditor
