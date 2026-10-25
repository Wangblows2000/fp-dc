/* Save data (memory card): options, 8 file slots, the Time Attack records, story progress and the cards, packed
 * little-endian with a version and a CRC-16 so a damaged or foreign file is ignored.
 *
 * v3 (session 13) follows the original's records.dat + file<n>.sav (build/re/spec_records_saves.md, RE; checked against
 * the user's reference saves in place, RV-file): every table is indexed by the stage index s, so all current and future
 * stages have a slot and adding a stage needs no format change. Totals are not stored (the original recomputes them as
 * sums of the per-stage blocks, RE 954). Payload 1286 bytes; with the VMU header and the 3-frame icon the memory-card
 * file is 6 blocks. v1 (session 6) and v2 (session 10) files are read and converted (save_load), then written as v3 on
 * the next save. */
#include <string.h>
#include "../fp.h"

save_t SV;

static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t c = 0xffff;
    while (n--) { c ^= (uint16_t)*p++ << 8; for (int k = 0; k < 8; k++) c = c & 0x8000 ? (uint16_t)(c << 1 ^ 0x1021) : (uint16_t)(c << 1); }
    return c;
}

/* serialised layout: 'FPS' ver | payload | crc16 */
typedef struct { uint8_t *p; size_t n, cap; int bad; } buf_t;
static void w8(buf_t *b, unsigned v) { if (b->n < b->cap) b->p[b->n] = (uint8_t)v; else b->bad = 1; b->n++; }
static void w16(buf_t *b, unsigned v) { w8(b, v & 255); w8(b, v >> 8 & 255); }
static void w32(buf_t *b, uint32_t v) { w16(b, v & 0xffff); w16(b, v >> 16); }
static unsigned r8(buf_t *b) { if (b->n < b->cap) return b->p[b->n++]; b->bad = 1; return 0; }
static unsigned r16(buf_t *b) { unsigned v = r8(b); return v | r8(b) << 8; }
static uint32_t r32(buf_t *b) { uint32_t v = r16(b); return v | (uint32_t)r16(b) << 16; }

static void io3(buf_t *b, int wr) {
#define F8(x)  do { if (wr) w8(b, (x)); else (x) = r8(b); } while (0)
#define F16(x) do { if (wr) w16(b, (x)); else (x) = r16(b); } while (0)
#define F32(x) do { if (wr) w32(b, (x)); else (x) = r32(b); } while (0)
    F8(SV.difficulty); F8(SV.music_volume); F8(SV.sfx_volume); F8(SV.sensitivity); F8(SV.letterbox);
    for (int i = 0; i < SAVE_FILES; i++) {
        save_file_t *f = &SV.file[i];
        F8(f->used); F8(f->character); F8(f->mode); F8(f->last_stage); F8(f->lives); F8(f->shield); F16(f->counter);
        for (int s = 0; s < SAVE_STAGES; s++) { F32(f->stage[s].time); F16(f->stage[s].crystals); F8(f->stage[s].deaths); }
    }
    for (int s = 0; s < SAVE_STAGES; s++)
        for (int c = 0; c < SAVE_CHARS; c++) { F32(SV.rec[s][c].time); F16(SV.rec[s][c].crystals); }
    for (int c = 0; c < SAVE_CHARS; c++) F8(SV.progress[c]);
    for (unsigned k = 0; k < sizeof SV.cards; k++) F8(SV.cards[k]);
#undef F8
#undef F16
#undef F32
}

/* v1 / v2 (sessions 6-12): options, 8 files {used, character, difficulty, mode, stage, lives, crystals, petals, time,
 * deaths}, records rec[16][4] in frames, then (v2) letterbox and card_bits[4] (DV card ids 1..10 in word 0) */
static void read_v12(buf_t *b, int ver) {
    SV.difficulty = r8(b); SV.music_volume = r8(b); SV.sfx_volume = r8(b); SV.sensitivity = r8(b); (void)r8(b);   /* "unlocked": no original field */
    for (int i = 0; i < SAVE_FILES; i++) {
        save_file_t *f = &SV.file[i];
        f->used = r8(b); f->character = r8(b); (void)r8(b); f->mode = r8(b); (void)r8(b);
        f->lives = r8(b); unsigned crystals = r16(b); (void)r8(b); uint32_t time = r32(b); unsigned deaths = r16(b);
        f->last_stage = 0xff;
        if (f->used) {   /* a used v1/v2 file was written by the only results the port had: its route's first stage */
            int s = chr_is_milla(f->character) ? 0 : 1;
            f->last_stage = (uint8_t)s;
            f->stage[s].time = time; f->stage[s].crystals = (uint16_t)crystals; f->stage[s].deaths = (uint8_t)(deaths > 255 ? 255 : deaths);
            int c = chr_slot(f->character); if (SV.progress[c] < s + 1) SV.progress[c] = (uint8_t)(s + 1);
        }
    }
    for (int s = 0; s < 16; s++)
        for (int c = 0; c < SAVE_CHARS; c++) {
            uint32_t t = r32(b); unsigned k = r16(b);
            if (s >= SAVE_STAGES) continue;
            SV.rec[s][c].time = (uint32_t)((uint64_t)t * 1000 / 60);   /* frames -> ms */
            SV.rec[s][c].crystals = (uint16_t)k;
        }
    if (ver >= 2) {
        SV.letterbox = r8(b);
        unsigned w0 = r16(b); for (int a = 1; a < 4; a++) (void)r16(b);
        for (int id = 1; id <= 10; id++) if (w0 >> (id - 1) & 1) save_card_set(id);
    }
}

