.. _dialogs:

Dialoge
=======

ImageEditor enthält zwei zentrale Dialoge: den **Open-Image-Dialog** zum Öffnen von Bilddateien
und den **Konfigurations-Dialog** zum Anpassen aller applikationsweiten Einstellungen.

.. contents:: Inhalt
   :local:
   :depth: 2


.. _open-dialog:

Open-Image-Dialog
-----------------

Der Dialog wird über ``File → Open`` (oder die entsprechende Schaltfläche in der Werkzeugleiste)
geöffnet und ist in drei Reiter unterteilt.

Reiter: Open from local disk
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. figure:: _static/open_local.png
   :align: center
   :alt: Open from local disk
   :width: 95%

   Reiter *Open from local disk*: eingebetteter Datei-Browser.

Zeigt einen nativen Qt-Dateidialog (``DontUseNativeDialog``).
Ein Doppelklick auf eine Datei schließt den Dialog und öffnet die Datei direkt.

Unterstützte Dateiformate:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Format
     - Beschreibung
   * - PNG, JPG, BMP
     - Gängige Rasterformate
   * - TIF / TIFF
     - Tagged Image File (inkl. BigTIFF)
   * - H5 / HDF5
     - Hierarchical Data Format – öffnet einen weiteren HDF5-Browser-Dialog
   * - .list
     - Textdatei mit Pfadliste (→ Reiter *Open from filelist*)

Reiter: Open from web
~~~~~~~~~~~~~~~~~~~~~~

.. figure:: _static/open_web.png
   :align: center
   :alt: Open from web
   :width: 95%

   Reiter *Open from web*: Bild per URL laden.

Ermöglicht das direkte Laden eines Bildes über eine URL.
Unterstützte Protokolle:

* ``http://`` / ``https://`` – öffnet direkt eine Bilddatei
* ``github://`` – Kurzform für Raw-GitHub-Inhalte; die Basis-URL wird im
  :ref:`config-main`-Reiter des Konfigurations-Dialogs festgelegt.

Reiter: Open from filelist
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. figure:: _static/open_filelist.png
   :align: center
   :alt: Open from filelist
   :width: 95%

   Reiter *Open from filelist*: Liste von Bilddateien.

Lädt eine ``.list``-Textdatei, die zeilenweise ``Titel\tDateipfad``-Einträge enthält.
Nach dem Laden werden alle Einträge in einer zweispaltigen Tabelle (**Title** / **File**)
angezeigt. Ein Doppelklick auf eine Zeile öffnet das entsprechende Bild.

Über **Browse list file** kann eine ``.list``-Datei ausgewählt werden.


.. _config-dialog:

Konfigurations-Dialog
---------------------

Geöffnet über ``Edit → Config``.  Der Dialog besitzt sechs Reiter und drei
globale Schaltflächen:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Schaltfläche
     - Funktion
   * - **Load**
     - Einstellungen aus einer ``.ini``-Datei laden
   * - **Save As**
     - Aktuelle Einstellungen in eine ``.ini``-Datei speichern
   * - **Default**
     - Alle Einstellungen auf die Standardwerte zurücksetzen
   * - **Close**
     - Dialog schließen (Änderungen werden sofort wirksam)


.. _config-main:

Reiter: Main
~~~~~~~~~~~~~

.. figure:: _static/config_main.png
   :align: center
   :alt: Config – Main-Reiter
   :width: 95%

   Reiter *Main*: allgemeine Anwendungseinstellungen.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Option
     - Beschreibung
   * - **Enable logging**
     - Aktiviert Debug-Ausgaben in der Konsole.
   * - **Window size**
     - Anfangsgröße des Hauptfensters: ``default``, ``maximum``, ``fullscreen``, ``mni``.
   * - **Perspective mode**
     - Ermöglicht perspektivische Transformationen für Bildebenen.
   * - **Binary masking**
     - Schränkt Maskenwerte auf 0 oder 1 ein.
   * - **Crosshair**
     - Zeigt ein Fadenkreuz-Overlay an der Cursorposition.
   * - **Show docks at startup**
     - Blendet alle Dock-Fenster beim Start automatisch ein.
   * - **Cursor size** (0–128)
     - Radius des Pinsel-Vorschaukreises in Pixeln.
   * - **Cursor fill color**
     - Füllfarbe des Cursor-Kreises.
   * - **Cursor border color**
     - Randfarbe des Cursor-Kreises.
   * - **GitHub base URL**
     - Basis-URL für das ``github://``-Protokoll beim Laden von Dateien aus GitHub.


