#!/bin/bash
#
# Script to calibrate a camera using a calibration clip

#### FIXME make these optional args
DIRECTORY="${HOME}/at/"
CALIB_FILE="calib_*.mkv"  #### FIXME take this as a required arg
BOARD_PATH="${HOME}/at/board_letter/board.json"

python3 src/calibrate.py "${DIRECTORY}${CALIB_FILE}" --board "${BOARD_PATH}"
