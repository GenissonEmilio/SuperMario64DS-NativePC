# Mod API for the SM64DS PC port

* `sm64ds_mod.h`: the plugin API v1 header (code mods).
* `sample/`: a sample plugin and how to write one (`sample/README.md`).
* `README-sound.md`: replacing music and sound effects (`sound/` in a mod).
* `sdat_list.py`: lists the sound archive members a mod can replace.

Mods are switched on by settings.json "EnabledMods". Mods are for single player
only: in a lobby, a VS match, a party, or the Adventure hub (settings.json
AdventureGhosts) every mod is off for the whole run.

## Packaging note (release kit)

The kit must ship `walk_window.symtab` in the same folder as `walk_window.exe`.
The plugin loader reads game symbols from it (it is written next to the exe after
every build by `port/tools/mk_symtab.py`). Without it, a kit has no symbols for
plugins: the loader falls back to `walk_window.map`, which the kit does not ship,
and logs that no table was found. Generate it from the exe being shipped.
