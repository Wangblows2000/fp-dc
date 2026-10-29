#!/usr/bin/env python3
"""
The credits text of the original (frame index 85, src/game/ending.c), read from the executable like the gallery's tables
(tools/import_gallery.py elf_strings): the string literals of the text objects' constructors.

usage: import_ending.py <chowdren_binary> <decompiled_dir> <out_dir>
  chowdren_binary  the original Linux executable (bin64/Chowdren), or the committed table tables/strings.json
  decompiled_dir   build/re: create_creditslist_4189_int__int.c (CreditsList_4189: paragraph 0 = the cast and the staff,
                   shown by Message_4188 at y 240; paragraph 1 = the testers and the thanks, Message2_4196 at y 2752; RE
                   event 22790) and CreditsStageNames_4202__CreditsStageNames_4202_int__int.c (the 14 stage names of the
                   statistics, paragraph s = stage index s; RE 22723)
                   (tools/re/decomp.sh 'create_creditslist_4189(int, int)' 'CreditsStageNames_4202::CreditsStageNames_4202(int, int)')
  out_dir          converted2/end (game data, gitignored)

Output: credits.txt  'FPCR1 <n0> <n1> <ns>' then n0 lines of paragraph 0, n1 lines of paragraph 1 and ns stage names
        (CR removed, the leading spaces kept: the original centres its lines with spaces in the 8x20 font).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from import_gallery import elf_strings

def main():
    binary, re_dir, out = sys.argv[1:4]
    lst = elf_strings(binary, os.path.join(re_dir, 'create_creditslist_4189_int__int.c'))
    names = elf_strings(binary, os.path.join(re_dir, 'CreditsStageNames_4202__CreditsStageNames_4202_int__int.c'))
    if len(lst) < 2 or len(names) < 14: raise SystemExit('credits text not found (%d paragraphs, %d names)' % (len(lst), len(names)))
    p0, p1 = (s.replace('\r', '').split('\n') for s in lst[:2])
    names = [n.replace('\r', '') for n in names[:14]]
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'credits.txt'), 'w', encoding='latin1', newline='\n') as f:
        f.write('FPCR1 %d %d %d\n' % (len(p0), len(p1), len(names)))
        for l in p0 + p1 + names: f.write(l + '\n')
    print('credits.txt: %d + %d lines, %d stage names' % (len(p0), len(p1), len(names)))

if __name__ == '__main__':
    main()
