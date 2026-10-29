#!/usr/bin/env python3
"""
Freedom Planet builder: your Assets.dat -> the port's data dir -> the Dreamcast disc image.
BUILDING.md has the user-facing description; build_dreamcast.bat / build_dreamcast.sh call this file.

usage: fpbuild.py dc   [Assets.dat or the game folder] [options] [--cdi FILE (default: fpdc.cdi in the top folder)] [--elf FILE]
       fpbuild.py data <Assets.dat or the game folder> [options]
       fpbuild.py selftest
options: --out DIR (data dir, default build/builder/data)  --work DIR (intermediates, default build/builder)
         --voices-pc DIR (a Windows install's Data/voices, for the Linux copy's placeholder clips)  -j N (parallel level
         builds)  --force (redo every step)  --local-tools (use the python3 / ffmpeg / cc / mkdcdisc / KallistiOS installed
         on this computer instead of the pinned downloads; missing ones are still downloaded)

Inputs (read-only, never written): Assets.dat of Freedom Planet 1.21.5, Windows GOG (md5 4085c983...) or Linux
(md5 9d00124a...), and the Data folder next to it: Data/BGM (music), Data/font.bmp, Data/voices. Everything else the
pipeline needs from the original program is committed in tables/ (tables/README.md). The Windows Assets.dat is mapped to
the Linux image ids with tables/winmap_4085c983.json.
Tools: the pinned downloads of tools/builder/downloads.json (tools/builder/fetch.py: Python packages, ffmpeg, wav2adpcm,
mkdcdisc, the Dreamcast program fpdc.elf), each checked against its SHA-256.
Every step is skipped when its inputs, its tools and its outputs are unchanged (stamps in <out>/.fpbuild).
Host-only tool (FP_HOST_ENV): nothing here runs on the Dreamcast.
"""
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
HERE = os.path.join(REPO, 'tools', 'builder')
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools', 'audio'))
import fetch

ASSETS = {
    '9d00124aa9e4498d217f951afb962171': ('linux', 'Linux 1.21.5 (31.5 MB)'),
    '4085c983fb918703ce459f17f69c474c': ('windows', 'Windows GOG 1.21.5 (39 MB)'),
}
ASSETS_OTHER = {   # fp-assets.py knows these, the port does not (its tables come from the 1.21.5 builds)
    'fb95f5c4809e76cae933be20ca51a660': 'a newer Linux build (17162 images)',
    '97fa3c33c6fc72bece978c06cacf3ebf': 'the Nintendo Switch build',
    'f14f24317d0323d63231cdbba511f254': 'an older release',
}
LINUX_ASSETS_MD5 = '9d00124aa9e4498d217f951afb962171'
TABLES = os.path.join(REPO, 'tables')
FRAMES = os.path.join(TABLES, 'frames')
STRINGS = os.path.join(TABLES, 'strings.json')
WINMAP = os.path.join(TABLES, 'winmap_4085c983.json')
LINUX_FMT = os.path.join(REPO, 'tools', 'fmt_linux_9d00124a.json')
SOURCE_DATE = '1793232000'      # 2026-10-29 00:00 UTC: the disc's dates (IP.BIN, ISO), so a rebuild gives the same image

# json frame (= the original's set_frame index + 1) -> level dir; build/rebuild_cmp.sh's list + Battle Glacier + Final Dreadnought
LEVELS = [(17, 'at1'), (18, 'at2'), (19, 'at3'), (20, 'at4'), (21, 'dv1'), (22, 'dv2'), (23, 'dv3'), (24, 'dv4'),
          (25, 'rm1'), (26, 'rm2'), (27, 'rm3'), (28, 'rm4'), (29, 'rm5'), (30, 'fn1'), (31, 'fn2'), (32, 'fn3'),
          (33, 'fn4'), (34, 'fn5'), (35, 'fn6'), (36, 'sb1'), (37, 'sb2'), (38, 'sb3'), (39, 'sb4'), (40, 'jc1'),
          (41, 'jc2'), (42, 'jc3'), (43, 'jc4'), (44, 'jc5'), (45, 'tb1'), (46, 'tb2'), (47, 'tb3'), (48, 'tb4'),
          (49, 'tb5'), (50, 'tb6'), (51, 'pl1'), (52, 'pl2'), (53, 'pl3'), (54, 'pl4'), (55, 'pl5'), (56, 'pl6'),
          (57, 'th1'), (58, 'th2'), (59, 'th3'), (60, 'bg1'), (61, 'bg2'), (62, 'bg3'), (63, 'bg4'), (64, 'bg5'),
          (65, 'bg6'), (66, 'fd1a'), (67, 'fd1b'), (68, 'fd2a'), (69, 'fd2b'), (70, 'fd3a'), (71, 'fd3b'), (72, 'fd4a'),
          (73, 'fd4b')]
GAL_EXCLUDE = ('^(artwork|fade|achievementdescription|helpmessage|blackborder|backgroundsystembox|separatormasterobjects2|'
               'playerinput|masterinput|inputsp1|globalinputdata|mouseobject|sarassteam.*|list.*|mapdata2?|savedata|'
               'windowcontrol|recorddata|icon.*|charname1|active2)$')
STUB_MAX = 4000      # the Linux copy's placeholder voice clips are 3,564 bytes
NEED_GB = 3.0        # free disk space for a first build (work dir, data dir, disc tree, image)

