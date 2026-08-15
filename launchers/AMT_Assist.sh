#!/bin/bash
# AMT_Assist.sh - Launcher for R36S (EmuELEC/ArkOS ports)
# Place in: /storage/roms/ports/

export HOME=/storage
export LD_LIBRARY_PATH=/usr/lib32:/usr/lib:$LD_LIBRARY_PATH

# Change to the app directory inside ports
cd /storage/roms/ports/App_AMT_Assist

# Cấp quyền đọc input cho app (tránh lỗi permission denied khi cắm phím)
sudo chmod a+rw /dev/input/event*

# Log output for debugging
./AMT_Assist > /tmp/AMT_Assist.log 2>&1
