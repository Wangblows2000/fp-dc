/* VMU screen (48x32, 1 bpp): an animated crystal on the title and menus, and the life icon of the
 * character being played (with the lives count) during gameplay. The art comes from the original's
 * images (tools/make_vmu_icons.py -> ui/vmu_lcd.bin, converted for a black-and-white panel); this file
 * only composes the bitmap and hands it to the platform when it changes. */
#include <stdlib.h>
#include <string.h>
#include "../fp.h"

static uint8_t *lcd;               /* 'FVL1' file */
static int nframes, ncrystal;
static uint8_t cur[192], last[192];
static int sent, tick;

/* 3x5 digits for the lives count (own glyphs) */
static const uint8_t DIG[10][5] = {
    {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
    {7,4,7,1,7},{7,4,7,5,7},{7,1,1,2,2},{7,5,7,5,7},{7,5,7,1,7}};

static void pset(int x, int y) { if (x >= 0 && x < 48 && y >= 0 && y < 32) cur[y * 6 + x / 8] |= (uint8_t)(0x80 >> (x & 7)); }

/* digit d scaled 2x at (x, y): 6x10 px */
static void digit(int x, int y, int d) {
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (DIG[d][r] & (4 >> c)) { pset(x + 2 * c, y + 2 * r); pset(x + 2 * c + 1, y + 2 * r); pset(x + 2 * c, y + 2 * r + 1); pset(x + 2 * c + 1, y + 2 * r + 1); }
}

void vmuscr_init(void) {
    size_t n;
    free(lcd); lcd = plat_load_file("ui/vmu_lcd.bin", &n);
    nframes = ncrystal = 0;
    if (lcd && n >= 8 && !memcmp(lcd, "FVL1", 4)) {
        nframes = lcd[4] | lcd[5] << 8; ncrystal = lcd[6] | lcd[7] << 8;
        if ((size_t)(8 + nframes * 192) > n) nframes = ncrystal = 0;
    }
    sent = 0; tick = 0;
}

static void push(void) {
    if (sent && !memcmp(cur, last, sizeof cur)) return;
    if (plat_vmu_lcd(cur) == 0) { memcpy(last, cur, sizeof cur); sent = 1; }
}

/* title / menus / loading: the crystal spin (one frame every 6 ticks, about 10 fps) */
void vmuscr_crystal(void) {
    if (!ncrystal) return;
    int k = (tick++ / 6) % ncrystal;
    memcpy(cur, lcd + 8 + k * 192, 192);
    push();
}

/* gameplay: life icon of character c (0 Lilac, 1 Carol, 2 Carol on the bike, 3 Milla, 4 extra) and lives */
void vmuscr_life(int c, int lives) {
    if (nframes <= ncrystal) return;
    if (c < 0 || c >= nframes - ncrystal) c = 0;
    memcpy(cur, lcd + 8 + (ncrystal + c) * 192, 192);
    if (lives < 0) lives = 0;
    if (lives > 99) lives = 99;
    /* "x" then the count, right half, vertically centred (x 32..47) */
    for (int i = 0; i < 4; i++) { pset(31 + i, 14 + i); pset(34 - i, 14 + i); }
    if (lives >= 10) { digit(36, 11, lives / 10); digit(42, 11, lives % 10); }
    else digit(39, 11, lives);
    push();
}

const uint8_t *vmuscr_current(void) { return cur; }
