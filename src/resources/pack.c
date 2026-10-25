/* Disc packs: every data directory (dv1/, ui/, sfx/ ...) is one file "<dir>.pak" on the disc
 * (tools/pack_disc.py), so a level load is one sequential read instead of ~22 file opens and seeks,
 * and the entries are LZ4-compressed (DV1: 6.2 MB -> about 0.7 MB to read).
 *
 * Format (little endian): "FPK1" u32 count, count x { char name[36]; u32 crc32(raw); u32 offset; u32 stored; u32 raw; }
 * then the entries, each at a 32-byte aligned offset. stored == raw: kept uncompressed; otherwise one
 * LZ4 block (no frame header) that decodes to exactly raw bytes.
 *
 * Only one pack is held in RAM at a time (the whole compressed file); it is released at the end of
 * each load (pack_release), so the steady-state memory cost is zero. Files that are read outside loads
 * (ui/vmu_*, ui/loading.*, levels.txt) and the gallery's on-demand files (gal/) stay loose on the disc. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../fp.h"

typedef struct { char name[36]; uint32_t crc, off, stored, raw; } pak_ent_t;

static char cur_dir[24];
static uint8_t *cur_buf;
static size_t cur_size;
static uint32_t cur_n;
static int cur_missing;          /* the directory has no pack: use loose files without retrying */

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

/* LZ4 block decoder (format: lz4/doc/lz4_Block_format.md). Returns 0 when exactly dst_len bytes were
 * produced from exactly src_len input bytes, -1 on any malformed or out-of-bounds sequence. */
int lz4_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len) {
    const uint8_t *ip = src, *iend = src + src_len;
    uint8_t *op = dst, *oend = dst + dst_len;
    while (ip < iend) {
        unsigned tok = *ip++;
        size_t lit = tok >> 4;
        if (lit == 15) { unsigned b; do { if (ip >= iend) return -1; b = *ip++; lit += b; } while (b == 255); }
        if (lit > (size_t)(iend - ip) || lit > (size_t)(oend - op)) return -1;
        memcpy(op, ip, lit); op += lit; ip += lit;
        if (ip == iend) break;                                   /* last sequence: literals only */
        if (iend - ip < 2) return -1;
        size_t off = ip[0] | (ip[1] << 8); ip += 2;
        if (off == 0 || off > (size_t)(op - dst)) return -1;
        size_t ml = (tok & 15) + 4;
        if ((tok & 15) == 15) { unsigned b; do { if (ip >= iend) return -1; b = *ip++; ml += b; } while (b == 255); }
        if (ml > (size_t)(oend - op)) return -1;
        const uint8_t *m = op - off;
        if (off >= ml) { memcpy(op, m, ml); op += ml; }
        else while (ml--) *op++ = *m++;                          /* overlapping copy (run) */
    }
    return op == oend ? 0 : -1;
}

void pack_release(void) {
    free(cur_buf); cur_buf = NULL; cur_size = 0; cur_n = 0; cur_dir[0] = 0; cur_missing = 0;
}

static int pack_select(const char *dir) {
    if (cur_dir[0] && !strcmp(cur_dir, dir)) return cur_missing ? -1 : 0;
    pack_release();
    snprintf(cur_dir, sizeof cur_dir, "%s", dir);
    char fn[48]; snprintf(fn, sizeof fn, "%s.pak", dir);
    size_t sz = 0;
    uint8_t *b = plat_read_raw(fn, &sz);
    if (!b) { cur_missing = 1; return -1; }
    uint32_t n = sz >= 8 ? rd32(b + 4) : 0;
    if (sz < 8 || memcmp(b, "FPK1", 4) || n > 4096 || 8 + (size_t)n * sizeof(pak_ent_t) > sz) {
        plat_log("pack %s: invalid\n", fn); free(b); cur_missing = 1; return -1;
    }
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *e = b + 8 + i * sizeof(pak_ent_t);
        uint32_t off = rd32(e + 40), st = rd32(e + 44);
        if (off > sz || st > sz - off || memchr(e, 0, 36) == NULL) { plat_log("pack %s: bad entry %u\n", fn, i); free(b); cur_missing = 1; return -1; }
    }
    cur_buf = b; cur_size = sz; cur_n = n;
    return 0;
}

static const uint8_t *pack_find(const char *name) {
    const char *sl = strchr(name, '/');
    if (!sl || (size_t)(sl - name) >= sizeof cur_dir || strchr(sl + 1, '/')) return NULL;
    /* files kept loose by tools/pack_disc.py (LOOSE): never read a pack just to miss them */
    if (!strncmp(sl + 1, "loading.", 8) || !strncmp(sl + 1, "vmu_", 4)) return NULL;
    if (!strncmp(name, "gal/", 4)) return NULL;          /* gallery art / sounds: loose, read one at a time on demand */
    char dir[24]; memcpy(dir, name, sl - name); dir[sl - name] = 0;
    if (pack_select(dir) < 0) return NULL;
    for (uint32_t i = 0; i < cur_n; i++) {
        const uint8_t *e = cur_buf + 8 + i * sizeof(pak_ent_t);
        if (!strcmp((const char *)e, sl + 1)) return e;
    }
    return NULL;
}

/* CRC-32 of an entry's uncompressed content (lets a level load keep identical resident textures) */
int pack_entry_crc(const char *name, uint32_t *crc) {
    const uint8_t *e = pack_find(name);
    if (!e) return -1;
    *crc = rd32(e + 36);
    return 0;
}

/* name = "<dir>/<file>". Returns a malloc'd (32-byte aligned on the DC) copy, or NULL with *found = 0
 * when the directory has no pack or the pack has no such entry (the caller then tries a loose file). */
void *pack_load(const char *name, size_t *size, int *found) {
    *found = 0;
    const uint8_t *e = pack_find(name);
    if (e) {
        uint32_t off = rd32(e + 40), st = rd32(e + 44), raw = rd32(e + 48);
        *found = 1;
        if (raw == 0 || raw > 8u * 1024 * 1024) return NULL;
        uint8_t *out = plat_alloc_aligned(raw);
        if (!out) { plat_log("pack: out of memory (%s, %u)\n", name, (unsigned)raw); return NULL; }
        uint64_t t = plat_time_us();
        if (st == raw) memcpy(out, cur_buf + off, raw);
        else if (lz4_decode(cur_buf + off, st, out, raw) < 0) { plat_log("pack: corrupt entry %s\n", name); free(out); return NULL; }
        LP.decode_us += plat_time_us() - t; LP.npacked++; LP.alloc_bytes += raw; LP.nalloc++;
        *size = raw;
        return out;
    }
    return NULL;
}
