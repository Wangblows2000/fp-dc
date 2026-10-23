/* Save data: options, 8 file slots and Time Attack records, packed little-endian with a version and a
 * CRC-16 so a damaged or foreign file is ignored. Payload is well under 1 KB; with the VMU header and
 * the 3-frame icon the memory-card file is 5 blocks (2.5 KB) - far below the 50 KB budget.
 * Field choice follows what the original stores (RE_NOTES "Save data"). */
#include <string.h>
#include "../fp.h"

save_t SV;

static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t c = 0xffff;
    while (n--) { c ^= (uint16_t)*p++ << 8; for (int k = 0; k < 8; k++) c = c & 0x8000 ? (uint16_t)(c << 1 ^ 0x1021) : (uint16_t)(c << 1); }
    return c;
}

/* serialised layout (bytes): 'FPS' ver | options | files | records | crc16 */
typedef struct { uint8_t *p; size_t n, cap; int bad; } buf_t;
static void w8(buf_t *b, unsigned v) { if (b->n < b->cap) b->p[b->n] = (uint8_t)v; else b->bad = 1; b->n++; }
static void w16(buf_t *b, unsigned v) { w8(b, v & 255); w8(b, v >> 8 & 255); }
static void w32(buf_t *b, uint32_t v) { w16(b, v & 0xffff); w16(b, v >> 16); }
static unsigned r8(buf_t *b) { if (b->n < b->cap) return b->p[b->n++]; b->bad = 1; return 0; }
static unsigned r16(buf_t *b) { unsigned v = r8(b); return v | r8(b) << 8; }
static uint32_t r32(buf_t *b) { uint32_t v = r16(b); return v | (uint32_t)r16(b) << 16; }

static void io(buf_t *b, int wr, int ver) {
#define F8(x)  do { if (wr) w8(b, (x)); else (x) = r8(b); } while (0)
#define F16(x) do { if (wr) w16(b, (x)); else (x) = r16(b); } while (0)
#define F32(x) do { if (wr) w32(b, (x)); else (x) = r32(b); } while (0)
    F8(SV.difficulty); F8(SV.music_volume); F8(SV.sfx_volume); F8(SV.sensitivity); F8(SV.unlocked);
    for (int i = 0; i < SAVE_FILES; i++) {
        save_file_t *f = &SV.file[i];
        F8(f->used); F8(f->character); F8(f->difficulty); F8(f->mode); F8(f->stage);
        F8(f->lives); F16(f->crystals); F8(f->petals); F32(f->time); F16(f->deaths);
    }
    for (int s = 0; s < SAVE_STAGES; s++)
        for (int c = 0; c < SAVE_CHARS; c++) { F32(SV.rec[s][c].time); F16(SV.rec[s][c].crystals); }
    if (ver >= 2) {                     /* v2 block: appended so v1 data reads unchanged */
        F8(SV.letterbox);
        for (int a = 0; a < SAVE_CARD_ACTS; a++) F16(SV.card_bits[a]);
    }
#undef F8
#undef F16
#undef F32
}

void save_defaults(void) {
    memset(&SV, 0, sizeof SV);
    SV.difficulty = 1; SV.music_volume = 100; SV.sfx_volume = 100; SV.sensitivity = 70; SV.letterbox = 1;
}

#define SAVE_VER 2               /* v1 (session 6) files still load; they are rewritten as v2 on the next save */
int save_store(void) {
    uint8_t data[1024];
    buf_t b = { data, 0, sizeof data - 2, 0 };
    w8(&b, 'F'); w8(&b, 'P'); w8(&b, 'S'); w8(&b, SAVE_VER);
    io(&b, 1, SAVE_VER);
    if (b.bad) return -1;
    uint16_t c = crc16(data, b.n);
    data[b.n] = c & 255; data[b.n + 1] = c >> 8;
    return plat_save_write(data, b.n + 2);
}

int save_load(void) {
    uint8_t data[1024];
    save_defaults();
    int n = plat_save_read(data, sizeof data);
    if (n < 8 || data[0] != 'F' || data[1] != 'P' || data[2] != 'S' || (data[3] != 1 && data[3] != SAVE_VER)) return -1;
    if (crc16(data, n - 2) != (data[n - 2] | data[n - 1] << 8)) return -1;
    buf_t b = { data + 4, 0, (size_t)n - 6, 0 };
    save_t keep = SV;
    io(&b, 0, data[3]);
    if (b.bad) { SV = keep; return -1; }
    return 0;
}

size_t save_payload_size(void) {
    uint8_t data[1024];
    buf_t b = { data, 0, sizeof data, 0 };
    io(&b, 1, SAVE_VER);
    return b.n + 6;
}

int save_card_total(void) {
    int n = 0;
    for (int a = 0; a < SAVE_CARD_ACTS; a++) for (unsigned v = SV.card_bits[a]; v; v &= v - 1) n++;
    return n;
}
