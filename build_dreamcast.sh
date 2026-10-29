#!/bin/sh
# Freedom Planet -> Dreamcast disc image (fpdc.cdi, written next to this script), from your copy of the game. See BUILDING.md.
#   ./build_dreamcast.sh ["/path/to/Freedom Planet/Assets.dat"] [--cdi FILE] [-j N] [--force] [--local-tools]
#   (without the path: an Assets.dat next to the project or up to three folders above it, else a file dialog)
# First run: downloads a portable Python and the pinned tools (about 70 MB) into build/cache/; nothing is installed.
exec bash "$(dirname "$0")/tools/builder/bootstrap.sh" dc "$0" "the Dreamcast disc image fpdc.cdi (next to this script)" "$@"
