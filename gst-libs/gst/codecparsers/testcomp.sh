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

gcc gstav1parser.c -std=gnu90 -Werror=declaration-after-statement -Werror=unused-value -Werror=maybe-uninitialized -Werror=array-bounds -Werror=aggressive-loop-optimizations -Werror=return-type -Werror=unused-but-set-variable -Werror=unused-variable -Werror=parentheses -c -o dummy -I/home/imedia/SourceCode/gstreamer/gstreamer/libs -I/home/imedia/SourceCode/gstreamer/gstreamer -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/ -I/home/imedia/SourceCode/gstreamer/gstreamer/build -I/home/imedia/SourceCode/gsoc/gst-plugins-bad/gst-libs -DGST_USE_UNSTABLE_API
