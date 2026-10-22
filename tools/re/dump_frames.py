#!/usr/bin/env python3
"""
Recover frame (level) layouts from the symbol-bearing Linux Chowdren binary.

usage: dump_frames.py <Chowdren ELF> <out_dir> [frame numbers...]

For each Frames::on_frame_N_init we disassemble with objdump and track the
immediate values loaded into edi/esi/edx before each call:
    create_<class>_<id>(int x /*edi*/, int y /*esi*/) -> FrameObject*
    Frame::add_background_object(Frame*, FrameObject*, int layer /*edx*/)
    Frame::add_object(Frame*, FrameObject*, int layer /*edx*/)
Frame size comes from the first stores `movl $W,0x8(%rdi)` / `movl $H,0xc(%rdi)`.
Output: <out_dir>/frame_N.json {frame, width, height, objects:[{cls,id,x,y,layer,kind,addr}]}
Also writes classes.json (class -> create address, size).
Evidence-only: objects whose coordinates could not be resolved get x/y null.
This mirrors freedom_editor/tools/dump_frames_linux_64.py (r2-based) without radare2.
"""
import json, os, re, subprocess, sys

def sh(args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout

def to_int(s):
    v = int(s, 16)
    return v - (1 << 32) if v >= 1 << 31 else v

REG32 = {'edi': 'edi', 'esi': 'esi', 'edx': 'edx', 'ecx': 'ecx',
         'rdi': 'edi', 'rsi': 'esi', 'rdx': 'edx', 'rcx': 'ecx'}

def main():
    elf, out = sys.argv[1], sys.argv[2]
    want = set(int(a) for a in sys.argv[3:])
    os.makedirs(out, exist_ok=True)
    syms = sh(['nm', '-S', elf])
    frames = {}
    for l in syms.splitlines():
        p = l.split()
        if len(p) == 4 and p[2] in 'Tt':
            m = re.match(r'_ZN6Frames\d+on_frame_(\d+)_initEv$', p[3])
            if m:
                frames[int(m.group(1))] = (int(p[0], 16), int(p[1], 16))
    classes = {}
    for l in sh(['nm', '-S', '-C', elf]).splitlines():
        m = re.match(r'([0-9a-f]+) ([0-9a-f]+) T create_(\w+?)_(\d+)\(int, int\)$', l)
        if m:
            classes['%s_%s' % (m.group(3), m.group(4))] = {'addr': int(m.group(1), 16), 'size': int(m.group(2), 16)}
    json.dump(classes, open(os.path.join(out, 'classes.json'), 'w'), indent=0, sort_keys=True)

    call_re = re.compile(r'^\s*([0-9a-f]+):\s+call\s+[0-9a-f]+ <([^>]+)>')
    mov_re = re.compile(r'^\s*([0-9a-f]+):\s+mov\s+\$0x([0-9a-f]+),%(e?[sd]i|edx|ecx)\b')
    xor_re = re.compile(r'^\s*[0-9a-f]+:\s+xor\s+%(e[sd]i|edx|ecx),%\1')
    store_re = re.compile(r'^\s*[0-9a-f]+:\s+movl\s+\$0x([0-9a-f]+),0x(8|c)\(%rdi\)')
    clob_re = re.compile(r'^\s*[0-9a-f]+:\s+\S+\s+.*,%(e?[sd]i|edx|ecx|r[sd]i|rdx|rcx)$')

    summary = {}
    for n in sorted(frames):
        if want and n not in want:
            continue
        addr, size = frames[n]
        asm = sh(['objdump', '-d', '-C', '--no-show-raw-insn',
                  '--start-address=0x%x' % addr, '--stop-address=0x%x' % (addr + size), elf])
        regs = {}
        width = height = None
        pending = None
        objs = []
        for line in asm.splitlines():
            m = store_re.match(line)
            if m and (width is None or height is None):
                if m.group(2) == '8' and width is None: width = int(m.group(1), 16)
                if m.group(2) == 'c' and height is None: height = int(m.group(1), 16)
                continue
            m = mov_re.match(line)
            if m:
                regs[REG32.get(m.group(3), m.group(3))] = to_int(m.group(2)); continue
            m = xor_re.match(line)
            if m:
                regs[m.group(1)] = 0; continue
            m = call_re.match(line)
            if m:
                fn = m.group(2)
                cm = re.match(r'create_(\w+?)_(\d+)\(int, int\)', fn)
                if cm:
                    pending = {'cls': cm.group(1), 'id': int(cm.group(2)),
                               'x': regs.get('edi'), 'y': regs.get('esi'), 'addr': int(m.group(1), 16)}
                elif fn.startswith('Frame::add_background_object') or fn.startswith('Frame::add_object'):
                    if pending:
                        pending['layer'] = regs.get('edx')
                        pending['kind'] = 'bg' if 'background' in fn else 'obj'
                        objs.append(pending)
                        pending = None
                regs = {}          # calls clobber argument registers
                continue
            m = clob_re.match(line)
            if m:
                regs.pop(REG32.get(m.group(1), m.group(1)), None)
        json.dump({'frame': n, 'width': width, 'height': height, 'objects': objs},
                  open(os.path.join(out, 'frame_%d.json' % n), 'w'))
        cnt = {}
        for o in objs:
            cnt[o['cls']] = cnt.get(o['cls'], 0) + 1
        top = sorted(cnt.items(), key=lambda kv: -kv[1])[:6]
        summary[n] = {'w': width, 'h': height, 'n': len(objs),
                      'unresolved': sum(1 for o in objs if o['x'] is None or o['y'] is None), 'top': top}
        print(n, width, height, len(objs), summary[n]['unresolved'], top, flush=True)
    json.dump(summary, open(os.path.join(out, 'summary.json'), 'w'), indent=1)

if __name__ == '__main__':
    main()
