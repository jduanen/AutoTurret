#!/bin/bash
#
# Script to filter objects from YOLO's JSON output

jq -R 'fromjson? | select(.text != "No objects detected") | .objects'
