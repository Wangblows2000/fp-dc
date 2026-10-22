#!/bin/bash
# Fresh boot -> controllable Dragon Valley 1 (frame index 20) through the REAL menus (v1.21.5 Linux).
# Verified 2026-10-22: 0 (GalaxyTrail) -> 82 (opening) -> 2 (logo) -> 3 (title/file/mode) -> 81 (prologue) -> 75 (Lilac & Carol) -> 20 (DV1)
set -e
cd /home/claude/fpdc
sh tools/orig/start_orig.sh
export DISP=:93 DISPLAY=:93
tools/orig/key.sh - wait:4 Return wait:2 Return wait:3       # splash/opening -> logo -> title menu
tools/orig/key.sh - Return wait:2 Return wait:2 Return wait:2 # START GAME -> NEW File 1 (Normal) -> ADVENTURE
W=$(xdotool search --name 'Freedom Planet' | head -1); xdotool windowfocus --sync $W
n=0
while ! grep -aq 'Setting frame: 20' /tmp/fp_run.log && [ $n -lt 300 ]; do   # z advances dialogue
  xdotool keydown z; timeout 0.1 tail -f /dev/null; xdotool keyup z; timeout 0.6 tail -f /dev/null; n=$((n+1))
done
grep -a 'Setting frame' /tmp/fp_run.log | tail -1
