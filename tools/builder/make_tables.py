#!/usr/bin/env python3
"""
Developer tool: (re)generate the builder's committed tables. End users never run this.

  make_tables.py winmap <linux_extract_dir> <windows_extract_dir> <out.json>
      Image id map from the Windows GOG Assets.dat (md5 4085c983..., 17162 images) to the Linux one the pipeline reads
      (md5 9d00124a..., 16885 images). Both dirs are fp-assets.py -x outputs. An image matches when its PNG and its
      _meta.txt (hot spot, action point) are byte-identical. Output: runs [linux_first, linux_last, windows_id - linux_id]
      and, for the Linux ids without an identical Windows image, a 'platform' entry: the Linux size and meta (numbers) and
      a Windows source: 'exact' (same pixels, other meta: the Linux meta is written), 'subst' (the same object's PC-version
      image, e.g. ATK / SPC keyboard labels instead of B / C) or none (a transparent image of the Linux size).
      Audio: the 475 sounds are compared too (the builder links them only if the table says they are identical).

  make_tables.py labels <linux_extract_dir> <windows_extract_dir> <winmap.json>
      For the platform images that have a PC substitute ('subst'), the console labels redrawn on the PC image: Milla's HUD
      panel (GUIScoreTimeRings_345: the pad labels B / C, which the port turns into B / X, D-110, instead of ATK / SPC) and
      the dig hint (GUIWeaponSlot_426: down + B). Stored as a 'labels' recipe per image: the box that differs, one
      character per pixel ('?' keep the PC pixel, '.' transparent, a letter = the colour of the PC pixel at colours[letter],
      so no colour value is stored either) and checked to give the Linux pixels exactly. Images whose differing pixels
      need a colour the PC image does not have, or more than LABEL_MAX pixels, get no recipe.

  make_tables.py layout <bin64/Chowdren> <decompiled_dir> <tables_dir>
      The tables the pipeline reads from the Linux program (md5 65f22f98...), committed in tables/ so that a Windows
      Assets.dat alone is enough to build (tables/README.md):
        frames/frame_<N>.json, classes.json  tools/re/dump_frames.py (objdump)   level / screen layouts
        frames/anims.json                    tools/re/dump_anims.py (pyelftools) animation tables
        frames/backdrops.json                tools/re/dump_backdrops.py          backdrop (tiled) layers
        strings.json                         the gallery / credits text: the string literals that the List* / credits
                                             constructors (<decompiled_dir>, Ghidra output, build/re) pass, read from
                                             the binary (import_gallery.elf_strings, which reads this file instead of
                                             the binary when given a .json)
"""
import hashlib, json, os, re, sys

def digests(d, kind, pre, ext):
    n = len([f for f in os.listdir(os.path.join(d, kind)) if f.endswith('.' + ext)])
    out = []
    for i in range(n):
        b = open(os.path.join(d, kind, '%s_%d.%s' % (pre, i, ext)), 'rb').read()
        mp = os.path.join(d, kind, '%s_%d_meta.txt' % (pre, i))
        m = open(mp, 'rb').read() if os.path.exists(mp) else b''
        out.append((hashlib.md5(b).hexdigest(), hashlib.md5(m).hexdigest()))
    return out

def meta(d, i):
    return [int(v, 0) for v in open(os.path.join(d, 'images', 'img_%d_meta.txt' % i)).read().split()]

def visdiff(a, b):
    """pixels that look different: another alpha, or (when visible) a channel more than 16 apart"""
    return sum(1 for p, q in zip(a.getdata(), b.getdata())
               if p[3] != q[3] or (p[3] and max(abs(p[0] - q[0]), abs(p[1] - q[1]), abs(p[2] - q[2])) > 16))

