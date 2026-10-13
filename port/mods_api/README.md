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

## Settings that switch a folder on or off

A files mod may declare settings in its `settings.json` (the launcher draws them
on the mod's page and writes the player's choice to `settings.values.json` next
to it). A bool or choice setting that carries a `"files"` entry switches a
folder of the mod on or off:

```json
[
  { "key": "so-long-bowser", "label": "So long, King Bowser", "type": "bool",
    "default": true, "files": "parts/so-long-bowser" },
  { "key": "quality", "label": "Quality", "type": "choice",
    "choices": ["hd", "ds"], "default": "hd", "files": "parts/quality" }
]
```

* `files` is a plain folder inside the mod (letters, digits, `-` `_` `.` and
  `/` only, no `..`). It is laid out like a mod: `files/`, `sound/`,
  `textures_hd/`, `palettes/` (single-wave `.wav` files included).
* bool: the folder is used when the value is `true`; with no value saved the
  `default` decides (no default means off).
* choice: the folder `<files>/<value>` is used.
* A part counts as a mod of its own, listed straight after its mod, so it
  wins over the mod's own files. The game log says for each such setting
  whether it was on or off and where the value came from (`[mods] <id>: setting
  ...`). The value is read once at start-up.
* A files mod that has none of `files/`, `sound/`, `textures_hd/`, `palettes/`
  (in itself or in an applied part) gets one `WARNING ... replaces nothing` line
  in the log.
