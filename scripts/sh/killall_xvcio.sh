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

