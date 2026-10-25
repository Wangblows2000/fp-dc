/* VMU screen (48x32, 1 bpp): an animated crystal on the title and menus, and the character's rotating file-select
 * portraits in a stage and on the file select (session 16). The art comes from the original's
 * images (tools/make_vmu_icons.py -> ui/vmu_lcd.bin, converted for a black-and-white panel); this file
 * only composes the bitmap and hands it to the platform when it changes. */
#include <stdlib.h>
#include <string.h>
#include "../fp.h"

static uint8_t *lcd;               /* 'FVL1' file */
static int nframes, ncrystal;
static uint8_t cur[192], last[192];
static int sent, tick;

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

/* gameplay and the file select: the character's file-select portraits (FileSaveSlotCharacter_109, 4 per character),
 * a new one every 6 s. The tick counts game updates (fixed 60 Hz logic frames), so the rotation does not depend on the
 * render rate; it keeps running through pauses and menus. Lives are not shown (session 16). character: G1 value. */
#define VMU_PER_CHAR 4
void vmuscr_char(int character) {
    int c = character == 2 || character == 3 ? 1 : character == 4 ? 2 : 0;   /* Lilac, Carol (on foot / bike), Milla */
    if (nframes < ncrystal + 3 * VMU_PER_CHAR) return;
    int k = (int)((tick++ / (6 * FPS)) % VMU_PER_CHAR);
    memcpy(cur, lcd + 8 + (ncrystal + c * VMU_PER_CHAR + k) * 192, 192);
    push();
}

const uint8_t *vmuscr_current(void) { return cur; }
