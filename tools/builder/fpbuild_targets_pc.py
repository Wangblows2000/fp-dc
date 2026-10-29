"""
The PC versions for tools/builder/fpbuild.py (this branch only): targets win32 and linux.

  win32  output/win32/: fpdc.exe (32-bit Windows, SDL2), SDL2.dll, SDL2-LICENSE.txt, README.txt, data/
  linux  output/linux/: fpdc (x86_64, SDL2 linked in: no libsdl2 needed; glibc 2.27 or newer), README.txt, data/

The programs are the project's published builds (tools/builder/downloads_pc.json, release builder-v1, SHA-256 checked;
tools/builder/build_release_pc.sh makes them), or with --local-tools compiled here by pc/build_win32.sh / pc/build_linux.sh.
data/ is the data dir the builder made (hard links when the file system allows, else copies). --pc-out DIR: another folder.
Host-only tool (FP_HOST_ENV).
"""
import os, shutil

def ADD_ARGS(p):
    p.add_argument('--pc-out', default=None, help='the folder for the PC version (default output/win32 or output/linux)')

def _copy_data(fb, src, dst):
    if os.path.isdir(dst): shutil.rmtree(dst)
    n = 0
    for r, ds, fs in os.walk(src):
        ds[:] = [d for d in ds if d != '.fpbuild']
        rel = os.path.relpath(r, src)
        os.makedirs(os.path.join(dst, rel), exist_ok=True)
        for f in fs:
            fb.link(os.path.join(r, f), os.path.join(dst, rel, f)); n += 1
    return n

def _install(fb, name, dest, mode=None):
    p = fb.fetch.get(name, fb.LOG.say)
    if os.path.exists(dest): os.remove(dest)
    shutil.copyfile(p, dest)
    if mode: os.chmod(dest, mode)

def build_pc(a, tools, fb, which):
    dest = os.path.abspath(a.pc_out) if a.pc_out else os.path.join(fb.REPO, 'output', which)
    os.makedirs(dest, exist_ok=True)
    exe = os.path.join(dest, 'fpdc.exe' if which == 'win32' else 'fpdc')
    if a.local_tools:
        script = os.path.join(fb.REPO, 'pc', 'build_win32.sh' if which == 'win32' else 'build_linux.sh')
        fb.LOG.say('[10] compile the PC version here: %s %s' % (os.path.relpath(script, fb.REPO), dest))
        fb.run(['sh', script, dest])
    else:
        fb.LOG.say('[10] the PC program (the published build, SHA-256 checked) ...')
        if which == 'win32':
            _install(fb, 'fpdc-win32.exe', exe)
            _install(fb, 'SDL2.dll', os.path.join(dest, 'SDL2.dll'))
            _install(fb, 'SDL2-LICENSE.txt', os.path.join(dest, 'SDL2-LICENSE.txt'))
        else:
            _install(fb, 'fpdc-linux-x86_64', exe, 0o755)
        shutil.copyfile(os.path.join(fb.REPO, 'pc', 'README.txt'), os.path.join(dest, 'README.txt'))
    fb.LOG.say('[11] the game data -> %s' % os.path.join(dest, 'data'))
    n = _copy_data(fb, a.out, os.path.join(dest, 'data'))
    fb.LOG.say('DONE: %s (%d data files). Start %s; README.txt lists the keys.' % (dest, n, os.path.basename(exe)))

TARGETS = {
    'win32': ('the Windows PC version (output/win32/fpdc.exe)', False, lambda a, t, fb: build_pc(a, t, fb, 'win32')),
    'linux': ('the Linux PC version (output/linux/fpdc)', False, lambda a, t, fb: build_pc(a, t, fb, 'linux')),
}
