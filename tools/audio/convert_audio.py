#!/usr/bin/env python3
"""
The audio conversions of the asset pipeline (ASSET_PIPELINE.md), in Python so the drag-and-drop builder runs them on Windows
too (no shell). tools/audio/convert_bgm.sh, convert_sfx.sh and convert_gallery.sh run this file with the same arguments.

  convert_audio.py bgm     <bgm_dir> <out_dir> [rate=32000]
      The original BGM (Ogg Vorbis) to AICA 4-bit ADPCM streams for the Dreamcast (D-013): <out_dir>/<track>.adp
      (headerless, byte-interleaved stereo for snd_stream_start_adpcm) and <out_dir>/bgm_manifest.csv (track, src_bytes,
      src_rate, duration_s, adp_bytes, rate, loop). Loop: the original loops every track whole-file (SD: ChowdrenAudio has
      no loop points; RESEARCH_LOG); adpcm_loop_seal.py cleans the seam (D-033).
  convert_audio.py sfx     <extracted_audio_dir> <voices_dir> <out_dir> [rate=22050]
      Original sounds to Dreamcast sound-effect WAVs (AICA 4-bit ADPCM, mono, for snd_sfx_load): the IDs the original plays
      (Media::play_id, traced: RUNTIME_MAP "Sound IDs") and the voices; <out_dir>/sfx_manifest.csv. The list is SFX below.
      VOX_PC: the Windows install's Data/voices (Prince Dail's / Brevon's lines; default the staged copy).
  convert_audio.py gallery <extracted_audio_dir> <voices_dirs> <fallback_voices_dir> <out_dir>
      Gallery sounds (session 15; spec_gallery 3.2 / 7), loaded on demand by the gallery (src/game/fe_gallery.inc), loose
      on the disc (data/gal/): the 19 voice clips of the sound cards (ListSongFilenames type 2, RE 1043), mono ADPCM at
      11025 Hz -> <name>.wav, or, when longer than one sample may be (snd_sfx_load: <= 65534 samples, i.e. 5.9 s at
      11025 Hz), streamed like a music card: <name>.adp, stereo ADPCM at 32 kHz, sealed like the BGM (session 18:
      sx_08b02, 33 s). Source: the first of <voices_dirs> (a list joined with os.pathsep: ':' on Linux, ';' on Windows)
      holding eng/<xx>/<name>.ogg, else <fallback_voices_dir> (reported as PLACEHOLDER). Plus the samples played by the
      sound cards that have no file (RV t_gal6: 0 1 2 3 57 188 362) at 11025 Hz and the gallery's UI sound 443 at 22050 Hz.

  convert_audio.py story   <extracted_audio_dir> <voices_dirs> <tables/story> <out_data_dir>
      Adventure scenes (session 25): the voice of every dialogue line the ported scene scripts show -> story/vo/<voice>.wav
      (mono ADPCM 11025 Hz, lower for a clip over 5.9 s), from the first of <voices_dirs> (os.pathsep list) holding a genuine
      eng/<xx>/<voice>.ogg; placeholder stubs (< 4000 bytes) and missing clips are skipped (text-only lines). Plus the
      scripts' sound ids the sfx list lacks (sfx/s<id>.wav). Manifest story/vo_manifest.csv.

Tools: FFMPEG (default ffmpeg on the PATH), WAV2ADPCM (default tools/audio/wav2adpcm, compiled with cc from KallistiOS
utils/wav2adpcm or the identical third_party/wav2adpcm when missing). Durations come from tools/audio/oggdur.py (the same
text as ffprobe's format=duration).
"""
import os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import oggdur

def ffmpeg():
    return os.environ.get('FFMPEG') or 'ffmpeg'

def wav2adpcm():
    w = os.environ.get('WAV2ADPCM')
    if w: return w
    w = os.path.join(HERE, 'wav2adpcm')
    if not os.access(w, os.X_OK):
        kos = os.path.join(os.environ.get('KOS_BASE') or '/opt/toolchains/dc/kos', 'utils', 'wav2adpcm', 'wav2adpcm.c')
        src = kos if os.path.isfile(kos) else os.path.join(HERE, '..', '..', 'third_party', 'wav2adpcm', 'wav2adpcm.c')
        subprocess.run(['cc', '-O2', '-o', w, src], check=True)
    return w

