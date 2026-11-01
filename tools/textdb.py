#!/usr/bin/env python3
"""
The game's text (dialogue, captions, subtitles, gallery / credits strings) is NOT committed (D-123). The committed tables
tables/story/dialog.json and tables/strings.json keep their structure, but every text is a token
    "~T~<h>~<n>~<p>"   h: sha256 of the text's bytes (Latin-1), first 20 hex digits; n: its length in bytes;
                       p: sha256 of its first 24 bytes, first 12 hex (only for texts longer than 24 bytes, else "")
At build time the text is read back from the player's own game program (Windows FP.exe or Linux bin64/Chowdren, 1.21.5):
every NUL-terminated string literal in it (and the text part of a "<speaker> <anim> <voice> <text>" dialogue literal) is
hashed and matched. A literal that the linker merged into the tail of a longer one is found by its length before a NUL.
A text the user's build words differently (the Windows build's gallery warnings and credits) is taken from the literal
that starts with the same 24 bytes. A text that cannot be found is left empty (a silent / blank line) and reported.

usage: textdb.py materialize <FP.exe | Chowdren> [more binaries] [--out DIR]   (default DIR: build/text)
       textdb.py tokenize                 re-tokenize tables/ from full-text files in build/text (after make_tables.py)
       textdb.py check                    the committed tables hold tokens only (no plain game text)
Consumers call textdb.path('story/dialog.json' | 'strings.json'): $FP_TEXT_DIR, else build/text, else an error.
Host-only tool (FP_HOST_ENV): nothing here runs on the Dreamcast.
"""
import hashlib, json, os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
TABLES = os.path.join(ROOT, 'tables')
FILES = ('story/dialog.json', 'strings.json')
TOK = re.compile(r'^~T~([0-9a-f]{20})~(\d+)~([0-9a-f]{12})?$')
LINE = re.compile(rb'^(\d\d) (\d\d|xx) ([a-z0-9_]+) (.*)$', re.S)
PRE = 24

def _h(b): return hashlib.sha256(b).hexdigest()[:20]
def _p(b): return hashlib.sha256(b[:PRE]).hexdigest()[:12] if len(b) > PRE else ''

def token(text):
    b = text.encode('latin1')
    return '~T~%s~%d~%s' % (_h(b), len(b), _p(b))

# where the text sits in each table (everything else: ids, numbers, voice names, ink, structure stays as it is)
def _walk(rel, d, fn):
    if rel == 'strings.json':
        for k, v in d['files'].items(): d['files'][k] = [fn(t) for t in v]
    else:
        for grp in ('dialog', 'lipsync'):
            for k, v in d.get(grp, {}).items():
                for p in v: p[3] = fn(p[3])
        for k in d.get('captions', {}): d['captions'][k] = fn(d['captions'][k])
        for k, v in d.get('subtitles', {}).items(): d['subtitles'][k] = [fn(t) for t in v]
    return d

