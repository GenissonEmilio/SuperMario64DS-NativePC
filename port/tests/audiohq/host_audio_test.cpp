// Host-side test of the AUDIOHQ1 audio code, off Windows and with no ROM.
//
// Builds the real hal/sdat/{sdat,mixer,sseq,sd_audio,vorbis_impl}.cpp against a
// tiny synthetic sound_data.sdat (make_test_sdat.py), loads a mod folder through
// the same sdat_apply_mods path the game uses, plays the test music sequence
// through the real sequencer and mixer, and writes the mix to a WAV.
//
//   host_audio_test --sdat test.sdat --mods DIR --out mix.wav --seconds 8
//        [--hq 0|1] [--at T:ACTION ...]
//
// ACTIONs: start | stop | pause | resume | vol=N (0..127) | db10=N (0..-723) |
//          note=PROG:KEY:VEL:DUR  (one extra note on player 1, program/key/velocity,
//          duration in sequencer ticks) |
//          raw=HEXBYTES  (an arbitrary sequence on player 1) |
//          arc=N  (start entry N of the first sequence archive on player 1, the way
//          the game's system-sound probe starts a SEQARC entry)
#include "sdat.h"

#ifdef OLD_BUILD
#define MIXRATE() ((int)SD_MIX_RATE)       // pre-AUDIOHQ1 tree: the only rate there was
#else
#define MIXRATE() sd_mix_rate()
#endif

#include <dirent.h>
#include <fnmatch.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include "shim/windows.h"

extern "C" void *data_020a5bb8;
void *data_020a5bb8;
int out_volume_pct(void) { return 100; }

// ---- FindFile shim -------------------------------------------------------
struct FindState { std::vector<WIN32_FIND_DATAA> items; size_t at; };

HANDLE FindFirstFileA(const char *pattern, WIN32_FIND_DATAA *fd)
{
    std::string pat = pattern;
    size_t sl = pat.rfind('/');
    std::string dir = sl == std::string::npos ? "." : pat.substr(0, sl);
    std::string wild = sl == std::string::npos ? pat : pat.substr(sl + 1);
    DIR *d = opendir(dir.c_str());
    if (!d) return INVALID_HANDLE_VALUE;
    FindState *fs = new FindState();
    fs->at = 0;
    while (dirent *e = readdir(d)) {
        if (fnmatch(wild.c_str(), e->d_name, 0) != 0) continue;
        std::string full = dir + "/" + e->d_name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        WIN32_FIND_DATAA f;
        memset(&f, 0, sizeof f);
        f.dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0;
        f.nFileSizeLow = (unsigned)st.st_size;
        f.nFileSizeHigh = (unsigned)((unsigned long long)st.st_size >> 32);
        snprintf(f.cFileName, sizeof f.cFileName, "%s", e->d_name);
        fs->items.push_back(f);
    }
    closedir(d);
    if (fs->items.empty()) { delete fs; return INVALID_HANDLE_VALUE; }
    *fd = fs->items[0];
    fs->at = 1;
    return fs;
}
int FindNextFileA(HANDLE h, WIN32_FIND_DATAA *fd)
{
    FindState *fs = (FindState *)h;
    if (fs->at >= fs->items.size()) return 0;
    *fd = fs->items[fs->at++];
    return 1;
}
int FindClose(HANDLE h) { delete (FindState *)h; return 1; }

// ---- mod list hooks --------------------------------------------------------
static std::string g_modDir;
static int mods_count(void) { return g_modDir.empty() ? 0 : 1; }
static const char *mods_id(int) { return "testmod"; }
static const char *mods_folder(int) { return g_modDir.c_str(); }

extern "C" {
extern int (*g_sdat_mods_count)(void);
extern const char *(*g_sdat_mods_id)(int);
extern const char *(*g_sdat_mods_folder)(int);
}

struct Ev { double t; std::string a; };

static void put32(FILE *f, unsigned v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, unsigned v) { unsigned short s = (unsigned short)v; fwrite(&s, 2, 1, f); }

