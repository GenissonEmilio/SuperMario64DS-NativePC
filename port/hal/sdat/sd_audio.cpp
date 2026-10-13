// Plain .wav / .ogg for sound mods. See sd_audio.h.
#include "sd_audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../../third_party/stb_vorbis/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

namespace {

inline unsigned rl16(const sd_u8 *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
inline unsigned rl32(const sd_u8 *p)
{ return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }

// ---- WAV ------------------------------------------------------------------

struct Wav {
    int tag;                // 1 PCM, 3 float
    int channels, rate, bits;
    size_t dataOff, dataLen;
    int hasSmpl;
    unsigned long long loopStart, loopEnd;      // loopEnd exclusive
    int frameBytes;
    unsigned long long frames;
};

bool wav_parse(const sd_u8 *d, size_t n, Wav &w, std::string &why)
{
    memset(&w, 0, sizeof w);
    if (n < 44 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
        why = "not a WAV file (no RIFF/WAVE header)";
        return false;
    }
    bool haveFmt = false, haveData = false;
    size_t p = 12;
    unsigned long long smplStart = 0, smplEnd = 0;
    int smpl = 0;
    while (p + 8 <= n) {
        const sd_u8 *ck = d + p;
        size_t len = rl32(ck + 4);
        const size_t body = p + 8;
        if (!memcmp(ck, "fmt ", 4)) {
            if (len < 16 || body + 16 > n) { why = "WAV fmt chunk is cut short"; return false; }
            w.tag = (int)rl16(d + body);
            w.channels = (int)rl16(d + body + 2);
            w.rate = (int)rl32(d + body + 4);
            w.bits = (int)rl16(d + body + 14);
            if (w.tag == 0xfffe) {
                if (len < 26 || body + 26 > n) { why = "WAV extensible fmt chunk is cut short"; return false; }
                w.tag = (int)rl16(d + body + 24);   // first two bytes of the sub-format GUID
            }
            haveFmt = true;
        } else if (!memcmp(ck, "data", 4)) {
            w.dataOff = body;
            // a streamed WAV may state 0 or 0xFFFFFFFF; take what the file holds
            if (len > n - body || len == 0) len = n - body;
            w.dataLen = len;
            haveData = true;
        } else if (!memcmp(ck, "smpl", 4) && len >= 36 + 24 && body + 36 + 24 <= n) {
            unsigned loops = rl32(d + body + 28);
            if (loops >= 1) {
                const sd_u8 *lp = d + body + 36;
                smplStart = rl32(lp + 8);
                smplEnd = (unsigned long long)rl32(lp + 12) + 1;    // inclusive in the file
                smpl = 1;
            }
        }
        if (len > n - body) break;
        p = body + len + (len & 1);
    }
    if (!haveFmt) { why = "WAV has no fmt chunk"; return false; }
    if (!haveData) { why = "WAV has no data chunk"; return false; }
    const bool intOk = w.tag == 1 && (w.bits == 8 || w.bits == 16 || w.bits == 24 || w.bits == 32);
    const bool fltOk = w.tag == 3 && (w.bits == 32 || w.bits == 64);
    if (!intOk && !fltOk) {
        char b[96];
        snprintf(b, sizeof b, "WAV format %d with %d bits is not supported "
                 "(use plain PCM 8/16/24/32 bit or float)", w.tag, w.bits);
        why = b;
        return false;
    }
    if (w.channels < 1 || w.channels > 8) { why = "WAV channel count is not 1 to 8"; return false; }
    if (w.rate < 1000 || w.rate > 384000) { why = "WAV sample rate is outside 1000 to 384000 Hz"; return false; }
    w.frameBytes = w.channels * (w.bits / 8);
    w.frames = w.dataLen / (size_t)w.frameBytes;
    if (w.frames == 0) { why = "WAV holds no audio"; return false; }
    w.hasSmpl = smpl;
    w.loopStart = smplStart;
    w.loopEnd = smplEnd;
    return true;
}

// One WAV frame -> two floats-free s16 values (left, right).
inline void wav_frame(const Wav &w, const sd_u8 *f, int &l, int &r)
{
    int v[2];
    const int step = w.bits / 8;
    for (int c = 0; c < 2; c++) {
        const sd_u8 *s = f + (c < w.channels ? c : 0) * step;
        int x = 0;
        if (w.tag == 1) {
            if (w.bits == 8) x = ((int)s[0] - 128) << 8;
            else if (w.bits == 16) x = (sd_s16)rl16(s);
            else if (w.bits == 24) x = (int)(sd_s16)rl16(s + 1);
            else x = (int)(sd_s16)rl16(s + 2);
        } else {
            double fv;
            if (w.bits == 32) { float t; memcpy(&t, s, 4); fv = t; }
            else { double t; memcpy(&t, s, 8); fv = t; }
            if (fv > 1.0) fv = 1.0;
            if (fv < -1.0) fv = -1.0;
            x = (int)floor(fv * 32767.0 + 0.5);
        }
        v[c] = x;
    }
    l = v[0];
    r = v[1];
}

void note_loop(SdAudioInfo &i, unsigned long long frames, unsigned long long ls,
               unsigned long long le, bool tagged)
{
    i.hasLoop = 0;
    i.loopStart = 0;
    i.loopEnd = frames;
    if (!tagged) return;
    if (le > frames) le = frames;
    if (ls >= le || le - ls < 2) {
        i.loopNote = "loop tags do not describe a loop inside the file, ignored";
        return;
    }
    i.hasLoop = 1;
    i.loopStart = ls;
    i.loopEnd = le;
}

// ---- OGG ------------------------------------------------------------------

bool ieq_prefix(const char *s, const char *key, const char **val)
{
    size_t k = strlen(key);
    for (size_t i = 0; i < k; i++) {
        char c = s[i];
        if (!c) return false;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c != key[i]) return false;
    }
    if (s[k] != '=') return false;
    *val = s + k + 1;
    return true;
}

/* Every Ogg page carries a CRC-32 (polynomial 0x04c11db7, no reflection, the
   field itself counted as zero). stb_vorbis does not look at it, and a file
   with damaged bytes in its setup headers can make it free garbage while it
   gives up. So the pages are checked first: a damaged or cut-off file is
   refused here with a plain reason and never reaches the decoder. Trailing
   bytes that do not start another page are ignored. */
bool ogg_pages_ok(const sd_u8 *d, size_t n, std::string &why)
{
    static unsigned tab[256];
    static bool built;
    if (!built) {
        for (unsigned i = 0; i < 256; i++) {
            unsigned r = i << 24;
            for (int k = 0; k < 8; k++) r = (r & 0x80000000u) ? (r << 1) ^ 0x04c11db7u : (r << 1);
            tab[i] = r;
        }
        built = true;
    }
    size_t p = 0;
    unsigned pages = 0;
    while (p < n) {
        if (n - p < 4 || memcmp(d + p, "OggS", 4) != 0) {
            if (pages >= 2) return true;                     // junk after the last page
            why = pages ? "damaged Ogg file (a page header is missing)" : "not an Ogg file";
            return false;
        }
        if (n - p < 27 || d[p + 4] != 0) { why = "damaged or cut-off Ogg file (page header)"; return false; }
        const size_t segs = d[p + 26];
        if (n - p < 27 + segs) { why = "cut-off Ogg file (page table)"; return false; }
        size_t body = 0;
        for (size_t k = 0; k < segs; k++) body += d[p + 27 + k];
        const size_t len = 27 + segs + body;
        if (len > n - p) { why = "cut-off Ogg file (the last page is incomplete)"; return false; }
        unsigned crc = 0;
        for (size_t k = 0; k < len; k++) {
            const unsigned b = (k >= 22 && k < 26) ? 0u : d[p + k];
            crc = (crc << 8) ^ tab[((crc >> 24) ^ b) & 0xff];
        }
        if (crc != rl32(d + p + 22)) {
            char b[96];
            snprintf(b, sizeof b, "damaged Ogg file (page %u fails its checksum)", pages);
            why = b;
            return false;
        }
        p += len;
        pages++;
    }
    return true;
}

bool ogg_open_info(const sd_u8 *d, size_t n, stb_vorbis *&v, SdAudioInfo &i,
                   std::string &why)
{
    if (!ogg_pages_ok(d, n, why)) return false;
    int err = 0;
    v = stb_vorbis_open_memory(d, (int)(n > 0x7fffffff ? 0x7fffffff : n), &err, NULL);
    if (!v) {
        char b[64];
        snprintf(b, sizeof b, "not a readable OGG Vorbis file (error %d)", err);
        why = b;
        return false;
    }
    stb_vorbis_info vi = stb_vorbis_get_info(v);
    i.isOgg = 1;
    i.channels = vi.channels;
    i.rate = (int)vi.sample_rate;
    if (vi.channels < 1 || vi.channels > 8) {
        stb_vorbis_close(v); v = 0;
        why = "OGG channel count is not 1 to 8";
        return false;
    }
    if (i.rate < 1000 || i.rate > 384000) {
        stb_vorbis_close(v); v = 0;
        why = "OGG sample rate is outside 1000 to 384000 Hz";
        return false;
    }
    unsigned len = stb_vorbis_stream_length_in_samples(v);
    i.frames = len;
    if (len == 0) {
        stb_vorbis_close(v); v = 0;
        why = "OGG holds no audio";
        return false;
    }
    stb_vorbis_comment vc = stb_vorbis_get_comment(v);
    long long ls = -1, ll = -1, le = -1;
    for (int k = 0; k < vc.comment_list_length; k++) {
        const char *val = 0;
        const char *c = vc.comment_list[k];
        if (!c) continue;
        if (ieq_prefix(c, "LOOPSTART", &val)) ls = atoll(val);
        else if (ieq_prefix(c, "LOOPLENGTH", &val)) ll = atoll(val);
        else if (ieq_prefix(c, "LOOPEND", &val)) le = atoll(val);
    }
    bool tagged = ls >= 0 || ll >= 0 || le >= 0;
    unsigned long long s0 = ls >= 0 ? (unsigned long long)ls : 0;
    unsigned long long e0 = len;
    if (le >= 0) e0 = (unsigned long long)le;
    else if (ll >= 0) e0 = s0 + (unsigned long long)ll;
    note_loop(i, len, s0, e0, tagged);
    return true;
}

class WavSource : public SdSource {
public:
    WavSource(const sd_u8 *d, const Wav &w) : d_(d), w_(w), at_(0) {}
protected:
    int rawRead(sd_s16 *out, int frames) override
    {
        unsigned long long left = w_.frames - at_;
        if ((unsigned long long)frames > left) frames = (int)left;
        const sd_u8 *p = d_ + w_.dataOff + at_ * (size_t)w_.frameBytes;
        for (int k = 0; k < frames; k++, p += w_.frameBytes) {
            int l, r;
            wav_frame(w_, p, l, r);
            out[k * 2] = (sd_s16)l;
            out[k * 2 + 1] = (sd_s16)r;
        }
        at_ += (unsigned long long)frames;
        return frames;
    }
    bool seek(unsigned long long f) override
    {
        if (f >= w_.frames) return false;
        at_ = f;
        return true;
    }
private:
    const sd_u8 *d_;
    Wav w_;
    unsigned long long at_;
};

class OggSource : public SdSource {
public:
    explicit OggSource(stb_vorbis *v, int ch) : v_(v), ch_(ch) {}
    ~OggSource() override { if (v_) stb_vorbis_close(v_); }
protected:
    int rawRead(sd_s16 *out, int frames) override
    {
        if (ch_ == 1) {
            sd_s16 tmp[1024];
            int total = 0;
            while (total < frames) {
                int want = frames - total > 1024 ? 1024 : frames - total;
                int got = stb_vorbis_get_samples_short_interleaved(v_, 1, tmp, want);
                if (got <= 0) break;
                for (int k = 0; k < got; k++) {
                    out[(total + k) * 2] = tmp[k];
                    out[(total + k) * 2 + 1] = tmp[k];
                }
                total += got;
            }
            return total;
        }
        int total = 0;
        while (total < frames) {
            int got = stb_vorbis_get_samples_short_interleaved(
                v_, 2, out + total * 2, (frames - total) * 2);
            if (got <= 0) break;
            total += got;
        }
        return total;
    }
    bool seek(unsigned long long f) override
    {
        if (f == 0) return stb_vorbis_seek_start(v_) != 0;
        return stb_vorbis_seek(v_, (unsigned)f) != 0;
    }
private:
    stb_vorbis *v_;
    int ch_;
};

}  // namespace

