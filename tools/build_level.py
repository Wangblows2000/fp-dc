#!/usr/bin/env python3
"""
Rebuild runtime data for one recovered level into <out_dir>/<name>/ (each level has its own palettes + Lilac bank because
palettize.py packs per level). Shared sounds/music stay in <out_dir>. (tools/build_level.sh runs this file; it is Python so
the drag-and-drop builder runs it on Windows too, without a shell.)

usage: tools/build_level.py <frame(on_frame_N)> <name> <out_dir>
  FP_FRAMES / FP_IMAGES: the input dirs (tools/builder sets them; default the lead's tree: build/frames,
  extracted/linux/images)
"""
import os, re, shutil, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def py(*args, env=None):
    subprocess.run([sys.executable, '-B'] + [str(a) for a in args], check=True, cwd=ROOT, env=env)

def main(F='21', N='dv1', O='converted2'):
    O = os.path.abspath(O)
    os.makedirs(O, exist_ok=True)
    T = os.path.join(O, N + '.tmp')
    if os.path.isdir(T): shutil.rmtree(T)
    os.makedirs(T)
    frames = os.path.abspath(os.environ.get('FP_FRAMES') or os.path.join(ROOT, 'build', 'frames'))
    images = os.path.abspath(os.environ.get('FP_IMAGES') or os.path.join(ROOT, 'extracted', 'linux', 'images'))
    env = dict(os.environ, FP_FRAMES=frames, FP_IMAGES=images, PYTHONDONTWRITEBYTECODE='1')
    tools = os.path.join(ROOT, 'tools')
    py(os.path.join(tools, 'import_frame.py'), frames, images, F, N, T, env=env)
    # Carol's banks (session 10) get their own palette for bank 2, which replaces Lilac's when Carol plays (pal2c.pal).
    # Overlays with <= 15 colours (Lilac's hair, Carol's tail) are 4 bpp blocks at the start of that group (session 11).
    # Milla (session 11): same scheme, pal2m.pal
    for who, pal4, pal in (('carol', '--pal4=0:^caroltail', 'pal2c.pal'), ('milla', '--pal4=0:^millatail|^millafx', 'pal2m.pal')):
        sub = os.path.join(T, who); os.makedirs(sub)
        for f in sorted(os.listdir(T)):
            if f.startswith(who) and f.endswith('.tex'): shutil.move(os.path.join(T, f), os.path.join(sub, f))
        py(os.path.join(tools, 'palettize.py'), sub, '^' + who, '--gid=2', pal4, env=env)
        shutil.move(os.path.join(sub, 'pal2.pal'), os.path.join(T, pal))
    py(os.path.join(tools, 'palettize.py'), T, '^%s_p[0-9]' % N, '_par[0-9]', '^lilac', '_obj', '--flex=_ens', '--flex=_shl',
       '--pal4=1:_p[xw]', '--pal4=1:_pv', '--pal4=2:^lilachair', env=env)
    for who in ('carol', 'milla'):
        sub = os.path.join(T, who)
        for f in sorted(os.listdir(sub)):
            if f.endswith('.tex'): shutil.move(os.path.join(sub, f), os.path.join(T, f))
        os.rmdir(sub)
    # parallax sets with more than 15 colours per image (left in ARGB1555 by palettize, Fortune Night): PAL8 in the free top
    # of the character group 2 (pal2 / pal2c / pal2m); no-op for every other level (session 17)
    py(os.path.join(tools, 'par_top.py'), T, '^%s_p[xw]' % N, env=env)
    if os.path.isdir(os.path.join(O, N)): shutil.rmtree(os.path.join(O, N))
    os.rename(T, os.path.join(O, N))
    legacy = os.path.join(ROOT, 'converted')      # the old root sfx_*.wav / stage1.pcm (lead's tree only)
    if os.path.isdir(legacy):
        for f in sorted(os.listdir(legacy)):
            if re.match(r'^sfx_.*\.wav$', f) or f == 'stage1.pcm': shutil.copyfile(os.path.join(legacy, f), os.path.join(O, f))
    # levels.txt: one built level dir per line (read by the FP_DEBUG_SELECT menu)
    lt = os.path.join(O, 'levels.txt')
    names = set(l.strip() for l in open(lt, encoding='utf-8')) - {''} if os.path.isfile(lt) else set()
    names.add(N)
    with open(lt + '.tmp', 'w', encoding='utf-8', newline='\n') as f: f.write(''.join(n + '\n' for n in sorted(names)))
    os.replace(lt + '.tmp', lt)

if __name__ == '__main__':
    main(*sys.argv[1:4])
