#!/usr/bin/env python3
"""
The Dreamcast disc: the disc tree from a data dir (copy, pre-twiddle, pack, pad) and the CDI image (mkdcdisc).
`make cdi` / `make cdi-image` run this file; so does the drag-and-drop builder (tools/builder/fpbuild.py), on Windows too.

  make_disc.py tree <data_dir> <disc_dir> [--bgm FILE...] [--demo FILE]
      <disc_dir>/data: levels.txt; per level of levels.txt its *.tex *.spr *.lv2 *.pal (*.bin: parallax pieces); ui/ (scenes,
      banks, font), bgm/ (the ADPCM music library, or the --bgm files only), sfx/ (ADPCM sfx / voices), end/ (the end of
      the game), gal/ (gallery: loose, on demand); then tools/twiddle_disc.py (pre-twiddled textures), tools/pack_disc.py
      (one .pak per directory), --demo as data/demo.txt (verification builds: scripted input, plat_dc demo_load) and
      tools/pad_disc.py (every file a multiple of 32 bytes).
  make_disc.py cdi <fpdc.elf> <data_dir> <disc_dir> <out.cdi> [--mkdcdisc PATH] [--bgm FILE...] [--demo FILE]
      the tree, then mkdcdisc (bootable MIL-CD image, data under /cd/data).
"""
import argparse, glob, os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
LABEL = ['-n', 'FREEDOM PLANET DC TEST', '-a', 'fpdc']

def copy(pattern_dir, patterns, dest, required=True):
    os.makedirs(dest, exist_ok=True)
    n = 0
    for p in patterns:
        files = sorted(glob.glob(os.path.join(glob.escape(pattern_dir), p)))
        if not files and required: raise SystemExit('make_disc: no %s in %s' % (p, pattern_dir))
        for f in files:
            if os.path.isfile(f): shutil.copyfile(f, os.path.join(dest, os.path.basename(f))); n += 1
    return n

def tree(data, disc, bgm=None, demo=None):
    if os.path.isdir(disc): shutil.rmtree(disc)
    d = os.path.join(disc, 'data'); os.makedirs(d)
    shutil.copyfile(os.path.join(data, 'levels.txt'), os.path.join(d, 'levels.txt'))
    for lv in open(os.path.join(data, 'levels.txt'), encoding='utf-8').read().split():
        copy(os.path.join(data, lv), ['*.tex', '*.spr', '*.lv2', '*.pal'], os.path.join(d, lv))
        copy(os.path.join(data, lv), ['*.bin'], os.path.join(d, lv), required=False)    # *_par.bin: parallax pieces
    # front end (ui scenes + banks + font), ADPCM music library (.adp), ADPCM sfx / voices
    copy(os.path.join(data, 'ui'), ['*.ui', '*.spr', '*.tex', '*.bin'], os.path.join(d, 'ui'))
    os.makedirs(os.path.join(d, 'bgm'))
    for f in (bgm or sorted(glob.glob(os.path.join(glob.escape(os.path.join(data, 'bgm')), '*.adp')))):
        shutil.copyfile(f, os.path.join(d, 'bgm', os.path.basename(f)))
    copy(os.path.join(data, 'sfx'), ['*.wav'], os.path.join(d, 'sfx'))
    copy(os.path.join(data, 'end'), ['*'], os.path.join(d, 'end'))     # the end of the game (tools/import_ending.sh): end.pak
    copy(os.path.join(data, 'gal'), ['*.tex', '*.pal', '*.wav', '*.txt'], os.path.join(d, 'gal'))   # gallery: loose, on demand
    copy(os.path.join(data, 'gal'), ['*.adp'], os.path.join(d, 'gal'), required=False)            # (.adp: streamed sound cards)
    py = [sys.executable, '-B']
    subprocess.run(py + [os.path.join(HERE, 'twiddle_disc.py'), d], check=True)
    subprocess.run(py + [os.path.join(HERE, 'pack_disc.py'), d], check=True)
    if demo: shutil.copyfile(demo, os.path.join(d, 'demo.txt'))
    subprocess.run(py + [os.path.join(HERE, 'pad_disc.py'), d], check=True)

def cdi(elf, data, disc, out, mkdcdisc, bgm=None, demo=None):
    tree(data, disc, bgm, demo)
    # run in the output folder with the bare file name: the CDI records its output path, which would put the builder's
    # folder (and the user's name) into the image
    out = os.path.abspath(out); a = os.path.abspath
    subprocess.run([a(mkdcdisc), '-e', a(elf), '-D', a(disc)] + LABEL + ['-o', os.path.basename(out), '--allow-overwrite',
                                                                   '--no-banner', '-N'], check=True, cwd=os.path.dirname(out))

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('cmd', choices=['tree', 'cdi'])
    p.add_argument('args', nargs='+')
    p.add_argument('--bgm', nargs='*', default=None)
    p.add_argument('--demo', default=None)
    p.add_argument('--mkdcdisc', default=os.path.join(HERE, '..', 'third_party', 'mkdcdisc', 'builddir', 'mkdcdisc'))
    a = p.parse_args()
    if a.cmd == 'tree' and len(a.args) == 2: tree(*a.args, bgm=a.bgm, demo=a.demo or None)
    elif a.cmd == 'cdi' and len(a.args) == 4: cdi(*a.args, a.mkdcdisc, bgm=a.bgm, demo=a.demo or None)
    else: p.error('wrong number of arguments')

if __name__ == '__main__':
    main()
