# PLAYINT1 design: player-to-player interactions in party and coop rooms

Status: DRAFT, written on a Linux VM. The rules are proven on the host (87 rows, `python3 port/tools/party_hits_proof.py`);
the game side (`port/hal/party_hits.cpp`) passes `g++ -fsyntax-only` but has NEVER been built with MSVC or run. Every
number marked TUNE is a first guess.

## 1. What it does (defaults from ROOMSET_LATER.md)

| Attacker does | Victim's console does | Attacker's console does |
|---|---|---|
| punch / kick / spin / dive / slide kick (`melee`) | `Player::Hurt` with 0 damage + shove away, stun = the ROM's hurt state | nothing |
| ground-pound landing within ~220 units (`pound`) | same, bigger shove | nothing |
| lands on the friend's head (`stomp`) | small shove + 20-frame squash | bounces up (`+0xa8 = 0x30000`), cosmetic squash on the friend's body |
| Yoshi tongue touches the friend (`eat`) | held ~45 frames in front of the eater, then spat along the eater's facing; no egg, no damage | gets an ack and a release, nothing else |

Toggles (the launcher exports them from the lobby plan; ROOMSET1 owns the lobby side):
`SM64DS_ROOM_INTERACT=0/1` (default 1) turns all of it off. `SM64DS_ROOM_DAMAGE=0/1` (default 0): a believed hit also costs one
health unit (the `b` argument of `Player::Hurt`; `b=0` is no damage, confirmed in `src/_ZN6Player4HurtERK7Vector3j5Fix12IiEjjj.cpp`:
`func_ov002_020d91e0(self, b << 8, 1)`). Eat never carries damage.

## 2. Who decides: the victim

No referee exists in a party room, so the VICTIM'S console decides (F5SCOUT-PARTY 3.2). The attacker only says "I touched you".

### 2.1 Detection on the attacker's console (pose and position, not the body cylinder)
`attack_for_pose` (core) maps my own Player state + frames since entering it + vertical speed to an interaction. `attacker_contact`
then checks the friend's newest SYNG position (from `comms_hub_seat_info`) against the reach rule for that kind (melee and eat also need
the friend inside +-0x3000 of my facing). State indices are the ROM's 78-state order; the proof cross-checks them against
`arena_contract.STATE_NAMES`.

Why not pair the cylinder: the party cylinder is flags 0 / vuln 0x00400000 on purpose (`party_cyl.cpp`). Giving it attack vuln bits
(`set_extra_vuln`) would also pair it with every enemy attack cylinder in MY world; the audit would then withhold the cylinder (the
body goes non-solid). So this lane does not touch `party_cyl.cpp`. Cost: the contact test is a window + distance, a little looser than
the ROM's attack cylinders. Windows are TUNE (`kMeleeFirst/Last`, etc.).

### 2.2 Wire: SYNH as it is, no relay change
SYNH is 24 bytes (`hub_presence.h`): attacker (relay-stamped), victim (byte 6, the relay routes by it), `hit_kind`, `seq`, yaw, x y z.
The relay accepts `hit_kind < 1` only (`relay.py HIT_KINDS = 1`, rechecked by the proof) and ignores `seq`. So `hit_kind` stays 0 and the
interaction rides in `seq` bits 12..15 (kinds: 0 melee, 1 pound, 2 stomp, 3 eat claim, 4 eat ack, 5 eat release), bits 0..11 are a
counter. x y z is the attacker's position (Fix12), yaw his facing. The same message goes back victim -> eater for ack / release (SYNH
is routed by byte 6, so the reply names the eater as its victim). Relay cap is 10/s per source; the attacker re-claims a victim at most
once per 30 frames.
If a later lane prefers a clean `hit_kind`, the only changes are `HIT_KINDS` in relay.py and `kHitKinds` in hub_presence.h; this code
would then send `hit_kind = ikind`. Not needed now.

