/* Proximity voice chat, the game half. Lane VOICE.
 *
 * hal/voice_chat.h carries the datagram, the settings and the test hooks. This
 * file is the machine: capture -> IMA ADPCM -> the seam's aux channel ->
 * a per-peer jitter buffer -> a distance-attenuated add into the finished
 * stereo mix.
 *
 * =========================================================================
 * THE ONE PROPERTY THAT MATTERS: THIS DOES NOT TOUCH THE GAME
 * =========================================================================
 * Everything below reads game state and writes none of it. The two reads are
 * data_0209f394[slot] (the per-slot Player pointers) and data_0209f250 (which
 * slot this console is), both taken as raw words at the offsets
 * hal/comms_sync.cpp and hal/player_fields.h already use, and both used only
 * to compute a floating-point volume that never re-enters the simulation.
 *
 * It is not on the lockstep path. func_0203ea5c.c's exchange is what the game
 * blocks on; voice_tick runs after it, in the same place sync_tick does, and
 * every send goes out through send_aux which the seam's own ordering rule puts
 * behind the input record. A dropped voice datagram is 40 ms of silence and
 * nothing else -- there is no retransmit, no ack, and no state on either side
 * that a loss can corrupt, because every ADPCM block carries its own predictor
 * (see the header).
 *
 * It runs on ONE THREAD. Capture is polled (CALLBACK_NULL, see
 * hal/voice_capture_win.cpp), voice_tick is called from the frame loop, and
 * the mix hook is called from sd_mix_render, which the same frame loop drives
 * through sd_out_push. So there is no lock anywhere in this file and no
 * cross-thread hand-off to get wrong.
 *
 * =========================================================================
 * THE FALLOFF
 * =========================================================================
 * Full VoiceVolume inside VoiceNearRadius, silence at and beyond
 * VoiceFarRadius, and between them
 *
 *     gain = log(far / d) / log(far / near)
 *
 * which is 1 at the near radius, 0 at the far one, and falls fastest where the
 * listener is closest -- the shape distance actually has, and the shape a
 * linear ramp does not. The radii are in the game's own world units, the
 * integer part of the Fix12 positions at Actor+0x5c.
 *
 * =========================================================================
 * MONO, DELIBERATELY
 * =========================================================================
 * Every remote voice is mixed at equal level into both channels. Panning was
 * scoped and dropped rather than guessed: it needs the world handedness of the
 * heading word at Actor+0x8e, this lane derived no measurement for that, and a
 * left/right image that is backwards is worse than no image at all. The seam
 * for it is the pan field on RemoteVoice, which is written once and read once.
 */
#include "hal/voice_chat.h"

#include "hal/comms_seam.h"
#include "hal/comms_loopback.h"
#include "hal/comms_hub.h"
#include "hal/host_settings.h"
#include "hal/sdat/sdat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

extern "C" {
// The per-slot Player pointers and which slot this console is. Same two
// symbols hal/comms_sync.cpp reads, declared the same way.
extern void *data_0209f394[];
extern unsigned char data_0209f250;
// The level this console is in (signed, levels 0..51), the same symbol and
// the same widening hal/comms_sync.cpp uses when it writes SYNG level_id.
extern signed char data_0209f2f8;
}

