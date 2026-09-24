Tools
=====

ImageEditor provides five interactive editing tools, each activated via its own
toolbar button at the top of the main window.  Only one tool is active at a
time; switching tools swaps the tool-specific control strip shown beneath the
main toolbar.

.. contents::
   :local:
   :depth: 2

Paint Tool
----------

The Paint tool lets you draw freehand strokes directly onto the currently
selected layer with full undo/redo support.

**Activating the tool**

Click the **Brush** action in the edit toolbar.  The cursor changes to a circle
that reflects the current brush size.

**Controls**

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Control
     - Description
   * - Color picker button
     - Opens a colour dialog.  The button's background always shows the active
       paint colour.  It is also updated automatically when you sample a pixel
       with the pipette (middle-click on the image).
   * - Brush size (1–50 px)
     - Spin box that sets the radius of the circular brush in pixels.
   * - Hardness (1–100 %)
     - Slider that controls edge softness.  At 100 % the brush has a hard,
       opaque edge; lower values produce a soft Gaussian falloff towards
       transparency.
   * - Undo / Redo
     - Each completed stroke (mouse-button-release) is one undo step.
       Undo and Redo buttons are also in the main toolbar.

**Workflow**

1. Select the target layer in the :doc:`Layers dock <docks>`.
2. Pick a colour and set brush size and hardness.
3. Click and drag on the canvas to paint.
4. Release the mouse to commit the stroke to the undo stack.
5. Press **Undo** or use :kbd:`Ctrl+Z` to revert the last stroke.

.. note::
   Strokes are painted into the layer's own image buffer.  They do not affect
   the base image or any other layer.


Mask Classes Tool
-----------------

The Mask Classes tool paints semantic class labels onto a dedicated mask layer.
Each class occupies a separate colour channel in the mask image and can be used
to drive the lasso cut operation independently.

**Activating the tool**

Select the **Mask Classes** toolbar button.

**Controls**

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Control
     - Description
   * - Open / Save / Create mask
     - Load an existing mask image (PNG), save the current mask to disk, or
       create a new blank mask the same size as the base image.
   * - Index (active class)
     - Drop-down list of all defined classes, each shown with its assigned
       colour swatch.  Painting and erasing always affects the selected class
       only.
   * - Type (mask cut mode)
     - Defines how this class interacts with the lasso cut:

       - **Ignore mask** – the class is not considered during cutting.
       - **Mask labels** – painted pixels are made transparent in the cut
         layer.
       - **Only mask labels** – only painted pixels are kept in the cut layer.
       - **Copy where mask** – image content is copied wherever this class is
         painted.
       - **Inpainting** – painted regions are filled with inpainting.
   * - Paint / Erase
     - Toggle between painting the class label onto the mask and erasing
       existing labels.
   * - Brush size (1–50 px)
     - Radius of the mask paint brush in pixels.
   * - Opacity (0–100 %)
     - Transparency of the coloured mask overlay on the image.  This is a
       display-only setting; the stored mask data is unaffected.

**Workflow**

1. Create or open a mask file.
2. Select the class index you want to label.
3. Choose a cut mode that matches how this class should be used later.
4. Paint over the regions that belong to this class.
5. Switch to the **Lasso** or **Polygon** tool and apply a cut; the mask
   classes you set here will guide which pixels are included or excluded.

.. note::
   Class colours are defined in the application configuration file (``config.ini``).
   See :doc:`technical` for details on the mask file format.


Free Selection (Lasso) Tool
---------------------------

The Lasso tool lets you draw a freehand selection contour.  When you close the
contour or release the mouse, the enclosed region is extracted from the base
image and placed on a new layer.

**Activating the tool**

Click the **Lasso** action in the lasso toolbar.

**Controls**

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Control
     - Description
   * - Image threshold (0–255)
     - Pixels below this grey value are treated as transparent when the cut is
       applied.  Set to 0 to keep all pixels regardless of brightness.
   * - Mask cut mode
     - Controls how the mask layer (class index 0) interacts with the lasso
       cut:

       - **Ignore mask** – cut without considering the mask.
       - **Mask labels** – masked pixels are made transparent in the new
         layer.
       - **Only mask labels** – only masked pixels are included.
       - **Copy where mask** – image pixels are copied wherever the mask is
         painted.

**Workflow**

1. Optionally set a brightness threshold and mask cut mode.
2. Click and drag to trace the outline of the region you want to extract.
3. Release the mouse (or close the contour back to the start point) to apply
   the cut.