**Coop rooms.** The relay only routes SYNH in party sessions (`relay.py` Kind SYNH sessions = {KIND_PARTY}; `comms_hub.cpp shape_ok_send`
the same). Without a relay change, coop carries the identical 24 bytes as the payload of a SYNB (coop control) datagram with sub-type
`0x41` (0x40 is COOPTP's toggle; FOLD9B2 moved this one; outside every range W56 claims: 0x00-0x3f). SYNB is broadcast, so the victim byte inside the payload filters. The coop control
receive side is registered by party_hits (hook 3, SYNB 0x41). Cost: a broadcast instead of a unicast, at most a few per second.
The honest alternative is a relay change to add KIND_COOP to SYNH's sessions plus one `shape_ok_send` line (coordinator decision, needs a
redeploy); then `send_wire`'s coop branch is deleted. I did not make it.

### 2.3 Validation on the victim (`Judge::decide`, in this order)
1. room is party / coop and `SM64DS_ROOM_INTERACT` on, otherwise `no_room` / `toggle_off` (nothing ever applies in solo, VS or world rooms);
2. the kind is a victim-side kind (reply kinds and unknown kinds are `bad_kind`); attacker seat 0..3, not me;
3. a fresh SYNL (<= 3 s) from him: in a level (not a menu or cutscene), same level, same area, same act (act compared only when both known);
4. a fresh SYNG position (<= 1.5 s): the position he CLAIMS must be within 400 units of where I last SAW him (`pos_mismatch`);
5. reach, judged on MY truth: where I saw him vs where my body is now, per-kind horizontal radius + vertical window (`reach_rule`, includes
   150 units of lag allowance); a forged claim position cannot extend it because the seen position is used;
