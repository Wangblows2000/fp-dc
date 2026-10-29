# tables/: data read from the original program, committed so the builder needs only Assets.dat

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
| `strings.json` | gallery names / descriptions / song lists / hints and the credits text: the string literals the program's List* / credits constructors pass | `tools/import_gallery.py elf_strings_all` over the decompiled constructors (Ghidra output, `build/re`) |
| `story/dialog.json` | Adventure: every DialogData object's paragraphs ("speaker anim voice text"), the scene captions, and the story actors' ink (create_<class>: set_shader / blend alpha) | `tools/re/dump_dialog.py` (`make_tables.py story`) |
| `story/scenes.json`, `story/scripts/*.scn`, `story/rows.json` | Adventure: the scene inventory (RE / RV / INFERRED per field) and the ported scenes' scripts and sprite rows, from RV traces of the original (positions, timings, sound ids: numbers only) | by hand / `tools/story_arc1.sh` (`tools/story_rv2scn.py`); docs/ADVENTURE.md |
| `winmap_4085c983.json` | Windows GOG `Assets.dat` (md5 `4085c983...`) image ids -> the Linux image ids the pipeline uses; numbers only (sizes, hot spots, id offsets) | `tools/builder/make_tables.py winmap` |

To regenerate (developers, with the Linux program and the decompiled tree):

    python3 tools/builder/make_tables.py layout /path/to/bin64/Chowdren build/re tables
    python3 tools/builder/make_tables.py story /path/to/bin64/Chowdren tables      # (also part of layout)
    python3 tools/builder/make_tables.py winmap <linux fp-assets dir> <windows fp-assets dir> tables/winmap_4085c983.json

`frames/` is byte-identical to the `build/frames` the project's tools default to (`FP_FRAMES`). Nothing here comes from
`Assets.dat`: no images, sounds or other extracted game files are committed (the builder extracts those from the
user's own copy every time), and no decompiled code.
