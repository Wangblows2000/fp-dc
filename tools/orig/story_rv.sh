#!/bin/bash
# RV traces of the ORIGINAL game's Adventure story scenes (session 25, tools/orig/trace_story.py).
# Run ONLY from the lead's tree helpers, wrapped:  flock -o /home/claude/fprun/.orig.lock tools/orig/story_rv.sh <mode> <out.log> <nframes>
# modes:
#   new        fresh boot -> START GAME -> NEW File 1 (Normal) -> ADVENTURE, trace from the mode choice on (prologue, first scenes)
#   room:I:G4:G1   after the menu route (Adventure, Lilac), restart frame index I with g0 = 2, g4 = G4, g1 = G1 and trace it
# env: STORY_TAP (default 90: tap J every 90 frames to advance the dialogue), STORY_KEYS, STORY_CLASSES, STORY_SETG
# Output: the trace log (format: trace_story.py) + <out>.png screenshots are not taken (use tools/orig/key.sh).
set -e
MODE=$1; OUT=$(readlink -f "$2"); N=${3:-3000}
cd "$(dirname "$0")/../.."
REPO=$(pwd)
export STORY_FRAMES=${STORY_FRAMES:-$REPO/tables/frames} STORY_TAP=${STORY_TAP:-90}
pkill -x Xvfb || true; timeout 2 tail -f /dev/null || true
trap 'kill -9 $(pgrep -x Chowdren) 2>/dev/null; true' EXIT
cd /home/claude/fpdc
export DISP=:93 DISPLAY=:93
for try in 1 2 3; do                                   # the game window sometimes does not come up in time: start again
  sh tools/orig/start_orig.sh > /dev/null 2>&1
  [ -n "$(xdotool search --name 'Freedom Planet' 2>/dev/null | head -1)" ] && break
  pkill -x Xvfb || true; timeout 3 tail -f /dev/null || true
done
tools/orig/key.sh - wait:4 Return wait:2 Return wait:3 > /dev/null        # splash / opening -> logo -> title menu
tools/orig/key.sh - Return wait:2 Return wait:2 > /dev/null               # START GAME -> NEW File 1 (Normal): mode select
PID=$(pgrep -x Chowdren | head -1)
cd "$REPO"
case $MODE in
new)
  # tracer first (real keys pass through until STORY_TAP_FROM), then Return = ADVENTURE (the default entry)
  STORY_TAP_FROM=${STORY_TAP_FROM:-200} STORY_START_FRAME=3 \
    timeout 7200 gdb -q -batch -p $PID -ex "set \$out=\"$OUT\"" -ex "set \$nframes=$N" \
      -ex "python exec(open('tools/orig/trace_story.py').read())" -ex continue > "$OUT.gdb" 2>&1 &
  GP=$!
  timeout 6 tail -f /dev/null || true
  /home/claude/fpdc/tools/orig/key.sh - Return > /dev/null
  wait $GP || true ;;
room:*)
  IFS=: read -r _ I G4 G1 <<< "$MODE"
  # start the Adventure (as 'new'), let it reach the prologue, then restart the requested room with the globals set
  tools/orig/key.sh - Return wait:3 > /dev/null
  STORY_START_FRAME=$I timeout 7200 gdb -q -batch -p $PID -ex 'break *0x9fcb7a' -ex continue \
    -ex "set {double}(*(long*)0x285ec68 + 0) = 2" -ex "set {double}(*(long*)0x285ec68 + 8) = ${G1:-0}" \
    -ex "set {double}(*(long*)0x285ec68 + 32) = $G4" \
    -ex "call ((void(*)(void*,int))0x9fc8c0)((void*)0x285ec60, $I)" -ex delete \
    -ex "set \$out=\"$OUT\"" -ex "set \$nframes=$N" \
    -ex "python exec(open('tools/orig/trace_story.py').read())" -ex continue > "$OUT.gdb" 2>&1 || true ;;
esac
grep -c . "$OUT"
