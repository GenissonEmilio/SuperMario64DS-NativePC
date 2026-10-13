// Plain .wav / .ogg files for sound mods (lane AUDIOHQ1).
//
// Portable: no windows, no game state, so the same code is built into the game
// and into the Linux test harness. It does three jobs:
//
//   1. sd_audio_probe     read a file's header and loop tags, decide if it is usable
//   2. sd_audio_decode_mono   a whole file to one mono PCM16 clip (a replaced WAVE)
//   3. SdSource           a file read forward in small blocks as stereo PCM16, with
//                         loop handling (a replaced MUSIC TRACK). An .ogg stays
//                         compressed in memory and is decoded as it is played, so a
//                         three minute track costs its file size, not 35 MB.
//
// Loop points: WAV `smpl` chunk (first loop; its end is inclusive in the file and
// is made exclusive here), OGG comments LOOPSTART / LOOPLENGTH / LOOPEND
// (LOOPEND is the first frame NOT played, like LOOPSTART + LOOPLENGTH). All in
// sample frames of the file's own rate.
#ifndef SM64DS_PORT_SD_AUDIO_H
#define SM64DS_PORT_SD_AUDIO_H

#include "sdat.h"

#include <string>
#include <vector>

struct SdAudioInfo {
    int isOgg;
    int channels;                   // as stored in the file
    int rate;                       // Hz
    unsigned long long frames;      // length in sample frames
    int hasLoop;                    // loopStart / loopEnd came from the file's tags
    unsigned long long loopStart;   // frames
    unsigned long long loopEnd;     // frames, exclusive
    std::string loopNote;           // one line about the loop tags ("" = none or fine)
};

// Is this file (n bytes at d) a usable .wav or .ogg? On false, `why` says why in
// a few plain words. Does not decode the audio.
bool sd_audio_probe(const sd_u8 *d, size_t n, bool isOgg, SdAudioInfo &info,
                    std::string &why);

// The first bytes decide, not the extension: "RIFF" is a WAV, "OggS" an OGG.
// Returns 0 = WAV, 1 = OGG, -1 = neither.
int sd_audio_sniff(const sd_u8 *d, size_t n);

struct SdClip {
    std::vector<sd_s16> mono;       // mono mix of the file
    int rate;                       // Hz, as decoded (not yet range-limited)
    int hasLoop;
    unsigned long long loopStart, loopEnd;
    std::string loopNote;
};

// Decode the whole file to mono PCM16. maxFrames caps the length.
bool sd_audio_decode_mono(const sd_u8 *d, size_t n, bool isOgg,
                          unsigned long long maxFrames, SdClip &out,
                          std::string &why);

// Resample a mono clip to another rate (4-point Hermite). Loop points are
// scaled with it. Used to bring a clip's rate into the SWAV header's range.
void sd_audio_resample_mono(SdClip &c, int newRate);

// ---- a replaced wave, as an SWAV record and a rebuilt SWAR ----------------

// Turn a decoded clip into one SWAV record (12 byte header + PCM16 data, the
// format the host's decode_swav reads). The SWAV header's rate is 16 bits, so
// a clip above 65535 Hz is resampled to 48000 and one below 4000 Hz up to the
// next whole multiple of its rate that reaches 4000. The header's loop start is
// counted in 4 byte words, so a loop start is moved to an even sample (one
// leading sample is dropped when it is odd) and the length is made even.
// Loop: the clip's own tags; with none, the wave loops (the whole clip) only if
// the original wave looped (origLoops). `note` gets one line about anything
// notable (a rate change, a loop decision); `why` explains a refusal.
bool sd_swav_from_clip(SdClip clip, bool origLoops, std::vector<sd_u8> &rec,
                       std::string &note, std::string &why);

struct SdSwavRepl {
    unsigned index;                 // wave number in the archive
    std::vector<sd_u8> rec;         // from sd_swav_from_clip
};

// Rebuild the SWAR image (base, n bytes) with the listed waves swapped for the
// given records. Every other wave is copied byte for byte, the wave count and
// order are unchanged, and the sizes in the headers are fixed up. False (with
// `why`) if the base is not a SWAR or an index is outside it.
bool sd_swar_rebuild(const sd_u8 *base, size_t n,
                     const std::vector<SdSwavRepl> &repl,
                     std::vector<sd_u8> &out, std::string &why);

// A file played forward as stereo PCM16 at its own rate.
class SdSource {
public:
    SdAudioInfo info;
    bool loop;                      // false: play to the end of the file once
    unsigned long long wraps;       // how many times it has jumped back to the loop start

    SdSource() : loop(true), wraps(0), pos_(0) {}
    virtual ~SdSource() {}

    // Up to `frames` stereo frames. Returns how many it wrote; 0 means the
    // source is finished (it never returns 0 while it can still loop).
    int read(sd_s16 *stereo, int frames);

protected:
    virtual int rawRead(sd_s16 *stereo, int frames) = 0;   // sequential
    virtual bool seek(unsigned long long frame) = 0;
    unsigned long long pos_;        // frame the next rawRead returns
};

// Open d (n bytes, which must outlive the source) for streaming. Null on a bad
// file, with `why` set.
SdSource *sd_source_open(const sd_u8 *d, size_t n, bool isOgg, std::string &why);

// Hermite (Catmull-Rom) interpolation of y0..y1 at fraction f, given the
// neighbours ym1 and y2. Shared by the mixer.
static inline float sd_hermite(float ym1, float y0, float y1, float y2, float f)
{
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}

#endif
