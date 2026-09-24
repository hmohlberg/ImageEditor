Features
========

ImageEditor combines interactive image editing with a reproducible,
JSON-based project history so that every transformation step can be
replayed, inspected, and applied in batch mode.

Image Editing Tools
-------------------

Paint & Erase
   Free-hand painting with a configurable brush (size, hardness, colour).
   Painted strokes are recorded as undo-able commands.

Lasso Cut
   Draw a free-form selection and cut the enclosed region out of the layer.

Polygon Selection & Annotation
   Create, edit, and label closed polygons directly on the image.
   Each polygon supports:

   - Adding, moving, and deleting individual vertices
   - Laplace smoothing and Douglas–Peucker simplification
   - Translation of the whole polygon
   - Measurement display (area, perimeter, centroid, bounding box)
     scaled to physical units via a configurable pixel scale

Layer Transforms
   Each image layer can be independently:

   - Translated, rotated, scaled, and mirrored
   - Warped using a four-corner perspective transform
   - Deformed with **cage warp** — a grid of control points that
     smoothly deform the layer content via bilinear interpolation

Mask / Class Labels
   Paint semantic class masks on top of a layer.
   Up to 10 label classes with individually configurable colours
   and opacity.

JSON Project History
--------------------

Every operation is appended to a JSON project file.  This enables:

- **Replay**: load a project file to recreate the exact state of all layers
- **Batch processing**: apply a project to a different image via CLI
  without opening a GUI
- **Inspection**: the history is human-readable and can be edited

Batch Mode
----------

Run without a GUI by providing ``--output`` or ``--batch``::

    ./ImageEditor --file image.png --project task.json --output result.png

All operations supported in the GUI are also available in batch mode,
including BigTIFF export.

Large Image Support
-------------------

BigTIFF / Pyramid TIFF
   Multi-resolution TIFF pyramids are displayed in a dedicated tile
   viewer.  Tiles are loaded on demand — the full file is never read
   into RAM.  Supported codecs: Deflate, LZW, JPEG, uncompressed.
   Requires libtiff ≥ 4.0.

HDF5 Image Viewer *(optional)*
   Datasets stored under ``/pyramid/00``–``/pyramid/N`` (factor-of-4
   downscale per level) are displayed with the same tile-based approach.
   Requires libhdf5.

Colour LUT / Colour Table
   A toolbar selector applies lookup tables (Jet, Viridis, Plasma,
   Inferno, Hot, Cold, Copper; histology-specific: Nissl, Myelin)
   to the view and all active layers simultaneously.

Configuration
-------------

The **Config dialog** (accessible from the toolbar) lets you adjust:

- Brush cursor size and colour
- Cage control point size and colour
- Grid, cage warp, lasso, and polygon colours and widths
- Layer transform handle size and colour
- Interpolation mode (nearest / linear / bicubic)
- Pixel scale (µm per pixel) used for polygon measurements
- Per-session logging
