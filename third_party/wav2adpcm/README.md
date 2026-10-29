wav2adpcm.c: unmodified copy of KallistiOS `utils/wav2adpcm/wav2adpcm.c` (KOS git 5f834c1), KOS License
(LICENSE.KOS; the AICA encoder inside is public domain, see the file header). The audio converters
(tools/audio/convert_audio.py) compile it with `cc` when no KallistiOS tree is installed; the drag-and-drop builder uses
the builds of this file published with the project (wav2adpcm.exe, wav2adpcm-linux-x86_64: tools/builder/build_release.sh).
