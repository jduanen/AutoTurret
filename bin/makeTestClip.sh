#!/bin/bash
#
# Script to capture test clip (to evaluate gain and exposure settings)

#### FIXME make these optional args

SECS=3
FPS=60
EXPOSURE=30
GAIN=8
LABEL="test"

python3 src/record.py --seconds ${SECS} --fps ${FPS} --exposure ${EXPOSURE} --gain ${GAIN} --label "${LABEL}"