int sd_audio_sniff(const sd_u8 *d, size_t n)
{
    if (n >= 12 && !memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WAVE", 4)) return 0;
    if (n >= 4 && !memcmp(d, "OggS", 4)) return 1;
    return -1;
}

bool sd_audio_probe(const sd_u8 *d, size_t n, bool isOgg, SdAudioInfo &info,
                    std::string &why)
{
    info = SdAudioInfo();
    info.isOgg = isOgg ? 1 : 0;
    info.loopEnd = 0;
    if (isOgg) {
        stb_vorbis *v = 0;
        if (!ogg_open_info(d, n, v, info, why)) return false;
        stb_vorbis_close(v);
        return true;
    }
    Wav w;
    if (!wav_parse(d, n, w, why)) return false;
    info.channels = w.channels;
    info.rate = w.rate;
    info.frames = w.frames;
    note_loop(info, w.frames, w.loopStart, w.loopEnd, w.hasSmpl != 0);
    return true;
}

bool sd_audio_decode_mono(const sd_u8 *d, size_t n, bool isOgg,
                          unsigned long long maxFrames, SdClip &out,
                          std::string &why)
{
    SdAudioInfo info;
    if (!sd_audio_probe(d, n, isOgg, info, why)) return false;
    if (info.frames > maxFrames) {
        char b[96];
        snprintf(b, sizeof b, "%llu sample frames is over the %llu frame limit "
                 "for one wave", info.frames, maxFrames);
        why = b;
        return false;
    }
    out.mono.clear();
    out.rate = info.rate;
    out.hasLoop = info.hasLoop;
    out.loopStart = info.loopStart;
    out.loopEnd = info.loopEnd;
    out.loopNote = info.loopNote;
    std::string w2;
    SdSource *s = sd_source_open(d, n, isOgg, w2);
    if (!s) { why = w2; return false; }
    s->loop = false;
    out.mono.reserve((size_t)info.frames);
    sd_s16 buf[2048 * 2];
    for (;;) {
        int got = s->read(buf, 2048);
        if (got <= 0) break;
        for (int k = 0; k < got; k++)
            out.mono.push_back((sd_s16)(((int)buf[k * 2] + (int)buf[k * 2 + 1]) / 2));
        if (out.mono.size() > info.frames + 4096) break;    // never trust a header's length
    }
    delete s;
    if (out.mono.size() < 2) { why = "the file decoded to no audio"; return false; }
    if (out.loopEnd > out.mono.size()) out.loopEnd = out.mono.size();
    if (out.hasLoop && out.loopStart + 2 > out.loopEnd) out.hasLoop = 0;
    return true;
}

void sd_audio_resample_mono(SdClip &c, int newRate)
{
    if (newRate <= 0 || newRate == c.rate || c.mono.size() < 2) { c.rate = newRate > 0 ? newRate : c.rate; return; }
    const double ratio = (double)c.rate / (double)newRate;      // source frames per new frame
    const size_t inN = c.mono.size();
    size_t outN = (size_t)floor((double)inN / ratio);
    if (outN < 2) outN = 2;
    std::vector<sd_s16> o(outN);
    auto at = [&](long long i) -> float {
        if (i < 0) i = 0;
        if (i >= (long long)inN) i = (long long)inN - 1;
        return (float)c.mono[(size_t)i];
    };
    for (size_t k = 0; k < outN; k++) {
        double pos = (double)k * ratio;
        long long i = (long long)pos;
        float f = (float)(pos - (double)i);
        float v = sd_hermite(at(i - 1), at(i), at(i + 1), at(i + 2), f);
        int q = (int)floor(v + 0.5f);
        o[k] = (sd_s16)(q < -32768 ? -32768 : (q > 32767 ? 32767 : q));
    }
    c.mono.swap(o);
    c.loopStart = (unsigned long long)floor((double)c.loopStart / ratio + 0.5);
    c.loopEnd = (unsigned long long)floor((double)c.loopEnd / ratio + 0.5);
    if (c.loopEnd > outN) c.loopEnd = outN;
    if (c.hasLoop && c.loopStart + 2 > c.loopEnd) c.hasLoop = 0;
    c.rate = newRate;
}

int SdSource::read(sd_s16 *out, int frames)
{
    int got = 0;
    int stalls = 0;
    while (got < frames) {
        const unsigned long long limit =
            (info.hasLoop && loop) ? info.loopEnd : info.frames;
        if (pos_ >= limit) {
            if (!loop) break;
            const unsigned long long ls = info.hasLoop ? info.loopStart : 0;
            if (ls >= limit || ++stalls > 2 || !seek(ls)) break;
            pos_ = ls;
            wraps++;
            continue;
        }
        int want = frames - got;
        if ((unsigned long long)want > limit - pos_) want = (int)(limit - pos_);
        int n = rawRead(out + got * 2, want);
        if (n <= 0) {
            // the file ended before its header said it would: treat as the limit
            if (!loop || ++stalls > 2) break;
            pos_ = limit;
            continue;
        }
        pos_ += (unsigned long long)n;
        got += n;
        stalls = 0;
    }
    return got;
}

SdSource *sd_source_open(const sd_u8 *d, size_t n, bool isOgg, std::string &why)
{
    SdAudioInfo info;
    if (isOgg) {
        stb_vorbis *v = 0;
        if (!ogg_open_info(d, n, v, info, why)) return 0;
        OggSource *s = new OggSource(v, info.channels);
        s->info = info;
        return s;
    }
    Wav w;
    if (!wav_parse(d, n, w, why)) return 0;
    info.isOgg = 0;
    info.channels = w.channels;
    info.rate = w.rate;
    info.frames = w.frames;
    note_loop(info, w.frames, w.loopStart, w.loopEnd, w.hasSmpl != 0);
    WavSource *s = new WavSource(d, w);
    s->info = info;
    return s;
}

bool sd_swav_from_clip(SdClip c, bool origLoops, std::vector<sd_u8> &rec,
                       std::string &note, std::string &why)
{
    note.clear();
    if (c.mono.size() < 4) { why = "the clip is shorter than 4 samples"; return false; }
    char b[160];
    if (c.rate > 65535) {
        snprintf(b, sizeof b, "rate %d Hz is above the 65535 Hz the wave format holds, resampled to 48000 Hz", c.rate);
        note = b;
        sd_audio_resample_mono(c, 48000);
    } else if (c.rate < 4000) {
        int m = (4000 + c.rate - 1) / c.rate;
        snprintf(b, sizeof b, "rate %d Hz is below 4000 Hz, resampled to %d Hz", c.rate, c.rate * m);
        note = b;
        sd_audio_resample_mono(c, c.rate * m);
    }
    std::vector<sd_s16> pcm = c.mono;
    bool loop = false;
    unsigned long long ls = 0, le = pcm.size();
    if (c.hasLoop) {
        ls = c.loopStart;
        le = c.loopEnd;
        if (le > pcm.size()) le = pcm.size();
        if (ls + 2 > le) { ls = 0; le = pcm.size(); }
        else loop = true;
    }
    if (loop) {
        if (ls / 2 > 65535) {
            snprintf(b, sizeof b, "loop start %llu is past the 131070 samples the wave format can address, the clip plays once", ls);
            if (!note.empty()) note += "; ";
            note += b;
            loop = false;
        }
    }
    if (!loop && origLoops && !c.hasLoop) {
        loop = true;
        ls = 0;
        le = pcm.size();
        if (!note.empty()) note += "; ";
        note += "no loop tags and the original wave loops, so the whole clip loops";
    }
    if (loop) {
        pcm.resize((size_t)le);
        if (ls & 1) { pcm.erase(pcm.begin()); ls--; le--; }
        if (pcm.size() & 1) pcm.pop_back();
        if (pcm.size() < ls + 2) { why = "the loop is too short once aligned"; return false; }
    } else {
        if (pcm.size() & 1) pcm.push_back(0);
    }
    const size_t words = pcm.size() / 2;                // 4 byte words
    const size_t loopWords = loop ? (size_t)(ls / 2) : 0;
    if (words > 0x3fffffffu) { why = "the clip is too long"; return false; }
    rec.assign(12 + words * 4, 0);
    rec[0] = 1;                                         // PCM16
    rec[1] = loop ? 1 : 0;
    const unsigned rate = (unsigned)c.rate;
    rec[2] = (sd_u8)rate; rec[3] = (sd_u8)(rate >> 8);
    const unsigned t = rate ? 16756991u / rate : 0;     // the DS timer value; the host does not read it
    rec[4] = (sd_u8)t; rec[5] = (sd_u8)(t >> 8);
    rec[6] = (sd_u8)loopWords; rec[7] = (sd_u8)(loopWords >> 8);
    const unsigned nl = (unsigned)(words - loopWords);
    rec[8] = (sd_u8)nl; rec[9] = (sd_u8)(nl >> 8); rec[10] = (sd_u8)(nl >> 16); rec[11] = (sd_u8)(nl >> 24);
    for (size_t k = 0; k < pcm.size(); k++) {
        rec[12 + k * 2] = (sd_u8)(pcm[k] & 0xff);
        rec[12 + k * 2 + 1] = (sd_u8)(((unsigned)pcm[k] >> 8) & 0xff);
    }
    return true;
}

bool sd_swar_rebuild(const sd_u8 *base, size_t n,
                     const std::vector<SdSwavRepl> &repl,
                     std::vector<sd_u8> &out, std::string &why)
{
    if (n < 0x3c || memcmp(base, "SWAR", 4) != 0) { why = "not a wave archive"; return false; }
    const unsigned cnt = rl32(base + 0x38);
    if (cnt > (n - 0x3c) / 4) { why = "wave count runs past the archive"; return false; }
    for (size_t k = 0; k < repl.size(); k++)
        if (repl[k].index >= cnt) { why = "wave number is outside the archive"; return false; }
    const size_t tbl = 0x3c + (size_t)cnt * 4;
    out.assign(base, base + tbl);
    std::vector<const SdSwavRepl *> by(cnt, (const SdSwavRepl *)0);
    for (size_t k = 0; k < repl.size(); k++) by[repl[k].index] = &repl[k];
    for (unsigned i = 0; i < cnt; i++) {
        const unsigned off = rl32(base + 0x3c + (size_t)i * 4);
        const sd_u8 *src = 0;
        size_t len = 0;
        if (by[i]) {
            src = by[i]->rec.data();
            len = by[i]->rec.size();
        } else if (off) {
            if (off < tbl || off > n || n - off < 12) { why = "a wave offset is outside the archive"; return false; }
            unsigned long long words = (unsigned long long)rl16(base + off + 6) + rl32(base + off + 8);
            len = 12 + (size_t)(words * 4);
            if (len > n - off) { why = "a wave runs past the archive"; return false; }
            src = base + off;
        }
        unsigned newOff = 0;
        if (src) {
            while (out.size() & 3) out.push_back(0);
            newOff = (unsigned)out.size();
            out.insert(out.end(), src, src + len);
        }
        out[0x3c + (size_t)i * 4] = (sd_u8)newOff;
        out[0x3c + (size_t)i * 4 + 1] = (sd_u8)(newOff >> 8);
        out[0x3c + (size_t)i * 4 + 2] = (sd_u8)(newOff >> 16);
        out[0x3c + (size_t)i * 4 + 3] = (sd_u8)(newOff >> 24);
    }
    while (out.size() & 3) out.push_back(0);
    const unsigned total = (unsigned)out.size();
    out[8] = (sd_u8)total; out[9] = (sd_u8)(total >> 8); out[10] = (sd_u8)(total >> 16); out[11] = (sd_u8)(total >> 24);
    const unsigned dsz = total - 0x10;
    out[0x14] = (sd_u8)dsz; out[0x15] = (sd_u8)(dsz >> 8); out[0x16] = (sd_u8)(dsz >> 16); out[0x17] = (sd_u8)(dsz >> 24);
    return true;
}
