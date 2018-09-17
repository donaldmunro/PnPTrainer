# PnPTrainer

PnPTrainer is an open source Computer Vision (CV) tool matching 3D points from a
pointcloud loaded from a .ply file to 2D features detected in an image and saving the
result to a database (currently just flat YAML, JSON and XML files).(see also
[PlanarTrainer](https://www.github.com/donaldmunro/PlanarTrainer) for a similar type
application for 2D specific feature selection)

## Usage
1. Use the command line to load a ply file and a related image file:
   ```
   Options:
     -h, --help         Displays this help.
     -v, --version      Displays version information.
     -s <shaders>       GLSL Shader directory (default shaders/)
     -S <scale>         Scaling factor in point cloud view (default 1)
     -P <point-size>    Point size in point cloud view
     -f                 Flip Y and Z axis for point cloud data (default off).
     -r <click-radius>  Click radius for 2D features (5)
     -b                 Choose only best (by response) 2D feature if multiple
                        features are in click radius.
   Arguments:
      image              Image file (png, jpg)
      three-d             3D pointcloud file (ply)
   ```
   For example PnPTrainer -S 100 -f -b images/20171122163055.165-94478.413933571.jpg ply/20171122163055.165-94478.413933571.ply

   See [TangoCamera](https://www.github.com/donaldmunro/TangoCamera) for an example (requires Google Tango hardware) of an application that
   can generate a ply and image file. A Kinect should be able to do the same.

2. Three windows are opened.
   1. The Image Window which has tabs for the image and feature detector details. Can be used to
      detect 2D features in the image.
   2. The Point Cloud window can be used to view and select points from the point cloud and rotate or
      zoom the point cloud.
   3. The Match Window is used to match 3D points to 2d features using zoomed views from the Image Window and
      the Point Cloud window.

3. A 3D point can be selected in the Point Cloud window by **right** clicking on it. Left clicking and dragging in
   the Point Cloud window rotates the point cloud while the mouse wheel zooms.
   ![Pointcloud Screenshot](doc/pointcloud.png?raw=true "PointCloud Screenshot")

   After selecting a point it is also displayed in a zoomed view in the Match Window:
   ![Matchwin Screenshot](doc/matchwin-1.png?raw=true "Matchwin Screenshot")


4. Select a feature detector from the Feature Detection tab. Several options specific
   to each feature detector may also be amended, although in many cases some specific
   knowledge of the detector may be required. Note SIFT and SURF will only be available
   if compiled with a version of OpenCV which contains non-free (patented) contributions.
   Press the Detect button or press Ctrl-D to perform the detection (you can also change
   the color of the circles representing selected and unselected keypoints before pressing
   Detect.)

   ![Features Screenshot](doc/features.png?raw=true "Features Screenshot")

5. Next select a region containing features in the Image Windows by dragging with the
   left mouse button. After completion of the drag the Match Window will be updated with
   the region.

   ![Match Screenshot 2](doc/matchwin-2.png?raw=true "Match Screenshot 2")


6.  Select a 2D feature point in the Match Window image region to match to the 3D
    point displayed in the 3D region of the Match Window. The selected 3D point can
    also be updated in the Match Window (in this case the PointCloud window selected
    point is also updated).

    The match can be confirmed by pressing Enter in the Match Window (Ctrl-Backspace will
    undo last confirmation). Press Ctrl-S to save matches to a JSON file.


