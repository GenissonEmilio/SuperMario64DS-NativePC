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
record number that `sdat_list.py` prints in its first column (see below).
The numbered form works for named members too.

## Finding the names

`sdat_list.py` (next to this file) prints every member from your own extracted
`sound_data.sdat` (the player's ROM dump, `data/sound_data.sdat`):

    python sdat_list.py path/to/sound_data.sdat

Each line is: the file name to use, the type, the archive's file number, and
the size in bytes. A name that appears twice (two records sharing one file)
is the same member.

## Rules

- A replacement must be the same kind of file. The game checks the four-letter
  tag at the start of the file (`SSEQ`, `SBNK`, `SWAR`, `SSAR`, `STRM`) and
  that the file is not shorter than the size it states. A file that fails is
  skipped (one line in the game log) and the original stays.
- A name the archive does not have is ignored (one log line).
- If several enabled mods replace the same member, the one lowest in the
  launcher's list (last in `EnabledMods`) wins.
- A replacement sequence keeps the original's bank and wave archive links
  (those live in the archive's info table, not in the file). To use new
  instruments, replace the bank and wave archive too, or pick a sequence that
  uses the same bank as the one you replace.
- The log line at boot, `[mods] sound: replaced N archive members: ...`,
  lists what took effect.
- `SM64DS_MODS=0` in the environment turns all mods off, sound included.
