#!/bin/sh
# Freedom Planet -> Dreamcast disc image (output/fpdc.cdi), from your copy of the game. See BUILDING.md.
#   ./build_dreamcast.sh "/path/to/Freedom Planet/Assets.dat" [--cdi FILE] [-j N] [--force] [--local-tools]
# First run: downloads a portable Python and the pinned tools (about 70 MB) into build/cache/; nothing is installed.
exec bash "$(dirname "$0")/tools/builder/bootstrap.sh" dc "$0" "the Dreamcast disc image output/fpdc.cdi" "$@"