def run(cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)

def to_wav(src, wav, ac, rate, af=None):
    run([ffmpeg(), '-v', 'error', '-y', '-i', src] + (['-af', af] if af else []) +
        ['-ac', str(ac), '-ar', str(rate), '-sample_fmt', 's16', wav])

def stream(src, adp, tmp, rate=32000):
    """stereo ADPCM stream, whole-file loop seam sealed (D-033)"""
    wav = os.path.join(tmp, 'x.wav')
    to_wav(src, wav, 2, rate)
    run([wav2adpcm(), '-n', '-i', '-t', wav, adp])
    run([sys.executable, '-B', os.path.join(HERE, 'adpcm_loop_seal.py'), adp])
    os.remove(wav)

def manifest(path, header):
    f = open(path, 'w', encoding='utf-8', newline='\n'); f.write(header + '\n'); f.flush()
    return f

# ------------------------------------------------------------------------------------------------------------------ bgm
def bgm(src, out, rate='32000'):
    os.makedirs(out, exist_ok=True); tmp = tempfile.mkdtemp()
    m = manifest(os.path.join(out, 'bgm_manifest.csv'), 'track,src_bytes,src_rate,duration_s,adp_bytes,rate,loop')
    tot = dur = 0
    for f in sorted(x for x in os.listdir(src) if x.endswith('.ogg')):
        n = f[:-4]; s = os.path.join(src, f); a = os.path.join(out, n + '.adp')
        stream(s, a, tmp, rate)
        us, sr = oggdur.info(s)
        m.write('%s,%d,%d,%s,%d,%s,whole\n' % (n, os.path.getsize(s), sr, oggdur.duration_text(s), os.path.getsize(a), rate))
        tot += os.path.getsize(a); dur += us / 1e6
    m.close(); shutil.rmtree(tmp)
    print('%d tracks, %.1f MB ADPCM, %.0f s' % (len([x for x in os.listdir(src) if x.endswith('.ogg')]), tot / 1e6, dur))

# ------------------------------------------------------------------------------------------------------------------ sfx
# The sounds, in the order the conversions run (a later entry for the same name converts it again and adds a manifest line,
# as the shell version did). ('id', n) = <audio>/audio_<n>.ogg -> s<n>.wav; ('vox', 'xx/name') = <voices>/eng/xx/name.ogg;
# ('voxpc', 'xx/name') = $VOX_PC/eng/xx/name.ogg; ('lilac', None) = every <voices>/eng/li/lilac_*.ogg. rate None = the
# default (22050); trim: tails below -48 dB cut (None: only the ids in TRIM).
IDS = ('0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 21 23 24 25 26 27 28 29 30 31 32 33 34 35 37 38 39 40 41 42 43 45 46 47 '
       '48 49 50 51 53 56 57 58 61 63 64 74 76 78 80 81 95 98 100 113 114 115 116 117 118 119 120 121 122 126 128 133 139 140 '
       '141 154 159 160 161 162 174 176 220 221 232 288 289 290 307 311 313 314 315 319 320 387 396 405 409 410 411 412 413 '
       '414 170 178 179 180 181 182 183 184 185 186 202 203 204 205 206 207 244 415 416 417 418 419 420 421 422 201 55 62 69 '
       '99 172 173 240 242 251 252 278 306 308 316 54 59 60 67 70 77 101 110 135 213 219 426')
IDS += ' 75 111 260 262'            # Relic Maze acts 3-5 gimmicks (relic_gimb.c): pickaxe, light stone, hand-car, elevator
IDS += ' 68 73 96 99 149 312'       # RM3 Claw sequence (rm_claw.c)
IDS += ' 111 132 134 146 148 149 151 152'   # RM5 Kingdom Stone / Mantalith (rm_mantalith.c)
# Fortune Night stage groups (session 17, D-087): tails below -48 dB are cut (about 30 % of sound RAM); never for looped ids
# (62, 260, 262) or anything outside the FN groups
TRIM = set(('70 75 99 127 241 288 134 137 143 163 164 165 166 167 168 169 171 187 188 191 192 193 194 195 196 197 198 199 200 '
            '208 212 216 217 218 219 223 224 225 226 227 228 229 230 231 243 249 254 256 257 258 259 261 317 371 428 430 436 '
            '103 156 237 255 264 263 265 266 267 268 269 270 271 272 273 274 438 446 71 155 445').split())
