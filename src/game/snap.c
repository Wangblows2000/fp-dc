/* Game-state snapshots (host route search, tools/play): every module registers its mutable statics here;
 * G itself is always included. Valid only within one loaded level (pointers into level data are kept). */
#include <string.h>
#include <stdlib.h>
#include "../fp.h"

#define SNAP_MAX 32
static struct { void *p; size_t n; } reg[SNAP_MAX];
static int nreg;

void snap_register(void *p, size_t n) {
    for (int i = 0; i < nreg; i++) if (reg[i].p == p) return;
    if (nreg < SNAP_MAX) { reg[nreg].p = p; reg[nreg].n = n; nreg++; }
}
size_t snap_size(void) { size_t n = sizeof G; for (int i = 0; i < nreg; i++) n += reg[i].n; return n; }
void snap_save(void *buf) {
    uint8_t *b = buf; memcpy(b, &G, sizeof G); b += sizeof G;
    for (int i = 0; i < nreg; i++) { memcpy(b, reg[i].p, reg[i].n); b += reg[i].n; }
}
void snap_load(const void *buf) {
    const uint8_t *b = buf; memcpy(&G, b, sizeof G); b += sizeof G;
    for (int i = 0; i < nreg; i++) { memcpy(reg[i].p, b, reg[i].n); b += reg[i].n; }
}
