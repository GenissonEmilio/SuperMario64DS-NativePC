# WPARTY2: shared stars in a world party + rebindable party keys

Built on port/l7-pinvite1. Linux VM: nothing here was built as the game; relay tests ran, and world_party.cpp / settings_live.cpp pass `g++ -fsyntax-only`.

## What changed
Relay
- `port/tools/relay/relay.py`: SYNX is accepted in KIND_HUB sessions as well as KIND_PARTY (same length and `party_field_ok` checks, cap 2/s, sender seat stamped). SYNH / SYNW / SYNB are unchanged.
- `port/tools/relay/test_relay_v2.py`: hub SYNX now forwarded in the kind table and the fuzz table; new rows: hub SYNX with bad course / star / reserved / version dropped as field drops, hub SYNX with good fields forwarded to every other seat stamped with the sender.

Game
- `hal/comms_hub.cpp` `room_kind_ok`: kHubPartyStar is allowed in a world room too (send and receive). Without this the console refuses the send. SYNH stays party-only.
- `hal/party_stars.cpp`: new `world_party_room()` (world room + world party active + I am in a party). `party_room()` is true there as well as in a party room, so the observer announces and the receive path runs. In a world room `seat_ok()` takes a star only from `world_party_member(seat)`. The frame hook runs in a world room without SM64DS_PARTY. Party-room behaviour is unchanged.
- `hal/party_stars.h`: `kPsSeats` 4 -> 16 (world has 16 seats; per-seat limit / visit / sequence tables grow, a party room still uses 0..3).
- `hal/star_popup.cpp`: `mode_now()` returns kParty in a world room while I am in a world party; `star_popup_party_rx` accepts seats 0..15 and, in a world room, only world-party members. Popup + exit still require the sender in my level and act (existing `same` test); others get the toast.
- Not in a party in a world room: nothing is sent (mode kNone / party_room false), and anything received is dropped.

Rebinding
- `hal/host_settings.h/.cpp`: new rows `KeyPartyInvite` (B, 0x42), `KeyPartyLeave` (N, 0x4e), `PadPartyInvite` (0x0001 d-pad UP), `PadPartyLeave` (0x0002 d-pad DOWN); enums `HOST_KEY_PARTY_INVITE/LEAVE`, `HOST_PAD_PARTY_INVITE/LEAVE` appended before the COUNTs.
- `hal/settings_live.cpp`: matching INFO[] rows (the static_assert on SL_COUNT forces this), and `pad_reserved` lets the two party pad rows hold d-pad bits (0x000f); every other row still cannot.
- `hal/world_party.cpp/.h`: new `world_party_set_binds(vk_inv, vk_leave, pad_inv, pad_leave)`, called once a frame from `chat_pad_gate` in tests/walk_window.cpp with `host_setting_*`. world_party stays free of host_settings (it is in the shared comms slice). Env overrides `SM64DS_PARTY_KEY_INVITE/LEAVE` still win; new `SM64DS_PARTY_PAD_INVITE/LEAVE` (button mask) too. Mask 0 = unbound.
- `tests/smoke_settings.cpp`: default tables extended.

## Tests run (Linux)
- `python3 port/tools/relay/test_relay_v2.py`: 22 tests, 1121 checks, 0 failed (includes "party / coop kinds" 211 checks with the new hub SYNX rows and the fuzz).
- `python3 port/tools/relay/test_referee.py`: 24 tests, 498 checks, 0 failed.
- `g++ -fsyntax-only` on hal/world_party.cpp and hal/settings_live.cpp: clean.

## Launcher spec (Controls page; launcher repo is not here)
Add four rows to the Controls page, same widgets as "Rotate camera left":
| label | settings.json key | kind | default |
|---|---|---|---|
| Party invite / accept (keyboard) | `KeyPartyInvite` | VK 0..255 | 66 (B) |
| Party leave (keyboard) | `KeyPartyLeave` | VK 0..255 | 78 (N) |
| Party invite / accept (pad) | `PadPartyInvite` | XInput mask 0..0x3ffff | 1 (d-pad UP) |
| Party leave (pad) | `PadPartyLeave` | XInput mask 0..0x3ffff | 2 (d-pad DOWN) |
Integers, absent key = default, 0 = unbound. The pad capture for these two rows must accept d-pad buttons (0x1 UP, 0x2 DOWN, 0x4 LEFT, 0x8 RIGHT); other rows keep refusing them. Keep unknown keys on round trip. Boot-latched in the launcher; the in-game settings page (settings_live) already edits them live once a R_BIND row is added for them (optional: rows "Party invite" / "Party leave" next to "Rotate camera right" in settings_page.cpp, not done here).

## A local agent must build / prove
1. Build the game (port target, plus smoke_settings and smoke_settings_live, whose tables/INFO grew by 4 rows; smoke_settings_live may have positional assumptions I could not run).
2. Row: two consoles in a world room, in one world party (UP, UP), both in the same course: console A collects a star -> B receives it (SYNX relayed, "[pstar] rx ... -> ok"), star saved on B. Popup/exit shows for B when in the same level+act.
3. Row: a third console in the same world room, not in the party: it receives nothing (`[pstar] rx ... -> room` / seat drop, no save).
4. Row: after a member leaves (DOWN, DOWN) a star it collects no longer reaches the old party.
5. Row: against the OLD (undeployed) relay the new client's SYNX is dropped as a "route" drop, harmlessly: no disconnect, presence/party bodies unchanged, only stars not shared.
6. Row: party room (SM64DS_PARTY) stars and popup unchanged (existing party_stars_proof.py).
7. Rebinding: edit the four keys in settings.json, confirm the new key/button invites and the old no longer does; env override still wins.
8. Link check: CMake slice lists unchanged; world_party.cpp still has no host_settings dependency.

## Relay redeploy (needs Tango's word, not done)
Redeploy port/tools/relay/relay.py to the relay host using the existing deploy line (same as the last relay update; see out/PINVITE1/DEPLOY.md). Until then world-party stars silently do not travel; everything else works.

## Open questions
- `kPsSeats` is 16 for party rooms too; harmless but the party-room seat gate in `comms_hub` still limits them to 0..3.
- A founder who invited nobody counts as "in a party" (party id set), so it announces stars; nobody accepts them (non-members drop). Intended.
