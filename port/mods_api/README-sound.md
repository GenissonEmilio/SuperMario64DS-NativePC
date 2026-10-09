# Sound mods

A mod can replace music, sound effects, instrument banks and wave data. Put the
replacement files in the mod's `sound/` folder:

```
mods/<id>/sound/NCS_BGM_MINIOVER.sseq
mods/<id>/sound/NCS_BANK_BGM_USAGI.sbnk
```

## Naming

The game keeps all its sound in one archive, `sound_data.sdat`. Every member
of it has a name inside the archive. A file in `sound/` is named

    <the archive's name for the member><extension>

for example `NCS_BGM_PERFECT.sseq`. Names are not case sensitive. The
extension says what kind of member it is and must match it:

| extension | what it is |
|-----------|------------|
| `.sseq`   | a sequence: the notes of a piece of music or a sound effect |
| `.sbnk`   | a bank: which instrument plays which note and how it sounds |
| `.swar`   | a wave archive: the recorded samples the banks use |
| `.ssar`   | a sequence archive: a bundle of short sequences (the sound effects and voices) |
| `.strm`   | a stream: long audio played from the cartridge (this archive has none) |

A few members have no name in the archive. Name those by type and record
number, four digits: `sseq_0042.sseq`, `sbnk_0007.sbnk`. The number is the
record number that `sdat_list.py` prints in its third column (see below).
The numbered form works for named members too.

## Finding the names

`sdat_list.py` (next to this file) prints every member from your own extracted
`sound_data.sdat` (the player's ROM dump, `data/sound_data.sdat`):

    python sdat_list.py path/to/sound_data.sdat

Each line is: the file name to use, the type, the record number, the archive's
file number, and the size in bytes. The record number is the NNNN of the
numbered form (`sseq_0003.sseq`). A name that appears twice (two records
sharing one file) is the same member.

## Rules

- A replacement must be the same kind of file, and the game checks it before
  using it. A file that fails any check is skipped (one line in the game log
  saying which) and the original stays. The checks:
  - the file starts with its four-letter tag (`SSEQ`, `SBNK`, `SWAR`, `SSAR`,
    `STRM`), is at least 16 bytes, and the size it states (bytes 8 to 11) is
    not larger than the file. The stated size is the size used; anything after
    it is ignored.
  - `.sseq`: a `DATA` block at byte 16, and the offset of the music data
    (bytes 24 to 27) inside the file.
  - `.sbnk`: a `DATA` block at byte 16; the instrument count (byte 56) fits in
    the file; every instrument record is one of the known types (empty, a
    single instrument, a drum set or a key split) and its data lies inside the
    file; a sample reference names a wave archive slot 0 to 3.
  - `.swar`: a `DATA` block at byte 16; the wave count (byte 56) fits in the
    file; every wave offset is inside the file and each wave's sample data fits.
  - `.ssar`: a `DATA` block at byte 16; the record table fits before the data
    offset (bytes 24 to 27); every record (12 bytes each) points inside the data.
  - `.strm`: a `HEAD` block at byte 16 followed by a `DATA` block that fits.
  - after all files are read, every bank that uses a replaced bank or a
    replaced wave archive must only name waves that exist in its wave archive.
    If not, the replacement is dropped (one log line) and the original stays.
- Size limits: one file may be at most 8 MB (the biggest original is under
  300 KB), and all replacements together at most 64 MB. A file over a limit is
  skipped with a log line naming it. Replacements are taken in mod order, so
  when the total is used up, later files are the ones skipped.
- A name the archive does not have is ignored (one log line).
- If several enabled mods replace the same member, the one lowest in the
  launcher's list (last in `EnabledMods`) wins.
- A replacement sequence keeps the original's bank and wave archive links
  (those live in the archive's info table, not in the file). To use new
  instruments, replace the bank and wave archive too, or pick a sequence that
  uses the same bank as the one you replace.
- The log line at boot, `[mods] sound: replaced N archive members: ...`,
  lists what took effect, each with the archive file number it replaced.
- `SM64DS_MODS=0` in the environment turns all mods off, sound included.