4. A new layer appears in the Layers dock containing the cut-out region.
5. Use the Layer tool to reposition or transform the new layer.

.. note::
   The lasso selection is not stored between sessions; it is consumed when
   the new layer is created.


Polygon Tool
------------

The Polygon tool creates and manipulates vector polygons that can be used to
make precise, editable selections.  Each polygon can be converted into a new
image layer.

**Activating the tool**

Click the **Polygon** toolbar button.

.. rubric:: Index

The **Index** drop-down at the left of the toolbar selects which polygon is
active.  Up to 10 polygons (each with a distinct colour) can exist
simultaneously on the same image.  Switching the index lets you build and edit
multiple independent regions.

.. rubric:: Operation modes

Once a polygon exists, select an operation from the **Operation mode** dropdown:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Mode
     - Description
   * - Select
     - Click a polygon to select it or click near a vertex to make that
       vertex the active point.
   * - Move polygon point
     - Click and drag an existing vertex to a new position.
   * - Add new polygon point
     - Click on a polygon edge to insert a new vertex at that location.
   * - Delete polygon point
     - Click a vertex to remove it from the polygon.
   * - Translate polygon
     - Click and drag anywhere inside the polygon to move the whole shape
       without changing its form.
   * - Smooth polygon
     - Applies one step of Laplace smoothing when triggered, rounding sharp
       corners without changing the vertex count.
   * - Reduce polygon
     - Simplifies the polygon using the Douglas–Peucker algorithm to reduce
       the number of vertices while preserving the overall shape.
   * - Delete polygon
     - Removes the entire polygon from the scene.
   * - Information
     - Double-click the polygon to display statistics (area in µm², perimeter,
       centroid coordinates) computed using the shoelace formula.

**Additional controls**

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Control
     - Description
   * - Undo / Redo
     - Undoes or reapplies the last polygon edit (vertex move, add, delete,
       translate, smooth, or reduce).  Each operation is one undo step.
   * - Create / Update polygon layer
     - Cuts the region enclosed by the current polygon out of the base image
       and places it on a new layer.  The button label changes to
       **Update polygon layer** once a layer exists, allowing you to re-cut
       with an updated polygon.

**Workflow**

1. Select a polygon index.
2. Click on the canvas to place vertices one by one, forming the outline.
3. Close the polygon by clicking near the first vertex.
4. Switch operation modes to refine the shape (move, add, delete points;
   translate; smooth; reduce).
5. Use **Information** mode to verify area and perimeter measurements.
6. Click **Create new polygon layer** to extract the enclosed region onto a
   new layer.


Layer Tool
----------

The Layer tool applies geometric transformations to the currently selected
layer.  All transforms are non-destructive and undoable while you are editing;
they are committed to the layer image when you switch to another tool or
another layer.

**Activating the tool**

Click the **Layer** toolbar button.

**Selecting a layer**

The **Layer** drop-down at the left of the toolbar selects which layer is
active.  Only one layer can be transformed at a time; the layer editor panel
can be opened with the **Editor** button for pixel-level editing.

**Transform modes**

Select a mode from the **Transform** drop-down.  Sub-controls for the chosen
mode appear in the strip to the right:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Mode
     - Controls and description
   * - Translate
     - Displays the current **X** and **Y** offset (read-only).  Drag the
       layer on the canvas to move it; offsets update in real time.
   * - Rotate
     - **Angle** spin box (degrees).  Drag the layer handle on the canvas, or
       type an angle directly.  **Reset perspective** restores the perspective
       transform to the identity.
   * - Scale
     - **X** and **Y** scale factors (read-only displays) updated as you drag
       a corner handle.  **Reset** returns the layer to its original size.
   * - Mirror
     - **Direction** drop-down (Horizontal / Vertical).  Click **Apply** to
       flip the layer image in place.
   * - Perspective
     - Drag the four corner handles to apply a projective transform.
       **Reset perspective** removes all perspective distortion.
   * - Cage warp
     - A free-form deformation tool driven by a cage of control points:

       - **Add control points** / **Remove control points** – toggle between
         adding and removing cage vertices on the canvas.
       - **Fix boundary** – locks the outermost cage points so only interior
         points deform the layer.
       - **Relaxation** – controls how much the deformation field smooths out
         (higher = smoother but less precise).
       - **Stiffness** – controls resistance to bending (higher = stiffer
         cage that deforms less).
       - **Undo** / **Redo** – step through cage-warp edits independently.
       - **Reset** – removes all cage deformation and restores the original
         layer geometry.
