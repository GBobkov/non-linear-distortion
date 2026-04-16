#!/bin/bash

echo "========================="
echo "Building Docker image..."
docker build -t aero-experiment .

echo -e "\n=== STOPPING PIPEWIRE ==="
systemctl --user stop pipewire.socket pipewire.service pipewire-pulse.socket pipewire-pulse.service wireplumber.service
sleep 1

echo -e "\n========================="
echo "RUNNING experiment..."
docker run -it --rm \
    --device /dev/snd \
    --group-add 63 \
    -v /dev/shm:/dev/shm \
    aero-experiment

echo -e "\n=== RESTARTING PIPEWIRE ==="
systemctl --user start pipewire.socket pipewire.service pipewire-pulse.socket pipewire-pulse.service wireplumber.service

echo "Done."
