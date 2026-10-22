#!/usr/bin/env python3
"""Catalogue the original's audio (BGM oggs, voice oggs, Assets.dat sounds) with ffprobe.
usage: catalog.py <bgm_dir> <voices_dir|-> <extracted_audio_dir> <out.csv> [voice_listing.json]
Columns: kind, name, bytes, duration_s, rate, channels, codec, loop_start, loop_length, source.
voice_listing.json (device_list_dir result) adds sizes for clips that are not staged (duration blank).
Loop tags: Vorbis comments LOOPSTART/LOOPLENGTH if present (none expected; see RESEARCH_LOG)."""
import csv, json, os, subprocess, sys

def probe(path):
    r = subprocess.run(['ffprobe', '-v', 'error', '-show_entries',
                        'format=duration,size:stream=sample_rate,channels,codec_name:format_tags:stream_tags',
                        '-of', 'json', path], capture_output=True, text=True)
    j = json.loads(r.stdout or '{}')
    s = (j.get('streams') or [{}])[0]; f = j.get('format', {})
    tags = {k.upper(): v for k, v in {**f.get('tags', {}), **s.get('tags', {})}.items()}
    return (f.get('size'), f.get('duration'), s.get('sample_rate'), s.get('channels'), s.get('codec_name'),
            tags.get('LOOPSTART', ''), tags.get('LOOPLENGTH', tags.get('LOOPEND', '')))

def main():
    bgm, voices, sfx, out = sys.argv[1:5]
    listing = json.load(open(sys.argv[5])) if len(sys.argv) > 5 else None
    rows = []
    for n in sorted(os.listdir(bgm)):
        if n.endswith('.ogg'): rows.append(('bgm', n) + probe(os.path.join(bgm, n)) + ('staged',))
    seen = set()
    if voices != '-' and os.path.isdir(voices):
        for root, _, files in os.walk(voices):
            for n in sorted(files):
                if n.endswith('.ogg'):
                    rel = os.path.relpath(os.path.join(root, n), voices); seen.add(rel)
                    rows.append(('voice', rel) + probe(os.path.join(root, n)) + ('staged',))
    if listing:
        for e in listing['entries']:
            if e['type'] == 'file' and e['name'] not in seen:
                rows.append(('voice', e['name'], e['size'], '', '', '', '', '', '', 'listing only (not staged)'))
    for n in sorted(os.listdir(sfx), key=lambda s: (len(s), s)):
        if n.endswith('.ogg') or n.endswith('.wav'):
            rows.append(('sfx', n) + probe(os.path.join(sfx, n)) + ('Assets.dat',))
    with open(out, 'w', newline='') as f:
        w = csv.writer(f); w.writerow(['kind', 'name', 'bytes', 'duration_s', 'rate', 'channels', 'codec', 'loop_start', 'loop_length', 'source'])
        w.writerows(rows)
    for k in ('bgm', 'voice', 'sfx'):
        rr = [r for r in rows if r[0] == k]
        dur = sum(float(r[3]) for r in rr if r[3])
        print('%-5s %4d files  %8.1f MB  %7.1f s probed (%d probed)  rates %s  channels %s' % (
            k, len(rr), sum(int(r[2] or 0) for r in rr) / 1e6, dur, sum(1 for r in rr if r[3]),
            sorted({r[4] for r in rr if r[4]}), sorted({r[5] for r in rr if r[5]})))
if __name__ == '__main__':
    main()
