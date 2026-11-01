#!/usr/bin/env python3
"""D-123: the game's text is not in the repository. tables/story/dialog.json and tables/strings.json hold tokens only, and
tools/textdb.py reads every text back from the player's game program.
Checks: no plain text in the committed tables; the token format; and, when a game program is at hand (argv, else the Linux
original at /home/claude/fprun/bin64/Chowdren), that every token is found in it and, when build/text_orig holds the
full text tables made by tools/builder/make_tables.py, that the result is the same text.
usage: test_textdb.py [FP.exe | bin64/Chowdren]"""
import json, os, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import textdb

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

bad = textdb.check()
check('tables_hold_no_plain_text', not bad, str(bad[:3]))
check('token_format', textdb.TOK.match(textdb.token('Three days later,')) and textdb.TOK.match(textdb.token('x' * 40)).group(3))
check('short_token_has_no_prefix', textdb.TOK.match(textdb.token('Hi!')).group(3) is None)

prog = sys.argv[1] if len(sys.argv) > 1 else '/home/claude/fprun/bin64/Chowdren'
if os.path.isfile(prog):
    out = tempfile.mkdtemp()
    st, out = textdb.materialize([prog], out)
    check('every_text_found', not st['missing'] and st['tokens'] > 1000, '%d tokens, %d missing' % (st['tokens'], len(st['missing'])))
    ref = os.path.join(ROOT, 'build', 'text_orig')
    if os.path.basename(prog) == 'Chowdren' and os.path.isdir(ref):
        for rel in textdb.FILES:
            a = json.load(open(os.path.join(out, rel), encoding='utf-8')); b = json.load(open(os.path.join(ref, rel), encoding='utf-8'))
            check('same_text_as_the_original_tables_' + rel.replace('/', '_'), a == b)
    os.environ['FP_TEXT_DIR'] = out
    check('path_uses_FP_TEXT_DIR', textdb.path('strings.json') == os.path.join(out, 'strings.json'))
else:
    print('NOTE no game program at %s: the materialize checks are skipped' % prog)
print('%d failed' % fails); sys.exit(1 if fails else 0)
