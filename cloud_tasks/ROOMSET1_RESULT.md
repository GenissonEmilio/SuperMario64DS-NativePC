# ROOMSET1 result: room settings in the lobby

Branch: cloud/roomset1, based on origin/port/l7-mpfold9.
Files: port/tools/lobby/app/server.py, test_units.py, test_client.py, README.md.

## What changed

- Party and coop rooms take a host-set `settings` object at create.
- The host can change it with a new `settings` verb, before Start.
- Every party and coop plan carries `settings`. Every party and coop view carries it too.
- VS and adventure rooms refuse settings. World rooms have no settings field.
- Contract stays at 6. The change is additive. A v6 create without `settings` gets the defaults.

## The API

Create (party or coop; coop still needs `save_name`):

```json
{"v": 6, "nick": "Tango", "game": "1.0.0", "session_type": "party",
 "settings": {"collision": 1, "interact": 1, "damage": 0, "starshare": 1}}
```

Any subset of the names is fine. Missing names keep their defaults. `{}` means all defaults.

Host update (before Start only):

```json
{"v": 6, "room": "K7QMR3", "token": "<host token>", "settings": {"damage": 1}}
```

Answer: `{"cursor": n, "settings": {...}}`. The room also pushes `{"kind": "settings", "settings": {...}}`.

Plan (every seat, every poll, create and join):

```json
"settings": {"collision": 1, "interact": 1, "damage": 0, "starshare": 1}
```

## Names, kinds and defaults

| name | party | coop | default | env the launcher sets |
|---|---|---|---|---|
| collision | yes | yes | 1 | SM64DS_ROOM_COLLISION |
| interact | yes | yes | 1 | SM64DS_ROOM_INTERACT |
| damage | yes | yes | 0 | SM64DS_ROOM_DAMAGE |
| tp | no | yes | 1 | SM64DS_ROOM_TP |
| starshare | yes | no | 1 | SM64DS_ROOM_STARSHARE |

Values are the integers 0 or 1 only.

## Refusals

| case | answer |
|---|---|
| settings is not a JSON object (null, list, string, number) | 400 bad_settings |
| a name the room's kind does not take, or an unknown name | 400 bad_setting_key |
| a value that is not int 0 or 1 (bool, float, string, 2) | 400 bad_setting_value |
| settings on a VS or adventure create | 400 bad_field |
| settings verb from a v5 caller | 400 bad_field (the verb is v6, same as `color` at v1) |
| settings update by a guest | 403 not_host |
| settings update on a VS or adventure room | 409 no_room_settings |
| settings update after Start | 409 already_started |
| settings update while not in lobby | 409 not_in_lobby |

A refused create makes no room. A refused update changes nothing.

## Why no contract bump

The new field is optional and the new verb is additive. Old v6 clients send no settings and get the defaults. Nothing that already worked changes.

One deploy note. If a v6 lobby without this change is already live, a launcher that sends settings gets `bad_field` from it. So deploy this lobby before a launcher that sends settings. If the live lobby must accept settings from a launcher before this lobby is deployed, bump to v7 instead. Please confirm which case applies before deploying.

## Test output

Run from port/tools/lobby.

- `python3 test_units.py`: 1279 passed, 2 failed.
  - The 2 failures are "the match timeout returns the room to lobby" and "with a failed timeout event".
  - Both fail on the base commit too, before any change here: 1166 passed, 2 failed.
  - They are timing rows in the VS match timeout code. This lane did not touch them.
- `python3 test_client.py selftest` (run against a local server it spawns): 218 passed, 0 failed. It was 210 before the new wire rows.
- `python3 test_security.py`: 48 passed, 0 failed.

New unit rows (test_room_settings, 37 rows):

- Defaults for each kind, on create, view, joiner plan and poll plan.
- Each toggle set alone to 0 and to 1, and all toggles at 0.
- Wrong-kind names, unknown names, bad values, non-object settings.
- Refused creates make no room.
- VS and adventure creates with settings are bad_field. A v5 create with settings is bad_field.
- Host-only partial update, the guest refusal, the event, the bad-value refusal.
- Start freezes the settings (409 already_started).
- VS update is 409 no_room_settings.
- Coop tp=0 reaches a late joiner.
- A path-like key on the settings verb is bad_field.
- The verb-table gate test covers `settings` at v1..v4 and v7 (bad_version).

Not run: the game build, the launcher, and any real launch. This VM cannot build or run the game or the WinForms launcher.

## What the LAUNCHER must do next

1. Add a room-settings panel to the private WinForms MP test launcher.
   - Show one checkbox per toggle at room create.
   - Show only the names the room kind takes (party or coop).
   - Default the boxes to the table above.
   - Send the box values as `settings` in create.
2. Give the host a way to change the toggles in the lobby before Start.
   - Send the `settings` verb with only the changed names.
   - Handle 409 already_started by greying the panel out.
3. Turn each plan's `settings` into env, for party and coop rooms only.
   - Set SM64DS_ROOM_COLLISION, SM64DS_ROOM_INTERACT, SM64DS_ROOM_DAMAGE to "0" or "1".
   - Set SM64DS_ROOM_TP for coop only.
   - Set SM64DS_ROOM_STARSHARE for party only.
   - Clear these names before every launch, like the other room names.
   - A world launch and a VS or adventure launch set none of them.
4. Use the same plan on join and on every poll, so a late joiner gets the host's toggles.
5. Send `settings` only to a lobby that has this change (see the deploy note).

## What the GAME must read

One line per env var, read at boot from the launch env:

- SM64DS_ROOM_COLLISION: 1 means friends are solid bodies, 0 means ghosts with no push. Default 1 (FIX9A).
- SM64DS_ROOM_INTERACT: 1 means hits, stomps and eats count, 0 means none. Default 1 (PLAYINT1).
- SM64DS_ROOM_DAMAGE: 1 means a friend's hit can hurt a friend, 0 means off. Default 0 (PLAYINT1).
- SM64DS_ROOM_TP: 1 means TP to friends is on, coop only. Default 1 (COOPTP1).
- SM64DS_ROOM_STARSHARE: 1 means stars are shared, party only. Default 1.

An unset variable should mean the default. A value other than 0 or 1 should be treated as unset.

## Open questions

- Is a v6 lobby already live? If yes, bump to v7 (see Why no contract bump).
- Should the party host's leave also reset the settings? This change keeps them on the room, and they pass with the room.
