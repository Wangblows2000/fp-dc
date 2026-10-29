#!/bin/sh
# The end-of-game scenes (session 24, src/game/ending.c) and the credits text -> <data_dir>/end/ (its own disc pack end.pak).
# usage: tools/import_ending.sh <data_dir>    (e.g. converted2)
# The steps are in tools/build_ending.py (Python, so the drag-and-drop builder runs them on Windows too); this wrapper keeps
# the documented command. Inputs: FP_FRAMES, FP_IMAGES, FP_RE, FP_BIN (see build_ending.py; defaults = the lead's tree).
exec python3 "$(dirname "$0")/build_ending.py" "${1:-converted2}"
