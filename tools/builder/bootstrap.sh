#!/bin/bash
# The Linux side of the build_*.sh scripts: the portable Python (downloaded once into build/cache/, SHA-256 checked), then
# tools/builder/fpbuild.py <target>. usage: tools/builder/bootstrap.sh <target> <script name> <what> [fpbuild arguments]
# FPB_LOCAL=1 (or --local-tools): use python3, ffmpeg, cc, mkdcdisc and KallistiOS of this computer instead.
set -u
HERE=$(cd "$(dirname "$0")/../.." && pwd)
TARGET=$1; SCRIPT=$2; WHAT=$3; shift 3
# the portable Python (python-build-standalone); the same pin as tools/builder/downloads.json
PY_NAME='cpython-3.13.14+20260610-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz'
PY_SHA=d607e18e83227a0527cf3f54a48be973a75358a6885e4be67d4b0359b6f1c880
PY_URL='https://github.com/astral-sh/python-build-standalone/releases/download/20260610/cpython-3.13.14%2B20260610-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz'
CACHE=$HERE/build/cache
PYDIR=$CACHE/python-3.13.14
die() { echo "ERROR: $*" >&2; [ -t 0 ] || { command -v zenity >/dev/null && zenity --error --text="$*" 2>/dev/null; }; exit 1; }

# (no Assets.dat argument, e.g. started from a file manager: fpbuild.py finds it next to the project or asks with zenity)
LOCAL=${FPB_LOCAL:-0}
for a in "$@"; do [ "$a" = --local-tools ] && LOCAL=1; done
if [ "$LOCAL" = 1 ]; then
  command -v python3 >/dev/null || die "python3 is not installed (--local-tools uses this computer's tools; see BUILDING.md)"
  exec python3 -X utf8 "$HERE/tools/builder/fpbuild.py" "$TARGET" "$@" --local-tools
fi
[ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ] || die "$SCRIPT runs on Linux x86_64 (on Windows: the .bat file of the same name)"

sha() { if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }
if [ ! -x "$PYDIR/bin/python3" ]; then
  ARC=
  for d in "${FPDC_PREBUILT:-}" "$HERE" "$HERE/prebuilt" "$CACHE/downloads"; do
    [ -n "$d" ] && [ -f "$d/$PY_NAME" ] && { ARC=$d/$PY_NAME; break; }
  done
  if [ -z "$ARC" ]; then
    mkdir -p "$CACHE/downloads"; ARC=$CACHE/downloads/$PY_NAME
    echo "First run: downloading a portable Python 3.13 (35 MB) into build/cache/ ..."
    if command -v curl >/dev/null; then curl -fL -# --retry 3 -o "$ARC.part" "$PY_URL" || die "download failed: $PY_URL"
    elif command -v wget >/dev/null; then wget -O "$ARC.part" "$PY_URL" || die "download failed: $PY_URL"
    else die "neither curl nor wget is installed: download $PY_URL into $CACHE/downloads/ yourself"; fi
    mv "$ARC.part" "$ARC"
  fi
  [ "$(sha "$ARC")" = "$PY_SHA" ] || die "$ARC has the wrong SHA-256 (damaged download?): delete it and run again"
  rm -rf "$PYDIR.part" && mkdir -p "$PYDIR.part" && tar xzf "$ARC" -C "$PYDIR.part" || die "could not unpack $ARC"
  # the archive holds python/: bin, lib, ...
  rm -rf "$PYDIR" && mv "$PYDIR.part/python" "$PYDIR" && rmdir "$PYDIR.part" || die "could not unpack $ARC"
fi
FPB_MANAGED=1 exec "$PYDIR/bin/python3" -X utf8 "$HERE/tools/builder/fpbuild.py" "$TARGET" "$@"