def winmap(L, W, out):
    from PIL import Image
    li, wi = digests(L, 'images', 'img', 'png'), digests(W, 'images', 'img', 'png')
    la, wa = digests(L, 'audio', 'audio', 'ogg'), digests(W, 'audio', 'audio', 'ogg')
    full, pix = {}, {}
    for j, x in enumerate(wi):
        full.setdefault(x, j); pix.setdefault(x[0], j)
    # monotonic walk: Windows inserts images, the shared ones keep their order
    m = {}; j = 0
    for i, x in enumerate(li):
        k = next((k for k in range(j, min(j + 400, len(wi))) if wi[k] == x), None)
        if k is not None: m[i] = k; j = k + 1
    runs = []
    for i in sorted(m):
        if runs and runs[-1][1] == i - 1 and runs[-1][2] == m[i] - i: runs[-1][1] = i
        else: runs.append([i, i, m[i] - i])
    plat = {}
    unused = sorted(set(range(len(wi))) - set(m.values()))
    sizes = {k: Image.open(os.path.join(W, 'images', 'img_%d.png' % k)).size for k in unused}
    for i in range(len(li)):
        if i in m: continue
        a = Image.open(os.path.join(L, 'images', 'img_%d.png' % i)).convert('RGBA')
        e = {'w': a.width, 'h': a.height, 'meta': meta(L, i)}
        if li[i][0] in pix:
            e['exact'] = pix[li[i][0]]
        else:
            # the PC version of the same image: same size, fewest differing pixels, among the Windows images that no
            # Linux image matched (nearer ids win ties); under a third of a small image (a label), under a tenth of a
            # larger one (a text panel in another language must not pass)
            ok = lambda n, area: n < area // 3 and (area <= 4096 or n <= area // 10)
            best = None
            for k in sorted(unused, key=lambda k: abs(k - i)):
                if sizes[k] != a.size: continue
                b = Image.open(os.path.join(W, 'images', 'img_%d.png' % k)).convert('RGBA')
                n = visdiff(a, b)
                if ok(n, a.width * a.height) and (best is None or n < best[0]): best = (n, k)
            if best: e['subst'] = best[1]; e['differing_px'] = best[0]
        plat[str(i)] = e
    res = {'windows_md5': '4085c983fb918703ce459f17f69c474c', 'linux_md5': '9d00124aa9e4498d217f951afb962171',
           'linux_images': len(li), 'windows_images': len(wi), 'runs': runs, 'platform': plat,
           'audio_identical': la == wa, 'sounds': len(la)}
    json.dump(res, open(out, 'w'), indent=0, sort_keys=True)
    print('%d runs, %d identical, %d platform-specific (%d exact pixels, %d substitutes, %d blank), audio identical: %s'
          % (len(runs), len(m), len(plat), sum('exact' in e for e in plat.values()), sum('subst' in e for e in plat.values()),
             sum('exact' not in e and 'subst' not in e for e in plat.values()), la == wa))

LABEL_MAX = 400      # differing pixels: label glyphs, not artwork

def label_recipe(a, b):
    """a: the Linux image, b: its PC substitute (RGBA, same size) -> the recipe that turns b into a, or None"""
    pa, pb = a.load(), b.load()
    diff = [(x, y) for y in range(a.height) for x in range(a.width) if pa[x, y] != pb[x, y]]
    if not diff or len(diff) > LABEL_MAX: return None
    x0, y0 = min(x for x, y in diff), min(y for x, y in diff); x1, y1 = max(x for x, y in diff), max(y for x, y in diff)
    sym, pos = {}, {}
    rows = []
    for y in range(y0, y1 + 1):
        r = ''
        for x in range(x0, x1 + 1):
            c = pa[x, y]
            if c == pb[x, y]: r += '?'; continue
            if c[3] == 0: r += '.'; continue
            if c not in sym:
                at = next(((u, v) for v in range(b.height) for u in range(b.width)
                           if pb[u, v] == c and not (x0 <= u <= x1 and y0 <= v <= y1)), None)
                if at is None: return None
                sym[c] = chr(ord('A') + len(sym)); pos[sym[c]] = list(at)
            r += sym[c]
        rows.append(r)
    return {'box': [x0, y0, x1, y1], 'rows': rows, 'colours': pos}

