# tables/: numeric data read from the original program (no game text)

The port's asset pipeline needs a few tables that are not in `Assets.dat` but in the original game's program: where each
object sits in each level, the animation tables, the backdrop layers, and the gallery and credits texts. They were
read once from the **Linux build of Freedom Planet 1.21.5, `bin64/Chowdren`** (md5 `65f22f98729ed6bddecdf4242197d2cf`)
and are committed here. With them, the builder (`build_dreamcast.*`, BUILDING.md) works from a Windows `Assets.dat`
alone: no Linux program and no decompiler are needed.

| File | What | Made by |
|---|---|---|
| `frames/frame_<N>.json` | layout of frame (screen / level) N-1: size, every object's class, id, position, layer | `tools/re/dump_frames.py` |
| `frames/classes.json` | object class -> constructor address and size | `tools/re/dump_frames.py` |
| `frames/anims.json` | per class: animations, directions, frame image ids, speeds, hot spots | `tools/re/dump_anims.py` |
| `frames/backdrops.json` | backdrop (tiled background) classes: image ids, layers | `tools/re/dump_backdrops.py` |
| `strings.json` | which string literals the program's List* / credits constructors pass (gallery names / descriptions / song lists / hints, the credits), as text tokens | `tools/import_gallery.py elf_strings_all` over the decompiled constructors (Ghidra output, `build/re`), then tokenized |
| `story/dialog.json` | Adventure: every DialogData object's paragraphs (speaker, anim, voice id, text token), the scene captions and subtitles (tokens), and the story actors' ink (create_<class>: set_shader / blend alpha) | `tools/re/dump_dialog.py` (`make_tables.py story`), then tokenized |
| `story/scenes.json`, `story/scripts/*.scn`, `story/rows.json` | Adventure: the scene inventory (RE / RV / INFERRED per field) and the ported scenes' scripts and sprite rows, from RV traces of the original (positions, timings, sound ids: numbers only) | by hand / `tools/story_arc1.sh` (`tools/story_rv2scn.py`); docs/ADVENTURE.md |
| `story/sids.json` | Adventure: each scene's permanent number (save v5 seen flags / story position; append-only, D-122) | by hand (tests/test_story_sids.py freezes them) |
| `winmap_4085c983.json` | Windows GOG `Assets.dat` (md5 `4085c983...`) image ids -> the Linux image ids the pipeline uses; numbers only (sizes, hot spots, id offsets) | `tools/builder/make_tables.py winmap` |

**No game text is committed (D-123).** In `strings.json` and `story/dialog.json` every text is a token
`~T~<hash>~<length>~<prefix hash>` (SHA-256 of the text's bytes, first 20 hex digits; its length; SHA-256 of its first 24
bytes for texts over 24 bytes). The builder resolves the tokens against the string literals of the player's own game
program (`FP.exe` or `bin64/Chowdren`, `tools/textdb.py materialize`); a token cannot be turned back into text without
it. All 1,436 texts are found in both the Linux and the Windows 1.21.5 programs (the Windows build's few differently
worded texts by the prefix hash). `python3 tools/textdb.py check` (and `fpbuild.py selftest`) fail if plain text is
committed here.

To regenerate (developers, with the Linux program and the decompiled tree; the full text goes to `build/text`):

    python3 tools/builder/make_tables.py layout /path/to/bin64/Chowdren build/re tables
    python3 tools/builder/make_tables.py story /path/to/bin64/Chowdren tables      # (also part of layout)
    python3 tools/builder/make_tables.py winmap <linux fp-assets dir> <windows fp-assets dir> tables/winmap_4085c983.json

`frames/` is byte-identical to the `build/frames` the project's tools default to (`FP_FRAMES`). Nothing here comes from
`Assets.dat`: no images, sounds, text or other extracted game files are committed (the builder extracts those from the
user's own copy every time), and no decompiled code.
