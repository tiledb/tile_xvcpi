#!/bin/bash

echo "Checking for existing tile_xvcpi processes..."

# Kill all running tile_xvcpi processes
sudo pkill -f tile_xvcpi

# Wait a moment to ensure they're gone
sleep 1

# Double-check and force kill if any remain
REMAINING=$(pgrep -f tile_xvcpi)
if [ ! -z "$REMAINING" ]; then
    echo "Force killing remaining processes: $REMAINING"
    sudo kill -9 $REMAINING
fi

echo "Starting tile_xvcpi servers..."

sudo ./tile_xvcpi -m 1 -p 2542 -s 1 -j 200 &
sudo ./tile_xvcpi -m 2 -p 2543 -s 1 -j 200 &
sudo ./tile_xvcpi -m 3 -p 2544 -s 1 -j 200 &
sudo ./tile_xvcpi -m 4 -p 2545 -s 1 -j 200 &

wait