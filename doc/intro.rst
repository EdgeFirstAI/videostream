Introduction
============



GStreamer
---------

VAAL includes a GStreamer plug-in which provides integration of the middleware
as a no-code/low-code interface usable from the command-line or further extended
using the GStreamer API from C, Python, or other languages supported through
GObject Introspection.

A detection model can be run from the command-line using a simple interface such
as the following which will print box information to the console.

gst-launch-1.0 v4l2src ! deepviewrt model=model.rtm ! boxdecode verbose=true ! fakesink

Through a quick mux the bounding boxes can in turn be drawn over the original
video and displayed to the screen.

gst-launch-1.0 mux example...

Under-the-hood VAAL ensures an optimal pipeline to feed the inference engine
and generating results in easy-to-use data structures.  This means creating user
applications with the latest vision-based AI models is easily achieved.

Programming interface
---------------------

VAAL provides a simple programming interface through the C API along with
bindings for Python and integration into many common frameworks.  Our goal is to
keep VAAL out of your way with a simple API to load models, feed inputs, and
read results in the smallest amount of time.

Examples are provided for interface with VAAL from C, Python, as well as
programmatic access to GStreamer from C and Python.  Examples will demonstrate
interoperability with other frameworks such as VideoStream, OpenCV, Numpy, and
others.

Support
-------

DeepView AI Middleware is commercially supported by Au-Zone Technologies through
our site https://support.deepviewml.com where you can access the latest versions
of the software as well as numerous resources to help you implement
state-of-the-art ML/AI applications.
