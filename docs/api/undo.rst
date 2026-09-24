Undo Commands
=============

All operations that modify layer data or polygon geometry are implemented as
``QUndoCommand`` subclasses so that every action can be undone and redone via
the standard Qt undo framework (``QUndoStack``).

Each command stores the data it needs to undo the operation.  Commands are
pushed onto the appropriate stack:

- **Layer-level stack** (``LayerItem::undoStack()``) — transform, paint, mask, lasso, and layer-management commands.
- **Cage-edit stack** (``LayerItem::cageEditStack()``) — control-point moves during an open cage-edit session.
- **Per-polygon stack** (``EditablePolygon::undoStack()``) — polygon vertex edits.

Layer Commands
--------------

.. doxygenclass:: TransformLayerCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: MoveLayerCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: MirrorLayerCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: DeleteLayerCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: DuplicateLayerCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: InvertLayerCommand
   :project: ImageEditor
   :members:

Paint Commands
--------------

.. doxygenclass:: PaintCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: PaintStrokeCommand
   :project: ImageEditor
   :members:

Mask Commands
-------------

.. doxygenclass:: MaskPaintCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: MaskStrokeCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: CreatePolygonMaskCommand
   :project: ImageEditor
   :members:

Cage Warp Commands
------------------

.. doxygenclass:: CageWarpCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: CageEditCommand
   :project: ImageEditor
   :members:

Polygon Commands
----------------

.. doxygenclass:: PolygonMovePointCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: PolygonInsertPointCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: PolygonDeletePointCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: PolygonSmoothCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: PolygonReduceCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: PolygonTranslateCommand
   :project: ImageEditor
   :members:

Other
-----

.. doxygenclass:: LassoCutCommand
   :project: ImageEditor
   :members:

.. doxygenclass:: PerspectiveWarpCommand
   :project: ImageEditor
   :members:
