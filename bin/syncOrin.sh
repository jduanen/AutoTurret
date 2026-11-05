#!/bin/bash
#
# Sync AutoTurret repo and Orin Nano Super

# change to AutoTurret repo
cd ${HOME}/Code/AutoTurret

# sync from here to Orin Nano
rsync -av --exclude .git/ --exclude at/ --exclude venv/ --exclude __pycache__/ --exclude '*.zip' ./ orinnano.lan:Code/AutoTurret/

# sync outputs from Orin Nano to here
rsync -av --exclude '*.mkv' orinnano.lan:at/ at/