Reiter: Cage
~~~~~~~~~~~~~

.. figure:: _static/config_cage.png
   :align: center
   :alt: Config – Cage-Reiter
   :width: 95%

   Reiter *Cage*: Cage-Warp-Einstellungen.

Steuert alle Parameter des **Cage-Warp-Werkzeugs** (Käfig-Verzerrung):

.. list-table::
   :widths: 35 65
   :header-rows: 1

   * - Option
     - Beschreibung
   * - **Claude quads**
     - Aktiviert den Claude-Quad-Subdivisions-Algorithmus.
   * - **Cage quads**
     - Verwendet Quad-basierte Käfigsteuerung statt Triangulierung.
   * - **Use GPU**
     - GPU-beschleunigte Cage-Warp-Berechnung via OpenGL.
   * - **GPU Catmull-Rom interpolation**
     - Catmull-Rom-Spline-Interpolation auf der GPU.
   * - **Live warp** *(nur GPU)*
     - Aktualisiert die Verzerrung interaktiv beim Ziehen von Kontrollpunkten.
   * - **No self-intersection**
     - Verhindert Überkreuzungen von Käfig-Kontrollpunkten.
   * - **Control point radius** (1–32 px)
     - Darstellungsradius der Kontrollpunkt-Handles.
   * - **Control point color**
     - Farbe der Kontrollpunkt-Handles.
   * - **Grid color**
     - Farbe der Käfig-Gitterlinien.
   * - **Cage warp color**
     - Farbe des Käfig-Rahmens.
   * - **Grid columns** (3–33)
     - Anzahl der Spalten im Käfig-Kontrollgitter.
   * - **Square quads**
     - Setzt die Zeilenanzahl automatisch so, dass Quads annähernd quadratisch sind.


Reiter: Scale
~~~~~~~~~~~~~

.. figure:: _static/config_scale.png
   :align: center
   :alt: Config – Scale-Reiter
   :width: 95%

   Reiter *Scale*: Einstellungen für Skalierungs- und Rotations-Handles.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Option
     - Beschreibung
   * - **Handle color**
     - Farbe der Skalierungs- und Rotations-Handles.
   * - **Handle size** (1–64 px)
     - Größe der Transform-Handles in Pixeln.


Reiter: Lasso
~~~~~~~~~~~~~

.. figure:: _static/config_lasso.png
   :align: center
   :alt: Config – Lasso-Reiter
   :width: 95%

   Reiter *Lasso*: Einstellungen für das Freihand-Auswahlwerkzeug.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Option
     - Beschreibung
   * - **Lasso color**
     - Farbe der Lasso-Auswahlkontur.
   * - **Lasso width** (0–20 px)
     - Linienbreite der Lasso-Kontur in Pixeln.


Reiter: Polygon
~~~~~~~~~~~~~~~~

.. figure:: _static/config_polygon.png
   :align: center
   :alt: Config – Polygon-Reiter
   :width: 95%

   Reiter *Polygon*: Einstellungen für das Polygon-Werkzeug.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Option
     - Beschreibung
   * - **Polygon width** (0–50 px)
     - Linienbreite der Polygon-Kontur.
   * - **Handle size** (1–64 px)
     - Größe der Vertex-Handles in Pixeln.
   * - **Handle color**
     - Farbe der Polygon-Vertex-Handles.


Reiter: ImageLayer
~~~~~~~~~~~~~~~~~~

.. figure:: _static/config_imagelayer.png
   :align: center
   :alt: Config – ImageLayer-Reiter
   :width: 95%

   Reiter *ImageLayer*: Einstellungen für Bildebenen-Transformationen.

.. list-table::
   :widths: 35 65
   :header-rows: 1

   * - Option
     - Beschreibung
   * - **Integer move only**
     - Beschränkt das Verschieben von Ebenen auf ganzzahlige Pixelpositionen.
   * - **Overlay opacity** (0.0–1.0)
     - Deckkraft des halbtransparenten Overlays beim Ctrl-Verschieben einer Ebene.
   * - **Rotation single step** (0.01°–90°)
     - Drehwinkel pro Schritt beim Rotieren mit dem Rotations-Handle.
   * - **Handle radius** (1.0–50.0 px)
     - Radius der Rotations- und Skalierungs-Handles.
   * - **Transformation mode**
     - Rendering-Qualität bei Transformationen: ``fast`` oder ``smooth``.
   * - **Interpolation mode**
     - Pixelinterpolation beim Skalieren/Rotieren: ``nearest``, ``linear``, ``bicubic``.
