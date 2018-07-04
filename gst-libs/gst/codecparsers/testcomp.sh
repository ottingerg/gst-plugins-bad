#!/bin/bash
echo ===========================================================
echo ===========================================================
echo ===========================================================
echo ===========================================================
echo ===========================================================
echo ===========================================================
echo ===========================================================
echo ===========================================================
echo ===========================================================
echo ===========================================================

gcc gstav1parser.c -c -o dummy -I/home/imedia/SourceCode/gstreamer/gstreamer/libs -I/home/imedia/SourceCode/gstreamer/gstreamer -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/ -I/home/imedia/SourceCode/gstreamer/gstreamer/build -I/home/imedia/SourceCode/gsoc/gst-plugins-bad/gst-libs -DGST_USE_UNSTABLE_API