def apply_labels(b, rec):
    """the PC image b with a label recipe applied (tools/builder/fpbuild.py map_windows does the same)"""
    b = b.copy(); px = b.load(); src = b.copy().load()
    x0, y0 = rec['box'][:2]
    col = {k: src[u, v] for k, (u, v) in rec['colours'].items()}
    for dy, r in enumerate(rec['rows']):
        for dx, ch in enumerate(r):
            if ch == '?': continue
            px[x0 + dx, y0 + dy] = (0, 0, 0, 0) if ch == '.' else col[ch]
    return b

def labels(L, W, table):
    from PIL import Image
    t = json.load(open(table))
    n = 0
    for k, e in sorted(t['platform'].items(), key=lambda kv: int(kv[0])):
        e.pop('labels', None)
        if 'subst' not in e: continue
        a = Image.open(os.path.join(L, 'images', 'img_%s.png' % k)).convert('RGBA')
        b = Image.open(os.path.join(W, 'images', 'img_%d.png' % e['subst'])).convert('RGBA')
        rec = label_recipe(a, b)
        if rec is None: continue
        if list(apply_labels(b, rec).getdata()) != list(a.getdata()): sys.exit('labels: recipe for %s does not reproduce it' % k)
        e['labels'] = rec; n += 1
        print('%s: labels %s, %d colours' % (k, rec['box'], len(rec['colours'])))
    json.dump(t, open(table, 'w'), indent=0, sort_keys=True)
    print('%d label recipes' % n)

STRING_FILES = ['ListSongFilenames_285__ListSongFilenames_285_int__int.c', 'ListSongs_283__ListSongs_283_int__int.c',
                'ListNames_272__ListNames_272_int__int.c', 'ListDescriptions_273__ListDescriptions_273_int__int.c',
                'ListWarnings_281__ListWarnings_281_int__int.c', 'create_creditslist_4189_int__int.c',
                'CreditsStageNames_4202__CreditsStageNames_4202_int__int.c']
BIN_MD5 = '65f22f98729ed6bddecdf4242197d2cf'

def layout(binary, re_dir, out):
    import subprocess
    tools = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
    sys.path.insert(0, tools)
    from import_gallery import elf_strings_all
    if hashlib.md5(open(binary, 'rb').read()).hexdigest() != BIN_MD5: sys.exit('%s is not the Linux 1.21.5 program' % binary)
    fr = os.path.join(out, 'frames'); os.makedirs(fr, exist_ok=True)
    for f in os.listdir(fr): os.remove(os.path.join(fr, f))
    py = [sys.executable, '-B']
    subprocess.run(py + [os.path.join(tools, 're', 'dump_frames.py'), binary, fr], check=True)
    subprocess.run(py + [os.path.join(tools, 're', 'dump_anims.py'), binary, os.path.join(fr, 'anims.json')], check=True)
    subprocess.run(py + [os.path.join(tools, 're', 'dump_backdrops.py'), binary, os.path.join(fr, 'backdrops.json')], check=True)
    res = {'binary_md5': BIN_MD5, 'files': {f: elf_strings_all(binary, os.path.join(re_dir, f)) for f in STRING_FILES}}
    with open(os.path.join(out, 'strings.json'), 'w', encoding='utf-8', newline='\n') as o:
        json.dump(res, o, indent=1, ensure_ascii=False); o.write('\n')
    print('%d frame files; strings: %s' % (len(os.listdir(fr)), ', '.join('%s %d' % (f.split('_')[0], len(v)) for f, v in res['files'].items())))

if __name__ == '__main__':
    if len(sys.argv) == 5 and sys.argv[1] == 'winmap': winmap(*sys.argv[2:])
    elif len(sys.argv) == 5 and sys.argv[1] == 'layout': layout(*sys.argv[2:])
    elif len(sys.argv) == 5 and sys.argv[1] == 'labels': labels(*sys.argv[2:])
    else: sys.exit(__doc__)
