#!/bin/bash
# MicTest.sh - Launcher for R36S (EmuELEC/ArkOS ports)
# Place in: /storage/EASYROMS/ports/

DIR="$(dirname "$0")"
cd "$DIR"
export HOME=/storage
export LD_LIBRARY_PATH=/usr/lib32:/usr/lib:$LD_LIBRARY_PATH

# Log output for debugging
./App_MicTest > /tmp/MicTest.log 2>&1
