#!/usr/bin/env python3
"""
The builder's downloads (tools/builder/downloads.json): pinned files, each checked against its recorded SHA-256 before use.

Where a file is looked for, in this order (the first copy found is used, after the SHA-256 check):
  1. $FPDC_PREBUILT (a folder)          2. next to the build_*.bat / .sh scripts (the checkout's top folder)
  3. <checkout>/prebuilt/               4. build/cache/downloads/ (earlier downloads)
  5. downloaded from its URL (the project's release https://github.com/Wangblows2000/fp-dc/releases/download/builder-v1/
     for the files built from this repository and the CDI writer, the original publishers for Python, its packages and ffmpeg)
A local copy with another SHA-256 is refused (with the name of the expected file), never used.

usage: fetch.py check                   verify every local copy that exists (no download)
       fetch.py get <name>...           fetch (or find) the files, print their paths
       fetch.py list                    the files of this platform, with sizes
Host-only tool (FP_HOST_ENV): nothing here runs on the Dreamcast.
"""
import hashlib, json, os, shutil, ssl, subprocess, sys, time, urllib.request, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
TABLE = os.path.join(HERE, 'downloads.json')
CACHE = os.path.join(REPO, 'build', 'cache')
DL = os.path.join(CACHE, 'downloads')

class FetchError(Exception): pass

def host_os():
    return 'windows' if os.name == 'nt' else ('linux' if sys.platform.startswith('linux') else sys.platform)

def table():
    """downloads.json, plus the files of any downloads_*.json beside it (e.g. the PC branch's)"""
    import glob
    t = json.load(open(TABLE, encoding='utf-8'))
    for extra in sorted(glob.glob(os.path.join(HERE, 'downloads_*.json'))):
        x = json.load(open(extra, encoding='utf-8'))
        for e in x['files']: e.setdefault('release_url', x.get('release_url', t['release_url']))
        t['files'] += x['files']
    return t

def entry(name):
    t = table()
    for e in t['files']:
        if e['name'] == name: return e, t
    raise FetchError('%s is not in tools/builder/downloads.json' % name)

def files_for(os_name=None, kinds=None):
    os_name = os_name or host_os()
    return [e for e in table()['files'] if e['os'] in (os_name, 'any') and (kinds is None or e['kind'] in kinds)]

def url_of(e, t=None):
    t = t or table()
    return e.get('url') or e.get('release_url', t['release_url']) + e['name']

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()

def search_dirs():
    d = []
    if os.environ.get('FPDC_PREBUILT'): d.append(os.path.abspath(os.environ['FPDC_PREBUILT']))
    d += [REPO, os.path.join(REPO, 'prebuilt'), DL]
    return d

def local(name, say=print):
    """the verified local copy of <name>, or None; a copy with another SHA-256 raises"""
    e, t = entry(name)
    for d in search_dirs():
        p = os.path.join(d, name)
        if os.path.isfile(p):
            h = sha256(p)
            if h != e['sha256']:
                if d == DL:      # a broken earlier download: fetch it again
                    os.remove(p); continue
                raise FetchError('%s has SHA-256 %s, expected %s (%s). Delete it, or put the file of the right version '
                                 'there.' % (p, h, e['sha256'], e.get('what', name)))
            return p
    return None

def _ssl_context():
    ctx = ssl.create_default_context()
    if not ctx.get_ca_certs() and os.name != 'nt':
        for p in ('/etc/ssl/certs/ca-certificates.crt', '/etc/pki/tls/certs/ca-bundle.crt', '/etc/ssl/ca-bundle.pem',
                  '/etc/ssl/cert.pem', '/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem'):
            if os.path.isfile(p):
                ctx.load_verify_locations(p); break
    return ctx

def _curl():
    c = shutil.which('curl.exe' if os.name == 'nt' else 'curl')
    return c

