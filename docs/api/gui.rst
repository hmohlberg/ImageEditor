GUI
===

The ``gui/`` module contains the main application window, the image canvas,
and all dialog and viewer windows.

MainWindow
----------

Application main window.  Owns the tool bars, the menu, the status bar, and
the layer editor dock.  Implements the ``IMainSystem`` interface so that core
and layer components can call back into the GUI.

.. doxygenclass:: MainWindow
   :project: ImageEditor
   :members:
   :undoc-members:

ImageView
---------

``QGraphicsView`` subclass that hosts the scene with all ``LayerItem`` objects.
Handles zoom, pan, crosshair overlay, and delegates mouse events to the active
layer and polygon modes.

.. doxygenclass:: ImageView
   :project: ImageEditor
   :members:
   :undoc-members:

LayerEditorView
---------------

Dock panel that lists all layers and lets the user reorder, show/hide, and
select them.

.. doxygenclass:: LayerEditorView
   :project: ImageEditor
   :members:
   :undoc-members:

ConfigDialog
------------

Dialog for editing ``EditorStyle`` settings at runtime.

.. doxygenclass:: ConfigDialog
   :project: ImageEditor
   :members:
   :undoc-members:

BigTiffViewer
-------------

Viewer for large TIFF pyramids served via the local tile server.

.. doxygenclass:: BigTiffViewer
   :project: ImageEditor
   :members:
   :undoc-members:
