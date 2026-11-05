#!/bin/bash
#
# Script that prints the current power mode of the Orin Nano Super

#### FIXME improve this all

nvpmodel -q ## --verbose

echo "CPU Freq: " `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq`

