Usage
=====

Graphical Mode
--------------

Launch the editor with an image and an optional project file::

    ./ImageEditor --file image.png --project project.json

If no ``--file`` is given, the editor opens with an empty canvas.
Use ``--docks`` to show the layer-list and undo-history panels at startup.

Batch Mode
----------

Providing ``--output`` automatically enables batch mode (no GUI)::

    ./ImageEditor --file image.png --project task.json --output result.png

This is the recommended mode for server environments or scripted pipelines.
Use Qt's ``offscreen`` platform plugin for machines without a display::

    QT_QPA_PLATFORM=offscreen ./ImageEditor --file image.png --project task.json --output result.png

Command-Line Options
--------------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``-f, --file <file>``
     - Input image file, HTTP/HTTPS URL, or ``.list`` file.
       Supported formats: ``png``, ``jpg``, ``bmp``, ``tif``/``tiff``
       (including BigTIFF pyramids), ``h5``/``hdf5``.
   * - ``--project <json>``
     - Path to a JSON project file. Use ``none`` to suppress project loading.
   * - ``-o, --output <file>``
     - Output image file. Providing this enables batch mode automatically.
   * - ``--class <file>``
     - Input image class (mask) file.
   * - ``--config <file>``
     - Path to a config ``.ini`` file.
   * - ``--batch``
     - Force batch mode (no GUI).
   * - ``--gui``
     - Force GUI mode even when no input is provided.
   * - ``--docks``
     - Show layer and undo-history dock panels at startup.
   * - ``--scale <factor>``
     - Coordinate scale factor between project-file resolution and BigTIFF
       resolution (default: ``20``, meaning project at 20 µm, BigTIFF at 1 µm).
       Also selects the pyramid level for raster export from a BigTIFF.
   * - ``--force``
     - Overwrite an existing output file.
   * - ``--save-json <file>``
     - In batch mode: save the loaded project in the latest format version.
   * - ``--save-intermediate <path>``
     - In batch mode: save an image after each processing step.
   * - ``--concatenate``
     - Concatenate image transformations in batch mode.
   * - ``--alpha-masking``
     - Force alpha channel mask processing.
   * - ``--skip-validation``
     - Skip all validation checks and force loading of the input image.
   * - ``--history [n]``
     - Print the history of the last calls to stdout.
       Optional: limit to the last ``n`` entries.
   * - ``--debug``
     - Enable debug output to stdout.
   * - ``--verbose``
     - Enable verbose output.
   * - ``--version, -v``
     - Print version and build information.
   * - ``--about``
     - Print version, authors, and license information.
   * - ``-h, --help``
     - Display help.

Examples
--------

Open a standard image with a project file:

.. code-block:: bash

   ./ImageEditor --file filename.png --project project.json

Open a large BigTIFF pyramid image:

.. code-block:: bash

   ./ImageEditor --file large_image.tif

Open an HDF5 pyramid image:

.. code-block:: bash

   ./ImageEditor --file brain_section.h5

Open with layer and history panels visible:

.. code-block:: bash

   ./ImageEditor --file image.png --docks

Apply a JSON project in batch mode:

.. code-block:: bash

   ./ImageEditor --file image.png --project task.json --output result.png

Export a BigTIFF pyramid at project resolution (scale factor 20) to PNG:

.. code-block:: bash

   ./ImageEditor --file large_image.tif --project none --output result.png --scale 20

Apply a project to a BigTIFF and write a new BigTIFF:

.. code-block:: bash

   ./ImageEditor --file large_image.tif --project task.json --output result.tif --scale 20
