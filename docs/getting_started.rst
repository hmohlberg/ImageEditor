Getting Started
===============

This section introduces the ImageEditor graphical user interface (GUI) and guides you through the first steps of working with images. You will learn how to navigate the interface, use the main tools and perform basic image repairs.

The step-by-step instructions will help you become familiar with the essential features and get started with your own image editing tasks.

Layout of the GUI
-----------------

.. image:: images/getting_started/getting_started-01.png
   :alt: ImageEditor user interface

.. image:: images/getting_started/getting_started-02.png
   :alt: ImageEditor user interface


1. Load an Image
----------------

Click **Open** in the top left and load an image from your files or a
sample image, e.g. ``/ImageEditor/samples/images/pm2382o.png``.

.. image:: images/getting_started/getting_started-03.png
   :alt: Open an image


2. Zoom and Pan
---------------

* Zoom in and out with your mouse wheel (Mac: ⌘ + Swipe up or down on the Magic Mouse)
* Shift your image view with **Shift + Left Mouse Button**. 
* **1:1**: Zoom into your image's resolution with the 1:1 button in the
  top menu row.
* **Fit**: Zoom out to fit the image into your current window.

.. image:: images/getting_started/getting_started-04.png
   :alt: Zoom and pan controls


3. Create Your First Layers
---------------------------

First, select the image regions you want to edit. These regions can then be moved, rotated, or transformed to restore the image as accurately as possible. Each selected region becomes a separate layer that you can edit independently.

Let’s start by defining two layers, representing the image regions we will work with in the following steps.

Lasso
~~~~~

* Selecting **Free selection** changes the corresponding menu on the right.
* Select **Create new lasso**.

.. image:: images/getting_started/getting_started-05.png
   :alt: Create a new lasso

* Hold your left mouse button and draw around the image area you would
  like to edit.
* Release your mouse button when you are done.
* You have created your first layer, which is added to the list of layers
  in the **Layers** dock on the right.

.. image:: images/getting_started/getting_started-06.png
   :alt: Lasso layer


Polygon
~~~~~~~

* Selecting **Polygon** changes the corresponding menu on the right.
* Select **Create new polygon**.

.. image:: images/getting_started/getting_started-07.png
   :alt: Create a new polygon

* Set a series of points around the image area you would like to edit.
  It will always be a closed polygon; add as many points as you need.
* Confirm your selection with **Esc**.

.. image:: images/getting_started/getting_started-08.png
   :alt: Polygon layer

* This does not create a new layer yet. You can still adjust the polygon until you are satisfied with the selection.
   * E.g., add a new point by selecting from the menu **Operation mode: Add new polygon point** or type **Ctrl+A** 
   * or **Move** (**Ctrl+M**) a point, or move (**Translate, Ctrl+T**) the whole polygone by selecting the option from the **Operation mode** menu
   * See `Shortcuts <https://imageeditor.readthedocs.io/en/latest/shortcuts.html/>`__ for a complete list and modifiere keys for other systems

.. image:: images/getting_started/getting_started-09.png
   :alt: operation mode menu

* Select **Create new polygon layer** to confirm your polygon selection

.. image:: images/getting_started/getting_started-09_1.png
   :alt: operation mode menu

* You have created your second layer, which is added to the list of layers
  in the **Layers** dock on the right.

.. image:: images/getting_started/getting_started-10.png
   :alt: layer doc
   :width: 300px

4. Edit the Layers
------------------
We have now defined two layers, each representing an image region that we can edit independently, for example by moving, rotating, or distorting it.

* Move to the first Layer again, our Lasso selection (Shift + mouse drag)
* Select **Layer 1** from the Layers Dock or the **Layer** menu

.. image:: images/getting_started/getting_started-11.png
   :alt: select layer


* Select **Translate** from the **Layer/Editor** menu
.. image:: images/getting_started/getting_started-12.png
   :alt: select Translate

* Click into the layer and move the selected image region

.. image:: images/getting_started/getting_started-13.png
   :alt: Translation

* Select **Rotate** from the **Layer/Editor** menu
* Click into the layer and move your mouse left or right to rotate the selected image region

.. image:: images/getting_started/getting_started-14.png
   :alt: Rotation

* Translate again
* For the final touch, select **Cage warp** the **Layer/Editor** menu
* Increase the number of **Cage control points**

.. image:: images/getting_started/getting_started-15.png
   :alt: Cage warp, control points

* Zoom in and move cage control points, such that the gap is closed

.. image:: images/getting_started/getting_started-16.png
   :alt: Cage warp, detail
   :width: 300px


5. Save and Load with History
-----------------------------

* Save your image as png-file and the history as json-file to ensure provenance tracking

.. image:: images/getting_started/getting_started-17.png
   :alt: save image and history
   
* For a complete provenance tracking add some meta data, i.e., your full name and comments if there is anything specific about your repair

.. image:: images/getting_started/getting_started-18.png
   :alt: enter meta data details
   :width: 400px

* To resume your previous work session, load your saved JSON file.

.. image:: images/getting_started/getting_started-19.png
   :alt: enter meta data details




