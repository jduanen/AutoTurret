#!/bin/bash
#
# Script to extract a frame from a clip and store it in a JPEG file

#### FIXME make these optional args and use them in the command
LABEL="test"
CLIPS_PATH="${HOME}/at/clips/${LABEL}_*.mkv"
JPEG_PATH="${HOME}/at/${LABEL}.jpg"
FRAME_NUM=1  #### FIXME take this as a required arg

python3 -c "import cv2,glob;c=cv2.VideoCapture(sorted(glob.glob('/home/jdn/at/clips/test_*.mkv'))[-1]);c.set(1,90);print(cv2.imwrite('/home/jdn/at/test.jpg',c.read()[${FRAME_NUM}]))"
