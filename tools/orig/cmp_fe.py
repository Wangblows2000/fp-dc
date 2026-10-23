#!/usr/bin/env python3
"""Compare a front-end trace of the ORIGINAL (tools/orig/run_fe.sh: .jsonl + .snd) with the port's host run
of the same input script (fpdc_host with FP_TRACE_SID=1 FP_TRACE_SND=1 FP_FE_TRACE=1).

usage: cmp_fe.py <orig.jsonl> <host_stdout.txt> [--cursor OFF] [--from F] [--to F]
Checks, frame by frame:
  - sound ids: the original logs play_id during logic frame f; the host prints "sid N" during its frame f
  - frame changes (original set_frame N  vs host "fe ... idx N")
  - cursor value: original member OFF alterable A at row f+1 (state after frame f) vs host "cur" at row f
Prints mismatches and a summary; exit 1 if any check fails."""
import json, sys, collections

def load_orig(path, cur_off):
    snd = collections.defaultdict(list); frames = {}
    for l in open(path.rsplit('.', 1)[0] + '.snd'):
        t = l.split()
        if len(t) >= 3 and t[1] == 'id': snd[int(t[0])].append(int(t[2]))
        if len(t) >= 3 and t[1] == 'set_frame': frames[int(t[0])] = int(t[2])
    cur = {}
    for l in open(path):
        r = json.loads(l)
        o = r['o'].get(cur_off)
        if isinstance(o, list): cur[r['f'] - 1] = int(o[8].get('A', 0))
    return snd, frames, cur

def load_host(path):
    snd = collections.defaultdict(list); frames = {}; cur = {}
    pend = []; last_idx = None
    for l in open(path):
        t = l.split()
        if not t: continue
        if t[0] == 'sid': pend.append(int(t[1])); continue
        if t[0] == 'f':
            f = int(t[1]); snd[f] += pend; pend = []
        if t[0] == 'fe':
            f = int(t[1]); kv = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
            cur[f] = int(kv['cur']); idx = int(kv['idx'])
            if last_idx is not None and idx != last_idx: frames[f] = idx
            last_idx = idx
    return snd, frames, cur

def main():
    a = sys.argv[1:]
    def opt(n, d):
        if n in a: i = a.index(n); v = a[i + 1]; del a[i:i + 2]; return v
        return d
    cur_off = opt('--cursor', 'db0'); f0 = int(opt('--from', 0)); f1 = int(opt('--to', 10 ** 9))
    os_, of, oc = load_orig(a[0], cur_off)
    hs, hf, hc = load_host(a[1])
    bad = 0
    for f in sorted(set(os_) | set(hs)):
        if f0 <= f <= f1 and sorted(os_.get(f, [])) != sorted(hs.get(f, [])):
            print('sound f%d: orig %s host %s' % (f, os_.get(f), hs.get(f))); bad += 1
    # set_frame logged during the original's frame f takes effect at the start of f+1; the host's row f
    # already shows the new screen: original f <-> host f-1
    hf1 = {f + 1: v for f, v in hf.items()}
    for f in sorted(set(of) | set(hf1)):
        if f0 <= f <= f1 and of.get(f) != hf1.get(f):
            print('frame change f%d: orig %s host %s' % (f, of.get(f), hf1.get(f))); bad += 1
    n = 0
    for f in sorted(oc):
        if f0 <= f <= f1 and f in hc:
            n += 1
            if oc[f] != hc[f]: print('cursor f%d: orig %d host %d' % (f, oc[f], hc[f])); bad += 1
    print('== %d mismatches (%d sound frames, %d frame changes, %d cursor frames compared)' % (bad, len(os_), len(of), n))
    sys.exit(1 if bad else 0)

if __name__ == '__main__':
    main()
