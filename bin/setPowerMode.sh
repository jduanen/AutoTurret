#!/bin/bash
#
# Script that sets the current power mode of the Orin Nano Super

#### FIXME improve this all and make an option to save this across reboots

# power modes
POWER_MODE_15W=0  # low power mode
POWER_MODE_25W=1  # default
POWER_MODE_MAX=2  # MAXN_SUPER max performance

#### FIXME make this a conditional
# set power mode
sudo nvpmodel -m ${POWER_MODE_MAX}

#### FIXME make this a conditional
# pin the clocks (until next reboot)
sudo jetson_clocks

# check that it was set correctly
nvpmodel -q ## --verbose
echo "CPU Freq: " `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq`

# set to MAXN_SUPER mode and make it stay across reboots
## sed -e 's/PM_CONFIG DEFAULT=1/PM_CONFIG DEFAULT=2/' /etc/nvpmodel/nvpmodel_p3767_0003_super.conf
