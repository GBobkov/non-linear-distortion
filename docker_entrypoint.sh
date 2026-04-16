#!/bin/sh

echo "=== Aero-Acoustic Experiment ==="
echo "Current user: $(whoami)"
echo "User groups: $(groups)"
echo ""
echo "Available audio devices:"
aplay -l 2>&1 || echo "  (none or permission denied)"
echo ""
echo "Checking binary:"
ls -la ./experiment
echo ""
echo "Starting experiment..."
exec ./experiment