int main(int argc, char **argv)
{
    std::string sdat, out = "mix.wav";
    double seconds = 6.0;
    std::vector<Ev> evs;
    bool bench = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](void) -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--sdat") sdat = next();
        else if (a == "--mods") g_modDir = next();
        else if (a == "--out") out = next();
        else if (a == "--seconds") seconds = atof(next().c_str());
        else if (a == "--hq") setenv("SM64DS_AUDIO_HQ", next().c_str(), 1);
        else if (a == "--bench") bench = true;
        else if (a == "--at") {
            std::string v = next();
            size_t c = v.find(':');
            evs.push_back({atof(v.substr(0, c).c_str()), v.substr(c + 1)});
        }
    }
    if (sdat.empty()) { fprintf(stderr, "need --sdat\n"); return 2; }
    setenv("SM64DS_SDAT", sdat.c_str(), 1);
    g_sdat_mods_count = mods_count;
    g_sdat_mods_id = mods_id;
    g_sdat_mods_folder = mods_folder;

    if (bench) {
        // 16 looping voices at assorted pitches, rendered for `seconds`, timed.
        // Reports CPU time per output frame and per voice-sample.
        sd_mix_reset();
        const int rate = MIXRATE();
        static sd_s16 pcm[4000];
        for (int i = 0; i < 4000; i++) pcm[i] = (sd_s16)(12000 * sin(2 * 3.14159265 * 400 * i / 16000));
        SdatWave w;
        memset(&w, 0, sizeof w);
        w.pcm = pcm; w.totalSamples = 4000; w.loop = 1; w.loopStart = 0; w.sampleRate = 16000;
        SdatNote n;
        memset(&n, 0, sizeof n);
        n.attack = n.decay = n.sustain = n.release = 127;
        for (int c = 0; c < 16; c++) {
            int ch = sd_mix_alloc(10);
            sd_mix_start(ch, &w, &n, 0, 64, (16000.0 / rate) * (0.5 + 0.1 * c), 10);
        }
        std::vector<sd_s16> b(1024 * 2);
        const long total = (long)(seconds * rate);
        clock_t t0 = clock();
        for (long d = 0; d < total; d += 1024) sd_mix_render(b.data(), 1024);
        double sec = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("bench rate=%d: %.3f s cpu for %.1f s audio (%.1f%% of one core), %.2f ns per voice-sample\n",
               rate, sec, seconds, 100.0 * sec / seconds, sec * 1e9 / ((double)total * 16));
        return 0;
    }

    if (!sdat_init()) { fprintf(stderr, "sdat_init failed\n"); return 3; }
    sd_mix_reset();
    sd_seq_reset();

    const int rate = MIXRATE();
    sd_u8 *seqEntry = sdat_info_entry(SDAT_REC_SEQ, 0);
    sd_u32 sseqFid = seqEntry[0] | (seqEntry[1] << 8);
    unsigned bankId = seqEntry[4] | (seqEntry[5] << 8);
    sd_u8 *bankEntry = sdat_info_entry(SDAT_REC_BANK, bankId);
    sd_u32 bankFid = bankEntry[0] | (bankEntry[1] << 8);
    sd_u8 *sseq = sdat_file(sseqFid);
    sd_u8 *sbnk = sdat_file(bankFid);
    // link the bank to its wave archive the way the game's loader would
    sdat_link_bank_waves(sbnk);
    sd_u32 dataOff = sseq[0x18] | (sseq[0x19] << 8) | (sseq[0x1a] << 16) | ((sd_u32)sseq[0x1b] << 24);

    FILE *f = fopen(out.c_str(), "wb");
    if (!f) return 4;
    fwrite("RIFF", 1, 4, f); put32(f, 0); fwrite("WAVEfmt ", 1, 8, f); put32(f, 16);
    put16(f, 1); put16(f, 2); put32(f, rate); put32(f, rate * 4); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, 0);

    const long total = (long)(seconds * rate);
    const int block = 128;
    std::vector<sd_s16> buf(block * 2);
    long done = 0;
    size_t nextEv = 0;
    // events are fired in time order
    for (size_t a = 0; a < evs.size(); a++)
        for (size_t b = a + 1; b < evs.size(); b++)
            if (evs[b].t < evs[a].t) std::swap(evs[a], evs[b]);
    while (done < total) {
        while (nextEv < evs.size() && evs[nextEv].t * rate <= (double)done) {
            const std::string &a = evs[nextEv++].a;
            fprintf(stderr, "[test] t=%.3f %s\n", (double)done / rate, a.c_str());
            if (a == "start") sd_seq_start(0, sseq + dataOff, 0, sbnk);
            else if (a == "stop") sd_seq_stop(0);
            else if (a == "pause") sd_seq_pause(0, 1);
            else if (a == "resume") sd_seq_pause(0, 0);
            else if (a.compare(0, 4, "vol=") == 0) sd_seq_set_volume(0, atoi(a.c_str() + 4));
            else if (a.compare(0, 5, "db10=") == 0) sd_seq_set_volume_db10(0, atoi(a.c_str() + 5));
            else if (a.compare(0, 4, "arc=") == 0) {
                sd_u8 *ae = sdat_info_entry(SDAT_REC_SEQARC, 0);
                sd_u8 *af = ae ? sdat_file(ae[0] | (ae[1] << 8)) : 0;
                unsigned n = (unsigned)atoi(a.c_str() + 4);
                if (af && n < (unsigned)(af[0x1c] | (af[0x1d] << 8))) {
                    sd_u32 dof = af[0x18] | (af[0x19] << 8) | (af[0x1a] << 16) | ((sd_u32)af[0x1b] << 24);
                    const sd_u8 *rec = af + 0x20 + n * 12;
                    sd_u32 so = rec[0] | (rec[1] << 8) | (rec[2] << 16) | ((sd_u32)rec[3] << 24);
                    sd_seq_start(1, af + dof, so, sbnk);
                }
            }
            else if (a.compare(0, 4, "raw=") == 0) {
                // arbitrary sequence bytes on player 1, as hex ("c50c c47f 8100 3c6430 ff")
                static sd_u8 raw[256];
                int n = 0;
                for (size_t i = 4; i + 1 < a.size() && n < 255; ) {
                    if (a[i] == ' ') { i++; continue; }
                    unsigned v = 0;
                    sscanf(a.c_str() + i, "%2x", &v);
                    raw[n++] = (sd_u8)v;
                    i += 2;
                }
                sd_seq_start(1, raw, 0, sbnk);
            }
            else if (a.compare(0, 5, "note=") == 0) {
                // a one-note sequence on player 1: 81 prog, key vel dur, ff
                int prog, key, vel, dur;
                sscanf(a.c_str() + 5, "%d:%d:%d:%d", &prog, &key, &vel, &dur);
                static sd_u8 mini[16];
                int n = 0;
                mini[n++] = 0x81; mini[n++] = (sd_u8)prog;
                mini[n++] = (sd_u8)key; mini[n++] = (sd_u8)vel; mini[n++] = (sd_u8)dur;
                mini[n++] = 0xff;
                sd_seq_start(1, mini, 0, sbnk);
            }
        }
        int n = block;
        if (done + n > total) n = (int)(total - done);
        sd_mix_render(buf.data(), n);
        fwrite(buf.data(), 4, (size_t)n, f);
        done += n;
    }
    long bytes = done * 4;
    fseek(f, 4, SEEK_SET); put32(f, (unsigned)(36 + bytes));
    fseek(f, 40, SEEK_SET); put32(f, (unsigned)bytes);
    fclose(f);
    fprintf(stderr, "[test] wrote %s: %ld frames at %d Hz\n", out.c_str(), done, rate);
    sd_mix_reset();
    return 0;
}
