#!/bin/bash
# MicTest.sh - Launcher for R36S (EmuELEC/ArkOS ports)
# Place in: /storage/roms/ports/

export HOME=/storage
export LD_LIBRARY_PATH=/usr/lib32:/usr/lib:$LD_LIBRARY_PATH

# Change to the app directory inside ports
cd /storage/roms/ports/App_MicTest

# Log output for debugging
./App_MicTest > /tmp/MicTest.log 2>&1