namespace port {
namespace {

using voice::kCapFrameSamples;   // 320
using voice::kCapRate;           // 16000

// ---------------------------------------------------------------------------
// THE WIRE
// ---------------------------------------------------------------------------
const unsigned kVoiceTagLE = 0x564e5953u;      // 'S','Y','N','V'
enum : int {
    kVersion       = 1,
    kHeaderBytes   = 12,
    kAdpcmBytes    = kCapFrameSamples / 2,     // 160
    kBlockBytes    = 4 + kAdpcmBytes,          // 164
    kFramesPerPkt  = 2,                        // 40 ms, 25 packets a second
    kMaxPktBytes   = kHeaderBytes + kFramesPerPkt * kBlockBytes,   // 340
};

// The carrier's aux message size, and the relay's payload cap. Both are
// checked here as well as where they are defined, because the datagram above
// is assembled in THIS file and a future frame-count change would be made
// here by somebody who never opens comms_loopback.cpp.
static_assert(kMaxPktBytes <= 384, "a voice datagram no longer fits the "
                                   "carrier's aux message size");
static_assert(kMaxPktBytes <= 700, "a voice datagram no longer fits the "
                                   "relay's 700-byte payload cap");

// ---------------------------------------------------------------------------
// IMA ADPCM, 4 bits a sample. The two tables are the format's own.
// ---------------------------------------------------------------------------
const int kStepTab[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
    209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724,
    796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132,
    7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767
};
const int kIndexTab[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

struct Codec { int pred; int index; };

inline void codec_clamp(Codec &c)
{
    if (c.pred > 32767) c.pred = 32767;
    if (c.pred < -32768) c.pred = -32768;
    if (c.index < 0) c.index = 0;
    if (c.index > 88) c.index = 88;
}

unsigned char adpcm_encode_one(Codec &c, int sample)
{
    const int step = kStepTab[c.index];
    int diff = sample - c.pred;
    unsigned char code = 0;
    if (diff < 0) { code = 8; diff = -diff; }

    /* The format's own three-bit magnitude search, written out rather than
       looped, because the reference decoder's reconstruction below has to
       mirror it exactly and a clever loop here is where the two drift. */
    int tmp = step;
    int delta = step >> 3;
    if (diff >= tmp) { code |= 4; diff -= tmp; delta += tmp; }
    tmp >>= 1;
    if (diff >= tmp) { code |= 2; diff -= tmp; delta += tmp; }
    tmp >>= 1;
    if (diff >= tmp) { code |= 1; delta += tmp; }

    if (code & 8) c.pred -= delta; else c.pred += delta;
    c.index += kIndexTab[code];
    codec_clamp(c);
    return code;
}

int adpcm_decode_one(Codec &c, unsigned char code)
{
    const int step = kStepTab[c.index];
    int delta = step >> 3;
    if (code & 4) delta += step;
    if (code & 2) delta += step >> 1;
    if (code & 1) delta += step >> 2;
    if (code & 8) c.pred -= delta; else c.pred += delta;
    c.index += kIndexTab[code];
    codec_clamp(c);
    return c.pred;
}

/* THE ENCODER'S STEP INDEX, CARRIED ACROSS FRAMES.
 *
 * The first version of this file reset it to 0 at every frame boundary and put
 * that zero in the header. That is a real defect and it was measured rather
 * than argued: index 0 is step size 7, so at the top of every 20 ms frame the
 * quantiser had to climb from 7 back up to whatever the signal needed, and the
 * first handful of samples of every frame were badly wrong. A sine round trip
 * through this codec read 18.7 dB SNR that way and 34.1 dB with the index
 * carried, which is the difference between "muffled with a tick 50 times a
 * second" and "a phone call".
 *
 * IT COSTS THE LOSS PROPERTY NOTHING, which is the only reason it is allowed.
 * The carried index is WRITTEN INTO THE BLOCK, so every block still describes
 * its own starting state completely and a decoder that never saw the previous
 * datagram still decodes this one exactly. The encoder is simply choosing a
 * better starting index than zero: one that already fits the signal. A lost
 * datagram still costs exactly the 40 ms it contained.
 *
 * The PREDICTOR is still reset to the frame's own first sample rather than
 * carried, and that half was always right: it makes the first residual zero, it
 * is one byte cheaper to reason about, and unlike the step index a stale
 * predictor is an audible DC jump rather than a slow re-converge. */
int g_enc_index;

/* One 20 ms frame into one 164-byte block. The block's header is the codec
   state BEFORE the first sample, so the decoder starts exactly where the
   encoder did and a lost frame costs nothing beyond itself. */
void encode_frame(const short *pcm, unsigned char *block)
{
    Codec c;
    c.pred = pcm[0];
    c.index = g_enc_index;
    codec_clamp(c);
    /* The starting predictor is the frame's first sample, which makes the
       first residual zero and costs one sample of accuracy at a frame edge in
       exchange for never carrying a PREDICTOR across a datagram. */
    block[0] = (unsigned char)(c.pred & 0xff);
    block[1] = (unsigned char)((c.pred >> 8) & 0xff);
    block[2] = (unsigned char)c.index;
    block[3] = 0;
    unsigned char *out = block + 4;
    for (int i = 0; i < kCapFrameSamples; i += 2) {
        const unsigned char lo = adpcm_encode_one(c, pcm[i]);
        const unsigned char hi = adpcm_encode_one(c, pcm[i + 1]);
        out[i / 2] = (unsigned char)(lo | (hi << 4));
    }
    g_enc_index = c.index;      /* where the next frame starts from */
}

void decode_frame(const unsigned char *block, short *pcm)
{
    Codec c;
    c.pred = (short)((unsigned short)block[0] | ((unsigned short)block[1] << 8));
    c.index = block[2];
    codec_clamp(c);
    const unsigned char *in = block + 4;
    for (int i = 0; i < kCapFrameSamples; i += 2) {
        const unsigned char b = in[i / 2];
        pcm[i]     = (short)adpcm_decode_one(c, (unsigned char)(b & 0x0f));
        pcm[i + 1] = (short)adpcm_decode_one(c, (unsigned char)(b >> 4));
    }
}

/* THE CODEC, MEASURED ON THE SHIPPED FUNCTIONS. SM64DS_VOICE_CODEC_SELFTEST=1.
 *
 * A sine at the amplitude a normal speaking voice reaches, pushed through the
 * REAL encode_frame and decode_frame above for a second of frames, reported as
 * a signal-to-noise ratio in dB. Not a Python model of the codec and not a
 * unit test of the tables: the point is to measure the code that actually
 * ships, across frame boundaries, so a regression in how the step index is
 * carried shows up as a number rather than as somebody's ear.
 *
 * The first frame is excluded from the measurement and deliberately so: the
 * encoder legitimately starts cold on the first frame of a call and spends it
 * converging, which is a real 20 ms and not a defect. What this asserts is the
 * steady state, which is every frame after it. Four bits a sample on a smooth
 * waveform should read comfortably above 30 dB; it read 18.7 while the step
 * index was being thrown away at every boundary. */
void codec_selftest()
{
    enum { kFrames = 50 };          /* one second */
    short in[kCapFrameSamples];
    short out[kCapFrameSamples];
    unsigned char block[kBlockBytes];
    double phase = 0.0;
    const double w = 2.0 * 3.14159265358979323846 * 440.0 / (double)kCapRate;
    double sig = 0.0, err = 0.0;
    const int save = g_enc_index;
    g_enc_index = 0;
    for (int f = 0; f < kFrames; ++f) {
        for (int i = 0; i < kCapFrameSamples; ++i) {
            in[i] = (short)(8000.0 * sin(phase));
            phase += w;
        }
        encode_frame(in, block);
        decode_frame(block, out);
        if (f == 0) continue;       /* the cold frame; see the banner */
        for (int i = 0; i < kCapFrameSamples; ++i) {
            const double d = (double)in[i] - (double)out[i];
            sig += (double)in[i] * (double)in[i];
            err += d * d;
        }
    }
    g_enc_index = save;
    const double snr = (err > 0.0 && sig > 0.0)
                     ? 10.0 * log10(sig / err) : 999.0;
    fprintf(stderr, "[voice] codec selftest: %d frames of 440 Hz at amplitude "
                    "8000 through the shipped IMA ADPCM, SNR %.1f dB\n",
            kFrames - 1, snr);
}

// ---------------------------------------------------------------------------
// THE JITTER BUFFER, one per remote slot.
//
// A ring of decoded 16 kHz samples with a fractional read cursor. It PRIMES:
// nothing is played until kPrimeFrames whole frames are sitting in it, which
// is what turns a network that delivers 40 ms in bursts into a continuous
// stream. It stops when it runs dry and re-primes rather than stuttering
// through an empty ring, because a buffer that plays whatever is there is a
// buffer that plays clicks.
//
// TWO TO FOUR FRAMES was the range asked for; three is the default, and it is
// the compromise the number has to be: two survives no reordering at all and
// four is 80 ms of added mouth-to-ear delay on top of the 40 ms the packing
// already costs.
// ---------------------------------------------------------------------------
enum : int {
    kPrimeFrames = 3,
    kMaxFrames   = 12,                              // 240 ms hard ceiling
    kRingSamples = kMaxFrames * kCapFrameSamples,
};

struct RemoteVoice {
    short pcm[kRingSamples];
    int   wr;                 // next write index
    int   rd;                 // next read index
    int   avail;              // samples between them
    unsigned frac;            // 16.16 position inside the sample at rd
    int   playing;
    unsigned last_seq;
    int   have_seq;
    unsigned last_ms;         // arrival of the last accepted datagram
    float gain;               // 0..1, distance * VoiceVolume, set by the tick
    float cur_gain;           // what the mixer is actually at, ramped
    float pan;                // 0.5 everywhere; see the banner
    int   dist;               // world units, for the report line
};

RemoteVoice g_rv[kCommsMaxPlayers];

void rv_reset(RemoteVoice &r)
{
    r.wr = r.rd = r.avail = 0;
    r.frac = 0;
    r.playing = 0;
    r.have_seq = 0;
    r.last_seq = 0;
    r.gain = r.cur_gain = 0.0f;
    r.pan = 0.5f;
    r.dist = -1;
}

void rv_push(RemoteVoice &r, const short *frame)
{
    /* A ring that is full is a listener who fell far enough behind that the
       oldest audio is stale. Dropping the OLDEST frame keeps the delay bounded
       -- the alternative, refusing the newest, pins the listener at the
       ceiling for the rest of the call. */
    if (r.avail + kCapFrameSamples > kRingSamples) {
        r.rd = (r.rd + kCapFrameSamples) % kRingSamples;
        r.avail -= kCapFrameSamples;
    }
    for (int i = 0; i < kCapFrameSamples; ++i) {
        r.pcm[r.wr] = frame[i];
        r.wr = (r.wr + 1) % kRingSamples;
    }
    r.avail += kCapFrameSamples;
    if (!r.playing && r.avail >= kPrimeFrames * kCapFrameSamples) r.playing = 1;
}

// ---------------------------------------------------------------------------
// STATE
// ---------------------------------------------------------------------------
int  g_inited;
int  g_on;                       // what VoiceEnabled said last tick
int  g_tone;                     // SM64DS_VOICE_TEST_TONE
int  g_report;                   // SM64DS_VOICE_REPORT
int  g_hooked;                   // the mixer hook is registered
int  g_vol;                      // VoiceVolume, 0..100
int  g_near, g_far;              // the two radii, world units
char g_mic[voice::kCapNameBytes];
/* VoiceMicIndex as of the last tick. -1 is "no index given", which is both the
   setting's default and the value that leaves VoiceMicDevice (and therefore
   auto-pick) in charge, so it has to be the initial value here too -- 0 would
   read as "the player explicitly asked for device 0" on the very first tick. */
int  g_mic_index = -1;

/* When the last cap_open attempt was made, so a device that will not open is
   retried on a timer instead of on every frame. 0 means "no attempt yet", which
   is why the store below never writes a literal 0. */
unsigned g_open_retry_ms;
enum : unsigned { kOpenRetryMs = 5000 };

short g_pending[kFramesPerPkt][kCapFrameSamples];
int   g_pending_n;
unsigned g_tx_seq;
double g_tone_phase;
unsigned g_tone_ms;              // the tone generator's own 20 ms clock

unsigned long long g_cap_frames, g_tx_pkts, g_rx_pkts, g_rx_bad, g_rx_dup;
unsigned g_report_ms;
/* Per sender slot, every datagram accepted from it since the process started.
   Kept outside RemoteVoice on purpose: rv_reset runs whenever a peer goes quiet
   for 400 ms, and a proof that asks "did every seat hear every talking seat"
   needs a count that a pause between two sentences does not wipe. */
unsigned long long g_rx_from[kCommsMaxPlayers];

// ---------------------------------------------------------------------------
// THE MIC GATE (run mp1 lane VOICE1). Voice goes on the wire only while the
// player is making sound.
//
// Each captured 20 ms frame is measured (RMS of its 320 samples). A frame at
// or above the threshold OPENS the gate; the gate then stays open for
// kGateHangFrames more frames after the last loud one (200 ms), so the quiet
// gaps inside a sentence and the soft tail of a word are sent rather than
// chopped. While the gate is closed the frames are drained and thrown away
// and NOTHING is sent: a room of sixteen players with their mics on and
// nobody speaking costs zero voice datagrams, where the open mic cost 25 a
// second from every one of them.
//
// ONE FRAME OF PRE-ROLL. The loudest part of a word is rarely its first 20
// ms; the frame just before the one that crossed the threshold usually holds
// the consonant that starts it. The newest quiet frame is kept, and when the
// gate opens it goes out first, in the same datagram as the frame that opened
// it. That costs nothing while quiet (a copy) and 20 ms of added audio per
// talk spurt.
//
// WHEN THE GATE CLOSES, a half-filled datagram is flushed at once rather than
// held for a second frame that is not coming, so the end of a sentence is not
// stuck in this console until the next time the player speaks.
//
// The threshold is an RMS amplitude on the s16 scale. The default, 256, is
// about -42 dBFS: well under normal speech into a headset or desk mic (roughly
// -30 to -20 dBFS) and above the hiss of a quiet room. SM64DS_VOICE_GATE
// overrides it; 0 turns the gate off entirely (every frame is "loud", which is
// the open mic this file shipped with, kept for A/B runs).
// ---------------------------------------------------------------------------
enum : int { kGateDefault = 256, kGateHangFrames = 10 };   // 10 x 20 ms = 200 ms
int   g_gate_thr = kGateDefault;
int   g_gate_open;                 // 1 while frames are being sent
int   g_gate_hang;                 // frames of hangover left
int   g_gate_level;                // RMS of the newest frame, for the report
short g_preroll[kCapFrameSamples];
int   g_have_preroll;
unsigned long long g_gate_opens, g_gate_quiet;   // spurts started, frames not sent

// ---------------------------------------------------------------------------
// THE ROOM'S VOICE SETTING (run mp1 lane VOICE1). SM64DS_VOICE_ALLOWED comes
// from the lobby's go plan: the host of a VS room can switch voice off for
// the match. "0" means this console behaves exactly as if VoiceEnabled were
// false for the whole process -- no recording device is opened, nothing is
// sent, nothing received is played -- whatever the player's own settings.json
// says. Absent, or any other value, is "allowed", so a solo boot, a test run
// and an adventure plan (which has no such field) are unaffected. Read once:
// the plan is fixed for the life of the process.
// ---------------------------------------------------------------------------
int g_allowed = 1;

// ---------------------------------------------------------------------------
// THE ADVENTURE ROUTE (run mp1 lane VOICE1). In adventure online there is no
// DS session: hal/comms_hub.cpp is the carrier, every console plays its own
// world, and the other players are ghosts. Voice rides the hub's 'SYNV' kind
// (same datagram, byte for byte, as in VS) and the sender is the seat the
// relay stamped, never a byte of the payload.
//
// PROXIMITY DECIDES WHETHER TO SEND AT ALL, not only how loud to play. Each
// tick, from the hub's newest ghost frame per seat: is any present seat in MY
// level and closer than VoiceFarRadius? If none is, gated audio is dropped
// here instead of going out (counted as "nobody"), because every receiver
// would play it at gain 0 anyway: in a world room of sixteen spread over the
// castle, a player talking alone in a course costs nobody any bandwidth. The
// gate still runs, so the talking flag on this player's ghost stays honest.
//
// On the receive side the distance is the same one: my body against the
// sender's newest ghost position, and a sender in another level is silent.
// ---------------------------------------------------------------------------
static_assert(kHubMaxSeats == kCommsMaxPlayers, "hub seats index g_rv");
int g_hub;                       // 1 while this tick runs over the hub
int g_in_range;                  // some seat can hear me (hub only)
int g_in_range_n;                // how many, for the report
unsigned long long g_tx_nobody;  // gated frames dropped: nobody in range

/* SM64DS_VOICE_TEST_GHOST=1: A PROOF STAND-IN FOR THE GHOST LAYER, OFF BY
   DEFAULT AND NEVER SET BY THE LAUNCHER. The proximity rule above reads the
   seats' positions out of their 'SYNG' ghost frames, which hal/comms_sync.cpp
   (lane GHOSTWIRE1) sends; on a build without that sender every seat has no
   ghost frame, nobody is ever in range, and voice over the hub could not be
   proven at all. With this knob the voice tick sends a minimal SYNG of this
   console's own body (live flag, talking flag, level, position; everything
   else zero) every second tick, through the hub's own kind table. Do not set
   it together with the real ghost sender: two sources of one seat's frames
   would fight over its seq. */
int g_test_ghost;
unsigned g_test_ghost_seq;
unsigned g_test_ghost_tick;

void test_ghost_send()
{
    if (!g_test_ghost || !g_hub) return;
    if ((g_test_ghost_tick++ & 1u) != 0) return;
    const char *me = (const char *)data_0209f394[data_0209f250];
    HubGhostFrame f;
    memset(&f, 0, sizeof f);
    f.tag = kHubTagGhost;
    f.version = 1;
    f.seat = 0xFF;
    f.flags = (unsigned char)(kGhostLive | (g_gate_open ? kGhostTalking : 0));
    f.seq = (unsigned short)(g_test_ghost_seq++);
    f.level_id = (unsigned short)(int)data_0209f2f8;
    if (me) {
        f.x = *(const int *)(me + 0x5c);
        f.y = *(const int *)(me + 0x60);
        f.z = *(const int *)(me + 0x64);
    }
    comms_hub_send(&f, (int)sizeof f);
}

// ---------------------------------------------------------------------------
// THE TEST SOURCE'S SHAPE (run mp1 lane VOICE1). Only meaningful with
// SM64DS_VOICE_TEST_TONE=1, which already replaces the microphone with a
// generated sine and opens no device. These make that sine behave like a
// person, so the gate can be proven without any real audio:
//   SM64DS_VOICE_TEST_TONE_AMP=<0..32767>   the sine's amplitude (16000)
//   SM64DS_VOICE_TEST_TONE_DUTY=<on>,<off>  milliseconds of tone, then of
//                                           silence, repeating (default: tone
//                                           all the time)
//   SM64DS_VOICE_TEST_NOISE=<0..32767>      a deterministic hiss of this peak
//                                           amplitude under everything, so
//                                           "silence" is a quiet room rather
//                                           than digital zero
// ---------------------------------------------------------------------------
int      g_tone_amp = 16000;
unsigned g_tone_on_ms, g_tone_off_ms;       // 0/0 = continuous
unsigned g_tone_t;                          // ms into the current on+off cycle
int      g_noise_amp;
unsigned g_noise_seed = 0x2545F491u;

void gate_reset();              // below, beside the gate itself

int env_on(const char *name)
{
    const char *v = getenv(name);
    return v && *v && strcmp(v, "0") != 0;
}

/* An integer env knob clamped to [lo, hi]; `def` when unset or not a number. */
int env_int(const char *name, int def, int lo, int hi)
{
    const char *v = getenv(name);
    if (!v || !*v) return def;
    char *end = 0;
    const long n = strtol(v, &end, 10);
    if (end == v) return def;
    if (n < lo) return lo;
    if (n > hi) return hi;
    return (int)n;
}

/* RMS of one frame, on the s16 scale. */
int frame_rms(const short *pcm)
{
    double acc = 0.0;
    for (int i = 0; i < kCapFrameSamples; ++i)
        acc += (double)pcm[i] * (double)pcm[i];
    return (int)sqrt(acc / (double)kCapFrameSamples);
}

unsigned now_ms()
{
#if defined(_WIN32)
    return GetTickCount();
#else
    return 0;
#endif
}

/* The recording devices, one line each, at first use. Enumeration opens
   nothing, so this is safe to run with VoiceEnabled off -- which is the point:
   a launcher lane building a device list wants the same names this build
   matches against, and a player who cannot get his microphone selected needs
   to see what the game thinks is on the machine. */
void dump_devices()
{
    char names[16][voice::kCapNameBytes];
    const int n = voice::cap_enumerate(names, 16);
    fprintf(stderr, "[voice] %d recording device%s on this machine\n", n,
            n == 1 ? "" : "s");
    for (int i = 0; i < n && i < 16; ++i)
        fprintf(stderr, "[voice]   device %d: '%s'\n", i, names[i]);
    if (n > 16)
        fprintf(stderr, "[voice]   (%d more, not listed)\n", n - 16);
}

// ---------------------------------------------------------------------------
// THE MIX HOOK. Called from sd_mix_render, AFTER the host master volume, at
// SD_MIX_RATE. See mixer.cpp's tail for why after.
//
// 16000 into 32768 is not a whole ratio, so the cursor is 16.16 fixed point
// stepping by 32000 per output frame (16000 * 65536 / 32768) with linear
// interpolation between the two samples it lands between -- the same
// resampling the DS mixer does in hardware and the same the sdat mixer does
// for every other voice.
// ---------------------------------------------------------------------------
enum : unsigned { kResampleStep = (unsigned)((16000ull << 16) / SD_MIX_RATE) };

void voice_mix_render(sd_s16 *dst, int frames)
{
    if (!g_on) return;
    for (int s = 0; s < kCommsMaxPlayers; ++s) {
        RemoteVoice &r = g_rv[s];
        if (!r.playing) { r.cur_gain = r.gain; continue; }
        if (r.gain <= 0.0f && r.cur_gain <= 0.0f) continue;

        /* The gain is ramped across the block rather than stepped at its
           start. A player crossing the near radius, or the launcher moving
           VoiceVolume, changes this number between two frames; applying it as
           a step puts a click in the output every time it moves. */
        const float g0 = r.cur_gain;
        const float g1 = r.gain;
        const float dg = frames > 0 ? (g1 - g0) / (float)frames : 0.0f;

        sd_s16 *o = dst;
        for (int k = 0; k < frames; ++k, o += 2) {
            if (r.avail < 2) { r.playing = 0; break; }   // dry: re-prime
            const int i0 = r.rd;
            const int i1 = (r.rd + 1) % kRingSamples;
            const float f = (float)(r.frac & 0xffff) / 65536.0f;
            const float sample = (float)r.pcm[i0] +
                                 ((float)r.pcm[i1] - (float)r.pcm[i0]) * f;
            const float g = g0 + dg * (float)k;
            const int v = (int)(sample * g);

            int l = o[0] + v;
            int rr = o[1] + v;
            o[0] = (sd_s16)(l < -32768 ? -32768 : (l > 32767 ? 32767 : l));
            o[1] = (sd_s16)(rr < -32768 ? -32768 : (rr > 32767 ? 32767 : rr));

            r.frac += kResampleStep;
            const int step = (int)(r.frac >> 16);
            if (step) {
                r.frac &= 0xffff;
                r.rd = (r.rd + step) % kRingSamples;
                r.avail -= step;
                if (r.avail < 0) r.avail = 0;
            }
        }
        r.cur_gain = g1;
    }
}

// ---------------------------------------------------------------------------
// THE FALLOFF. Returns 0..1 for a distance in world units.
// ---------------------------------------------------------------------------
float falloff(int dist)
{
    if (dist <= g_near) return 1.0f;
    if (dist >= g_far) return 0.0f;
    const double num = log((double)g_far / (double)dist);
    const double den = log((double)g_far / (double)g_near);
    if (den <= 0.0) return 0.0f;
    double g = num / den;
    if (g < 0.0) g = 0.0;
    if (g > 1.0) g = 1.0;
    return (float)g;
}

/* Distance between two slots' bodies, in world units, or -1 when either body
   is not mounted. The three words at +0x5c are Fix12; the shift to whole units
   happens BEFORE the multiply so the squares cannot overflow a 32-bit int on
   an arena-sized separation, and the result is the number the two radii are
   expressed in. */
int slot_distance(int a, int b)
{
    const char *pa = (const char *)data_0209f394[a];
    const char *pb = (const char *)data_0209f394[b];
    if (!pa || !pb) return -1;
    const int dx = (*(const int *)(pa + 0x5c) >> 12) -
                   (*(const int *)(pb + 0x5c) >> 12);
    const int dy = (*(const int *)(pa + 0x60) >> 12) -
                   (*(const int *)(pb + 0x60) >> 12);
    const int dz = (*(const int *)(pa + 0x64) >> 12) -
                   (*(const int *)(pb + 0x64) >> 12);
    const double d2 = (double)dx * dx + (double)dy * dy + (double)dz * dz;
    return (int)sqrt(d2);
}

// ---------------------------------------------------------------------------
// SETTINGS, every tick. Cheap: host_settings_poll does the file work on its
// own schedule and these five accessors are variable reads.
// ---------------------------------------------------------------------------
void refresh_settings()
{
    /* The room's setting outranks the player's: a host who switched voice off
       for the match gets a console with no device open, exactly as if the
       player had never turned it on. */
    const int want = (g_allowed && host_setting_voice_enabled()) ? 1 : 0;
    g_vol = host_setting_voice_volume();
    g_near = host_setting_voice_near_radius();
    g_far = host_setting_voice_far_radius();
    const char *mic = host_setting_voice_mic_device();
    const int mic_index = host_setting_voice_mic_index();

    if (want != g_on) {
        g_on = want;
        /* A player who just turned it back on is asking for another try, so
           the capture layer's failure latch is cleared here rather than left
           to expire. Same on the way off, so the next on is clean. */
        voice::cap_rearm();
        g_open_retry_ms = 0;
        fprintf(stderr, "[voice] VoiceEnabled -> %s\n", g_on ? "ON" : "off");
        if (!g_on) {
            /* OFF MEANS THE DEVICE IS GONE, not muted. This is the whole
               promise the header makes about a player's microphone, and it is
               one call, here. Every jitter buffer is dropped with it so a
               toggle back on does not replay 200 ms of what was said before
               the player turned it off. */
            voice::cap_close();
            for (int i = 0; i < kCommsMaxPlayers; ++i) rv_reset(g_rv[i]);
            gate_reset();
        }
    }
    if (!g_on) return;

    /* The tone generator is a REPLACEMENT for the device, not a source mixed
       beside it: a proof window must not depend on the machine it runs on
       having a microphone, and must not open one. */
    if (g_tone) return;

    /* ---- OPENING THE DEVICE, AND NOT SPINNING ON IT --------------------
       The old test here was "the name changed OR nothing is open", and on a
       box with no recording device -- or one whose microphone another program
       holds -- nothing is ever open, so this ran the whole enumerate-and-open
       path on every frame and printed the failure line sixty times a second.

       Two things fix it and they are different fixes for different events. A
       CHANGED NAME is a player action and gets an immediate retry, with the
       capture layer's per-name latch cleared so the new name is really tried.
       A STILL-CLOSED DEVICE with an unchanged name is a situation, not an
       event, and gets a five second retry: long enough that a machine with no
       microphone costs nothing and says nothing, short enough that plugging
       one in is picked up while the player is still wondering why it is quiet.
       The capture layer stays silent on those retries anyway -- its latch
       prints once per name -- so the backoff is about work, not noise. */
    const int name_changed = strcmp(mic, g_mic) != 0 || mic_index != g_mic_index;
    if (name_changed) {
        strncpy(g_mic, mic, sizeof g_mic - 1);
        g_mic[sizeof g_mic - 1] = '\0';
        g_mic_index = mic_index;
        /* Clears the capture layer's per-name failure latch AND its cached
           auto-pick choice. A player who just edited the device setting is
           owed a fresh scan; the five-second retry below is not a player
           action and deliberately does not come through here. */
        voice::cap_rearm();
    }
    if (voice::cap_is_open() && !name_changed) return;

    const unsigned now = now_ms();
    if (!name_changed && g_open_retry_ms &&
        (unsigned)(now - g_open_retry_ms) < kOpenRetryMs) return;
    g_open_retry_ms = now ? now : 1;
    voice::cap_open(g_mic, g_mic_index);
}

/* 440 Hz at full scale minus 6 dB, in 20 ms frames, phase carried across
   frames so the tone is continuous and a receiver's WAV shows one sine rather
   than a buzz at the frame rate. */
void tone_frame(short *out)
{
    const double w = 2.0 * 3.14159265358979323846 * 440.0 / (double)kCapRate;
    /* The duty cycle runs on the generator's own 20 ms clock, one step per
       frame, so "1000 on, 2000 off" is exactly 50 frames of tone and 100 of
       silence however the host loop is paced. */
    int amp = g_tone_amp;
    if (g_tone_on_ms || g_tone_off_ms) {
        const unsigned cycle = g_tone_on_ms + g_tone_off_ms;
        if (g_tone_t >= g_tone_on_ms) amp = 0;
        g_tone_t += 20;
        if (g_tone_t >= cycle) g_tone_t = 0;
    }
    for (int i = 0; i < kCapFrameSamples; ++i) {
        int v = (int)((double)amp * sin(g_tone_phase));
        if (g_noise_amp) {
            g_noise_seed = g_noise_seed * 1664525u + 1013904223u;
            v += (int)((g_noise_seed >> 16) % (unsigned)(2 * g_noise_amp + 1)) -
                 g_noise_amp;
        }
        out[i] = (short)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
        g_tone_phase += w;
        if (g_tone_phase > 2.0 * 3.14159265358979323846)
            g_tone_phase -= 2.0 * 3.14159265358979323846;
    }
}

/* The gate selftest's dry run: send_pending counts frames and datagrams here
   instead of touching any transport. See gate_selftest. */
int g_gate_dry;
unsigned long long g_dry_frames, g_dry_pkts;

void send_pending()
{
    if (g_gate_dry) {
        g_dry_frames += (unsigned long long)g_pending_n;
        if (g_pending_n) ++g_dry_pkts;
        g_pending_n = 0;
        return;
    }
    const CommsTransport *t = 0;
    if (g_hub) {
        if (!g_in_range) {
            g_tx_nobody += (unsigned long long)g_pending_n;
            g_pending_n = 0;
            return;
        }
    } else {
        t = comms_transport();
        if (!t || !t->send_aux) { g_pending_n = 0; return; }
    }

    unsigned char pkt[kMaxPktBytes];
    memcpy(pkt, &kVoiceTagLE, 4);
    pkt[4] = (unsigned char)kVersion;
    /* Diagnostic only on both carriers: the hub (and the relay) overwrite it
       with the seat they know sent it. */
    pkt[5] = (unsigned char)(g_hub ? comms_hub_my_seat()
                                   : (t->slot ? t->slot() : 0));
    pkt[6] = (unsigned char)g_pending_n;
    pkt[7] = 0;
    pkt[8]  = (unsigned char)(g_tx_seq & 0xff);
    pkt[9]  = (unsigned char)((g_tx_seq >> 8) & 0xff);
    pkt[10] = (unsigned char)((g_tx_seq >> 16) & 0xff);
    pkt[11] = (unsigned char)((g_tx_seq >> 24) & 0xff);
    for (int i = 0; i < g_pending_n; ++i)
        encode_frame(g_pending[i], pkt + kHeaderBytes + i * kBlockBytes);

    const int len = kHeaderBytes + g_pending_n * kBlockBytes;
    if (g_hub) {
        if (comms_hub_send(pkt, len)) ++g_tx_pkts;
    } else if (t->send_aux(pkt, len) == len) {
        ++g_tx_pkts;
    }
    g_tx_seq += (unsigned)g_pending_n;
    g_pending_n = 0;
}

/* Everything the gate remembers goes, so the next spurt starts clean: used
   whenever sending stops for a reason other than quiet (no session, voice
   off, no peer in range). */
void gate_reset()
{
    g_gate_open = 0;
    g_gate_hang = 0;
    g_have_preroll = 0;
    g_pending_n = 0;
}

/* THE GATE, one captured frame at a time. Returns 1 when `frame` is to be
   sent. Opening queues the pre-roll frame first; closing flushes whatever is
   pending. See the banner at g_gate_thr. */
int gate_frame(const short *frame)
{
    const int level = frame_rms(frame);
    g_gate_level = level;
    const int loud = g_gate_thr <= 0 || level >= g_gate_thr;
    if (loud) {
        if (!g_gate_open) {
            g_gate_open = 1;
            ++g_gate_opens;
            if (g_have_preroll) {
                memcpy(g_pending[g_pending_n], g_preroll, sizeof g_preroll);
                ++g_pending_n;
                if (g_pending_n >= kFramesPerPkt) send_pending();
            }
        }
        g_have_preroll = 0;
        g_gate_hang = kGateHangFrames;
        return 1;
    }
    if (g_gate_open && g_gate_hang > 0) {
        --g_gate_hang;
        return 1;
    }
    if (g_gate_open) {
        g_gate_open = 0;
        if (g_pending_n > 0) send_pending();
    }
    memcpy(g_preroll, frame, sizeof g_preroll);
    g_have_preroll = 1;
    ++g_gate_quiet;
    return 0;
}

/* THE GATE, MEASURED ON THE SHIPPED FUNCTION. SM64DS_VOICE_GATE_SELFTEST=1.
 *
 * A scripted talker pushed through the real gate_frame and send_pending, no
 * transport, no device: 25 frames of quiet-room hiss (peak 40, far under the
 * threshold), a 4-frame word at amplitude 8000, 25 frames of hiss, a single
 * loud frame, 40 frames of hiss. What must come out:
 *   first spurt   1 pre-roll + 4 loud + 10 hangover = 15 frames
 *   second spurt  1 pre-roll + 1 loud + 10 hangover = 12 frames
 *   everything else (the hiss outside those windows) = 0 frames
 * so 27 frames in 2 spurts and 14 datagrams: the first spurt's odd last
 * frame goes out alone when the gate closes rather than being held. Prints one
 * line with PASS or FAIL. The live gate state is saved and restored around it,
 * so it costs a running session nothing. */
void gate_selftest()
{
    short frame[kCapFrameSamples];
    const int save_thr = g_gate_thr;
    const unsigned save_seed = g_noise_seed;
    const unsigned long long save_opens = g_gate_opens, save_quiet = g_gate_quiet;
    g_gate_thr = kGateDefault;
    gate_reset();
    g_gate_dry = 1;
    g_dry_frames = g_dry_pkts = 0;
    g_gate_opens = 0;
    const int script[5][2] = { {25, 0}, {4, 8000}, {25, 0}, {1, 8000}, {40, 0} };
    double ph = 0.0;
    const double w = 2.0 * 3.14159265358979323846 * 440.0 / (double)kCapRate;
    int total = 0;
    for (int seg = 0; seg < 5; ++seg) {
        for (int f = 0; f < script[seg][0]; ++f, ++total) {
            for (int i = 0; i < kCapFrameSamples; ++i) {
                g_noise_seed = g_noise_seed * 1664525u + 1013904223u;
                const int hiss = (int)((g_noise_seed >> 16) % 81u) - 40;
                frame[i] = (short)((double)script[seg][1] * sin(ph) + hiss);
                ph += w;
            }
            if (gate_frame(frame)) {
                memcpy(g_pending[g_pending_n], frame, sizeof frame);
                ++g_pending_n;
                if (g_pending_n >= kFramesPerPkt) send_pending();
            }
        }
    }
    if (g_pending_n) send_pending();
    const unsigned long long frames = g_dry_frames, pkts = g_dry_pkts,
                             opens = g_gate_opens;
    const int ok = frames == 27 && opens == 2 && pkts == 14;
    fprintf(stderr, "[voice] gate selftest: %d frames scripted, %llu sent in "
                    "%llu datagrams over %llu spurts (want 27 in 14 over 2), "
                    "threshold %d, hangover %d frames: %s\n",
            total, frames, pkts, opens, kGateDefault, kGateHangFrames,
            ok ? "PASS" : "FAIL");
    g_gate_dry = 0;
    gate_reset();
    g_gate_thr = save_thr;
    g_noise_seed = save_seed;
    g_gate_opens = save_opens;
    g_gate_quiet = save_quiet;
}

/* `send` false means the device is drained and the audio thrown away.
   THAT IS NOT THE SAME AS NOT DRAINING IT. A waveIn ring nobody empties fills
   up in 160 ms and then stops; the moment a session forms, the first eight
   reads hand back 160 ms of what the room sounded like BEFORE the match
   started, all at once. Draining while there is nobody to send to costs one
   memcpy a frame and makes the first thing a peer hears be the present. */
void capture_pump(int send)
{
    short frame[kCapFrameSamples];
    /* At 60 Hz against 20 ms frames there are one or two waiting; the bound is
       the ring depth, and it is a bound rather than a while(1) so a device that
       somehow always reports ready cannot hold the frame loop. */
    for (int n = 0; n < 8; ++n) {
        if (g_tone) {
            /* CLOCKED OFF THE WALL, NOT OFF THE FRAME, and this is a real bug
               avoided rather than tidiness. A 60 Hz loop emitting one 20 ms
               frame per tick produces 60 frames a second where the receiver
               consumes 50, so the jitter ring fills, drops its oldest frame,
               and puts a glitch in the proof's own WAV once a second. Pacing
               it at 20 ms of wall clock makes the generated source behave
               exactly like the device it stands in for. */
            const unsigned t = now_ms();
            if (!g_tone_ms) g_tone_ms = t;
            /* AND IT OVERRUNS LIKE ONE (run mp1 lane VOICE1). A waveIn ring
               is eight 20 ms buffers deep: after a stall longer than that the
               device has lost the older audio, it does not hand it over late.
               Without this the generator kept every millisecond of a stall as
               backlog and paid it out at up to 8 frames a tick once the loop
               sped up, which on a crowded 16-console proof ran a talker at
               about 37 datagrams a second for a while and tripped the relay's
               30 a second voice allowance, a burst no microphone can make. */
            if ((unsigned)(t - g_tone_ms) > 180u) g_tone_ms = t - 160u;
            if ((unsigned)(t - g_tone_ms) < 20u) break;
            g_tone_ms += 20;
            tone_frame(frame);
        } else if (!voice::cap_read_frame(frame)) {
            break;
        }
        ++g_cap_frames;
        if (!send) { gate_reset(); continue; }
        if (!gate_frame(frame)) continue;
        memcpy(g_pending[g_pending_n], frame, sizeof frame);
        ++g_pending_n;
        if (g_pending_n >= kFramesPerPkt) send_pending();
    }
}

void recv_pump()
{
    unsigned char pkt[512];
    int from = -1;
    int n;
    for (;;) {
        from = -1;
        n = g_hub ? comms_hub_recv(kHubVoice, pkt, (int)sizeof pkt, &from)
                  : comms_recv_voice(pkt, (int)sizeof pkt, &from);
        if (n <= 0) break;
        if (from < 0 || from >= kCommsMaxPlayers) { ++g_rx_bad; continue; }
        if (n < kHeaderBytes + kBlockBytes) { ++g_rx_bad; continue; }
        if (pkt[4] != kVersion) { ++g_rx_bad; continue; }
        const int nf = pkt[6];
        if (nf < 1 || nf > kFramesPerPkt) { ++g_rx_bad; continue; }
        if (n < kHeaderBytes + nf * kBlockBytes) { ++g_rx_bad; continue; }

        const unsigned seq = (unsigned)pkt[8] | ((unsigned)pkt[9] << 8) |
                             ((unsigned)pkt[10] << 16) |
                             ((unsigned)pkt[11] << 24);
        RemoteVoice &r = g_rv[from];
        /* UNORDERED CHANNEL, so a datagram that is not newer than the last one
           accepted is a duplicate or a reordering, and either way playing it
           would put audio back in the buffer that has already been heard.
           Signed difference so the counter's wrap is not a cliff. */
        if (r.have_seq && (int)(seq - r.last_seq) <= 0) { ++g_rx_dup; continue; }
        r.have_seq = 1;
        r.last_seq = seq + (unsigned)(nf - 1);
        r.last_ms = now_ms();
        ++g_rx_pkts;
        ++g_rx_from[from];

        short frame[kCapFrameSamples];
        for (int i = 0; i < nf; ++i) {
            decode_frame(pkt + kHeaderBytes + i * kBlockBytes, frame);
            rv_push(r, frame);
        }
    }
}

/* Adventure: my body against `seat`'s newest ghost frame, in world units, or
   -1 when the seat is absent, has sent no ghost frame, is in another level,
   or my own body is not mounted. Same Fix12 shift-before-multiply rule as
   slot_distance. */
int hub_distance(int seat)
{
    HubSeatInfo si;
    if (!comms_hub_seat_info(seat, &si) || !si.present || !si.has_ghost)
        return -1;
    const unsigned short mine = (unsigned short)(int)data_0209f2f8;
    if ((unsigned short)si.level_id != mine) return -1;
    const char *me = (const char *)data_0209f394[data_0209f250];
    if (!me) return -1;
    const int dx = (*(const int *)(me + 0x5c) >> 12) - (si.x >> 12);
    const int dy = (*(const int *)(me + 0x60) >> 12) - (si.y >> 12);
    const int dz = (*(const int *)(me + 0x64) >> 12) - (si.z >> 12);
    const double d2 = (double)dx * dx + (double)dy * dy + (double)dz * dz;
    return (int)sqrt(d2);
}

/* Adventure: can anybody hear me this tick? */
void hub_range()
{
    const unsigned mask = comms_hub_seat_mask();
    int n = 0;
    for (int s = 0; s < kHubMaxSeats; ++s) {
        if (!(mask & (1u << s))) continue;
        const int d = hub_distance(s);
        if (d >= 0 && d < g_far) ++n;
    }
    g_in_range_n = n;
    g_in_range = n > 0;
}

/* Every remote slot's gain for the frame. A slot with no body, no recent
   audio, or a distance past the far radius goes to zero; the mixer ramps to
   it rather than cutting. */
void update_gains()
{
    const int me = g_hub ? comms_hub_my_seat() : (int)data_0209f250;
    const float vol = (float)g_vol / 100.0f;
    const unsigned t = now_ms();
    for (int s = 0; s < kCommsMaxPlayers; ++s) {
        RemoteVoice &r = g_rv[s];
        if (s == me) { r.gain = 0.0f; r.dist = -1; continue; }
        if (!r.playing && r.avail == 0) { r.gain = 0.0f; r.dist = -1; continue; }
        /* Nothing for 400 ms is a peer who stopped talking or dropped out.
           Dropping the buffer rather than letting it drain keeps a rejoin
           from replaying stale audio. */
        if (r.last_ms && (unsigned)(t - r.last_ms) > 400u) {
            rv_reset(r);
            continue;
        }
        const int d = g_hub ? hub_distance(s) : slot_distance(me, s);
        r.dist = d;
        r.gain = d < 0 ? 0.0f : falloff(d) * vol;
    }
}

void report_if_due()
{
    if (!g_report) return;
    const unsigned t = now_ms();
    if (g_report_ms && (unsigned)(t - g_report_ms) < 1000u) return;
    g_report_ms = t;
    voice_report("1s");
}

}  // namespace

// ---------------------------------------------------------------------------
void voice_tick()
{
    if (!g_inited) {
        g_inited = 1;
        g_tone = env_on("SM64DS_VOICE_TEST_TONE");
        g_report = env_on("SM64DS_VOICE_REPORT");
        for (int i = 0; i < kCommsMaxPlayers; ++i) rv_reset(g_rv[i]);
        if (env_on("SM64DS_VOICE_DEVICES")) dump_devices();
        if (env_on("SM64DS_VOICE_CODEC_SELFTEST")) codec_selftest();
        if (env_on("SM64DS_VOICE_GATE_SELFTEST")) gate_selftest();
        /* Owner-facing mic proof (lane MIC PROBE): the settings panel that
           would exercise the microphone is hidden until the capture fallback
           lands, so this is the only way to test one from a shipped bundle.
           Blocks for a few seconds; off by default, no other behaviour
           changes when unset. */
        if (env_on("SM64DS_VOICE_MIC_PROBE")) voice::cap_probe();
        g_gate_thr = env_int("SM64DS_VOICE_GATE", kGateDefault, 0, 32767);
        g_test_ghost = env_on("SM64DS_VOICE_TEST_GHOST");
        g_tone_amp = env_int("SM64DS_VOICE_TEST_TONE_AMP", 16000, 0, 32767);
        g_noise_amp = env_int("SM64DS_VOICE_TEST_NOISE", 0, 0, 32767);
        {
            const char *d = getenv("SM64DS_VOICE_TEST_TONE_DUTY");
            unsigned on = 0, off = 0;
            if (d && sscanf(d, "%u,%u", &on, &off) == 2 && on + off > 0 &&
                on <= 600000u && off <= 600000u) {
                g_tone_on_ms = on;
                g_tone_off_ms = off;
            }
        }
        {
            const char *a = getenv("SM64DS_VOICE_ALLOWED");
            g_allowed = !(a && strcmp(a, "0") == 0);
            if (!g_allowed)
                fprintf(stderr, "[voice] SM64DS_VOICE_ALLOWED=0: the host "
                                "switched voice off for this match; no "
                                "recording device will be opened and nothing "
                                "is sent or played\n");
        }
        if (g_gate_thr != kGateDefault)
            fprintf(stderr, "[voice] mic gate threshold %d (RMS)%s\n",
                    g_gate_thr, g_gate_thr ? "" : ": gate OFF, open mic");
        if (g_tone)
            fprintf(stderr, "[voice] SM64DS_VOICE_TEST_TONE=1: a generated "
                            "440 Hz tone replaces the microphone and NO "
                            "recording device is opened\n");
    }

    refresh_settings();
    if (!g_on) return;

    /* The mixer hook is registered on the first tick that finds voice on and
       never taken back down: the render side reads g_on itself, so a player
       toggling the key off silences it without the hook having to be
       unregistered mid-render from another point in the frame. */
    if (!g_hooked) { g_hooked = 1; sd_mix_set_aux_render(voice_mix_render); }

    /* NO SESSION, NOTHING ON THE WIRE. The device stays open -- it is the
       player's choice, not the session's, and closing and reopening it every
       time a match ends would be audible as a click on the next one -- and it
       is still DRAINED, for the reason capture_pump's own note gives. What
       stops is sending: a player sitting in the menus with voice on is not
       broadcasting into a socket with nobody on it. */
    /* WHICH CARRIER. The hub is installed only for adventure online and
       never together with the VS carrier (hal/comms_hub.h), so this is a
       choice, not a merge. Hub "live with anybody present" is the adventure
       counterpart of "connected with more than one player". */
    g_hub = comms_hub_active() ? 1 : 0;
    int live;
    if (g_hub) {
        live = comms_hub_state() == kHubLive && comms_hub_seat_mask() != 0;
    } else {
        const CommsReadout rr = comms_readout();
        live = rr.connected && rr.players > 1;
    }
    if (!live) {
        gate_reset();
        for (int i = 0; i < kCommsMaxPlayers; ++i)
            if (g_rv[i].playing || g_rv[i].avail) rv_reset(g_rv[i]);
        capture_pump(0);
        return;
    }

    if (g_hub) hub_range();
    capture_pump(1);
    test_ghost_send();
    recv_pump();
    update_gains();
    report_if_due();
}

int voice_talking()
{
    return (g_on && g_gate_open) ? 1 : 0;
}

void voice_report(const char *tag)
{
    /* Sized for sixteen seats: the fixed fields, sixteen rx counts and up to
       fifteen per-slot segments. 512 truncated the line past about ten. */
    char line[2048];
    int k = snprintf(line, sizeof line,
                     "[voice] %s: on=%d tone=%d dev=%d cap=%llu tx=%llu "
                     "rx=%llu bad=%llu dup=%llu vol=%d near=%d far=%d "
                     "allowed=%d gate=%d open=%d lvl=%d opens=%llu quiet=%llu "
                     "talking=%d hub=%d inrange=%d nobody=%llu rxs=",
                     tag ? tag : "-", g_on, g_tone, voice::cap_is_open(),
                     g_cap_frames, g_tx_pkts, g_rx_pkts, g_rx_bad, g_rx_dup,
                     g_vol, g_near, g_far, g_allowed, g_gate_thr, g_gate_open,
                     g_gate_level, g_gate_opens, g_gate_quiet, voice_talking(),
                     g_hub, g_in_range_n, g_tx_nobody);
    for (int s = 0; s < kCommsMaxPlayers && k > 0 && k < (int)sizeof line; ++s)
        k += snprintf(line + k, sizeof line - (size_t)k, "%s%llu",
                      s ? "," : "", g_rx_from[s]);
    for (int s = 0; s < kCommsMaxPlayers && k > 0 && k < (int)sizeof line; ++s) {
        const RemoteVoice &r = g_rv[s];
        if (!r.playing && r.avail == 0 && r.dist < 0) continue;
        k += snprintf(line + k, sizeof line - (size_t)k,
                      " | s%d d=%d g=%.3f buf=%d", s, r.dist, r.gain,
                      r.avail / kCapFrameSamples);
    }
    fprintf(stderr, "%s\n", line);
}

}  // namespace port
