# Sound mods: plain .wav and .ogg files

You do not need any DS tools. Put ordinary `.wav` or `.ogg` files in your mod's
`sound/` folder and the game plays them instead of the original sound.

(The older way, replacing whole archive members such as `.sseq`, `.sbnk` and
`.swar`, still works and is described in `README-sound.md`. Both can be used in
one mod.)

## What you can replace

| You put | Where | What it does |
|---|---|---|
| `NCS_BGM_CHIJOU.ogg` (or `.wav`) | `sound/` | plays a **recording** instead of that piece of music |
| `3.wav` (or `3.ogg`) | `sound/NCS_WAVE_SE_VOICE_YOSSY/` | replaces **one sound** (wave number 3) inside a wave archive |
| `NCS_STRM_SOMETHING.ogg` | `sound/` | replaces a stream member (see "Streams" below) |

Names are not case sensitive.

### Music: `sound/<sequence name>.ogg` or `.wav`

The name is the game's name for the piece of music, the same one
`README-sound.md` uses for `.sseq` files, without the extension:
`sound/NCS_BGM_CHIJOU.ogg` replaces the castle grounds theme.

* The recording plays at its own speed and its own pitch, always. Nothing the
  game does to the notes (tempo changes, transposing, pitch bends, sweeps,
  vibrato) reaches it.
* It follows what the game does to the **volume**: the music level, fades out
  and in (for example when you enter a course), stopping, starting a different
  piece, and pause (the recording holds still while the game is paused and
  carries on from the same spot).
* It is stereo if your file is stereo. The game's own panning does not apply.
* It loops. If your file has loop tags (below) it loops between them; with no
  tags it loops the whole file. A short jingle that the game plays once (a
  fanfare) plays through once: when the game's own version of the piece ends,
  your recording is allowed to finish and then stops, it does not start over.
* The original music keeps running **silently** underneath. That is on purpose:
  the game checks whether music is playing and when it has ended, and it must
  get the same answers it always did. You will not hear it.
* Only the whole piece can be replaced. Pieces that are built from layers the
  game turns on and off (some courses add an upbeat layer in certain places)
  play as one recording; the layer switching is not reproduced.

### One sound: `sound/<wave archive name>/<n>.wav` or `.ogg`

Sound effects and instruments are built from small recordings called waves,
stored in wave archives (`.swar`). `<wave archive name>` is the archive's name
and `<n>` is the wave's number in it, counting from 0, written in plain
digits: `sound/NCS_WAVE_SE_VOICE_YOSSY/3.wav`.

* Every sound that uses that wave now uses your recording. Waves you do not
  name stay as they were.
* Your recording plays at **its own pitch**, every time. The game normally
  plays one recording at different pitches for different notes; for a wave you
  replaced that is switched off, so the sound is the pitch you recorded
  whatever note, pitch bend, slide or vibrato the game asks for. Volume,
  panning, the fade-in and fade-out shape, and stopping still work as usual.
* The file is made mono (stereo files are mixed down). Rates from 4000 to
  65535 Hz are kept exactly; a file above 65535 Hz is converted to 48000 Hz, and
  one below 4000 Hz is raised to the next multiple that reaches 4000 Hz.
* Looping: the file's loop tags are used if it has them. With no tags the
  sound is played once, **unless the original wave loops** (engines, wind,
  water). A replacement for a looping sound with no tags loops the whole file
  (the game log says so). A loop start can be at most 131070 samples in; a
  later loop start is ignored and the sound plays once.
* At most 6 million samples per wave (about 2 minutes at 48 kHz).

### Streams: `sound/<stream name>.wav` or `.ogg`

Accepted and checked the same way, but this version of the game never starts a
stream member of this archive (it has none), so such a file is read, reported
in the log and not heard.

## Loop tags

A loop is a stretch of the file that repeats forever: the sound plays from the
beginning to the loop end, then jumps back to the loop start and repeats.

* **WAV**: a standard `smpl` chunk with one loop. Most audio editors can write
  it (in Audacity use a label or a loop editor; in other tools "loop points").
  Only the first loop in the chunk is read.
* **OGG**: comment fields (also called tags) in the file:
  * `LOOPSTART=<sample>` the first sample of the repeated part
  * `LOOPLENGTH=<samples>` how long it is, **or**
  * `LOOPEND=<sample>` the sample just after the repeated part
  If only a length or only an end is given, the loop starts at sample 0.
  Numbers count samples of the file at its own sample rate (44100 per second
  for a 44.1 kHz file), not seconds.

Loop tags that make no sense (a start after the end, past the end of the file)
are ignored and the log says so.

## Limits

* One music recording (or stream) file: 48 MB at most. All recordings of all
  your mods together: 128 MB. A `.ogg` stays compressed in memory and is played
  as it goes, so a three-minute song costs about the size of the file, not 35
  MB. A `.wav` is read straight from memory too.
* One wave file: 8 MB at most (the same limit as every other sound file). The
  rebuilt wave archives count toward the 64 MB limit on added archive data in
  `README-sound.md`.
* Supported: `.wav` as PCM 8, 16, 24 or 32 bit or 32/64 bit float, 1 to 8
  channels (extra channels beyond the first two are ignored); `.ogg` Vorbis, 1
  to 8 channels. Not supported: compressed WAV (ADPCM, mp3-in-wav), `.mp3`,
  `.flac`, Opus. Sample rates 1000 to 384000 Hz.
* Sound mods are switched off in multiplayer, like all mods.

## What happens with a bad file

The game never crashes over a sound file. A file it cannot use is skipped, the
stock sound stays, and **one line** in the game's log starts with `[mods] sound:`
and says which file and why, for example

    [mods] sound: mymod/NCS_WAVE_SE_VOICE_YOSSY/3.wav: bad audio file (WAV format 2 with 4 bits is not supported ...), skipped
    [mods] sound: mymod/NCS_BGM_CHIJOU.ogg: no sequence or stream with that name in the archive, ignored

An `.ogg` is also checked page by page against the checksums every Ogg page
carries, so a file damaged in a download, cut off, or written only partly is
refused with a plain reason ("damaged Ogg file (page 3 fails its checksum)")
instead of being handed to the decoder.

On a good run you see lines such as

    [mods] sound: mymod/NCS_BGM_CHIJOU.ogg: music track 'ncs_bgm_chijou' replaced by a recording (44100 Hz, 2 ch, 181.3 s, loop tags)
    [mods] sound: mymod/NCS_WAVE_SE_VOICE_YOSSY/3.wav -> wave 3 of file 114 (22050 Hz, 9800 samples)
    [audio] music stream start: player 0 plays 'NCS_BGM_CHIJOU.ogg' (44100 Hz, 2 ch, 181.3 s, loop tags)

## Finding the names

Run `sdat_list.py` (next to this file) on your own extracted `sound_data.sdat`:

    python sdat_list.py path/to/sound_data.sdat --waves

Every line is a name you can use. Music pieces are the `.sseq` lines whose names
start `NCS_BGM_`; wave archives are the `.swar` lines, and with `--waves` each
one is followed by its waves with their numbers, rates and lengths, so you can
see which number to use for `<n>`.

To find out which sound a given wave is, replace it with a loud beep, play the
game and listen; the wave numbers inside an archive are not labelled.

## Sound quality setting

The game mixes its sound at 48000 Hz with a smoother resampler by default
(`AudioQuality` in settings.json, `"hq"`). `"ds"` is the original console's
32768 Hz mixing, kept for people who want it exactly. Your files work the same
under both. (Environment variable `SM64DS_AUDIO_HQ=0` forces `"ds"`.)
