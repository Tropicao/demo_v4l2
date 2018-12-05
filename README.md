# V4L2 capture example

## Brief
This repository contains basic code to show how to capture raw video frames from an USB webcam, using V4L2 APIs.

## Usage  
Usage : ./builddir/demo_v4l2 [-c] [-d device] [-o directory [-f format]]  
Options :  
  * -c           print video device capabilities and quit  
  * -C           print video controls capabilities and quit  
  * -d           device to use (default: /dev/video0)  
  * -f           output format (can be 'raw' or 'jpeg', default = raw)  
  * -F           print device formats and quit  
  * -h           prints this help  
  * -o           output directory to use (default: local directory)  
