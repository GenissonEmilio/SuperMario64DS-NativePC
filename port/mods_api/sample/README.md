# Writing a code mod for the SM64DS PC port

A code mod is a folder with one 32-bit Windows DLL in it. The game loads the DLL
when it starts, hands it a table of functions (plugin API version 1, described in
`sm64ds_mod.h`), and calls it back every frame, when a level loads, when a ROM file
is read, and when an actor is spawned.

## Rules

* The DLL must be built for the exact game release it targets. A plugin finds
  game code by address, so it is only valid for one build. `mod.json` says which
  (`"game": "0.5.6"`); a mod built for another release is skipped, with the reason
  in the log.
* 32-bit (x86), `__cdecl`. Use the MSVC x86 tools. The interface is plain C.
* One DLL per mod, in `plugin/`.
* The plugin API version in `mod.json` (`"api": 1`) must match the game's.
* Plugins are not loaded in multiplayer (a lobby or VS match).
* A plugin's own state is not part of a save state. F8/F9 restore the game and
  not your variables, so keep your state derivable from the game.
* If a plugin crashes, the game switches it off for the rest of the run and keeps
  going. The log says which plugin and where. A file filter that crashes has the
  file put back as it was.
* The guard cannot stop a plugin that does not crash. If a callback never
  returns, the plugin calls ExitProcess, or it deadlocks in DllMain, the game
  freezes or exits and nothing can rescue it. Keep callbacks short and do not
  start threads.
* `replace_actor` returns NULL both when the game has no such actor and when the
  replacement table is full (32 replacements across all plugins, 16 per plugin);
  the log says which. When it returns NULL, nothing was changed: do not assume
  your factory is installed.
* Dev builds of the game report the version in their resource file (0.5.0 on the
  current development base, not the next release number). A test mod built for a
  dev exe puts that string in `mod.json` "game"; the shipped kit says its release.
* The symbol table (`walk_window.symtab`) is stamped with the exe it was made
  for. A table from another build is refused (one log line) and `symbol()`
  answers NULL for everything, so ship the table that came with the exe.
* Turn plugins off alone with `SM64DS_MOD_PLUGINS=0`, every mod with `SM64DS_MODS=0`.

## Folder layout

```
mods/my-mod/
  mod.json                  {"id":"my-mod","kind":"code","game":"0.5.6","api":1, ...}
  plugin/my_mod.dll         the plugin (exactly one DLL)
  settings.json             optional: the settings the launcher shows
  settings.values.json      the player's values; read with api->setting(api, key)
  files/ textures_hd/ palettes/   optional, same as a files mod
```

`id` is 1 to 64 characters of `a-z 0-9 -` and is also the folder name. A code mod
may carry `files/`, `textures_hd/` and `palettes/` as well; they apply the same way
they do for a files mod.

## What the API gives you

| call | what it does |
| --- | --- |
| `log` | write a line to the game log, prefixed `[mod:<id>]` |
| `setting` | the player's value for a key from `settings.values.json`, as text, or NULL |
| `on_frame` | called once per frame after the game's own frame logic |
| `on_level_load` | called when a level finishes loading, with the level number |
| `on_file` | edit a ROM file's bytes as it is loaded (after every files mod) |
| `replace_actor` | replace an actor's spawn function; you get the old one back to wrap |
| `symbol` | the address of a game function or global, by its name in this build |
| `alloc` / `free` | the only allocator allowed across the boundary |

Details are in comments in `sm64ds_mod.h`. Some things worth knowing:

* `on_file` may be called more than once for the same file (the game asks twice
  in some paths). Always edit from the pristine bytes you are given and do not
  keep state between calls.
* Several plugins run in the launcher's priority order; the last one wins.
* `symbol` names are the linker's names for this exact build. The game finds
  them in `walk_window.symtab` (or `walk_window.map`) next to the exe. C names
  are looked up with and without the leading underscore. You cast the result to
  the right function type yourself, and you take the consequences of getting it
  wrong: the guard catches a crash, it cannot undo a corrupted game.

## Build the sample

The sample in `sample/` does all of the above on a small scale: it logs, reads a
setting, counts frames, logs level loads, turns Mario's first palette green, and
wraps the coin's spawn function.

From an x86 developer prompt (the "x86 Native Tools Command Prompt for VS"):

```
cd sample
cmake -S . -B build -G Ninja
cmake --build build
```

or with the Visual Studio generator: `cmake -S . -B build -A Win32` then
`cmake --build build --config Release`.

Copy the DLL to `mods/sample/plugin/`, add a `mod.json`:

```
{"id":"sample","name":"Sample","version":"1.0","kind":"code","game":"0.5.6","api":1}
```

list `"sample"` in `EnabledMods` in `settings.json`, and start the game. The log
shows a `[plugins]` summary line and the `[mod:sample]` lines. Optional: put
`{"tint":"0x7C00"}` in `mods/sample/settings.values.json` for a different colour
(BGR555).