# Relic Maze stage set (D-047 stage sounds): converted at half rate so the set fits sound RAM next to Milla's group and the
# music stream (session 14: 380 KB at 22 kHz did not leave the stream's 64 KB buffer; snd_sfx_load also needs <= 65534 samples)
LOW = set('54 55 59 60 62 67 68 70 73 75 77 96 99 101 110 111 135 149 213 219 260 262 306 312 426'.split())
LOW |= set('132 134 146 148 151 152'.split())   # RM5 Mantalith (rm_mantalith.c)
H = 11025   # half rate

def ids(s, rate=H, trim=None): return [('id', i, rate, trim) for i in s.split()]
def vox(s, rate=H, trim=None, kind='vox'): return [(kind, v, rate, trim) for v in s.split()]

SFX = (
    [('id', i, H if i in LOW else None, None) for i in IDS.split()]
    + [('lilac', None, None, None)]
    # Carol / Milla stage-clear voices (RM5 results 6590-6592; game.c voice_play_results loads them only for the results)
    + vox('ca/carol_feelmypower mi/milla_yay mi/milla_superdog')
    # Fortune Night (session 17): the stage group SFX_FN holds the flow ids 61 and 134 (above); FN bosses (fn_boss.c): UFO
    # 168 / 169, bulldozer 165 / 166, Serpentine 191-200 / 317 (62 / 70 / 75 are RM ids)
    + ids('165 166 168 169 191 192 193 194 195 196 197 198 199 200 317')
    # Fortune Night enemies / gimmicks (fn_enemy.c, fn_gima.c, fn_gimb.c): per-act groups SFX_FNA / SFX_FNB in levels.c
    + ids('127 143 163 164 167 171 187 188 208 212 216 217 218 223 224 225 226 227 228 229 230 231 243 249 371 428 430')
    # Fortune Night Panther (fn_panther.c, SFX_FN6): assist strike 137, back cannon 254, laser 256 / 257, voices 258 / 259,
    # defeat 261, plasma 436; session 18 (fn_assist.c): 241 = Milla drops the box
    + ids('137 241 254 256 257 258 259 261 436')
    + vox('group/teamlilac_madeit')
    # Sky Battalion (session 19, per-act groups SFX_SB1..4 in levels.c): SB1 267-274 (Kujacker group), SB2 103 156 237 263
    # (flamethrower) 264 446, SB3 265, SB4 255 266 438; SB2 Spade (sb_spade.c: card throw 71, card burst 445, explosion 155)
    + ids('103 156 237 255 263 264 265 266 267 268 269 270 271 272 273 274 438 446 71 155 445')
    # Sky Battalion boss voices (session 21, level_def_t.vox): Kujacker 9012-9022 / 9163, Spade 9625-9628; Jade Creek 5's
    # Serpentine (serp_gotyounow 11197, serp_kungfu). Real files from the user's PC install; tails trimmed.
    + vox('da/da_04_01 da/da_04_02 da/da_04_03 da/da_04_04 ca/carol_fatchance mi/milla_icantiwont sp/spade_comeon '
          'sp/spade_goodenough sp/spade_comeon2 li/lilac_never li/lilac_hitmeifyoucan se/serp_gotyounow se/serp_kungfu', trim=True)
    # Jade Creek (session 21, SFX_JC1..5): 233, 275 (BullFrag volley), 72 (bubble spring), 292 (act-5 storm bolt), 427, 276 / 277
    # (Hovercop), 296-299 (Neera), 335 (TH bike ninja skid), 336 (Laser Iris), 363 (TH3 crowd)
    + ids('233 275 72 292 427 276 277 296 297 298 299 335 336 363', trim=True)
    # Thermal Base (session 21c, SFX_TB1..6): doors, gimmicks, lava fall, presser, enemies and bosses, the TB6 end; Syntax
    # voice sx_still (TB6; sx_lost is only a silent stub in the Linux data and not in the user's voice files)
    + ids('52 92 279 280 282 283 284 285 286 302 303 304 305 309 310 321 437', trim=True)
    + vox('sx/sx_still', trim=True)
    # Pangu Lagoon (session 23, SFX_PL1..6): sunflora spray, terreon jump, gimmicks, obelisk laser, the Holo-Dragon
    + ids('44 147 150 322 323 324 325 326 327 328 329 330 331 332 333 334', trim=True)
    # the end of the game (session 24, src/game/ending.c END_SFX): 425 rumble and 294 hero-ship engine (looped: tails kept),
    # 435 lift-off jets and 354 hull pieces (tails trimmed); 284 (Torque's rocket) is converted above
    + ids('425 294') + ids('435 354', trim=True)
    # Battle Glacier (session 24, SFX_BG1..6): 344 magnet launcher, 345 / 346 Elite spawner drop / land
    + ids('344 345 346', trim=True)
    # Battle Glacier bosses (session 24): Absolution and the background mech, Prince Dail; voices lilac_awesome (BG2 chopper
    # end), da_05_02 / da_05_03 / dail_no (Prince Dail: not in the Linux copy, the PC install's VOX_PC)
    + ids('295 342 343 406 407 408 349 351 355 356 357', trim=True)
    + vox('li/lilac_awesome', trim=True)
    + vox('da/da_05_02 da/da_05_03 da/dail_no', trim=True, kind='voxpc')
    # Final Dreadnought (session 24, SFX_FD1A..FD4B): gatling burst, gimmicks, bosses; 381 looped (tail kept); br_pa_03 (FD1b)
    + ids('376 358 359 360 368 369 370 372 379 388 341 347 348 373 374 380 389 390 392 393 395 382 383 384 385 386 375 377 378',
          trim=True)
    + ids('381')
    + vox('br/br_pa_03', trim=True, kind='voxpc')
)

TRIM_AF = 'areverse,silenceremove=start_periods=1:start_threshold=-48dB,areverse'

def sfx(src, voices, out, rate='22050'):
    os.makedirs(out, exist_ok=True); tmp = tempfile.mkdtemp()
    vpc = os.environ.get('VOX_PC') or '/mnt/user-data/uploads/Freedom Planet/Data/voices'
    m = manifest(os.path.join(out, 'sfx_manifest.csv'), 'name,source,duration_s,bytes')
    def conv(s, d, r, trim):
        name = os.path.basename(d)[:-4]
        if trim is None: trim = (name[1:] if name.startswith('s') else name) in TRIM
        wav = os.path.join(tmp, 'x.wav')
        to_wav(s, wav, 1, r, TRIM_AF if trim else None)
        run([wav2adpcm(), '-t', wav, d])
        m.write('%s,%s,%s,%d\n' % (name, os.path.basename(s), oggdur.duration_text(s), os.path.getsize(d))); m.flush()
    for kind, what, r, trim in SFX:
        r = r or rate
        if kind == 'id':
            conv(os.path.join(src, 'audio_%s.ogg' % what), os.path.join(out, 's%s.wav' % what), r, trim)
        elif kind == 'lilac':
            li = os.path.join(voices, 'eng', 'li')
            for f in sorted(f for f in os.listdir(li) if f.startswith('lilac_') and f.endswith('.ogg')):
                conv(os.path.join(li, f), os.path.join(out, f[:-4] + '.wav'), r, trim)
        else:
            base = voices if kind == 'vox' else vpc
            conv(os.path.join(base, 'eng', *what.split('/')) + '.ogg', os.path.join(out, what.split('/')[-1] + '.wav'), r, trim)
    m.close(); shutil.rmtree(tmp)
    rows = open(os.path.join(out, 'sfx_manifest.csv'), encoding='utf-8').read().splitlines()[1:]
    print('%d sounds, %.1f KB' % (len(rows), sum(int(l.split(',')[-1]) for l in rows) / 1024))

# -------------------------------------------------------------------------------------------------------------- gallery
GAL_VOICES = ('ca/carol_awesome ca/carol_feelmypower mi/milla_whimper mi/milla_shiny mi/milla_superdog se/serp_madlaugh '
              'li/lilac_wontgiveup li/lilac_nottoday li/lilac_hitmeifyoucan li/lilac_snakeman da/da_04_01 da/da_04_02 '
              'se/serp_gotyounow se/serp_bugeyed se/serp_kungfu se/serp_annihilation sx/sx_intrusion sx/sx_still sx/sx_08b02')

def gallery(src, voices, fallback, out):
    os.makedirs(out, exist_ok=True); tmp = tempfile.mkdtemp()
    m = manifest(os.path.join(out, 'gal_manifest.csv'), 'name,source,duration_s,bytes')
    def conv(s, d, r):
        wav = os.path.join(tmp, 'x.wav')
        to_wav(s, wav, 1, r)
        run([wav2adpcm(), '-t', wav, d])
        m.write('%s,%s,%s,%d\n' % (os.path.basename(d)[:-4], s, oggdur.duration_text(s), os.path.getsize(d))); m.flush()
    def voice(v):   # <xx>/<name>: the first genuine copy, else the fallback (placeholder)
        rel = os.path.join('eng', *v.split('/')) + '.ogg'
        for d in [d for d in voices.split(os.pathsep) if d]:
            if os.path.isfile(os.path.join(d, rel)): return os.path.join(d, rel)
        return os.path.join(fallback, rel)
    for v in GAL_VOICES.split():
        f = voice(v); n = v.split('/')[-1]
        for e in ('.wav', '.adp'):
            if os.path.exists(os.path.join(out, n + e)): os.remove(os.path.join(out, n + e))
        if os.path.abspath(f).startswith(os.path.abspath(fallback) + os.sep): print('PLACEHOLDER %s (%s)' % (n, f))
        smp = int(float(oggdur.duration_text(f)) * 11025 + 0.5)
        if smp <= 65534: conv(f, os.path.join(out, n + '.wav'), 11025)
        else:   # too long for a sample: streamed (music path, fe_gallery.inc gal_voice)
            a = os.path.join(out, n + '.adp'); stream(f, a, tmp)
            m.write('%s,%s,%s,%d\n' % (n, f, oggdur.duration_text(f), os.path.getsize(a))); m.flush()
    for i in (0, 1, 2, 3, 57, 188, 362): conv(os.path.join(src, 'audio_%d.ogg' % i), os.path.join(out, 's%d.wav' % i), 11025)
    conv(os.path.join(src, 'audio_443.ogg'), os.path.join(out, 's443.wav'), 22050)
    m.close(); shutil.rmtree(tmp)
    rows = open(os.path.join(out, 'gal_manifest.csv'), encoding='utf-8').read().splitlines()[1:]
    print('%d gallery sounds, %.1f KB' % (len(rows), sum(int(l.split(',')[-1]) for l in rows) / 1024))

# ------------------------------------------------------------------------------------------------------------------ story
STUB_MAX = 4000       # the Linux copy's placeholder voice clips are 3,564 bytes: never converted (the line stays text-only)

def story_needs(tables):
    """the voices and sound ids the ported scene scripts use: every line they show ('say' -> the DialogData paragraph's
    voice), every voice outside the dialogue ('o') and every sound id ('s')"""
    import json
    dlg = json.load(open(os.path.join(tables, 'dialog.json'), encoding='utf-8'))['dialog']
    voices, ids = set(), set()
    sd = os.path.join(tables, 'scripts')
    for f in sorted(os.listdir(sd)) if os.path.isdir(sd) else []:
        if not f.endswith('.scn'): continue
        h = None
        for l in open(os.path.join(sd, f), encoding='utf-8'):
            t = l.split()
            if not t: continue
            if t[0] == 'dialog' and t[1] != '-': h = t[1]
            elif len(t) >= 3 and t[1] == 'say' and h:
                v = dlg[h][int(t[2])][2]
                if 'xx' not in v: voices.add(v)
            elif len(t) >= 3 and t[1] == 'o': voices.add(t[2])
            elif len(t) >= 3 and t[1] == 's': ids.add(int(t[2]))
    return sorted(voices), sorted(ids)

def story(src, voices, tables, out):
    """Adventure scenes (session 25, src/game/story.c): the voice of every line the ported scripts show, one mono ADPCM clip
    per line at 11025 Hz (story/vo/<voice>.wav, loaded per line), lower when a clip is longer than one sample may be
    (snd_sfx_load: <= 65534 samples); placeholder stubs and missing clips are left out (the line is shown text-only). And
    the scripts' sound ids that the sfx step does not convert (sfx/s<id>.wav, 11025 Hz, tails trimmed for the TRIM ids)."""
    os.makedirs(os.path.join(out, 'story', 'vo'), exist_ok=True); os.makedirs(os.path.join(out, 'sfx'), exist_ok=True)
    tmp = tempfile.mkdtemp()
    m = manifest(os.path.join(out, 'story', 'vo_manifest.csv'), 'name,source,duration_s,rate,bytes')
    need, ids = story_needs(tables)
    roots = [d for d in voices.split(os.pathsep) if d]
    n = stub = miss = tot = 0
    for v in need:
        pre = v.split('_')[0][:2]
        src_v = None; best = -1
        for r in roots:                                  # the first genuine copy (a stub only if nothing better exists)
            p = os.path.join(r, 'eng', pre, v + '.ogg')
            if os.path.isfile(p) and os.path.getsize(p) > best: src_v, best = p, os.path.getsize(p)
            if best >= STUB_MAX: break
        d = os.path.join(out, 'story', 'vo', v + '.wav')
        if os.path.exists(d): os.remove(d)
        if not src_v: miss += 1; continue
        if best < STUB_MAX: stub += 1; continue
        dur = float(oggdur.duration_text(src_v))
        rate = 11025 if dur * 11025 <= 65000 else int(65000 / dur)
        wav = os.path.join(tmp, 'x.wav')
        to_wav(src_v, wav, 1, rate, TRIM_AF)
        run([wav2adpcm(), '-t', wav, d])
        tot += os.path.getsize(d); n += 1
        m.write('%s,%s,%s,%d,%d\n' % (v, os.path.basename(src_v), oggdur.duration_text(src_v), rate, os.path.getsize(d))); m.flush()
    have = set(f for f in os.listdir(os.path.join(out, 'sfx')) if f.endswith('.wav'))
    k = 0; bad = []
    for i in ids:
        if 's%d.wav' % i in have: continue
        wav = os.path.join(tmp, 'x.wav')
        try:                                             # tails trimmed as the sfx step does (TRIM); a clip that cannot be
            to_wav(os.path.join(src, 'audio_%d.ogg' % i), wav, 1, 11025, TRIM_AF if str(i) in TRIM else None)   # converted
            run([wav2adpcm(), '-t', wav, os.path.join(out, 'sfx', 's%d.wav' % i)]); k += 1      # stays silent
        except (subprocess.CalledProcessError, OSError): bad.append(i)
    if bad: print('story sounds not converted (silent): %s' % ' '.join(map(str, bad)))
    m.close(); shutil.rmtree(tmp)
    print('story voices: %d of %d converted (%.1f KB), %d placeholder stubs and %d missing left text-only; %d story sound ids added'
          % (n, len(need), tot / 1024, stub, miss, k))

if __name__ == '__main__':
    cmds = {'bgm': (bgm, 2, 3), 'sfx': (sfx, 3, 4), 'gallery': (gallery, 4, 4), 'story': (story, 4, 4)}
    if len(sys.argv) < 2 or sys.argv[1] not in cmds or not cmds[sys.argv[1]][1] <= len(sys.argv) - 2 <= cmds[sys.argv[1]][2]:
        sys.exit(__doc__)
    cmds[sys.argv[1]][0](*sys.argv[2:])
