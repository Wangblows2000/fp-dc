#!/bin/bash
# Decompile functions of the ORIGINAL executable by demangled-name substring (exact match via nm).
# usage: tools/re/decomp.sh 'Frames::event_func_2414()' ['Frames::event_func_2415()' ...]
# Output: build/re/<name>.c (gitignored RE material; never commit, never paste into src/).
# Needs the Ghidra project build/re/ghidra/fp (created once with -import ... -noanalysis).
G=${GHIDRA:-/opt/ghidra/ghidra_11.4.2_PUBLIC}/support/analyzeHeadless
B=${FP_BIN:-/home/claude/fprun/bin64/Chowdren}
cd "$(dirname "$0")/../.."
[ -f build/re/nm.txt ] || nm -C "$B" > build/re/nm.txt
args=()
for n in "$@"; do
  a=$(awk -v n="$n" '{ s=$0; sub(/^[0-9a-f]+ [A-Za-z] /, "", s); if (s == n) { print "0x" $1; exit } }' build/re/nm.txt)
  [ -z "$a" ] && { echo "no symbol: $n"; continue; }
  args+=("$(echo "$n" | sed 's/[^A-Za-z0-9_]/_/g; s/__*$//')=$a")
done
$G build/re/ghidra fp -process Chowdren -noanalysis -readOnly -scriptPath tools/re/ghidra -postScript Decomp.java build/re "${args[@]}" 2>&1 | grep -E "OK |NOT FOUND|failed|ERROR"
