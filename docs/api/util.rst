Utilities
=========

The ``util/`` module provides header-only geometric algorithms, image
processing helpers, and shared Qt widget utilities.

GeometryUtils
-------------

Header-only namespace with inline functions for coordinate mapping, quad
interpolation, and triangle/polygon hit-testing used by the warp subsystem.

.. doxygennamespace:: GeometryUtils
   :project: ImageEditor
   :members:
   :undoc-members:

BrushUtils
----------

.. doxygenfile:: BrushUtils.h
   :project: ImageEditor

QImageUtils
-----------

.. doxygenfile:: QImageUtils.h
   :project: ImageEditor

MaskUtils
---------

.. doxygenfile:: MaskUtils.h
   :project: ImageEditor

CageWarp
--------

.. doxygenfile:: CageWarp.h
   :project: ImageEditor

QuadWarp
--------

.. doxygenfile:: QuadWarp.h
   :project: ImageEditor

Interpolation
-------------

.. doxygenfile:: Interpolation.h
   :project: ImageEditor

GpuInfo
-------

.. doxygenstruct:: GpuInfo
   :project: ImageEditor
   :members:
   :undoc-members:
