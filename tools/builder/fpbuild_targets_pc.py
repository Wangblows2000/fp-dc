"""
The PC versions for tools/builder/fpbuild.py (this branch only): targets win32 and linux.

  win32  fpdc.exe (32-bit Windows, SDL2), SDL2.dll, SDL2-LICENSE.txt, README-PC.txt, data/
  linux  fpdc (x86_64, SDL2 linked in: no libsdl2 needed; glibc 2.27 or newer), README-PC.txt, data/
written next to the build_win32.bat / build_linux.sh scripts (the project's top folder; D-119), or to --pc-out DIR.

The programs are the project's prebuilt ones (prebuilt/, tools/builder/downloads_pc.json, SHA-256 checked;
tools/builder/build_release_pc.sh makes them), or with --local-tools compiled here by pc/compile_win32.sh / compile_linux.sh.
data/ is the data dir the builder made (hard links when the file system allows, else copies). Only the files a previous
build put there (data/.fpdc_files) are replaced or removed: other files in data/ (the repository's data/test1.txt) stay.
Host-only tool (FP_HOST_ENV).
"""
import os, shutil

def ADD_ARGS(p):
    p.add_argument('--pc-out', default=None, help='the folder for the PC version (default: the top folder, next to the scripts)')

MANIFEST = '.fpdc_files'

def _copy_data(fb, src, dst):
    """src (the builder's data dir) -> dst; only the files listed in dst/.fpdc_files (the previous build's) are removed"""
    os.makedirs(dst, exist_ok=True)
    man = os.path.join(dst, MANIFEST)
    old = set(open(man, encoding='utf-8').read().split('\n')) - {''} if os.path.isfile(man) else set()
    new = []
    for r, ds, fs in os.walk(src):
        ds[:] = sorted(d for d in ds if d != '.fpbuild')
        rel = os.path.relpath(r, src)
        os.makedirs(os.path.join(dst, rel), exist_ok=True)
        for f in sorted(fs):
            q = os.path.normpath(os.path.join(rel, f)).replace(os.sep, '/')
            t = os.path.join(dst, q)
            if os.path.lexists(t): os.remove(t)
            fb.link(os.path.join(r, f), t); new.append(q)
    for q in sorted(old - set(new)):                 # files of an earlier build that this one does not have
        t = os.path.join(dst, q)
        if os.path.isfile(t): os.remove(t)
    for r, ds, fs in os.walk(dst, topdown=False):    # and the folders that left empty
        if r != dst and not os.listdir(r): os.rmdir(r)
    open(man, 'w', encoding='utf-8').write('\n'.join(new) + '\n')
    return len(new)

def _install(fb, name, dest, mode=None):
    p = fb.fetch.get(name, fb.LOG.say)
    if os.path.exists(dest): os.remove(dest)
    shutil.copyfile(p, dest)
    if mode: os.chmod(dest, mode)

def build_pc(a, tools, fb, which):
    dest = os.path.abspath(a.pc_out) if a.pc_out else fb.REPO        # next to build_win32.bat / build_linux.sh
    os.makedirs(dest, exist_ok=True)
    exe = os.path.join(dest, 'fpdc.exe' if which == 'win32' else 'fpdc')
    if a.local_tools:
        script = os.path.join(fb.REPO, 'pc', 'compile_win32.sh' if which == 'win32' else 'compile_linux.sh')
        fb.LOG.say('[%d] compile the PC version here: %s %s' % (fb.DATA_STEPS + 1, os.path.relpath(script, fb.REPO), dest))
        fb.run(['sh', script, dest])
    else:
        fb.LOG.say('[%d] the PC program (prebuilt/, SHA-256 checked) ...' % (fb.DATA_STEPS + 1))
        if which == 'win32':
            _install(fb, 'fpdc-win32.exe', exe)
            _install(fb, 'SDL2.dll', os.path.join(dest, 'SDL2.dll'))
            _install(fb, 'SDL2-LICENSE.txt', os.path.join(dest, 'SDL2-LICENSE.txt'))
        else:
            _install(fb, 'fpdc-linux-x86_64', exe, 0o755)
        shutil.copyfile(os.path.join(fb.REPO, 'pc', 'README.txt'), os.path.join(dest, 'README-PC.txt'))
    fb.LOG.say('[%d] the game data -> %s' % (fb.DATA_STEPS + 2, os.path.join(dest, 'data')))
    n = _copy_data(fb, a.out, os.path.join(dest, 'data'))
    fb.LOG.say('DONE: %s (%d data files). Start %s; README-PC.txt lists the keys.' % (exe, n, os.path.basename(exe)))

TARGETS = {
    'win32': ('the Windows PC version (fpdc.exe next to build_win32.bat)', False, lambda a, t, fb: build_pc(a, t, fb, 'win32')),
    'linux': ('the Linux PC version (fpdc next to build_linux.sh)', False, lambda a, t, fb: build_pc(a, t, fb, 'linux')),
}