class Fail(Exception): pass

# ---------------------------------------------------------------------------------------------------------------- log
class Log:
    def __init__(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        self.path = path; self.f = open(path, 'a', encoding='utf-8'); self.t0 = time.time()
        self.f.write('\n==== %s  %s\n' % (time.strftime('%Y-%m-%d %H:%M:%S'), ' '.join(sys.argv))); self.f.flush()
    def say(self, msg):
        line = '[%5.0fs] %s' % (time.time() - self.t0, msg)
        try: print(line, flush=True)
        except UnicodeEncodeError: print(line.encode('ascii', 'replace').decode(), flush=True)
        self.f.write(line + '\n'); self.f.flush()
    def note(self, msg):
        self.f.write(msg + '\n'); self.f.flush()

LOG = None
TOOLENV = {}         # FFMPEG / WAV2ADPCM for the converters

def child_env(extra=None):
    e = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', PYTHONUTF8='1', PYTHONIOENCODING='utf-8')
    e.update(TOOLENV); e.update(extra or {})
    return e

def run(cmd, env=None, logfile=None, cwd=REPO):
    """run a command, output to the log (or its own log file); a failure shows the last lines"""
    LOG.note('$ ' + ' '.join(_q(c) for c in cmd))
    out = open(logfile, 'w', encoding='utf-8') if logfile else LOG.f
    out.flush()
    r = subprocess.run([str(c) for c in cmd], cwd=cwd, env=child_env(env), stdout=out, stderr=subprocess.STDOUT)
    if logfile: out.close()
    if r.returncode:
        src = logfile or LOG.path
        tail = open(src, encoding='utf-8', errors='replace').read().splitlines()[-25:]
        raise Fail('command failed (exit %d): %s\n  ... last lines of %s:\n    %s' % (
            r.returncode, ' '.join(_q(c) for c in cmd), src, '\n    '.join(tail)))

def py(*args):
    return [sys.executable, '-B'] + [str(a) for a in args]

def _q(s):
    s = str(s)
    return s if re.match(r'^[\w@%+=:,./\\-]+$', s) else '"' + s.replace('"', '\\"') + '"'

def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()

def tree_hash(paths):
    """content hash of tool files / dirs (a changed tool redoes the steps that use it)"""
    h = hashlib.sha1()
    for p in paths:
        p = os.path.join(REPO, p)
        files = [p] if os.path.isfile(p) else sorted(
            os.path.join(r, f) for r, ds, fs in os.walk(p) for f in fs
            if not f.endswith('.pyc') and '__pycache__' not in r and f != 'wav2adpcm')
        for f in files:
            h.update(os.path.relpath(f, REPO).replace('\\', '/').encode()); h.update(open(f, 'rb').read())
    return h.hexdigest()

def link(src, dst):
    try: os.link(src, dst)
    except OSError: shutil.copyfile(src, dst)

# ------------------------------------------------------------------------------------------------------------- inputs
GAME_DIRS = [r'C:\Program Files (x86)\Steam\steamapps\common\Freedom Planet', r'C:\Program Files\Steam\steamapps\common\Freedom Planet',
             r'C:\GOG Games\Freedom Planet', r'C:\Program Files (x86)\GOG Galaxy\Games\Freedom Planet',
             '~/.steam/steam/steamapps/common/Freedom Planet', '~/.local/share/Steam/steamapps/common/Freedom Planet',
             '~/GOG Games/Freedom Planet']

def auto_assets():
    """no file given (double-clicked, or started without arguments): an Assets.dat in the project folder, the current folder,
    up to three folders above the project (e.g. the project unpacked inside the Freedom Planet folder) or a usual Steam / GOG
    install; else a file dialog (Windows: PowerShell; Linux: zenity) when there is a screen"""
    dirs = [REPO, os.getcwd()]
    up = REPO
    for _ in range(3):
        up = os.path.dirname(up); dirs.append(up)
    dirs += [os.path.expanduser(d) for d in GAME_DIRS]
    for d in dirs:
        for c in (os.path.join(d, 'Assets.dat'), os.path.join(d, 'game', 'Assets.dat')):
            if os.path.isfile(c) and md5(c) in ASSETS:
                LOG.say('No file was given: using %s (found automatically)' % c); return c
    try:
        if os.name == 'nt':
            ps = ('Add-Type -AssemblyName System.Windows.Forms; $d = New-Object System.Windows.Forms.OpenFileDialog; '
                  '$d.Title = "Select the Assets.dat of your Freedom Planet folder"; $d.Filter = "Assets.dat|Assets.dat|All files|*.*"; '
                  'if ($d.ShowDialog() -eq "OK") { [Console]::Out.Write($d.FileName) }')
            r = subprocess.run(['powershell', '-NoProfile', '-STA', '-Command', ps], stdout=subprocess.PIPE, text=True, timeout=600)
        elif (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')) and shutil.which('zenity'):
            r = subprocess.run(['zenity', '--file-selection', '--title=Select the Assets.dat of your Freedom Planet folder'],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=600)
        else: r = None
        if r and r.stdout.strip(): return r.stdout.strip()
    except (OSError, subprocess.SubprocessError): pass
    raise Fail('no Assets.dat given and none found next to the project. Drag the Assets.dat of your Freedom Planet folder '
               'onto the build script (or give its path: build_dreamcast.bat "C:\\Games\\Freedom Planet\\Assets.dat").')

def find_inputs(a):
    if not a.assets: a.assets = auto_assets()
    src = os.path.abspath(a.assets)
    if not os.path.exists(src): raise Fail('not found: %s' % a.assets)
    if os.path.isfile(src) and os.path.basename(src).lower() != 'assets.dat':
        src = os.path.dirname(src)          # another file of the game folder was dropped (FP.exe, ...): its folder
    if os.path.isdir(src):
        cand = [os.path.join(src, 'Assets.dat'), os.path.join(src, 'game', 'Assets.dat')]
        found = next((c for c in cand if os.path.isfile(c)), None)
        if not found: raise Fail('no Assets.dat in the folder %s. Drop the Assets.dat of your Freedom Planet folder.' % src)
        src = found
    size = os.path.getsize(src)
    if size < 20e6 or size > 60e6:
        raise Fail('%s (%d bytes) is not a Freedom Planet Assets.dat (expected 39 MB Windows or 31.5 MB Linux).' % (src, size))
    LOG.say('Assets.dat: %s' % src)
    m = md5(src)
    if m in ASSETS_OTHER:
        raise Fail('this Assets.dat (md5 %s) is %s. The port needs Freedom Planet 1.21.5: the Windows GOG Assets.dat '
                   '(md5 4085c983...) or the Linux one (md5 9d00124a...).' % (m, ASSETS_OTHER[m]))
    if m not in ASSETS:
        raise Fail('unknown Assets.dat (md5 %s). Supported: Freedom Planet 1.21.5 Windows GOG (md5 4085c983...) or Linux '
                   '(md5 9d00124a...). A modded or other-version file cannot be used.' % m)
    kind, desc = ASSETS[m]
    LOG.say('  identified: %s, md5 %s' % (desc, m))
    d = os.path.dirname(src)
    def first(*paths):
        return next((p for p in paths if p and os.path.exists(p)), None)
    # the Data folder next to the Assets.dat; the GOG layout with the Linux copy in game/ is searched too
    dd = [os.path.join(d, 'Data'), os.path.join(d, 'game', 'Data')]
    up = d
    for _ in range(3):              # (an Assets.dat copied into a folder inside the game folder: the game's Data above it)
        up = os.path.dirname(up); dd.append(os.path.join(up, 'Data'))
    bgm = first(*[os.path.join(x, 'BGM') for x in dd])
    font = first(*[os.path.join(x, 'font.bmp') for x in dd])
    vox = first(*[os.path.join(x, 'voices') for x in dd])
    # the PC (Windows) clips for the Linux copy's placeholders: --voices-pc, or the Windows folder around a Linux copy
    vpc = os.path.abspath(a.voices_pc) if a.voices_pc else (
        first(os.path.join(os.path.dirname(d), 'Data', 'voices')) if kind == 'linux' else None)
    if vpc and vox and os.path.samefile(vpc, vox): vpc = None
    miss = [n for n, p in (('Data/BGM (the music)', bgm), ('Data/font.bmp', font), ('Data/voices', vox)) if not p]
    if miss:
        raise Fail('%s not found next to the Assets.dat (%s). Drop the Assets.dat that sits in your Freedom Planet folder, '
                   'with its Data folder beside it.' % (' and '.join(miss), os.path.join(d, 'Data')))
    nbgm = len([f for f in os.listdir(bgm) if f.endswith('.ogg')])
    if nbgm < 67: LOG.say('  WARNING: %s has %d .ogg tracks (the game has 67)' % (bgm, nbgm))
    LOG.say('  music: %s (%d tracks); font: %s' % (bgm, nbgm, font))
    LOG.say('  voices: %s%s' % (vox, ('  + PC voices %s' % vpc) if vpc else ''))
    la = first(os.path.join(d, 'game', 'Assets.dat')) if kind == 'windows' else None
    return dict(assets=src, md5=m, kind=kind, bgm=bgm, font=font, vox=vox, vpc=vpc,
                linux_assets=la if la and os.path.getsize(la) < 35e6 and md5(la) == LINUX_ASSETS_MD5 else None)

# -------------------------------------------------------------------------------------------------------------- tools
def setup_tools(a, need_disc):
    """Python packages, ffmpeg, wav2adpcm (and for the disc: mkdcdisc, fpdc.elf): pinned downloads, or --local-tools"""
    if sys.version_info < (3, 8): raise Fail('Python 3.8 or newer is needed (this is %s)' % sys.version.split()[0])
    osn = fetch.host_os()
    if osn not in ('windows', 'linux'):
        raise Fail('this builder runs on Windows (x64) and Linux (x86_64); %s is not supported' % osn)
    say = lambda m: LOG.say(m)
    managed = os.environ.get('FPB_MANAGED') == '1'
    if managed:
        import site, sysconfig
        sp = sysconfig.get_paths()['purelib']
        fetch.install_wheels(sp, say)
        if sp not in sys.path: site.addsitedir(sp)
    miss = []
    for mod, pkg in (('PIL', 'pillow'), ('lz4', 'lz4')):
        try: __import__(mod)
        except ImportError: miss.append(pkg)
    if miss:
        raise Fail('missing Python packages: %s (pip install %s, or run the build_dreamcast script without --local-tools)'
                   % (', '.join(miss), ' '.join(miss)))
    def pick(local, name):
        if a.local_tools and local: return local
        return fetch.tool(name, say)
    ffm = pick(shutil.which('ffmpeg'), 'ffmpeg-6.1.1-essentials_build.zip' if osn == 'windows' else 'ffmpeg-6.1.1-linux-x86_64')
    w2a_local = None
    if a.local_tools and shutil.which('cc'):
        w2a_local = os.path.join(a.work, 'wav2adpcm')
        src = os.path.join(REPO, 'third_party', 'wav2adpcm', 'wav2adpcm.c')
        if not os.path.isfile(w2a_local) or os.path.getmtime(w2a_local) < os.path.getmtime(src):
            os.makedirs(a.work, exist_ok=True); run(['cc', '-O2', '-o', w2a_local, src])
    w2a = pick(w2a_local, 'wav2adpcm.exe' if osn == 'windows' else 'wav2adpcm-linux-x86_64')
    TOOLENV.update(FFMPEG=ffm, WAV2ADPCM=w2a)
    LOG.say('  tools: Python %s (%s), ffmpeg %s, wav2adpcm %s' % (sys.version.split()[0], sys.executable, ffm, w2a))
    t = dict(ffmpeg=ffm, wav2adpcm=w2a)
    if need_disc:
        mk_local = None
        if a.local_tools:
            mk_local = next((p for p in (os.environ.get('MKDCDISC'), os.path.join(REPO, 'third_party', 'mkdcdisc', 'builddir', 'mkdcdisc'),
                                         shutil.which('mkdcdisc')) if p and os.path.isfile(p)), None)
        t['mkdcdisc'] = pick(mk_local, 'mkdcdisc.exe' if osn == 'windows' else 'mkdcdisc-linux-x86_64')
        LOG.say('  CDI writer: %s' % t['mkdcdisc'])
    return t

# -------------------------------------------------------------------------------------------------------------- steps
class Stamps:
    def __init__(self, out, force):
        self.dir = os.path.join(out, '.fpbuild'); self.force = force; os.makedirs(self.dir, exist_ok=True)
    def key(self, *parts):
        return hashlib.sha1(json.dumps(parts, sort_keys=True).encode()).hexdigest()
    def done(self, name, key, outputs=()):
        p = os.path.join(self.dir, name)
        return (not self.force and os.path.exists(p) and open(p).read() == key and all(os.path.exists(o) for o in outputs))
    def clear(self, name):
        p = os.path.join(self.dir, name)
        if os.path.exists(p): os.remove(p)
    def mark(self, name, key):
        open(os.path.join(self.dir, name), 'w').write(key)

def step(st, n, total, name, title, key, outputs, fn):
    if st.done(name, key, outputs):
        LOG.say('[%d/%d] %s: up to date' % (n, total, title)); return
    st.clear(name); t = time.time(); LOG.say('[%d/%d] %s ...' % (n, total, title))
    fn(); st.mark(name, key)
    LOG.say('[%d/%d] %s: done (%.0f s)' % (n, total, title, time.time() - t))

def _writable_retry(fn, path, *_):
    # Windows: a read-only file (e.g. a hard link to a read-only clip of an earlier build) cannot be deleted; clear the flag
    import stat
    os.chmod(path, stat.S_IWRITE); fn(path)

def rmtree(p):
    if os.path.isdir(p):
        if sys.version_info >= (3, 12): shutil.rmtree(p, onexc=_writable_retry)
        else: shutil.rmtree(p, onerror=_writable_retry)

def extract(assets, fmt, dest):
    tmp = dest + '.part'
    rmtree(tmp); rmtree(dest)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    cmd = py(os.path.join(REPO, 'third_party', 'fp-assets', 'fp-assets.py'), '-x', '-f', assets)
    if fmt: cmd += ['-m', fmt]
    run(cmd + [tmp]); os.rename(tmp, dest)

def map_windows(win, linux_full, dest):
    """Windows GOG extraction -> the Linux image / sound ids (tables/winmap_4085c983.json)"""
    from PIL import Image
    t = json.load(open(WINMAP, encoding='utf-8'))
    if not t['audio_identical']: raise Fail('winmap: audio not identical')
    tmp = dest + '.part'
    rmtree(tmp)
    os.makedirs(os.path.join(tmp, 'images')); src = os.path.join(win, 'images'); out = os.path.join(tmp, 'images')
    n = 0
    for a, b, off in t['runs']:
        for i in range(a, b + 1):
            link(os.path.join(src, 'img_%d.png' % (i + off)), os.path.join(out, 'img_%d.png' % i))
            link(os.path.join(src, 'img_%d_meta.txt' % (i + off)), os.path.join(out, 'img_%d_meta.txt' % i)); n += 1
    subst, relabel = [], []
    for k, e in sorted(t['platform'].items(), key=lambda kv: int(kv[0])):
        i = int(k)
        if linux_full:      # the Linux Assets.dat next to it: its own image (the console pad labels)
            link(os.path.join(linux_full, 'images', 'img_%d.png' % i), os.path.join(out, 'img_%d.png' % i))
            link(os.path.join(linux_full, 'images', 'img_%d_meta.txt' % i), os.path.join(out, 'img_%d_meta.txt' % i))
            continue
        if 'labels' in e:   # the PC image with the console labels redrawn (B / C on Milla's HUD, down + B): the Linux pixels
            apply_labels(Image.open(os.path.join(src, 'img_%d.png' % e['subst'])).convert('RGBA'), e['labels']).save(
                os.path.join(out, 'img_%d.png' % i))
            relabel.append(i)
        elif 'exact' in e or 'subst' in e:
            link(os.path.join(src, 'img_%d.png' % e.get('exact', e.get('subst'))), os.path.join(out, 'img_%d.png' % i))
            if 'subst' in e: subst.append('%d (PC image %d)' % (i, e['subst']))
        else:
            Image.new('RGBA', (e['w'], e['h'])).save(os.path.join(out, 'img_%d.png' % i)); subst.append('%d (blank)' % i)
        with open(os.path.join(out, 'img_%d_meta.txt' % i), 'w', newline='\n') as f:
            f.write('\n'.join('0x%x' % v for v in e['meta']) + '\n')
    if n + len(t['platform']) != t['linux_images']: raise Fail('winmap: %d images mapped, expected %d' % (n, t['linux_images']))
    os.makedirs(os.path.join(tmp, 'audio'))
    for f in os.listdir(os.path.join(win, 'audio')):
        link(os.path.join(win, 'audio', f), os.path.join(tmp, 'audio', f))
    rmtree(dest)
    os.rename(tmp, dest)
    if subst or relabel:
        LOG.say('  %d images differ between the console / Linux and the PC build: %d rebuilt with the console button labels '
                '(Milla\'s HUD, the dig hint: %s); %d from the PC version or blank (not shown by the port: PRESS (X) of the '
                'original title, the Mahjong hints, the Wii U sign; the fade edge is (0,0,0) black, the console one (8,0,0)): %s'
                % (len(subst) + len(relabel), len(relabel), ' '.join(map(str, relabel)), len(subst), ', '.join(subst)))
    else:
        LOG.note('platform-specific images taken from the Linux Assets.dat')

def apply_labels(b, rec):
    """tables/winmap 'labels' recipe (tools/builder/make_tables.py labels): the box rows, '?' = keep, '.' = transparent, a
    letter = the colour of the PC image's pixel at colours[letter]"""
    px = b.load(); col = {k: px[u, v] for k, (u, v) in rec['colours'].items()}
    x0, y0 = rec['box'][:2]
    for dy, r in enumerate(rec['rows']):
        for dx, ch in enumerate(r):
            if ch != '?': px[x0 + dx, y0 + dy] = (0, 0, 0, 0) if ch == '.' else col[ch]
    return b

def needed_voices():
    """the voice clips the converters read: eng/<xx>/<name>.ogg (and every lilac_*)"""
    import convert_audio as ca
    need = set(v for k, v, r, t in ca.SFX if k in ('vox', 'voxpc'))
    need |= set(ca.GAL_VOICES.split())
    return sorted(need)

def merge_voices(vox, vpc, dest):
    """the install's voices; a placeholder stub (or a missing clip) is taken from the PC voices when given. Copies, never
    hard links: a link shares the game file's read-only flag (Windows then refuses to delete it on the next build, seen
    under Wine in session 25) and anything done to the link would touch the game folder, which is only read."""
    rmtree(dest); os.makedirs(dest)
    n = rep = 0
    for root in [r for r in (vox, vpc) if r]:
        for r, ds, fs in os.walk(root):
            for f in sorted(fs):
                if not f.endswith('.ogg'): continue
                s = os.path.join(r, f); rel = os.path.relpath(s, root); d = os.path.join(dest, rel)
                if os.path.lexists(d):
                    if root == vpc and os.path.getsize(d) < STUB_MAX < os.path.getsize(s):
                        os.remove(d); shutil.copyfile(s, d); rep += 1
                    continue
                os.makedirs(os.path.dirname(d), exist_ok=True); shutil.copyfile(s, d); n += 1
    LOG.say('  voices: %d clips%s' % (n, ', %d placeholder clips replaced by the PC clips' % rep if vpc else ''))
    bad = []
    for v in needed_voices() + ['li/' + f[:-4] for f in sorted(os.listdir(os.path.join(dest, 'eng', 'li')))
                                if f.startswith('lilac_') and f.endswith('.ogg')]:
        p = os.path.join(dest, 'eng', *v.split('/')) + '.ogg'
        if not os.path.isfile(p): raise Fail('voice clip Data/voices/eng/%s.ogg is missing from %s' % (v, vox))
        if os.path.getsize(p) < STUB_MAX: bad.append(v)
    if bad:
        LOG.say('  WARNING: %d voice clips the port uses are 3.5 KB placeholders in this copy (%s): they stay silent. The '
                'Windows install has the real clips: drop its Assets.dat, or give its Data\\voices with --voices-pc.'
                % (len(bad), ', '.join(bad)))

# -------------------------------------------------------------------------------------------------------------- data
def build_data(a, inp):
    out, work = a.out, a.work
    os.makedirs(out, exist_ok=True); os.makedirs(work, exist_ok=True)
    st = Stamps(out, a.force)
    ex = os.path.join(work, 'extracted'); voices = os.path.join(work, 'voices')
    assets_dir = os.path.join(ex, 'assets-linux-ids')
    images, audio = os.path.join(assets_dir, 'images'), os.path.join(assets_dir, 'audio')
    total = 9
    env = dict(FP_FRAMES=FRAMES, FP_IMAGES=images, FP_RE=TABLES, FP_BIN=STRINGS)
    T = lambda *p: tree_hash(p)

    def s_extract():
        if inp['kind'] == 'linux':
            extract(inp['assets'], LINUX_FMT, assets_dir)
        else:
            win = os.path.join(ex, 'windows'); extract(inp['assets'], None, win)
            lin = None
            if inp['linux_assets']:
                lin = os.path.join(ex, 'linux'); extract(inp['linux_assets'], LINUX_FMT, lin)
            map_windows(win, lin, assets_dir)
            rmtree(win); rmtree(lin) if lin else None
    k_extract = st.key(inp['md5'], bool(inp['linux_assets']),
                       T('third_party/fp-assets', 'tools/fmt_linux_9d00124a.json', 'tables/winmap_4085c983.json'))
    step(st, 1, total, 'extract', 'extract Assets.dat (fp-assets, about 1-2 minutes)', k_extract, [images, audio], s_extract)

    merge_voices(inp['vox'], inp['vpc'], voices)
    vkey = sorted((os.path.relpath(os.path.join(r, f), voices).replace('\\', '/'), os.path.getsize(os.path.join(r, f)))
                  for r, ds, fs in os.walk(voices) for f in fs)

    base = st.key(k_extract, T('tables'))   # every later step depends on the extracted images and the tables
    tools_py = T('tools/import_frame.py', 'tools/palettize.py', 'tools/par_top.py', 'tools/convert.py', 'tools/build_level.py',
                 'tools/title_credit.py', 'tools/slice_manifest.json')
    jobs = max(1, a.jobs)
    def s_levels():
        todo = [(f, n) for f, n in LEVELS if not st.done('lv_' + n, st.key(base, tools_py, n), [os.path.join(out, n)])]
        LOG.say('  %d of %d levels to build, %d at a time (logs: %s)' % (len(todo), len(LEVELS), jobs, os.path.join(work, 'logs')))
        os.makedirs(os.path.join(work, 'logs'), exist_ok=True)
        done = [0]
        def one(fn):
            f, n = fn
            st.clear('lv_' + n)
            run(py(os.path.join(REPO, 'tools', 'build_level.py'), f, n, out), env=env, logfile=os.path.join(work, 'logs', n + '.log'))
            st.mark('lv_' + n, st.key(base, tools_py, n)); done[0] += 1
            LOG.say('  level %s done (%d/%d)' % (n, done[0], len(todo)))
        with concurrent.futures.ThreadPoolExecutor(jobs) as ex_:
            futs = [ex_.submit(one, j) for j in todo]
            try:
                for fu in futs: fu.result()
            except BaseException:
                for fu in futs: fu.cancel()          # stop at the first failure (the running ones finish)
                raise
        with open(os.path.join(out, 'levels.txt'), 'w', newline='\n') as f: f.write(''.join(n + '\n' for n in sorted(n for f, n in LEVELS)))
    step(st, 2, total, 'levels', 'levels (%d acts, the longest step)' % len(LEVELS), st.key(base, tools_py, [n for f, n in LEVELS]),
         [os.path.join(out, n) for f, n in LEVELS], s_levels)

    ui = os.path.join(out, 'ui'); tools_ui = T('tools/import_ui.py', 'tools/convert.py', 'tools/title_credit.py', 'tools/import_loading.py',
                                               'tools/make_vmu_icons.py', 'tools/import_font.py')
    def s_ui():
        imp = py(os.path.join(REPO, 'tools', 'import_ui.py'), FRAMES, images, ui)
        run(imp + ['title', '3', '4', '3+titlestar_53'])
        run(imp + ['chsel', '7'])
        run(imp + ['stsel', '12'])
        run(imp + ['tasel', '6'])
        run(imp + ['gal', '11', '11+effectswhiteburst_31', '-x', GAL_EXCLUDE])
        run(py(os.path.join(REPO, 'tools', 'import_loading.py'), FRAMES, images, ui))
        run(py(os.path.join(REPO, 'tools', 'make_vmu_icons.py'), images, ui))
        run(py(os.path.join(REPO, 'tools', 'import_font.py'), inp['font'], ui))
    step(st, 3, total, 'ui', 'front end: title, menus, loading, VMU icons, font', st.key(base, tools_ui, md5(inp['font'])),
         [os.path.join(ui, 'title_3.ui'), os.path.join(ui, 'font.tex')], s_ui)

    step(st, 4, total, 'end', 'ending and credits', st.key(base, tools_ui, T('tools/build_ending.py', 'tools/import_ending.py', 'tools/import_gallery.py'), md5(inp['font'])),
         [os.path.join(out, 'end', 'credits.txt')], lambda: run(py(os.path.join(REPO, 'tools', 'build_ending.py'), out), env=env))

    gal = os.path.join(out, 'gal')
    step(st, 5, total, 'gallery', 'gallery artworks and texts',
         st.key(base, T('tools/import_gallery.py')), [os.path.join(gal, 'gal_text.txt')],
         lambda: run(py(os.path.join(REPO, 'tools', 'import_gallery.py'), FRAMES, images, STRINGS, TABLES, gal)))

    ca = os.path.join(REPO, 'tools', 'audio', 'convert_audio.py')
    taud = T('tools/audio/convert_audio.py', 'tools/audio/oggdur.py', 'tools/audio/adpcm_loop_seal.py', 'third_party/wav2adpcm/wav2adpcm.c')
    ttool = [md5(TOOLENV['FFMPEG']), md5(TOOLENV['WAV2ADPCM'])]
    bkey = sorted((f, os.path.getsize(os.path.join(inp['bgm'], f))) for f in os.listdir(inp['bgm']))
    step(st, 6, total, 'bgm', 'music (%d tracks to ADPCM)' % len(bkey), st.key(bkey, taud, ttool),
         [os.path.join(out, 'bgm', 'bgm_manifest.csv')], lambda: run(py(ca, 'bgm', inp['bgm'], os.path.join(out, 'bgm'))))
    step(st, 7, total, 'sfx', 'sound effects and voices', st.key(k_extract, vkey, taud, ttool),
         [os.path.join(out, 'sfx', 'sfx_manifest.csv')],
         lambda: run(py(ca, 'sfx', audio, voices, os.path.join(out, 'sfx')), env=dict(VOX_PC=voices)))
    step(st, 8, total, 'galsnd', 'gallery sounds', st.key(k_extract, vkey, taud, ttool),
         [os.path.join(gal, 'gal_manifest.csv')], lambda: run(py(ca, 'gallery', audio, voices, voices, gal)))
    miss = [n for f, n in LEVELS if not os.path.isfile(os.path.join(out, n, n + '.lv2'))]
    if miss: raise Fail('levels missing after the build: %s' % ' '.join(miss))
    for t in [d for d in os.listdir(out) if d.endswith('.tmp')]: rmtree(os.path.join(out, t))
    LOG.say('[9/%d] data check: %d levels, %d ui files, %d music tracks, %d sounds, %d gallery files'
            % (total, len(LEVELS), len(os.listdir(ui)), len([f for f in os.listdir(os.path.join(out, 'bgm')) if f.endswith('.adp')]),
               len([f for f in os.listdir(os.path.join(out, 'sfx')) if f.endswith('.wav')]), len(os.listdir(gal))))
    LOG.say('data dir ready: %s' % out)

# ------------------------------------------------------------------------------------------------------------ Dreamcast
def kos_environ():
    k = os.environ.get('KOS_BASE')
    for p in ([os.path.join(k, 'environ.sh')] if k else []) + ['/opt/toolchains/dc/kos/environ.sh']:
        if os.path.isfile(p): return p
    return None

def build_dc(a, tools):
    env_sh = kos_environ() if a.local_tools and os.name != 'nt' else None
    if a.elf: elf = os.path.abspath(a.elf); how = 'given'
    elif env_sh:
        LOG.say('[10] compile fpdc.elf with KallistiOS (%s) ...' % env_sh)
        r = subprocess.run(['bash', '-c', 'source "$1" && make -j%d' % max(1, a.jobs), 'fpbuild', env_sh], cwd=REPO,
                           stdout=LOG.f, stderr=subprocess.STDOUT, env=child_env())
        if r.returncode: raise Fail('the KallistiOS build failed (log: %s)' % LOG.path)
        elf = os.path.join(REPO, 'fpdc.elf'); how = 'compiled with KallistiOS'
    else:
        elf = fetch.tool('fpdc.elf', LOG.say); how = 'the published program, SHA-256 checked'
    if not os.path.isfile(elf): raise Fail('Dreamcast program not found: %s' % elf)
    LOG.say('[10] Dreamcast program: %s (%s)' % (elf, how))
    disc = os.path.join(a.work, 'disc')
    cdi = os.path.abspath(a.cdi) if a.cdi else os.path.join(REPO, 'fpdc.cdi')     # next to build_dreamcast.bat / .sh
    os.makedirs(os.path.dirname(cdi), exist_ok=True)
    tmp = os.path.join(a.work, 'cdi', os.path.basename(cdi))     # (mkdcdisc records the file name in the image)
    rmtree(os.path.dirname(tmp)); os.makedirs(os.path.dirname(tmp))
    LOG.say('[11] disc image: copy, pre-twiddle, pack, pad the data (tools/make_disc.py), mkdcdisc ...')
    run(py(os.path.join(REPO, 'tools', 'make_disc.py'), 'cdi', elf, a.out, disc, tmp, '--mkdcdisc', tools['mkdcdisc']),
        env=dict(SOURCE_DATE_EPOCH=os.environ.get('SOURCE_DATE_EPOCH', SOURCE_DATE)))
    if os.path.exists(cdi): os.remove(cdi)
    shutil.move(tmp, cdi)
    rmtree(disc)
    LOG.say('DONE: %s (%.0f MB). Burn it to a CD-R (DiscJuggler, ImgBurn) or open it in an emulator (Flycast).'
            % (cdi, os.path.getsize(cdi) / 1e6))
    return cdi

# ------------------------------------------------------------------------------------------------------------- selftest
def selftest():
    """offline checks: the wrappers' Python pins agree with downloads.json, every table file is there, local copies of the
    downloads (if any) have their recorded SHA-256, oggdur / the sfx list are consistent"""
    ok = True
    t = fetch.table()
    for e in t['files']:
        if e['kind'] == 'python':
            for w in ('tools/builder/bootstrap.cmd', 'tools/builder/bootstrap.sh'):
                s = open(os.path.join(REPO, w), encoding='utf-8').read()
                if (e['os'] == 'windows') == w.endswith('.cmd') and not (e['name'] in s and e['sha256'] in s and
                                                                        (e.get('url') or '') in s):
                    print('FAIL %s does not pin %s / %s' % (w, e['name'], e['sha256'])); ok = False
    for f in [WINMAP, STRINGS, os.path.join(FRAMES, 'anims.json'), os.path.join(FRAMES, 'backdrops.json')] + \
             [os.path.join(FRAMES, 'frame_%d.json' % n) for n in [3, 4, 6, 7, 11, 12, 84, 86] + [f for f, n in LEVELS]]:
        if not os.path.isfile(f): print('FAIL missing table %s' % f); ok = False
    for e in t['files']:
        try: fetch.local(e['name'])
        except fetch.FetchError as ex: print('FAIL', ex); ok = False
    if os.path.exists(os.path.join(REPO, '.git')):         # (a ZIP download has no history: nothing to compare; .git is a file in a worktree)
        try: tree = subprocess.check_output(['git', '-C', REPO, 'rev-parse', 'HEAD:src'], text=True,
                                            stderr=subprocess.DEVNULL).strip()
        except (OSError, subprocess.CalledProcessError): tree = None
        for e in t['files']:
            if tree and e.get('src_tree') and e['src_tree'] != tree:
                print('FAIL prebuilt/%s was built from an older src/ (tree %s, now %s): run tools/builder/%s'
                      % (e['name'], e['src_tree'][:10], tree[:10], 'build_release.sh --program-only' if e['name'] ==
                         'fpdc.elf' else 'build_release_pc.sh')); ok = False
    import convert_audio as ca
    if len(needed_voices()) < 30 or not ca.SFX: print('FAIL sfx list'); ok = False
    print('selftest %s' % ('ok' if ok else 'FAILED'))
    return 0 if ok else 1

# targets: name -> (what it builds, needs the CDI writer, function(args, tools, this module) run after the data dir). More come from
# tools/builder/fpbuild_targets_*.py modules (TARGETS dict, ADD_ARGS(parser) optional), e.g. on the PC branch.
TARGETS = {'data': ('the data dir', False, None), 'dc': ('the Dreamcast disc image', True, lambda a, t, fb: build_dc(a, t))}
EXTRA_ARGS = []
def load_targets():
    import glob, importlib
    for f in sorted(glob.glob(os.path.join(HERE, 'fpbuild_targets_*.py'))):
        m = importlib.import_module(os.path.basename(f)[:-3])
        TARGETS.update(m.TARGETS)
        if hasattr(m, 'ADD_ARGS'): EXTRA_ARGS.append(m.ADD_ARGS)

def main():
    global LOG
    if sys.argv[1:2] == ['selftest']: return selftest()
    load_targets()
    p = argparse.ArgumentParser(description='Freedom Planet port builder (BUILDING.md)')
    p.add_argument('target', choices=sorted(TARGETS))
    p.add_argument('assets', nargs='?', help='Assets.dat (Windows GOG or Linux 1.21.5), or the folder holding it; '
                   'without it: found automatically next to the project, else asked with a file dialog')
    p.add_argument('extra', nargs='*', help=argparse.SUPPRESS)       # (a second dropped file is ignored)
    p.add_argument('--out', default=os.path.join(REPO, 'build', 'builder', 'data'))
    p.add_argument('--work', default=os.path.join(REPO, 'build', 'builder'))
    p.add_argument('--voices-pc', default=None)
    p.add_argument('-j', '--jobs', type=int, default=max(1, min(4, os.cpu_count() or 1)))
    p.add_argument('--force', action='store_true')
    p.add_argument('--cdi'); p.add_argument('--elf')
    p.add_argument('--local-tools', action='store_true', default=os.environ.get('FPB_LOCAL') == '1')
    for f in EXTRA_ARGS: f(p)
    a = p.parse_args()
    a.out = os.path.abspath(a.out); a.work = os.path.abspath(a.work)
    what, need_disc, fn = TARGETS[a.target]
    LOG = Log(os.path.join(a.work, 'build.log'))
    LOG.say('Freedom Planet builder: %s (log: %s)' % (what, LOG.path))
    try:
        os.makedirs(a.work, exist_ok=True)
        free = shutil.disk_usage(a.work).free / 1e9
        if free < NEED_GB and not os.path.isdir(os.path.join(a.out, 'bgm')):
            LOG.say('WARNING: %.1f GB free on the drive of %s; a first build needs about %.0f GB' % (free, a.work, NEED_GB))
        res = os.environ.get('FPB_RESULT')        # bootstrap.cmd: written only on success (a crash cannot report success)
        if res and os.path.exists(res): os.remove(res)
        inp = find_inputs(a)
        tools = setup_tools(a, need_disc)
        build_data(a, inp)
        if fn: fn(a, tools, sys.modules[__name__])
        if res: open(res, 'w').write('OK\n')
    except (Fail, fetch.FetchError) as e:
        LOG.say('ERROR: %s' % e); return 1
    except KeyboardInterrupt:
        LOG.say('interrupted'); return 130
    return 0

if __name__ == '__main__':
    sys.exit(main())
