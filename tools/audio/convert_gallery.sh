#!/bin/sh
# Gallery sounds (session 15; spec_gallery 3.2 / 7): the sound cards' voice clips and samples -> <out_dir> (data/gal/, loose).
# usage: convert_gallery.sh <extracted_audio_dir> <voices_dir[:voices_dir...]> <fallback_voices_dir> <out_dir>
# The conversion is tools/audio/convert_audio.py gallery (Python, so the drag-and-drop builder runs it on Windows too); this
# wrapper keeps the documented command.
exec python3 "$(dirname "$0")/convert_audio.py" gallery "$@"
