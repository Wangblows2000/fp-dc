#!/bin/sh
# Freedom Planet -> the PC version for Linux (output/linux/fpdc + data), from your copy of the game. See BUILDING.md.
#   ./build_linux.sh "/path/to/Freedom Planet/Assets.dat" [--pc-out DIR] [-j N] [--force] [--local-tools]
# First run: downloads a portable Python and the pinned tools (about 70 MB) into build/cache/; nothing is installed.
# The program has SDL2 linked in: no libsdl2 package is needed.
exec bash "$(dirname "$0")/tools/builder/bootstrap.sh" linux "$0" "the Linux PC version output/linux/fpdc" "$@"