def _download(url, dest, size, say):
    """curl when there is one (Windows 10 and 11 have curl.exe; it uses the system's certificates and proxy settings),
    else Python's urllib (then wget)"""
    tmp = dest + '.part'
    if os.path.exists(tmp): os.remove(tmp)
    c = _curl()
    if c and subprocess.run([c, '-fL', '--retry', '3', '-#', '-o', tmp, url]).returncode == 0:
        os.replace(tmp, dest); return
    if os.path.exists(tmp): os.remove(tmp)
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'fpdc-builder'})
        with urllib.request.urlopen(req, context=_ssl_context(), timeout=60) as r, open(tmp, 'wb') as f:
            got = 0; t0 = last = time.time()
            while True:
                b = r.read(1 << 20)
                if not b: break
                f.write(b); got += len(b)
                if time.time() - last > 5 and size:
                    last = time.time()
                    say('    %s: %d of %d MB (%.1f MB/s)' % (os.path.basename(dest), got >> 20, size >> 20, got / 1e6 / max(0.1, last - t0)))
    except Exception as ex:
        if os.path.exists(tmp): os.remove(tmp)
        if shutil.which('wget') and subprocess.run(['wget', '-q', '-O', tmp, url]).returncode == 0 and os.path.exists(tmp):
            os.replace(tmp, dest); return
        if os.path.exists(tmp): os.remove(tmp)
        raise FetchError('download failed: %s (%s)' % (url, ex))
    os.replace(tmp, dest)

def get(name, say=print):
    """the path of a verified copy of <name>: a local one, else downloaded into build/cache/downloads"""
    p = local(name, say)
    if p: return p
    e, t = entry(name)
    url = url_of(e, t)
    os.makedirs(DL, exist_ok=True)
    dest = os.path.join(DL, name)
    say('  downloading %s (%.1f MB): %s' % (e.get('what', name), e['size'] / 1e6, url))
    _download(url, dest, e['size'], say)
    h = sha256(dest)
    if h != e['sha256']:
        bad = dest + '.bad'
        os.replace(dest, bad)
        raise FetchError('%s: downloaded file has SHA-256 %s, expected %s (kept as %s). The download was damaged or the '
                         'file on the server changed; try again later.' % (url, h, e['sha256'], bad))
    return dest

# ----------------------------------------------------------------------------------------------- installs into the cache
def install_wheels(site, say=print):
    """the Python packages (wheels: zip files) of this platform, unpacked into <site> (the managed Python's site-packages)"""
    os.makedirs(site, exist_ok=True)
    for e in files_for(kinds=['wheel']):
        mark = os.path.join(site, '.fpdc-' + e['sha256'][:16])
        if os.path.exists(mark): continue
        p = get(e['name'], say)
        say('  installing %s' % e['what'])
        with zipfile.ZipFile(p) as z: z.extractall(site)
        open(mark, 'w').write(e['name'] + '\n')

def tool(name, say=print):
    """a program from downloads.json, ready to run: the path of the executable (unpacked from its archive when needed)"""
    e, t = entry(name)
    if e['kind'] == 'zip-member':        # e.g. ffmpeg.exe in the ffmpeg zip
        dest_dir = os.path.join(CACHE, 'tools', e['sha256'][:12])
        exe = os.path.join(dest_dir, os.path.basename(e['member']))
        if not os.path.isfile(exe):
            p = get(name, say)
            os.makedirs(dest_dir, exist_ok=True)
            with zipfile.ZipFile(p) as z:
                for m in [e['member']] + e.get('also', []):
                    with z.open(m) as src, open(os.path.join(dest_dir, os.path.basename(m)) + '.part', 'wb') as out:
                        shutil.copyfileobj(src, out)
                    os.replace(os.path.join(dest_dir, os.path.basename(m)) + '.part', os.path.join(dest_dir, os.path.basename(m)))
        return exe
    p = get(name, say)
    if os.name != 'nt' and not os.access(p, os.X_OK):
        # downloads / prebuilt copies may lack the executable bit: run a copy that has it
        dest_dir = os.path.join(CACHE, 'tools', e['sha256'][:12]); os.makedirs(dest_dir, exist_ok=True)
        exe = os.path.join(dest_dir, e.get('exe_name', name))
        if not os.path.isfile(exe) or sha256(exe) != e['sha256']:
            shutil.copyfile(p, exe + '.part'); os.chmod(exe + '.part', 0o755); os.replace(exe + '.part', exe)
        return exe
    return p

def main():
    a = sys.argv[1:]
    if a[:1] == ['check']:
        bad = 0
        for e in table()['files']:
            try:
                p = local(e['name'], print)
                print('%-70s %s' % (e['name'], p or '-'))
            except FetchError as ex:
                print('BAD', ex); bad += 1
        sys.exit(1 if bad else 0)
    elif a[:1] == ['get'] and len(a) > 1:
        for n in a[1:]: print(get(n))
    elif a[:1] == ['list']:
        for e in files_for():
            print('%-70s %8.1f MB  %s' % (e['name'], e['size'] / 1e6, url_of(e)))
    else:
        sys.exit(__doc__)

if __name__ == '__main__':
    try: main()
    except FetchError as ex: sys.exit('ERROR: %s' % ex)
