#!/usr/bin/env python3
"""Generates data/test1.txt, an ORIGINAL test layout (not Freedom Planet level data)."""
W, H = 200, 30
g = [['.'] * W for _ in range(H)]
ground = [24] * W            # top solid row per column
def fill_col(x, top):
    for y in range(top, H): g[y][x] = '#'
# terrain profile
for x in range(W): ground[x] = 24
for x in range(30, 46): ground[x] = 24 - (x - 30) // 2     # gentle up-right
for x in range(46, 60): ground[x] = 16
for x in range(60, 68): ground[x] = 16 + (x - 60)          # 45 down
for x in range(68, 80): ground[x] = 24
for x in range(80, 92): ground[x] = 99                      # pit (spikes at bottom)
for x in range(92, 120): ground[x] = 24
for x in range(120, 128): ground[x] = 23 - (x - 120)       # 45 up
for x in range(128, 150): ground[x] = 16
for x in range(150, 166): ground[x] = 16 + (x - 150) // 2  # gentle down
for x in range(166, W): ground[x] = 24
for x in range(W):
    t = ground[x]
    if t >= H: continue
    fill_col(x, t)
# slope tiles
for x in range(30, 46):
    t = ground[x]; g[t - 1][x] = 'a' if (x - 30) % 2 == 0 else 'b'
    # 'a' low half then 'b' high half, then next column's solid top rises by one
for x in range(60, 68):
    g[ground[x] - 1 + 0][x] = '\\' if False else g[ground[x] - 1][x]
# 45 down (surface falls to the right): tile at row ground[x]-1? use '\\' at row ground[x]
for x in range(60, 68):
    g[ground[x]][x] = '\\'
for x in range(120, 128):
    g[ground[x]][x] = '/'
for x in range(150, 166):
    t = ground[x]; g[t][x] = 'd' if (x - 150) % 2 == 0 else 'e'
# pit: spikes on floor at row 28
for x in range(80, 92):
    g[28][x] = '^'; g[29][x] = '#'
# one-way logs over pit
for x in list(range(83, 86)) + list(range(88, 91)): g[21][x] = '='
for x in range(100, 106): g[19][x] = '='
for x in range(136, 142): g[11][x] = '='
# side wall to test wall collision
for y in range(20, 24): g[y][112] = '#'; g[y][113] = '#'
# borders
for y in range(H): g[y][0] = '#'; g[y][W - 1] = '#'
# objects
g[23][4] = 'P'
for x in (12, 14, 16, 18, 50, 52, 54, 84, 89, 102, 104, 138, 139, 140, 175, 177): 
    y = min(ground[x], 26) - 2 if x not in (84, 89, 102, 104, 138, 139, 140) else {84:20,89:20,102:18,104:18,138:10,139:10,140:10}[x]
    g[y][x] = 'c'
for x, y in ((40, 14), (72, 19), (108, 18), (145, 12), (180, 19)): g[y][x] = 'h'
g[23][96] = 'k'
g[23][194] = 'G'
open('data/test1.txt', 'w').write(''.join(''.join(r) + '\n' for r in g))
