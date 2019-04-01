vpv
===

Image viewer designed for image processing experts.

* supports many image formats (including tiff, png, jpeg)
* can display many images side by side with automatic layouts
* attributs like zoom, panning or constrast are synchronized between windows by default
* image sequences are 'playable' as in a video viewer
* many other features, including: live reloading, image manipulations through plambda or octave, lua scripting, pre-loading and caching, histograms

Compilation
-----------

Requires cmake, SDL2, libpng, libjpeg and libtiff (with their headers).
The repository embeds many dependencies, see the folder ```external/```.

```sh
mkdir build
cd build
cmake ..
make
# change your PATH environnement variable so that the folder build/ is used
# or install the binary 'vpv' system-wide using the following command:
sudo make install
```


Concepts
--------

**Sequence**: ordered collection of images.

**Window**: zone to display one or more sequences. If multiple sequences are attached to a window, 'space' and 'backspace' are use to cycle between them.

**View**: indicate the portion of the image to display. Contains a center position and a zoom factor.

**Player**: indicate which image in the sequence should be displayed. Contains a few other parameters (check out the player GUI to see what it can do).

**Colormap**: transformation of the pixels of an image. Contains contrast/brightness (scale/bias) parameters (output=scale\*pixel+bias) and a shader (to display optical flow, greyscale or color images).

By combining these concepts, vpv becomes a very convenient image/video viewer.

For example, if two sequences share the same view, the same window and the same player, you can visualize the differences between two videos by flipping between them (using 'space').

If you want to see both of them at the same time, attach the two sequences to different windows, and vpv will show them side by side, still keeping the view and timing synchronized.

If the images of multiple sequences don't have the same intensities (one between 0-255 and one between 0-1 for example), you can use two Colormap objects. They automatically adjust their parameters to normalize the image and fit its number of channels.

Command line arguments
----------------------

Sequences can be specified either by escaped globbing (e.g. directory/my_images\_\\\*.png) or by a single image (in this case, it will be a one image sequence). If the argument is a directory, a sequence containing every files of the directory is created.

**nw**, **nv**, **np**, **nc** are used as arguments to respectively create a new window, view, player or colormap for the following sequence.
**aw**, **av**, **ap**, **ac** toggle the automatic creation of objects for each following sequences (for example, *1.png nw 2.png nw 3.png x.png* is equivalent to *aw 1.png 2.png 3.png aw x.png*). Note that **aw** is enabled by default which means that every sequences will be opened in a different window.

Display two sequences side by side:

```bash
vpv input_\*.png output_\*.png
```

Assuming *results/* contains subdirectory such as *results/noise=1/*, *results/noise=2/*, ... with images in them, the following command will display these sequences and the groundtruth in different windows:
```bash
vpv results/*/ groundtruth/
```

Display every images of the directory in the same window ('space' can be used to cycle through them):

```bash
vpv aw *.jpg
```

Shortcuts
---------

Press *h* to show the help window which describes every shortcuts.

Here is a non exhaustive list:
* *number* to bring focus to the n-th window.
* *ctrl+L* / *shift+ctrl+L* to cycle through layouts
* if a window is focused (blue title bar):
  * View
    * left click and drag to move the view.
    * *z* + wheel to change the zoom of the view.
    * *i* / *o* to zoom in and out. Clips to a power of two.
    * *r* to center the view and reset the zoom. *shift+r* to center and set the zoom to 1.
  * Colormap
    * *a* to automatically adjust contrast and brightness.
    * mouse wheel to adjust the contrast.
    * *shift* mouse wheel to adjust the brightness.
    * *s* / *shift+s* to cycle through shaders.
  * Player
    * *p* to toggle playback.
    * *left* / *right* to display the previous/next frame of the sequence(s).

Remarks
-------

Despite its name, vpv cannot open video files. Use ffmpeg to split a video into individual frames. This may change in the future.

In order to be reactive during video playback, the frames are loaded in advance by a thread and put to cache. The cache has a default memory limit of 2GB. Change it using the setting 'CACHE_LIMIT="XGB"' in your vpvrc. On Linux, you can also set 'CACHE_LIMIT="50%"' to use at max 50% of the available RAM at startup.
To automatically invalidate the cache when a file is changed on disk, a filesystem watcher can be enabled using the environment variable 'WATCH' (*env WATCH=1 vpv [args]*).
*F11* can also be used to flush the cache manually.

Similarly to the previous remark, the globbing expansion is only done at startup. If new images are saved to disk, vpv won't see them (except if you update the globbing in the sequence GUI).


Related projects
----------------

[pvflip](https://github.com/gfacciol/pvflip) (vpv is heavily inspired by pvflip)

[nomacs](https://github.com/nomacs/nomacs)

