Layer
=====

The ``layer/`` module contains all scene items that represent image layers,
masks, polygons, and the cage-warp deformation mesh.

LayerItem
---------

The central class of the layer system.  Every loaded image is wrapped in a
``LayerItem``, which handles painting, geometric transforms, and cage-warp
deformation.

.. doxygenclass:: LayerItem
   :project: ImageEditor
   :members:
   :undoc-members:

EditablePolygon
---------------

Model class for a single polygon annotation.  Geometry changes always go
through ``QUndoCommand`` subclasses so that undo/redo is supported.

.. doxygenclass:: EditablePolygon
   :project: ImageEditor
   :members:
   :undoc-members:

EditablePolygonItem
-------------------

QGraphicsItem that renders an ``EditablePolygon`` and handles mouse interaction
(point move, add, delete, smooth, reduce, info).

.. doxygenclass:: EditablePolygonItem
   :project: ImageEditor
   :members:
   :undoc-members:

MaskLayerItem
-------------

Renders a semantic segmentation mask as a coloured overlay on the scene.

.. doxygenclass:: MaskLayerItem
   :project: ImageEditor
   :members:
   :undoc-members:

CageMesh
--------

Stores the grid of cage control points used for free-form deformation.

.. doxygenclass:: CageMesh
   :project: ImageEditor
   :members:
   :undoc-members:

PerspectiveTransform
--------------------

.. doxygenclass:: PerspectiveTransform
   :project: ImageEditor
   :members:
   :undoc-members:
