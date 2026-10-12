// SDAT archive: load, parse, seat the root, decode SWAVs.
//
// The root object is the same shape func_02050f34 builds -- +0x7c FAT,
// +0x80 FILE, +0x84 INFO -- because six matched accessors in slice_gate10
// read exactly those three fields:
//
//   Sound::InfoSequenceEntry::GetWithID       root+0x84, rec +0x08
//   Sound::InfoInstrumentBankEntry::GetWithID root+0x84, rec +0x10
//   func_02050c14 (SEQARC)                    root+0x84, rec +0x0c
//   func_02050b4c (WAVEARC)                   root+0x84, rec +0x14
//   func_02050a84 (GROUP)                     root+0x84, rec +0x1c
//   func_020509b0 / func_02050a5c (FAT)       root+0x7c
//
// WHY NOT func_02050f34 ITSELF: it is not in any slice, and its own
// companion func_02050d54 allocates the three blocks off the game heap and
// pulls them in with FS_ReadFile through the card seam. That path is
// reproducible, but it buys nothing here -- the whole 4.4MB archive is
// resident either way, and going through it would put 4.4MB of sound data on
// the game's heap for no benefit. The root this file builds is
// field-for-field what func_02050f34 leaves behind; the difference is where
// the bytes live, and the accessors above cannot tell.
//
// RESIDENCY PRE-SEAT: func_020509b0(fileId) returns the runtime "loaded
// address" slot in the FAT record (FAT+0xc+id*16+8) and is what every
// residency check in func_02051bd0 / func_02051a98 / func_020514b4 tests.
// Filling all 282 of them with the resident address means the game correctly
// concludes nothing needs loading from the card -- which is true, the archive
// IS in memory. Load commands 0x1b/0x1c/0x1d therefore never fire, and the
// START command's data pointers land inside this image.
#include "sdat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <unordered_map>
#include <vector>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef PORT_REPO_ROOT
#define PORT_REPO_ROOT "."
#endif

SdatArchive g_sdat;

extern "C" void *data_020a5bb8;

// ---- little-endian readers ---------------------------------------------
static inline sd_u16 rd16(const sd_u8 *p) { return (sd_u16)(p[0] | (p[1] << 8)); }
static inline sd_u32 rd32(const sd_u8 *p)
{ return (sd_u32)p[0] | ((sd_u32)p[1] << 8) | ((sd_u32)p[2] << 16) | ((sd_u32)p[3] << 24); }

#ifdef PORT_ROM_CLEAN
/* A shipping build has no PORT_REPO_ROOT fallback. hal/asset_root_refuse.cpp
   is the write-up. */
extern "C" void port_asset_root_refuse(const char *wanted);
#endif

static const char *asset_root(void)
{
    const char *env = getenv("SM64DS_ASSET_ROOT");
#ifdef PORT_ROM_CLEAN
    if (!env)
        port_asset_root_refuse("extracted/dsd/files/data/sound_data.sdat");
#endif
    return env ? env : PORT_REPO_ROOT;
}

// ---- FAT ----------------------------------------------------------------

static sd_u8 *fat_entry(sd_u32 id)
{
    if (!g_sdat.fat || id >= g_sdat.fatCount) return 0;
    return g_sdat.fat + 0xc + id * 16;
}

sd_u8 *sdat_file(sd_u32 id)
{
    sd_u8 *e = fat_entry(id);
    if (!e) return 0;
    sd_u32 off = rd32(e);
    if (off >= g_sdat.size) return 0;
    return g_sdat.base + off;
}

sd_u32 sdat_file_size(sd_u32 id)
{
    sd_u8 *e = fat_entry(id);
    return e ? rd32(e + 4) : 0;
}

sd_u32 sdat_file_id_of(const void *p)
{
    if (!g_sdat.base || !p) return (sd_u32)-1;
    const sd_u8 *q = (const sd_u8 *)p;
    if (q < g_sdat.base || q >= g_sdat.base + g_sdat.size) return (sd_u32)-1;
    sd_u32 off = (sd_u32)(q - g_sdat.base);
    for (sd_u32 i = 0; i < g_sdat.fatCount; i++) {
        sd_u8 *e = g_sdat.fat + 0xc + i * 16;
        sd_u32 o = rd32(e), s = rd32(e + 4);
        if (off >= o && off < o + s) return i;
    }
    return (sd_u32)-1;
}

// The sequence's base CHANNEL PRIORITY (the SDAT's cpr byte), which is the
// priority every track of the player starts at on the ARM7. The SSEQ 0xC6
// opcode overrides it per track; a stream with no 0xC6 keeps it.
//
// This existed nowhere in the port before, and its absence inverted the ROM's
// voice-steal order in any busy scene. The opening cutscene is the worst case:
// its SFX carry cpr 96..127 (NCS_SE_VT_PEACH_LETTER and NCS_SE_VO_SLEEP1_Y are
// 127) and set no 0xC6, while NCS_BGM_OPENING carries cpr 64 and its lead
// track sets 0xC6 66. On hardware the SFX therefore dominate the music and the
// mix ducks cleanly. The port defaulted every track to 64, so the 127-priority
// voices ran at 64 -- level with the BGM -- and the 16-channel mixer stole
// between them at random, dropping the letter narration and the music in turn.
// That is the opening's "sounds overlap and cut out".
//
// seqBase is the file's DATA base (what the START command carries in b); the
// SSAR case reads the entry table beside it, the SSEQ case the INFO record.
//
// WHY ONLY cpr, AND NOT ppr OR THE PLAYER RECORD'S maxSeq. Those two are real
// and they are enforced -- but they are ARM9-side, and on the ARM9 side this
// project runs the ROM's own decompiled code, so implementing them here would
// be writing a second copy of something already running.
//
// The arbitration lives in func_0204f63c, which IS decompiled, in
// src/func_0204f63c.cpp, and is called from the sequence-start core in
// src/func_02051a98.c. Its body is the ROM's:
//
//     if (seqCount(+8) >= seqMax(+0x18)) {      // the PLAYER record's cap
//         it = lowest-priority sequence on this player;
//         if (!it) return 0;                    // nothing to evict
//         if (r7 < it[0x3d]) return 0;          // new ppr < victim ppr: REFUSE
//         stop(it);
//     }
//     if (!func_0204f364(r7)) return 0;         // global 16-slot pool, also
//                                               // gated on ppr
//
// maxSeq reaches that +0x18 field through Sound::Player::SetPlayableSeqCount,
// which hal/sdat/sound_abi.cpp deliberately hosts as an ALIAS onto
// data_020a4d6c + id*0x1c + 0x18 precisely so the write lands in the block
// func_0204f63c reads it back out of. That aliasing exists because the two DS
// views (data_020a4d6c and data_020a4d84) are one array on hardware.
//
// cpr is different, and that is why it needed seating here. It is not consumed
// on the ARM9 at all: func_02051a98 reads info->cpr at +7 and immediately ships
// it across as SND_SetSeqPriority -> SNDi_SetPlayerParam(offset 4, size 1) ->
// ARM7 command 3, which stores it to player->priority. The ARM7 is the side
// this port reimplements, so cpr is the one of the three that had nowhere to
// land until now.
//
// So the split is: ppr and maxSeq decide whether a SEQUENCE gets a player slot
// (ARM9, already faithful, not ours); cpr feeds the per-NOTE channel priority
// (ARM7, ours). They do not otherwise interact -- the ARM7 image contains no
// SDAT parsing at all, and the allocator's only caller is the note-on path.
//
// MEASURED with port/tools/opening_audio_measure.py, which IS the recipe --
// the numbers below cannot be quoted without it, because an earlier version of
// this measurement was written down as a list of environment variables and
// turned out to depend on the worktree's leftover save file. The opening only
// plays down the NEW FILE path, so a tree that has had the battery run against
// it boots to the title and stays there. The script forces the save absent and
// refuses to print anything unless the log says a cutscene script is running.
//
// Counts are note-ons: one [pd] on line per note that got a mixer channel, one
// [pd] off line per note that got nothing and is simply not heard.
// SM64DS_NO_AUDIO=1 clocks the mixer at exactly 1/60 s per video frame, so the
// run repeats exactly. Both builds are handed the SAME 1449 note-ons.
//
//                                       sounded / dropped
//   whole 4000-frame run   (1449 notes)
//     cons baseline                        1038 / 411   (28.4%)
//     this branch                          1140 / 309   (21.3%)
//   title and file-select only, 700 frames  (272 notes)
//     cons baseline                         264 / 8     (2.9%)
//     this branch                           234 / 38    (14.0%)
//   the cutscene itself, by subtraction    (1177 notes)
//     cons baseline                         774 / 403   (34.2%)
//     this branch                           906 / 271   (23.0%)
//
// So the opening cutscene gains 132 notes it used to lose, and the title and
// file-select LOSES 30 it used to keep. THE TITLE REGRESSION IS REAL AND IS
// NOT WAVED AWAY AS "hardware-correct": it is what the ROM's own arithmetic
// produces, because the title's music sits at cpr 64 while the effects over it
// sit at cpr 96, and 96+64 outranks 64+64. Whether that trade is worth
// shipping is a judgement for a listener, not for this comment.
//
// No track goes silent in any of these runs -- every track that loses notes
// also keeps some, so a losing part THINS rather than disappearing. It does
// thin badly: the opening's busiest music track drops 30 note-ons in a row at
// its worst, which is a real hole, not a texture change.
//
// THE ROM SAYS THIS DIRECTLY, INDEPENDENT OF THE PORT. Reading the cpr byte
// out of extracted/dsd/files/data/sound_data.sdat:
//
//   INFO SEQ records (83 of them, the music):
//     cpr 64:37  65:3  72:6  73:4  106:32  127:1
//   SSAR entries across the five sound-effect archives (1464 of them):
//     cpr 96:1143  127:269  and 52 others spread over 0,40,50,64,80,90,92,
//     98,100,110,115,120
//
// So 1412 of the game's 1464 sound effects are authored at 96 or 127, and
// exactly FOUR are at 64. The port ran all 1464 of them at 64, level with the
// music. That is the whole bug: with every voice at one priority the 16-channel
// allocator's "steal the lowest, oldest among equals" degenerates into "steal
// whatever started first", so in a busy scene the music and the effects evict
// each other by arrival order instead of by the order the ROM asked for.
int sdat_seq_priority(const sd_u8 *seqBase, sd_u32 startOff)
{
    if (!seqBase || !g_sdat.base || !g_sdat.info) return 64;
    sd_u32 fid = sdat_file_id_of(seqBase);
    if (fid == (sd_u32)-1) return 64;
    sd_u8 *f = sdat_file(fid);
    if (!f) return 64;

    if (!memcmp(f, "SSAR", 4)) {
        // entry table: count at +0x1c, entries at +0x20, 12 bytes each:
        // { seqOff u32, bank u16, vol u8, cpr u8, ppr u8, ply u8 }. seqOff is
        // relative to the DATA base, which is exactly startOff's frame.
        sd_u32 count = rd32(f + 0x1c);
        for (sd_u32 i = 0; i < count; i++) {
            sd_u8 *e = f + 0x20 + i * 12;
            if (rd32(e) == startOff) return e[7];
        }
        return 64;
    }
    if (!memcmp(f, "SSEQ", 4)) {
        // A standalone sequence carries its cpr in the INFO SEQ record whose
        // fileId names this file: { fileId u16, unk u16, bank u16, vol u8,
        // cpr u8, ... }.
        sd_u32 recOff = rd32(g_sdat.info + 8 + SDAT_REC_SEQ * 4);
        if (!recOff) return 64;
        sd_u8 *tbl = g_sdat.info + recOff;
        sd_u32 n = rd32(tbl);
        for (sd_u32 i = 0; i < n; i++) {
            sd_u32 v = rd32(tbl + 4 + i * 4);
            if (!v) continue;
            sd_u8 *e = g_sdat.info + v;
            if (rd16(e) == fid) return e[7];
        }
    }
    return 64;
}

// ---- INFO ---------------------------------------------------------------

sd_u8 *sdat_info_entry(int rec, sd_u32 id)
{
    if (!g_sdat.info || rec < 0 || rec >= SDAT_REC_COUNT) return 0;
    sd_u32 recOff = rd32(g_sdat.info + 8 + rec * 4);
    if (!recOff) return 0;
    sd_u8 *tbl = g_sdat.info + recOff;
    if (id >= rd32(tbl)) return 0;
    sd_u32 v = rd32(tbl + 4 + id * 4);
    return v ? g_sdat.info + v : 0;
}

// ---- SWAV decode --------------------------------------------------------

static const int kAdpcmIndexTab[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static const sd_s16 kAdpcmStepTab[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,
    80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,
    494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,
    2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,
    8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,
    27086,29794,32767
};

struct WaveCache {
    std::vector<sd_s16> pcm;
    SdatWave w;
};
static std::unordered_map<const void *, WaveCache> g_waves;

/* Drop every decoded wave. The cache is keyed by SWAV record ADDRESSES in the
   game arena, and an address only names the same sample while the arena around
   it stands still. A save-state restore that crossed an area rolls the arena
   back to different sample data at the same addresses, so the keys would hand
   out the other area's audio. lk6_savestate_load calls this beside the other
   sd_*_reset calls; the cache refills on demand at the next note-on. */
void sd_waves_reset(void)
{
    g_waves.clear();
}

/* Put THIS process's SDAT root back into data_020a5bb8. The global is a hosted
   DS symbol, so a save state captures and restores it like any other -- but
   what it holds is a HOST BOOT POINTER (sdat_init's calloc'd root object), not
   game state: it is written once at boot and never changes during play. An
   in-process restore rewrites it with the same value, harmless. A DISK state
   from an earlier run rewrites it with the OTHER process's heap address, and
   the first sound lookup (func_02050c14 walking root+0x84) dereferences a
   pointer into a heap that no longer exists. lk6_savestate_load calls this
   after the section copy so the live root always wins. */
/* RUNG R4's two, defined below and used by sd_sdat_reseat above them. */
int g_rom_root_seated;
extern "C" void sdat_seat_rom_residency(void);

void sd_sdat_reseat(void)
{
    /* RUNG R4. The root the ROM's own func_02050f34 seated is data_0209b4b4,
       a hosted DS global inside the .dsstate bracket -- so on that path this
       global stops being a host boot pointer at all and a disk state carries
       it correctly by itself. What a disk state still cannot carry is what
       the RESIDENCY slots hold: the ROM's FAT lives in the arena (captured,
       at a pinned base) but every address in it points into THIS process's
       malloc'd archive image. So re-seat those, which is the same argument
       this function was written for, one level down. */
    if (g_rom_root_seated) {
        sdat_seat_rom_residency();
        return;
    }
    if (g_sdat.root)
        data_020a5bb8 = g_sdat.root;   /* the extern "C" decl at the top */
}

/* RUNG R4: fill the residency slots of whatever FAT data_020a5bb8 names.

   WHAT THIS IS AND WHAT IT IS NOT. It is the same pre-seat sdat_init used to
   do, and the header's argument for it is unchanged and still true: the whole
   4.4 MB archive IS in memory, so 'nothing needs loading from the card' is a
   correct answer and the load commands 0x1b/0x1c/0x1d correctly never fire.
   What changed under rung R4 is WHOSE table gets the answer. The ROM read its
   own FAT off the card into the sound heap; this fills that one, so the
   ROM's own func_020509b0 -- which every residency test in func_02051bd0,
   func_02051a98 and func_020514b4 goes through -- reads the addresses the
   START command then carries, exactly as before.

   IT IS NOT THE ROM'S LOADER RUNNING. func_02051634 and the group load
   func_020134d8 are linked and would do this properly, out of the sound heap,
   and they stay unreached for two reasons that are not this rung's: the
   on-demand path needs a PLAYER sub-heap, which exists only when rung R3's
   argument is non-zero, and that argument is zero because hal/star_flow.cpp's
   hosted func_0203d974 answers 1. The lane's report names both.

   The FAT the ROM read has the same 16-byte records as this process's copy
   (offset, size, then two reserved words), so entry i's runtime address slot
   is at FAT + 0xc + i*16 + 8 either way. The count is read back out of the
   ROM's own header word rather than assumed from ours, and the two are
   compared: a disagreement means the ROM opened a DIFFERENT archive from the
   one this file parsed, and that is worth saying out loud rather than
   papering over. */
extern "C" void sdat_seat_rom_residency(void)
{
    sd_u8 *root = (sd_u8 *)data_020a5bb8;
    if (!root || !g_sdat.base) return;
    sd_u8 *fat = *(sd_u8 **)(root + 0x7c);
    if (!fat) {
        fprintf(stderr, "[sdat] the ROM's root has no FAT block -- the "
                        "archive open failed; sound stays silent\n");
        return;
    }
    sd_u32 n = rd32(fat + 8);
    if (n != g_sdat.fatCount)
        fprintf(stderr, "[sdat] WARNING: the ROM read a FAT of %u entries "
                        "and this process parsed %u -- different archives\n",
                n, g_sdat.fatCount);
    if (n > g_sdat.fatCount) n = g_sdat.fatCount;
    for (sd_u32 i = 0; i < n; i++) {
        sd_u8 *e = fat + 0xc + i * 16;
        sd_u32 off = rd32(g_sdat.fat + 0xc + i * 16);
        *(sd_u8 **)(e + 8) =
            (off < (sd_u32)g_sdat.size) ? (g_sdat.base + off) : 0;
    }
    g_rom_root_seated = 1;
    fprintf(stderr, "[sdat] the ROM opened its own archive: root %p, FAT %p "
                    "(%u files), INFO %p; residency seated into the ROM's "
                    "own table\n", (void *)root, (void *)fat, n,
            (void *)*(sd_u8 **)(root + 0x84));
}

// Decode one SWAV record (12-byte header + payload) into mono s16.
static const WaveCache *decode_swav(const sd_u8 *rec)
{
    auto it = g_waves.find(rec);
    if (it != g_waves.end()) return &it->second;

    int fmt = rec[0];
    int loop = rec[1];
    sd_u32 rate = rd16(rec + 2);
    sd_u32 loopWords = rd16(rec + 6);
    sd_u32 nonLoopWords = rd32(rec + 8);
    const sd_u8 *d = rec + 12;
    sd_u32 bytes = (loopWords + nonLoopWords) * 4;

    WaveCache wc;
    wc.w.sampleRate = rate ? rate : 32768;
    wc.w.loop = loop;
    wc.w.loopStart = 0;

    if (fmt == 0) {                       // PCM8, signed
        wc.pcm.resize(bytes);
        for (sd_u32 i = 0; i < bytes; i++)
            wc.pcm[i] = (sd_s16)((sd_s8)d[i] << 8);
        wc.w.loopStart = loopWords * 4;
    } else if (fmt == 1) {                // PCM16
        sd_u32 n = bytes / 2;
        wc.pcm.resize(n);
        for (sd_u32 i = 0; i < n; i++)
            wc.pcm[i] = (sd_s16)rd16(d + i * 2);
        wc.w.loopStart = loopWords * 2;
    } else if (fmt == 2) {                // IMA-ADPCM
        if (bytes < 4) return 0;
        sd_s32 pred = (sd_s16)rd16(d);
        int index = d[2] & 0x7f;
        if (index > 88) index = 88;
        sd_u32 nib = (bytes - 4) * 2;
        wc.pcm.reserve(nib);
        for (sd_u32 i = 0; i < nib; i++) {
            int code = (d[4 + (i >> 1)] >> ((i & 1) * 4)) & 0xf;
            sd_s32 step = kAdpcmStepTab[index];
            sd_s32 diff = step >> 3;
            if (code & 1) diff += step >> 2;
            if (code & 2) diff += step >> 1;
            if (code & 4) diff += step;
            pred += (code & 8) ? -diff : diff;
            if (pred < -0x8000) pred = -0x8000;
            if (pred > 0x7fff) pred = 0x7fff;
            index += kAdpcmIndexTab[code & 7];
            if (index < 0) index = 0;
            if (index > 88) index = 88;
            wc.pcm.push_back((sd_s16)pred);
        }
        // The 4-byte ADPCM header sits inside the "loop" word count, so the
        // loop point is (loopWords*4 - 4) bytes of nibbles in.
        wc.w.loopStart = loopWords >= 1 ? (loopWords * 4 - 4) * 2 : 0;
    } else {
        static int warned;
        if (!warned++) fprintf(stderr, "[sdat] unknown SWAV format %d\n", fmt);
        return 0;
    }

    wc.w.totalSamples = (sd_u32)wc.pcm.size();
    if (wc.w.loopStart >= wc.w.totalSamples) wc.w.loopStart = 0;
    auto res = g_waves.emplace(rec, std::move(wc));
    res.first->second.w.pcm = res.first->second.pcm.data();
    return &res.first->second;
}

int sdat_swar_wave(const sd_u8 *swar, sd_u32 index, SdatWave *out)
{
    if (!swar || memcmp(swar, "SWAR", 4) != 0) return 0;
    sd_u32 count = rd32(swar + 0x38);
    if (index >= count) return 0;
    sd_u32 off = rd32(swar + 0x3c + index * 4);
    if (!off) return 0;
    const WaveCache *wc = decode_swav(swar + off);
    if (!wc || wc->pcm.empty()) return 0;
    *out = wc->w;
    out->pcm = wc->pcm.data();
    return 1;
}

// ---- SBNK ---------------------------------------------------------------

static void read_note_rec(const sd_u8 *p, SdatNote *n)
{
    n->swav     = rd16(p);
    n->swar     = rd16(p + 2);
    n->baseNote = p[4];
    n->attack   = p[5];
    n->decay    = p[6];
    n->sustain  = p[7];
    n->release  = p[8];
    n->pan      = p[9];
}

/* THE NOTE TYPE. The ARM7's instrument lookup (0x037FE4B0) leaves the type in
 * the first byte of the record it hands back: the instrument's own type for a
 * single instrument, the region's own leading u16 for a drum set or key split.
 * The note-on (0x037FD49C) and the channel start (0x037FE3C4) both switch on
 * it:
 *
 *     1, 4   a sampled channel (0xFFFF = any of the 16)
 *     2      PSG pulse: the record's first u16 is the DUTY, not a wave index;
 *            channels 8..13 only (0x3F00)         -> 0x037FC46C
 *     3      PSG noise: channels 14 and 15 only (0xC000) -> 0x037FC420
 *
 * This reader used to treat every type 1..5 as a sampled record, so a pulse
 * instrument's duty (3 in all but a few of the 27) was read as a wave index
 * and the note played wave 3 of the linked SWAR instead of a square wave. */
int sdat_bank_note(const sd_u8 *sbnk, int prog, int note,
                   SdatNote *out, const sd_u8 **swarOut)
{
    if (!sbnk || memcmp(sbnk, "SBNK", 4) != 0) return 0;
    sd_u32 nInst = rd32(sbnk + 0x38);
    if (prog < 0 || (sd_u32)prog >= nInst) return 0;

    const sd_u8 *ir = sbnk + 0x3c + prog * 4;
    int type = ir[0];
    sd_u32 off = rd16(ir + 1);
    if (!type || !off) return 0;
    const sd_u8 *rec = sbnk + off;

    *swarOut = 0;
    if (type >= 1 && type <= 5) {
        read_note_rec(rec, out);
        out->type = (sd_u8)type;
    } else if (type == 0x10) {              // drum set
        int lo = rec[0], hi = rec[1];
        if (note < lo || note > hi) return 0;
        const sd_u8 *reg = rec + 2 + (note - lo) * 12;
        read_note_rec(reg + 2, out);
        out->type = reg[0];
    } else if (type == 0x11) {              // key split
        int region = -1;
        for (int i = 0; i < 8; i++) {
            if (rec[i] == 0) break;
            if (note <= rec[i]) { region = i; break; }
        }
        if (region < 0) return 0;
        const sd_u8 *reg = rec + 8 + region * 12;
        read_note_rec(reg + 2, out);
        out->type = reg[0];
    } else {
        return 0;
    }

    // PSG notes have no wave: the duty (pulse) or nothing (noise) is all the
    // record carries, and there is no SWAR to link.
    if (out->type == 2 || out->type == 3) return 1;

    // The linked SWAR comes out of the SBNK's own reserved wave-link slots,
    // which func_0205b78c patched when the game started the sequence -- the
    // same 32 bytes the hardware ARM7 reads.
    if (out->swar > 3) return 0;
    const sd_u8 *swar = *(const sd_u8 *const *)(sbnk + 0x18 + out->swar * 8);
    if (!swar) return 0;
    *swarOut = swar;
    return 1;
}

int sdat_link_bank_waves(sd_u8 *sbnk)
{
    if (!sbnk || !g_sdat.info || memcmp(sbnk, "SBNK", 4) != 0) return 0;
    sd_u32 fid = sdat_file_id_of(sbnk);
    if (fid == (sd_u32)-1) return 0;

    // Find the BANK info entry that names this file.
    sd_u32 recOff = rd32(g_sdat.info + 8 + SDAT_REC_BANK * 4);
    if (!recOff) return 0;
    sd_u8 *tbl = g_sdat.info + recOff;
    sd_u32 n = rd32(tbl);
    sd_u8 *entry = 0;
    for (sd_u32 i = 0; i < n; i++) {
        sd_u32 v = rd32(tbl + 4 + i * 4);
        if (!v) continue;
        sd_u8 *e = g_sdat.info + v;
        if (rd32(e) == fid) { entry = e; break; }
    }
    if (!entry) return 0;

    int filled = 0;
    for (int k = 0; k < 4; k++) {
        sd_u16 id = rd16(entry + 4 + k * 2);
        if (id == 0xffff) continue;
        sd_u8 **slot = (sd_u8 **)(sbnk + 0x18 + k * 8);
        if (*slot) continue;                       // the game already linked it
        sd_u8 *we = sdat_info_entry(SDAT_REC_WAVEARC, id);
        if (!we) continue;
        sd_u8 *swar = sdat_file(rd32(we));
        if (!swar || memcmp(swar, "SWAR", 4) != 0) continue;
        *slot = swar;
        filled++;
    }
    return filled;
}

// ---- volume curve -------------------------------------------------------

// 0..127 -> tenths of a dB in [-723, 0], the ARM7's own table.
//
// SM64DS's ARM7 carries exactly one such table, at 0x03805860 (arm7.bin file
// offset 0xd9c8), and it is the SQUARE-law one: round(400 * log10(v / 127))
// tenths of a dB, with entry 0 at -723 and entry 1 at -722. Every 0..127
// level the sound driver turns into decibels goes through it: the note's
// velocity in the channel update (0x037FC5E0), the track's volume and
// expression and the player's volume in the track update (0x037FD694..
// 0x037FD6B0), and the sustain level in the envelope (0x037FBCF0). The
// formula below reproduces the cartridge's 128 entries exactly (checked
// entry for entry against arm7.bin), so no ROM bytes are carried here.
//
// This used to be 200 * log10 -- the LINEAR curve, which is the ARM9's
// table data_02086384 and the right one for the fader values the game
// computes on the ARM9 side, but only half the attenuation the ARM7 applies
// to a 0..127 level. A velocity-64 note came out at -6 dB instead of -12, a
// track at volume 40 at -10 dB instead of -20, and a sustain of 60 held at
// -6.5 dB instead of -13: every soft part of every sequence played too loud.
int sd_cnv_vol(int v)
{
    static sd_s16 lut[128];
    static int built;
    if (!built) {
        built = 1;
        lut[0] = -723;
        lut[1] = -722;
        for (int i = 2; i < 128; i++)
            lut[i] = (sd_s16)floor(400.0 * log10((double)i / 127.0) + 0.5);
    }
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    return lut[v];
}

// ---- mods: sound/ replacements (run mods1, lane MODSOUND1) ---------------
//
// Each enabled mod (hal/mods.h, index 0 = first, the LAST wins) may carry a
// sound/ folder. A file there replaces one member of the archive, named by
// the archive's own SYMB name plus the type's extension (NCS_BGM_PERFECT.sseq)
// or, for a member the archive has no name for, by type and record number
// (sseq_0042.sseq, sbnk_0007.sbnk). The new bytes are appended to the in-memory
// image and the member's FAT record (offset, size) is repointed, so every
// later read of that member (sdat_file, the ROM's residency fill, the sequence
// and wave readers) sees the mod's bytes. Run before any pointer into the
// image exists.
//
// Nothing here may fail the boot or crash the game on a bad file. The readers
// further down trust the archive's bytes, so a file is only accepted after its
// own structure has been checked against its stated size (mod_member_ok):
// every offset the readers follow must land inside the file. Anything that
// fails is skipped with one log line and the stock member stays. Limits: a
// file over kModMaxFile is not read, the added bytes over kModMaxTotal are
// skipped (naming the file), and a zeroed tail of kModGuard bytes follows the
// last appended member so a reader that overruns by a little reads zeros.
// The mod list lives in hal/mods.cpp, which only the two game targets link;
// hal/sdat_mods.cpp (same two targets) fills these three pointers at start-up.
// Everywhere else they stay null and no mod is ever looked at.
extern "C" {
int (*g_sdat_mods_count)(void);
const char *(*g_sdat_mods_id)(int);
const char *(*g_sdat_mods_folder)(int);
}

// The biggest stock member is 284072 bytes, so 8 MB is about 28 times that.
static const size_t kModMaxFile  = 8u << 20;
static const size_t kModMaxTotal = 64u << 20;
static const size_t kModGuard    = 16u << 10;

static std::string lower_s(std::string s)
{ for (auto &c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32); return s; }

// Walks the instrument records of an SBNK. Structure is checked against n;
// onWave(slot, swav) is called for every PCM sample reference and may veto it.
template <class F>
static bool sbnk_walk(const sd_u8 *d, size_t n, F onWave, const char *&why)
{
    if (n < 0x3c) { why = "bank header cut short"; return false; }
    sd_u32 cnt = rd32(d + 0x38);
    if (cnt > (n - 0x3c) / 4) { why = "bank record count runs past the file"; return false; }
    for (sd_u32 i = 0; i < cnt; i++) {
        unsigned t = d[0x3c + i * 4];
        size_t off = rd16(d + 0x3d + i * 4);
        if (t == 0) continue;
        if (t >= 1 && t <= 5) {
            if (off + 10 > n) { why = "bank instrument runs past the file"; return false; }
            if (t == 1 && !onWave(rd16(d + off + 2), rd16(d + off), why)) return false;
        } else if (t == 16 || t == 17) {
            size_t ne, first;
            if (t == 16) {
                if (off + 2 > n || d[off + 1] < d[off]) { why = "bank drum set is malformed"; return false; }
                ne = (size_t)d[off + 1] - d[off] + 1; first = off + 2;
            } else {
                if (off + 8 > n) { why = "bank key split runs past the file"; return false; }
                for (ne = 0; ne < 8 && d[off + ne]; ne++) {}
                first = off + 8;
            }
            if (first + ne * 12 > n) { why = "bank instrument list runs past the file"; return false; }
            for (size_t k = 0; k < ne; k++) {
                const sd_u8 *e = d + first + k * 12;
                unsigned et = rd16(e);
                if (et > 5) { why = "bank sub-instrument has an unknown type"; return false; }
                if (et == 1 && !onWave(rd16(e + 4), rd16(e + 2), why)) return false;
            }
        } else { why = "bank instrument has an unknown type"; return false; }
    }
    return true;
}

// Is d (n = the size it states) a sane member of this kind? why = first problem.
static bool mod_member_ok(int kind, const sd_u8 *d, size_t n, const char *&why)
{
    why = "";
    if (kind == 0) {                                   // SSEQ: DATA block, data offset inside
        if (n < 0x1c || memcmp(d + 0x10, "DATA", 4) != 0) { why = "no DATA block"; return false; }
        sd_u32 dof = rd32(d + 0x18);
        if (dof < 0x1c || dof >= n) { why = "sequence data offset is outside the file"; return false; }
        return true;
    }
    if (kind == 2) {                                   // SBNK
        if (n < 0x3c || memcmp(d + 0x10, "DATA", 4) != 0) { why = "no DATA block"; return false; }
        return sbnk_walk(d, n, [](unsigned slot, unsigned, const char *&w) {
            if (slot > 3) { w = "bank names a wave archive slot above 3"; return false; }
            return true; }, why);
    }
    if (kind == 3) {                                   // SWAR: count + offset table
        if (n < 0x3c || memcmp(d + 0x10, "DATA", 4) != 0) { why = "no DATA block"; return false; }
        sd_u32 cnt = rd32(d + 0x38);
        if (cnt > (n - 0x3c) / 4) { why = "wave count runs past the file"; return false; }
        for (sd_u32 i = 0; i < cnt; i++) {
            sd_u32 o = rd32(d + 0x3c + i * 4);
            if (o < 0x3c + cnt * 4 || o > n || n - o < 12) { why = "wave offset is outside the file"; return false; }
            unsigned long long words = (unsigned long long)rd16(d + o + 6) + rd32(d + o + 8);    // loop start + loop length, in words
            if (words * 4 > n - o - 12) { why = "wave data runs past the file"; return false; }
        }
        return true;
    }
    if (kind == 1) {                                   // SSAR: records of 12 bytes, data offset
        if (n < 0x20 || memcmp(d + 0x10, "DATA", 4) != 0) { why = "no DATA block"; return false; }
        sd_u32 dof = rd32(d + 0x18), cnt = rd32(d + 0x1c);
        if (dof > n || cnt > (n - 0x20) / 12 || 0x20 + (size_t)cnt * 12 > dof) { why = "record table is outside the file"; return false; }
        for (sd_u32 i = 0; i < cnt; i++) {
            sd_u32 o = rd32(d + 0x20 + i * 12);
            if (o != 0xffffffffu && o >= n - dof) { why = "record points outside the file"; return false; }
        }
        return true;
    }
    // STRM: HEAD block, then a DATA block that fits
    if (n < 0x30 || memcmp(d + 0x10, "HEAD", 4) != 0) { why = "no HEAD block"; return false; }
    sd_u32 hs = rd32(d + 0x14);
    if (hs < 0x30 || hs > n - 0x10 || n - 0x10 - hs < 8 || memcmp(d + 0x10 + hs, "DATA", 4) != 0) {
        why = "no DATA block after the header block"; return false; }
    if (rd32(d + 0x14 + hs) > n - 0x10 - hs) { why = "stream data block runs past the file"; return false; }
    return true;
}

static sd_u8 *sdat_apply_mods(sd_u8 *buf, size_t &len)
{
    if (!g_sdat_mods_count || !g_sdat_mods_id || !g_sdat_mods_folder) return buf;
    int nmods = g_sdat_mods_count();
    if (nmods <= 0) return buf;

    static const struct { int kind; const char *ext; const char *magic; } kTypes[] = {
        {0, "sseq", "SSEQ"}, {1, "ssar", "SSAR"}, {2, "sbnk", "SBNK"},
        {3, "swar", "SWAR"}, {7, "strm", "STRM"} };
    int nblocks = rd16(buf + 0x0e);
    sd_u32 oSymb = 0, oInfo = 0, oFat = 0;
    for (int i = 0; i < nblocks && i < 8; i++) {
        sd_u32 off = rd32(buf + 0x10 + i * 8);
        if (!off || off + 12 > len) continue;
        if (!memcmp(buf + off, "SYMB", 4)) oSymb = off;
        else if (!memcmp(buf + off, "INFO", 4)) oInfo = off;
        else if (!memcmp(buf + off, "FAT ", 4)) oFat = off;
    }
    if (!oInfo || !oFat) return buf;
    sd_u32 fatCount = rd32(buf + oFat + 8);
    if (fatCount > (len - oFat - 0xc) / 16) fatCount = (sd_u32)((len - oFat - 0xc) / 16);

    // name table: lowercase "<name>.<ext>" and "<ext>_<NNNN>.<ext>" -> (file id, kind).
    // SYMB name lists are 4-byte entries (a name offset) for every kind except
    // SEQARC (kind 1), whose entries are 8 bytes: name offset, then the offset
    // of the archive's own sub-name table.
    struct Hit { sd_u32 fid; int kind; };
    std::unordered_map<std::string, Hit> byName;
    for (auto &t : kTypes) {
        sd_u32 lo = rd32(buf + oInfo + 8 + t.kind * 4);
        if (!lo || (size_t)oInfo + lo + 4 > len) continue;
        sd_u32 n = rd32(buf + oInfo + lo);
        sd_u32 sl = 0, sn = 0;
        const sd_u32 stride = t.kind == 1 ? 8 : 4;
        if (oSymb) {
            sl = rd32(buf + oSymb + 8 + t.kind * 4);
            if (sl && (size_t)oSymb + sl + 4 <= len) sn = rd32(buf + oSymb + sl);
            else sl = 0;
        }
        for (sd_u32 i = 0; i < n && (size_t)oInfo + lo + 4 + i * 4 + 4 <= len; i++) {
            sd_u32 ro = rd32(buf + oInfo + lo + 4 + i * 4);
            if (!ro || (size_t)oInfo + ro + 4 > len) continue;
            sd_u32 fid = rd32(buf + oInfo + ro) & 0xffffff;
            if (fid >= fatCount) continue;
            char num[32];
            snprintf(num, sizeof num, "%s_%04u.%s", t.ext, i, t.ext);
            byName.emplace(num, Hit{fid, t.kind});
            if (sl && i < sn && (size_t)oSymb + sl + 4 + (size_t)i * stride + 4 <= len) {
                sd_u32 so = rd32(buf + oSymb + sl + 4 + i * stride);
                if (so && (size_t)oSymb + so < len) {
                    const char *nm = (const char *)buf + oSymb + so;
                    size_t maxl = len - (oSymb + so);
                    size_t l = strnlen(nm, maxl);
                    if (l && l < maxl)
                        byName.emplace(lower_s(std::string(nm, l) + "." + t.ext), Hit{fid, t.kind});
                }
            }
        }
    }

    struct Rep { std::vector<sd_u8> data; std::string name, mod; int kind; };
    std::unordered_map<sd_u32, Rep> reps;
    std::vector<sd_u32> order;
    size_t added = 0;                                  // padded bytes of the replacements held now
    auto pad4 = [](size_t x) { return (x + 3) & ~(size_t)3; };
    for (int m = 0; m < nmods; m++) {
        const char *folder = g_sdat_mods_folder(m), *mid = g_sdat_mods_id(m);
        if (!folder || !mid) continue;
        std::string pat = std::string(folder) + "/sound/*";
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pat.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string fn = fd.cFileName, lf = lower_s(fn);
            size_t dot = lf.rfind('.');
            std::string ext = dot == std::string::npos ? "" : lf.substr(dot + 1);
            const char *magic = 0;
            for (auto &t : kTypes) if (ext == t.ext) magic = t.magic;
            if (!magic) {
                fprintf(stderr, "[mods] sound: %s/%s: not .sseq/.sbnk/.swar/.ssar/.strm, ignored\n", mid, fn.c_str());
                continue;
            }
            auto it = byName.find(lf);
            if (it == byName.end()) {
                fprintf(stderr, "[mods] sound: %s/%s: no such member in the archive, ignored\n", mid, fn.c_str());
                continue;
            }
            // the size is known before anything is read or allocated
            unsigned long long fsz = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (fsz > kModMaxFile) {
                fprintf(stderr, "[mods] sound: %s/%s: %llu bytes is over the %u byte limit for one file, skipped\n",
                        mid, fn.c_str(), fsz, (unsigned)kModMaxFile);
                continue;
            }
            if (fsz < 0x10) {
                fprintf(stderr, "[mods] sound: %s/%s: too short to be a %s file, skipped\n", mid, fn.c_str(), magic);
                continue;
            }
            std::string full = std::string(folder) + "/sound/" + fn;
            FILE *f = fopen(full.c_str(), "rb");
            if (!f) { fprintf(stderr, "[mods] sound: %s/%s: cannot open, ignored\n", mid, fn.c_str()); continue; }
            std::vector<sd_u8> d((size_t)fsz);
            size_t got = fread(d.data(), 1, d.size(), f);
            fclose(f);
            if (got != d.size() || memcmp(d.data(), magic, 4) != 0) {
                fprintf(stderr, "[mods] sound: %s/%s: not a valid %s file (magic), skipped\n", mid, fn.c_str(), magic);
                continue;
            }
            sd_u32 stated = rd32(d.data() + 8);
            if (stated < 0x10 || stated > d.size()) {
                fprintf(stderr, "[mods] sound: %s/%s: stated size %u does not fit the %u byte file, skipped\n",
                        mid, fn.c_str(), (unsigned)stated, (unsigned)d.size());
                continue;
            }
            d.resize(stated);                           // the stated size is the member's size
            const char *why = "";
            if (!mod_member_ok(it->second.kind, d.data(), d.size(), why)) {
                fprintf(stderr, "[mods] sound: %s/%s: bad %s file (%s), skipped\n", mid, fn.c_str(), magic, why);
                continue;
            }
            sd_u32 fid = it->second.fid;
            size_t had = reps.count(fid) ? pad4(reps[fid].data.size()) : 0;
            if (added - had + pad4(d.size()) > kModMaxTotal) {
                fprintf(stderr, "[mods] sound: %s/%s: would pass the %u byte limit on added data, skipped\n",
                        mid, fn.c_str(), (unsigned)kModMaxTotal);
                continue;
            }
            if (!reps.count(fid)) order.push_back(fid);
            Rep &r = reps[fid];
            added = added - had + pad4(d.size());
            r.data.swap(d); r.name = fn; r.mod = mid; r.kind = it->second.kind;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    // A bank names samples by (wave archive slot, wave number). Some stock banks name waves their
    // archive does not have (instruments the game never plays), so the check is relative: a bank
    // may not name a missing wave that it did not already name with the stock archive. A bank that
    // fails, with a replaced bank or wave archive involved, loses the replacement(s).
    auto view = [&](sd_u32 fid, bool stockOnly, const sd_u8 *&p, size_t &n) {
        auto it = reps.find(fid);
        if (!stockOnly && it != reps.end()) { p = it->second.data.data(); n = it->second.data.size(); return true; }
        if (fid >= fatCount) return false;
        sd_u32 o = rd32(buf + oFat + 0xc + fid * 16), s = rd32(buf + oFat + 0x10 + fid * 16);
        if ((size_t)o + s > len) return false;
        p = buf + o; n = s; return true;
    };
    for (bool again = true; again; ) {
        again = false;
        sd_u32 blo = rd32(buf + oInfo + 8 + 2 * 4), wlo = rd32(buf + oInfo + 8 + 3 * 4);
        if (!blo || !wlo) break;
        sd_u32 nb = rd32(buf + oInfo + blo), nw = rd32(buf + oInfo + wlo);
        for (sd_u32 bi = 0; bi < nb && !again; bi++) {
            if ((size_t)oInfo + blo + 4 + bi * 4 + 4 > len) break;
            sd_u32 ro = rd32(buf + oInfo + blo + 4 + bi * 4);
            if (!ro || (size_t)oInfo + ro + 12 > len) continue;
            sd_u32 bfid = rd32(buf + oInfo + ro) & 0xffffff;
            bool involved = reps.count(bfid) != 0;
            sd_u32 swfid[4];
            for (int s = 0; s < 4; s++) {
                swfid[s] = 0xffffffffu;
                sd_u32 wa = rd16(buf + oInfo + ro + 4 + s * 2);
                if (wa >= nw || wa == 0xffff || (size_t)oInfo + wlo + 4 + wa * 4 + 4 > len) continue;
                sd_u32 wro = rd32(buf + oInfo + wlo + 4 + wa * 4);
                if (!wro || (size_t)oInfo + wro + 4 > len) continue;
                swfid[s] = rd32(buf + oInfo + wro) & 0xffffff;
                if (reps.count(swfid[s])) involved = true;
            }
            if (!involved) continue;
            // the set of (slot, wave) pairs the bank names that its wave archives lack
            auto missing = [&](bool stockOnly, std::set<unsigned> &out) {
                const sd_u8 *bp; size_t bn; const char *w = "";
                if (!view(bfid, stockOnly, bp, bn)) return false;
                return sbnk_walk(bp, bn, [&](unsigned slot, unsigned swav, const char *&) {
                    const sd_u8 *wp; size_t wn;
                    if (slot > 3 || swfid[slot] == 0xffffffffu || !view(swfid[slot], stockOnly, wp, wn) ||
                        wn < 0x3c || swav >= rd32(wp + 0x38)) out.insert(slot << 16 | swav);
                    return true; }, w);
            };
            std::set<unsigned> now, was;
            bool okNow = missing(false, now);
            missing(true, was);
            bool bad = !okNow;
            for (unsigned k : now) if (!was.count(k)) bad = true;
            if (!bad) continue;
            // drop the replacement(s) that took part: the bank, or else the wave archives
            std::vector<sd_u32> drop;
            if (reps.count(bfid)) drop.push_back(bfid);
            else for (int s = 0; s < 4; s++) if (swfid[s] != 0xffffffffu && reps.count(swfid[s])) drop.push_back(swfid[s]);
            for (sd_u32 fid : drop) {
                fprintf(stderr, "[mods] sound: %s (%s): bank file %u would use a wave its wave archive does not have, replacement dropped, stock kept\n",
                        reps[fid].name.c_str(), reps[fid].mod.c_str(), (unsigned)bfid);
                added -= pad4(reps[fid].data.size());
                reps.erase(fid);
                for (size_t k = 0; k < order.size(); k++) if (order[k] == fid) { order.erase(order.begin() + k); break; }
            }
            again = !drop.empty();
        }
    }

    if (order.empty()) {
        fprintf(stderr, "[mods] sound: no members replaced\n");
        return buf;
    }
    size_t at0 = pad4(len);
    size_t total = at0 + added + kModGuard;
    sd_u8 *nb = (sd_u8 *)malloc(total);
    if (!nb) { fprintf(stderr, "[mods] sound: out of memory, no members replaced\n"); return buf; }
    memset(nb, 0, total);
    memcpy(nb, buf, len);
    size_t at = at0;
    std::string list;
    for (sd_u32 fid : order) {
        Rep &r = reps[fid];
        memcpy(nb + at, r.data.data(), r.data.size());
        sd_u8 *e = nb + oFat + 0xc + fid * 16;
        e[0] = (sd_u8)at; e[1] = (sd_u8)(at >> 8); e[2] = (sd_u8)(at >> 16); e[3] = (sd_u8)(at >> 24);
        sd_u32 sz = (sd_u32)r.data.size();
        e[4] = (sd_u8)sz; e[5] = (sd_u8)(sz >> 8); e[6] = (sd_u8)(sz >> 16); e[7] = (sd_u8)(sz >> 24);
        at += pad4(r.data.size());
        list += (list.empty() ? "" : ", ") + r.name + " (" + r.mod + ", file " + std::to_string((unsigned)fid) + ")";
    }
    free(buf);
    len = total;
    fprintf(stderr, "[mods] sound: replaced %u archive members: %s\n", (unsigned)order.size(), list.c_str());
    return nb;
}

// ---- init ---------------------------------------------------------------

int sdat_init(void)
{
    if (g_sdat.base) return 1;

    char path[512];
    const char *env = getenv("SM64DS_SDAT");
    if (env) snprintf(path, sizeof path, "%s", env);
    else snprintf(path, sizeof path,
                  "%s/extracted/dsd/files/data/sound_data.sdat", asset_root());

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[sdat] cannot open %s -- sound stays silent\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0x40) { fclose(f); return 0; }
    sd_u8 *buf = (sd_u8 *)malloc((size_t)len);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f); free(buf);
        fprintf(stderr, "[sdat] short read on %s\n", path);
        return 0;
    }
    fclose(f);

    if (memcmp(buf, "SDAT", 4) != 0) {
        fprintf(stderr, "[sdat] %s is not an SDAT\n", path);
        free(buf);
        return 0;
    }

    {
        size_t mlen = (size_t)len;
        try {
            buf = sdat_apply_mods(buf, mlen);
            len = (long)mlen;
        } catch (...) {
            // the stock image is only freed after a replacement succeeds, so it is still intact
            fprintf(stderr, "[mods] sound: failed while applying replacements, stock archive kept\n");
        }
    }
    g_sdat.base = buf;
    g_sdat.size = (size_t)len;

    int nblocks = rd16(buf + 0x0e);
    sd_u8 *symb = 0, *file = 0;
    for (int i = 0; i < nblocks && i < 8; i++) {
        sd_u32 off = rd32(buf + 0x10 + i * 8);
        if (!off || off >= (sd_u32)len) continue;
        sd_u8 *b = buf + off;
        if (!memcmp(b, "SYMB", 4)) symb = b;
        else if (!memcmp(b, "INFO", 4)) g_sdat.info = b;
        else if (!memcmp(b, "FAT ", 4)) g_sdat.fat = b;
        else if (!memcmp(b, "FILE", 4)) file = b;
    }
    (void)symb;
    if (!g_sdat.info || !g_sdat.fat) {
        fprintf(stderr, "[sdat] missing INFO or FAT block\n");
        free(buf);
        g_sdat.base = 0;
        return 0;
    }
    g_sdat.fatCount = rd32(g_sdat.fat + 8);

    // The root object. 0x100 bytes covers everything func_02050f34 touches
    // (+0x30 FSFile, +0x74 handle pair, +0x7c/0x80/0x84 the three blocks);
    // the first 0x30 is the SDAT header itself, as on hardware.
    sd_u8 *root = (sd_u8 *)calloc(1, 0x100);
    if (!root) { free(buf); g_sdat.base = 0; return 0; }
    memcpy(root, buf, 0x30);
    *(sd_u8 **)(root + 0x7c) = g_sdat.fat;
    *(sd_u8 **)(root + 0x80) = file;
    *(sd_u8 **)(root + 0x84) = g_sdat.info;
    g_sdat.root = root;

    // NEITHER THE ROOT SEAT NOR THE PRE-SEAT HAPPENS HERE ANY MORE (run
    // link100, lane SND2, rung R4). data_020a5bb8 = root and the 282-entry
    // residency fill used to stand on these lines. src/func_02050f34.c is
    // what writes data_020a5bb8 on the DS, and it runs now -- it opens this
    // same archive through the ROM's own open-by-name FS and reads the
    // header, the INFO block and the FAT into the rung-R2 sound heap -- so
    // a seat here would be a second writer racing the cartridge's own. The
    // residency fill moved with it, into sdat_seat_rom_residency() below,
    // which fills the FAT THE ROM READ instead of this process's copy and
    // is called from hal/sdat/consumer.cpp at the line after the open.
    // g_sdat.root is still built: it is what sd_sdat_reseat falls back to
    // when the ROM's open did not happen (a target or a run with no sound
    // bring-up), and it costs 0x100 bytes.

    fprintf(stderr,
            "[sdat] %s: %ld bytes, %u files, seq=%u seqarc=%u bank=%u "
            "wavearc=%u group=%u; parsed, NOT seated -- rung R4 hands the "
            "seat to the cartridge's own func_02050f34\n",
            path, len, g_sdat.fatCount,
            rd32(g_sdat.info + rd32(g_sdat.info + 8 + SDAT_REC_SEQ * 4)),
            rd32(g_sdat.info + rd32(g_sdat.info + 8 + SDAT_REC_SEQARC * 4)),
            rd32(g_sdat.info + rd32(g_sdat.info + 8 + SDAT_REC_BANK * 4)),
            rd32(g_sdat.info + rd32(g_sdat.info + 8 + SDAT_REC_WAVEARC * 4)),
            rd32(g_sdat.info + rd32(g_sdat.info + 8 + SDAT_REC_GROUP * 4)));
    return 1;
}