6. my body can be hit now: not in a disabled state (the referee's list + level enter, teleport, cannon, talk, respawn), no invincibility,
   not taking damage / no-control, not in a mouth, stage mode normal, no cutscene, not already held (`victim_busy`);
7. eat only: not eaten in the last 150 frames (`eat_busy`);
8. cooldown: one believed claim per attacker per 30 frames (a refused claim does not consume it).
Every refusal is counted per reason (`Counters`, `Stats::refused_by`) and logged as `[phit] REFUSE ...` when `SM64DS_PHIT_LOG>=1`.

### 2.4 Applying (the victim's own console moving its own player)
A believed claim waits for the frame hook (the hub pump runs earlier in the frame), then:
- shove / stomp: `Player::Hurt(me, attackerPos, damageUnits, speed, 0, 0, 0)`: `a5 = 0` on purpose, because Hurt adds 2 to the damage
  when `a5 != 0` (that is the arena's variant select) and that would make a "no damage" shove cost 2. Then `+0x98` (horizontal speed) and
  `+0xa8` (vertical speed) are re-asserted after Hurt, as `apply_hit_on_me` does. Stun length is the ROM's St_Hurt (TUNE, maybe too long).
- eat: `EatHold` (core) holds me for 45 frames (hard stop 90). Each frame my body is placed 60 units in front of the eater's last SYNG
  position along the eater's yaw at claim time, speeds zeroed, `+0x709` (no-control) set. At the end: clear the flag, ChangeState to the
  ROM's neutral state (`data_ov002_0211013c`), spit along the claim yaw, send `eat release`. Abort (no spit) when: the eater leaves the
  level/area or goes silent, I open a menu, the room toggle is off. The eater side keeps an `EatClaimBook`: an ack/release is believed
  only from the seat claimed and within 20 frames of the claim.
  This is a SYNTHETIC hold, not the ROM's `St_InYoshiMouth`. Doing the real one (as `vs_arena_eat.cpp` does) needs an eater Player body
  whose Behavior runs, which party ghost bodies do not have. The cost: the eater sees no swallowed-body animation inside Yoshi, only the
  friend riding in front of him. Fine for a first version; a later "PEAT2" can swap in the ROM state.
- stomp attacker: bounce immediately when the claim is sent (feels right even if the friend refuses), squash shown on the friend's body via
  `squash_scale_for_seat` (optional follower hook 4).

## 3. Forged and odd claims

| Forgery | Result |
|---|---|
| out-of-reach position / far from where I saw him | `out_of_reach` / `pos_mismatch` |
| different level / area / act, or attacker in a menu | `wrong_level` / `wrong_area` / `wrong_act` / `not_in_level` |
| spam | `cooldown`, plus the relay's 10/s source cap |
| claim naming a different victim | never delivered: the hub drops a SYNH whose byte 6 is not my seat; the coop path checks the payload |
| claim "from" me / an out-of-range seat | `bad_seat` (the attacker byte is relay-stamped; the coop path uses the relay-stamped header seat) |
| reply kinds (ack / release) sent as a claim | `bad_kind` |
| hit while I am hurt / invulnerable / in a cutscene / star-get / already held | `victim_busy` |
| fake ack/release to an eater | ignored unless from the victim seat claimed, in time |
| a hostile member shoving within believable range | possible: at most one shove per 30 frames, and my own game applies the hurt rules. The worst case is griefing, never health loss (damage is off by default; with the toggle on, one unit per believed hit, no star loss) |

## 4. Off in solo, VS and world rooms
`port_party_hits_frame` returns before doing anything unless `comms_hub_room_kind()` is party or coop and the interact toggle is on. A VS
arena hub reports `kHubRoomWorld` (arena shape), so VS never sees it. A solo console has no hub. The hub registration (`hub_on_kind`) happens
only in a party room, and `comms_hub_send` additionally refuses SYNH outside a party room. Log line at install says which.

## 5. Interaction with existing bodies
`party.cpp` / `party_cyl.cpp` are unchanged. The friend's body is still solid-push only and still cannot collect or trigger anything; nothing
here registers an attack cylinder against it. The attacker never writes a friend's body (except the cosmetic scale); every hit lands on the
victim's own body.

## 6. HOOK LINES for the local agent (I did not edit existing files)
1. `port/tests/walk_window.cpp` ~line 631 (declarations): `void port_party_hits_frame(void);`
2. same file ~line 18236, right AFTER `port_party_stars_frame();`: `port_party_hits_frame();`
3. DONE in code (FOLD9B2): party_hits registers SYNB 0x41 itself with `coop_ctl_register` when the room is coop (no dispatcher edit needed). Coop only.
4. optional, `gw_follow` / the party follower after it poses a party body: `int sy = port::party_hits::squash_scale_for_seat(seat);
   if (sy != 0x1000) *(int*)((char*)body + 0x84) = sy;` (+0x84 is the Y scale, `party.cpp draw_diag` prints 0x80..0x88). Restore 0x1000
   otherwise.
5. build lists: add `port/hal/party_hits.cpp` to whatever lists `party_stars.cpp` (the walk_window target in `port/CMakeLists.txt` or the
   MSVC project); `party_hits_core.h` is header-only.
6. launcher / lobby (ROOMSET1): export `SM64DS_ROOM_INTERACT` and `SM64DS_ROOM_DAMAGE` from the plan; unset means the defaults (1 / 0).
Nothing in `src/` or `include/` is touched. Relay: no change. `hub_presence.h`: no change.

## 7. Things I could not check (Linux VM)
- MSVC build; whether `vs_arena_state_index` links in a party build without the arena (it is a plain function in `vs_arena.cpp`; if not,
  copy the 78-entry state table lookup).
- Every TUNE value, the attack windows and reach numbers against real animations, the stun length, `+0x709` as the right "no control" switch for
  the hold, the bounce speed, whether `Hurt` with `b=0` plays any unwanted voice/particle (it skips the voice when `b==0`, from the source).
- That the pump (`comms_hub_pump`) runs before `port_party_hits_frame` (it does in `port_hub_presence_frame` per `hub_presence.cpp` comments).
- Real-wire behaviour: the proof builds a SYNH with the repo's own encode/validate and checks the relay constant, but nothing went through a relay.

## 8. Host proof (runs here)
`python3 port/tools/party_hits_proof.py` -> 87 rows, 0 failed. Pairs every refusal with a control that removes just that check and shows
the forged claim then lands (out-of-reach x4, level/area/act/menu, toggle, no room, busy, in-mouth, forged position, cooldown, eat re-arm).
Also: pose/contact rows, eat hold and claim-book rows, wire rows against the real `hub_party_hit_validate`, the state table cross-check.

## 9. Live rows for a local agent
`python3 port/tools/party_hits_proof.py --live-rows` prints them (L1..L11: punch, pound/stomp, toggle off, no room, forged far claim with
`SM64DS_PHIT_LAB=forge:melee:900` and its control `SM64DS_PHIT_NOREACH=1`, level, repeat, damage toggle, eat, coop, solid body unchanged).
Log tag `[phit]`; `SM64DS_PHIT_LOG=1` prints refusals and claims sent.