def _dump(d, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as o: json.dump(d, o, ensure_ascii=False, indent=1); o.write('\n')

def path(rel, need_text=True):
    """the table with its text: $FP_TEXT_DIR/<rel>, else build/text/<rel>; need_text=False: the committed (tokenized) one"""
    if not need_text: return os.path.join(TABLES, rel)
    for d in (os.environ.get('FP_TEXT_DIR'), os.path.join(ROOT, 'build', 'text')):
        if d and os.path.isfile(os.path.join(d, rel)): return os.path.join(d, rel)
    raise SystemExit('textdb: no game text for %s. Run: python3 tools/textdb.py materialize "<your Freedom Planet folder>/FP.exe" '
                     '(or bin64/Chowdren); the drag-and-drop builder does this by itself.' % rel)

class Index:
    """every string literal of the game program(s), by hash"""
    def __init__(self, binaries):
        self.full, self.pre, self.data = {}, {}, []
        for b in binaries:
            data = open(b, 'rb').read(); self.data.append(data)
            for m in re.finditer(rb'[^\0]+', data):
                lit = m.group(0)
                self.full.setdefault(_h(lit), lit)
                mm = LINE.match(lit)
                if mm: self.full.setdefault(_h(mm.group(4)), mm.group(4))
                if len(lit) > PRE: self.pre.setdefault(_p(lit), lit)
    def tail(self, h, n):
        """a literal merged into the tail of a longer one: the n bytes before a NUL"""
        for data in self.data:
            for m in re.finditer(rb'\0', data):
                e = m.start()
                if e >= n and data[e - 1] != 0 and _h(data[e - n:e]) == h: return data[e - n:e]
        return None

def materialize(binaries, out=None):
    out = out or os.path.join(ROOT, 'build', 'text')
    ix = Index(binaries)
    st = {'tokens': 0, 'exact': 0, 'tail': 0, 'variant': 0, 'missing': []}
    pending = {}
    def fn(t):
        m = TOK.match(t) if isinstance(t, str) else None
        if not m: return t
        st['tokens'] += 1
        h, n, p = m.group(1), int(m.group(2)), m.group(3) or ''
        if h in ix.full: st['exact'] += 1; return ix.full[h].decode('latin1')
        pending.setdefault((h, n, p), None)
        return t
    tabs = {rel: _walk(rel, json.load(open(os.path.join(TABLES, rel), encoding='utf-8')), fn) for rel in FILES}
    # the slower fallbacks once per unresolved text
    by_n = {}
    for (h, n, p) in pending: by_n.setdefault(n, []).append((h, p))
    found = {}
    if pending:
        for data in ix.data:
            ends = [m.start() for m in re.finditer(rb'(?<=[^\0])\0', data)]
            for n, hs in by_n.items():
                want = {h for h, p in hs if h not in found}
                if not want: continue
                for e in ends:
                    if e >= n:
                        k = _h(data[e - n:e])
                        if k in want: found[k] = ('tail', data[e - n:e]); want.discard(k)
                        if not want: break
        for (h, n, p) in pending:
            if h not in found and p and p in ix.pre: found[h] = ('variant', ix.pre[p])
    def fn2(t):
        m = TOK.match(t) if isinstance(t, str) else None
        if not m: return t
        r = found.get(m.group(1))
        if not r: st['missing'].append(m.group(1)); return ''
        st[r[0]] += 1; return r[1].decode('latin1')
    for rel, d in tabs.items(): _dump(_walk(rel, d, fn2), os.path.join(out, rel))
    return st, out

def tokenize(src=None, dst=None, keep=None):
    """full-text tables in src (default build/text) -> tokens in dst (default tables/); keep: also copy the full text there"""
    src = src or os.path.join(ROOT, 'build', 'text')
    for rel in FILES:
        p = os.path.join(src, rel)
        if not os.path.isfile(p): continue
        d = json.load(open(p, encoding='utf-8'))
        if keep and os.path.abspath(keep) != os.path.abspath(src): _dump(d, os.path.join(keep, rel))
        _dump(_walk(rel, d, lambda t: t if TOK.match(t) else token(t)), os.path.join(dst or TABLES, rel))

def check():
    bad = []
    for rel in FILES:
        d = json.load(open(os.path.join(TABLES, rel), encoding='utf-8'))
        _walk(rel, d, lambda t: t if TOK.match(t) or bad.append((rel, t[:40])) else t)
    return bad

if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['materialize'] and len(a) > 1:
        out = None
        if '--out' in a: i = a.index('--out'); out = a[i + 1]; a = a[:i] + a[i + 2:]
        st, out = materialize(a[1:], out)
        print('game text: %d texts, %d found (%d merged tails, %d worded differently in this build), %d missing -> %s'
              % (st['tokens'], st['tokens'] - len(st['missing']), st['tail'], st['variant'], len(st['missing']), out))
        sys.exit(0)
    if a[:1] == ['tokenize']: tokenize(a[1] if len(a) > 1 else None); print('tables tokenized'); sys.exit(0)
    if a[:1] == ['check']:
        bad = check(); print('plain text in tables: %d' % len(bad)); [print(' ', b) for b in bad[:10]]; sys.exit(1 if bad else 0)
    print(__doc__); sys.exit(2)
