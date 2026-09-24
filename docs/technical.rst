Technical Notes
===============

BigTIFF / Pyramid TIFF Viewer
------------------------------

Large TIFF and BigTIFF files stored as multi-resolution pyramids (multiple
IFDs sorted by decreasing resolution) are opened in a dedicated tile viewer
that **never loads the full image into memory**.

Tiles are read on demand via ``TIFFReadRGBATile`` and cached in an LRU tile
cache.  Pan and zoom are smooth at any zoom factor because the viewer
automatically selects the best pyramid level for the current view.

- Requires libtiff ≥ 4.0 (``libtiff-dev`` on Linux, ``brew install libtiff`` on macOS).
- All TIFF compression codecs supported by libtiff (Deflate, LZW, JPEG, uncompressed, …).
- Both tiled and strip-based TIFFs are supported.
- The colour LUT toolbar applies to the BigTIFF viewer in real time.

BigTIFF Batch Processing
~~~~~~~~~~~~~~~~~~~~~~~~

In batch mode, BigTIFF input is handled without loading the full file into RAM:

.. list-table::
   :header-rows: 1
   :widths: 15 20 65

   * - Input
     - Output
     - Behaviour
   * - BigTIFF
     - ``.tif`` / ``.tiff``
     - Tile-based pipeline: project operations are applied per-tile at
       every pyramid level; the result is a new BigTIFF pyramid.
   * - BigTIFF
     - ``.png`` / other raster
     - The pyramid level closest to ``finest_width / scale`` is decoded
       in full via ``TIFFReadRGBAImage``, the project is applied in
       memory, and the result is written as a raster image.

The ``--scale`` factor (default 20) controls which pyramid level is selected
for raster export and how project-space coordinates map to BigTIFF pixel space.

HDF5 Image Viewer
-----------------

HDF5 files are opened in a dedicated tile-based viewer.  Expected dataset
layout:

- ``/pyramid/00`` — full-resolution image (mandatory)
- ``/pyramid/01``, ``/pyramid/02``, … — downscaled levels at factor-of-4 per level (optional)
- ``/Image`` — fallback when no pyramid group is present

Datasets must be 2D (grayscale) or 3D (rows × columns × channels) with
``uint8`` data type.  Chunk dimensions from the HDF5 dataset are used as the
tile size (typically 2048 × 2048).

- HDF5 support is **optional**: CMake detects ``libhdf5`` automatically.
- Can be disabled with ``-DWITH_HDF5=OFF``.
- Requires libhdf5 (``libhdf5-dev`` on Linux, ``brew install hdf5`` on macOS).
- For non-identity colour LUTs, RGB images are converted to luminance first.

JSON Project File
-----------------

Every editing operation is recorded as a JSON entry in the project file.
The file can be:

- loaded back into the GUI to restore the complete editing state
- applied to a different image in batch mode
- shared, edited by hand, or inspected with any text editor

The ``--save-json`` option re-serialises a loaded project in the latest
schema version, which is useful after migrating from an older release.

Pixel Scale
-----------

Many measurement and export operations depend on a **pixel scale factor** that
converts pixel distances to physical (metric) units.  The default is
**20 µm/pixel** (matching the 20 µm resolution of typical BigBrain sections).

The scale is configurable:

- In the **Config dialog** under *Main → pixelScale*
- In the config ``*.ini`` file: ``Main/pixelScale = 20.0``
- Via the ``--scale`` CLI option (for BigTIFF pyramid level selection)

Polygon measurement results (area, perimeter, bounding box) shown in the
**Information** mode are automatically scaled to µm and mm using this factor.

Offscreen / Headless Operation
-------------------------------

On servers without a display, use Qt's ``offscreen`` platform plugin::

    export QT_QPA_PLATFORM=offscreen
    ./ImageEditor --file image.png --project task.json --output result.png

No framebuffer or X11 session is required in this mode.
