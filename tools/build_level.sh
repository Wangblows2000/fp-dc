#!/bin/sh
# Rebuild runtime data for one recovered level into <out_dir>/<name>/ (palettes + banks per level).
# usage: tools/build_level.sh <frame(on_frame_N)> <name> <out_dir>
# The steps are in tools/build_level.py (Python, so the drag-and-drop builder runs them on Windows too); this wrapper keeps
# the documented command. FP_FRAMES / FP_IMAGES: other input dirs (tools/builder); defaults = the lead's tree.
exec python3 "$(dirname "$0")/build_level.py" "${1:-21}" "${2:-dv1}" "${3:-converted2}"
