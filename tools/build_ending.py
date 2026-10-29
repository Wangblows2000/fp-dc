#!/usr/bin/env python3
"""
The end-of-game scenes (session 24, src/game/ending.c): the space ending (frame index 83 = frame_84.json) and the credits
(frame index 85 = frame_86.json) as UI scenes, plus the credits text (tools/import_ending.py).
(tools/import_ending.sh runs this file; it is Python so the drag-and-drop builder runs it on Windows too.)

usage: tools/build_ending.py <data_dir>    (e.g. converted2; writes <data_dir>/end/: end_*, cred_*, credits.txt; its own disc
       pack end.pak, so the front end's ui.pak stays as it was)
inputs (defaults = the lead's tree; tools/builder sets them): FP_FRAMES (dump_frames output or tables/frames), FP_IMAGES
       (extracted images), FP_RE (decompiled constructors), FP_BIN (bin64/Chowdren, or tables/strings.json)
Only the animations the port draws are exported (-k): the cast stands in its first pose, the played character waves at the
end (anim 37 direction 1, RE 22777 / 22782-22784); the full-screen shockwave frames (426x240 x 18) and the black Fade
objects are drawn as rectangles instead (ending.c).
"""
import os, shutil, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def main(D='converted2'):
    D = os.path.abspath(D)
    E = os.environ
    FR = os.path.abspath(E.get('FP_FRAMES') or os.path.join(ROOT, 'build', 'frames'))
    IM = os.path.abspath(E.get('FP_IMAGES') or os.path.join(ROOT, 'extracted', 'linux', 'images'))
    RE = E.get('FP_RE') or os.path.join(ROOT, 'build', 're')
    BIN = E.get('FP_BIN') or '/home/claude/fprun/bin64/Chowdren'
    env = dict(E, PYTHONDONTWRITEBYTECODE='1')
    py = lambda *a: subprocess.run([sys.executable, '-B'] + list(a), check=True, cwd=ROOT, env=env)
    ui = os.path.join(ROOT, 'tools', 'import_ui.py'); end = os.path.join(D, 'end')
    # frame index 83: space, the Dreadnought, the lift-off jets, the hero ship, Brevon's mech, debris, dust, the explosions it creates
    py(ui, FR, IM, end, 'end', '84', '84+effectsexplosion06_435', '-x', '^(shockwave|levelcardfade|ringsparkle|separatormasterobjects2)$')
    # frame index 85: the credits (the Scientist is NPCHologram1's direction 3; its direction 0 is Syntax's full-size hologram
    # eye) (stars, clouds, sky, the cast, Torque's rocket, MP2, Thanks for playing, CONTINUE)
    P = '^(npc|brevon|themagister|gongshield|lamppostfront|serpentinechestplate|playerskinspade|playerskintorque)'
    py(ui, FR, IM, end, 'cred', '86', '-x', '^(fade|separatormasterobjects2|lamppostback|playerskinsashhair)$',
       '-k', '^npchologram1$=0.3', '-k', P + '=0.0', '-k', '^playerskin(sash|carol|milla)(tail)?$=37.1', '-k', '^star$=0.0')
    py(os.path.join(ROOT, 'tools', 'import_ending.py'), BIN, RE, end)
    for f in ('font.spr', 'font.tex'):   # the credits font in the ending's own pack (end.pak: one pack per load)
        shutil.copyfile(os.path.join(D, 'ui', f), os.path.join(end, f))

if __name__ == '__main__':
    main(*sys.argv[1:2])
