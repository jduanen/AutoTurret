#!/bin/bash
#
# Script to record a calibration clip of the charuco test board

#### FIXME make these optional args
SECS=120
FPS=60
EXPOSURE=30
GAIN=8
LABEL="calib"

python3 src/record.py --seconds ${SECS} --fps ${FPS} --exposure ${EXPOSURE} --gain ${GAIN} --label "${LABEL}"
