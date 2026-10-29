#!/bin/sh
# Freedom Planet -> the PC version for Linux (fpdc + data, written next to this script), from your copy of the game.
#   ./build_linux.sh ["/path/to/Freedom Planet/Assets.dat"] [--pc-out DIR] [-j N] [--force] [--local-tools]
#   (without the path: an Assets.dat next to the project or up to three folders above it, else a file dialog). See BUILDING.md.
# First run: downloads a portable Python and the pinned tools (about 70 MB) into build/cache/; nothing is installed.
# The program has SDL2 linked in: no libsdl2 package is needed.
exec bash "$(dirname "$0")/tools/builder/bootstrap.sh" linux "$0" "the Linux PC version fpdc (next to this script)" "$@"