void save_defaults(void) {
    memset(&SV, 0, sizeof SV);
    SV.difficulty = 1; SV.music_volume = 100; SV.sfx_volume = 100; SV.sensitivity = 70; SV.letterbox = 1;
    for (int i = 0; i < SAVE_FILES; i++) SV.file[i].last_stage = 0xff;
}

#define SAVE_VER 3
#define SAVE_BUF 1536            /* 4 + payload (1286) + 2, with room to spare */
int save_store(void) {
    uint8_t data[SAVE_BUF];
    buf_t b = { data, 0, sizeof data - 2, 0 };
    w8(&b, 'F'); w8(&b, 'P'); w8(&b, 'S'); w8(&b, SAVE_VER);
    io3(&b, 1);
    if (b.bad) return -1;
    uint16_t c = crc16(data, b.n);
    data[b.n] = c & 255; data[b.n + 1] = c >> 8;
    return plat_save_write(data, b.n + 2);
}

int save_load(void) {
    uint8_t data[SAVE_BUF];
    save_defaults();
    int n = plat_save_read(data, sizeof data);
    if (n < 8 || data[0] != 'F' || data[1] != 'P' || data[2] != 'S' || data[3] < 1 || data[3] > SAVE_VER) return -1;
    if (crc16(data, n - 2) != (data[n - 2] | data[n - 1] << 8)) return -1;
    buf_t b = { data + 4, 0, (size_t)n - 6, 0 };
    if (data[3] == SAVE_VER) io3(&b, 0); else read_v12(&b, data[3]);
    if (b.bad) { save_defaults(); return -1; }
    return 0;
}

size_t save_payload_size(void) {
    uint8_t data[SAVE_BUF];
    buf_t b = { data, 0, sizeof data, 0 };
    io3(&b, 1);
    return b.n + 6;
}

/* cards (records x20): id n in 1..100 */
int  save_card_has(int id) { return id >= 1 && id <= SAVE_CARDS && (SV.cards[(id - 1) >> 3] >> ((id - 1) & 7) & 1); }
void save_card_set(int id) { if (id >= 1 && id <= SAVE_CARDS) SV.cards[(id - 1) >> 3] |= (uint8_t)(1 << ((id - 1) & 7)); }
/* title-screen cheat (session 15): everything the save tracks as unlocked. Progress = every stage for every character
 * (Time Attack locks, RE 641-643) and all 100 cards (the gallery's unlock source); records, files and options untouched. */
void save_unlock_all(void) {
    for (int c = 0; c < SAVE_CHARS; c++) SV.progress[c] = SAVE_STAGES;
    for (int id = 1; id <= SAVE_CARDS; id++) save_card_set(id);
}
int save_card_total(void) { int n = 0; for (int id = 1; id <= SAVE_CARDS; id++) n += save_card_has(id); return n; }

/* file totals = sums of the stage blocks (RE 954) */
uint32_t save_file_time(const save_file_t *f) { uint32_t t = 0; for (int s = 0; s < SAVE_STAGES; s++) t += f->stage[s].time; return t; }
int save_file_crystals(const save_file_t *f) { int n = 0; for (int s = 0; s < SAVE_STAGES; s++) n += f->stage[s].crystals; return n; }
int save_file_deaths(const save_file_t *f) { int n = 0; for (int s = 0; s < SAVE_STAGES; s++) n += f->stage[s].deaths; return n; }

/* The stage end (the port has no routing frame 8, so its writes happen here, once, at the results' D = 150):
 *  - records (RE 907 / 908, any mode, Time Attack and story alike): a time over 10 s replaces a slower or missing one;
 *    the crystal count is the best ever, independently of the time;
 *  - story only (RE 954 / 955: Classic / Adventure with a file): the stage block, the last stage, the progress and the
 *    values carried into the next stage. Time Attack never touches a file. */
void save_stage_end(int s, int character, uint32_t time_ms, int crystals, int deaths, int story, int file_slot) {
    if (s < 0 || s >= SAVE_STAGES) return;
    int c = chr_slot(character);
    save_rec_t *r = &SV.rec[s][c];
    if (time_ms > 10000 && (r->time == 0 || time_ms < r->time)) r->time = time_ms;
    if (crystals > r->crystals) r->crystals = (uint16_t)(crystals > 65535 ? 65535 : crystals);
    if (!story) return;
    if (SV.progress[c] < s + 1) SV.progress[c] = (uint8_t)(s + 1);
    if (file_slot < 0 || file_slot >= SAVE_FILES) return;
    save_file_t *f = &SV.file[file_slot];
    f->stage[s].time = time_ms; f->stage[s].crystals = (uint16_t)(crystals > 65535 ? 65535 : crystals); f->stage[s].deaths = (uint8_t)(deaths > 255 ? 255 : deaths);
    f->last_stage = (uint8_t)s; f->used = 1; f->character = (uint8_t)character;
}
