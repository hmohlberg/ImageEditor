Dock Panels
===========

ImageEditor has two dock panels anchored to the right side of the main window:
**Layers** and **Undo History**.  Both panels can be shown or hidden from the
**View** menu and are dockable (they can be undocked into floating windows).

.. contents::
   :local:
   :depth: 2

Layers Dock
-----------

The Layers dock lists every layer in the current project.  It is the central
place for managing layer visibility, order, and properties.

**Opening the dock**

Show the Layers dock via **View → Layers** or by clicking the eye-icon button
in the main toolbar.

.. rubric:: Layer list

Each row in the list corresponds to one layer and shows:

- A **checkbox** (eye icon) on the left — checked = layer is visible.
- The **layer name** (the region or polygon that produced the layer).

**Reordering layers**

Drag a row up or down to change the stacking order.  The layer drawn on top
in the scene always has the highest z-value; moving a row up in the list
raises it visually above the others.

**Selection**

Click a row to make that layer the active target for the Paint, Layer, and
Polygon tools.  Only one layer can be selected at a time.

**Visibility toggle**

Click the checkbox on any row to show or hide that layer without deleting it.
Hidden layers are excluded from export and from the lasso/polygon cut
operations.

**Context menu**

Right-click a row to open the layer context menu:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Action
     - Description
   * - Save Layer as…
     - Exports the layer's image (including any applied transforms) as a PNG
       file.
   * - Edit Layer
     - Opens the layer image in the built-in layer editor (pixel editor view)
       for direct pixel-level editing.
   * - Delete Layer
     - Asks for confirmation, then removes the layer from the project.  This
       operation is **not** undoable via the undo stack — use with care.
   * - Merge Layer
     - Flattens the selected layer down onto the layer directly beneath it.
   * - Duplicate Layer
     - Creates an exact copy of the layer (image and all transforms) and
       inserts it immediately above the original.  The duplicate is added to
       the undo stack and can be undone.
   * - Rename Layer
     - Opens an inline text editor so you can give the layer a meaningful
       name.
   * - Link to Image
     - Toggles a link between the layer and the base image so that the layer
       moves in sync with any base-image pan or zoom.  The layer name shows
       "(linked)" while this is active.
   * - Center Layer
     - Pans the canvas so the layer is centred in the view.


Undo History Dock
-----------------

The Undo History dock shows the complete undo stack as a scrollable list.  It
gives you a visual overview of every recorded operation and lets you jump to
any earlier state in one click.

**Opening the dock**

Show the Undo History dock via **View → Undo History** or the corresponding
toolbar button.

.. rubric:: Reading the history list

- The **topmost enabled row** is the most recently applied command.
- Rows **below the current index** (greyed out and italic) are operations that
  have been undone; they can be redone.
- Clicking any row jumps the project directly to that state (equivalent to
  pressing Undo or Redo repeatedly until that step is reached).

.. rubric:: History file

The full undo stack can be saved to a JSON file and reloaded later, allowing
you to resume an editing session exactly where you left off.  Three actions in
the main toolbar control this:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Action
     - Description
   * - Sort and merge history
     - Re-orders and merges overlapping paint strokes to reduce file size and
       improve playback performance.
   * - Save history as…
     - Writes the current undo stack to a ``.json`` file of your choosing.
   * - Open history file
     - Loads a previously saved ``.json`` history and replays all commands,
       reconstructing the exact editing state.

.. note::
   The JSON history file stores every command with its parameters (layer index,
   polygon vertices, transform matrices, stroke points, …).  It is the primary
   format for sharing and archiving editing sessions.  See :doc:`technical`
   for the full file-format specification.

.. rubric:: Context menu

Right-click a row in the Undo History dock for additional options:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Action
     - Description
   * - Jump back to this point
     - Sets the undo-stack index to the selected row — identical to clicking
       the row.
   * - Rename command
     - Opens an input dialog to give the selected command a custom name,
       making the history list easier to read for complex sessions.
   * - Delete command
     - Removes the selected command from the undo stack permanently.  Use
       this to clean up accidental operations before saving the history file.
