Getting Started
===============

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

* Zoom in and out with your mouse wheel.
* Shift your image view with **Shift + Left Mouse Button**.
* **1:1**: Zoom into your image's resolution with the 1:1 button in the
  top menu row.
* **Fit**: Zoom out to fit the image into your current window.

.. image:: images/getting_started/getting_started-04.png
   :alt: Zoom and pan controls


3. Create Your First Layers
------------------------

Your first select image regions you want to edit, e.g. that you want to move, rotate, transform, etc in order to restore the image as best as possible.
Each selected region defines a layer that you can edit then.

So, first lets define two layers, i.e., regions we want to edit in the follow up steps.

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

* This does not yet create a new layer. You can still edit the polygon if you are not happy.
   * E.g., add a new point by selecting from the menu 'Operation mode: > Add new polygon point' or type cmd+A (on MacOS) | strg+A (on Windows) | ctrl+A (on Linux) 
   * or Move (cmd+M) a point, or move (translate, cmd+T) the whole polygone by selecting the option from the 'OPeration mode' menu

.. note::

   add screenshot operation mode menu.

* Select **Create new polygon layer** to confirm your polygon selection
* You have created your second layer, which is added to the list of layers
  in the **Layers** dock on the right.

.. note::

   add screenshot layer doc.

4. Edit the Layers
------------------
* Move to the first Layer again, our Lasso selection (Shift + mouse drag)
* Select Layer 1 from the Layers Dock or the **Layer** menu

.. note::

   add screenshot select layer.

* Select **Translate** from the **Layer/Editor** menu
.. note::

   add screenshot select Translate.

* Click into the layer and move the selected image region

.. note::

   add screenshot Translation.

* Select **Rotate** from the **Layer/Editor** menu
* Click into the layer and move your mouse left or right to rotate the selected image region

.. note::

   add screenshot Rotation.
* Translate again
.. note::

   add screenshot Translation2.
* For the final touch, select **Cage warp** the **Layer/Editor** menu
* Increase the number of **Cage control points**
.. note::

   add screenshot Cage warp, control points.
* Zoom in and move cage control points, such that the gap is closed

.. note::

   add screenshot Cage warp1 and warp2






