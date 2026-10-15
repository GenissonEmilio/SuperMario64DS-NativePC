# Minigame names: runtime checks

[names.md](names.md) marks some names **medium** or **low** confidence. The code
settles their mechanics but not their game-facing meaning: which graphic a banner
id shows, what an animation looks like, or what a flag means to the player. This
page lists what to watch, in melonDS, to settle each one. None of it changes code.
A confirmed or corrected name is then a plain in-place symbol rename, like the
ones this pass made.

## Setup

The repo already drives the melonDS GDB stub from Python (`tools/trace/rsp.py`).
`tools/trace/README.md` has the exact `melonDS.toml` keys and the stub's quirks:
one client per launch, detach before closing, never send a break.
1. Start melonDS with JIT off and the GDB stub on (port 3333), then load the EU ROM.
2. Enter the minigame menu: the title screen's minigame option, or the rec-room
   objects in the castle.
3. Attach with a short script built on `rsp.py`. It sets the breakpoints below,
   and on each hit logs `pc`, `r0`-`r3` and `lr` (resolve the caller with
   `tools/trace/symindex.py`), then continues.
   - `tools/trace/gdb_harness.py --names <symbol,...>` does this for symbols it can resolve.
   - `tools/trace/behavior.py` records inputs and outputs per call.
4. Play the game named in each row and note what is on screen when the breakpoint fires.

All addresses are EU (ASMP). ov004, ov005 and ov006 are resident only while a
minigame or the menu runs. ov005 and ov006 share 0x020bfec0, so a breakpoint in
one is meaningless while the other is loaded.

## What to watch

| Question | Breakpoint (symbol, address) | Log | Play |
|---|---|---|---|
| What each shared state (0..19) shows | `Mg_SetSharedState` 0x020b0a54 | r0 = state | Win and lose a round each in Cup, Memory, Card, Roulette, Snowball, and one timed game (Bomroom or Coin). Note the banner and the jingle. |
| Which graphic each banner id is | `Mg_ShowBanner` 0x020b0cac | r0 = file id, r1/r2 = x/y | The same plays. Fill in the "Guess" column of names.md's state table. |
| One-shot banner override (e.g. "Perfect") | `Mg_SetNextBanner` 0x020b67e8 | r0 = id | Memory with no misses (expect 6); Snowball (4, 0xe). |
| What jingle ids 1, 3 and 4 sound like | `Mg_PlayJingle` 0x020ae274 | r0 = id | Any game; listen at the hit. |
| `StartRound(state)` is really "start or restart a round" | `Mg_CallStartRound` (see symbols.txt) | r1 = finished state | Cup: lose a life (the game calls slot 18 with -1 itself) versus clear a round. |
| `OnNewRecord` fires as the record banner goes up | `dScMgBase_c::OnNewRecord` 0x020b2990, plus the Cup/Sound/Teresa/3DEsp overrides | hit or not | Beat a saved record in Cup or Sound. |
| `OnDealDone` fires when the dealer finishes dealing | `dScMgBase_c::OnDealDone` 0x020b298c, plus the BSC/Card/Memory/Memory2 overrides | hit timing | Memory or Card: watch the dealer deal. |
| What Presenter animations 1, 2, 4 and 7 look like | `Presenter_PlayAnim1/2/4/7` 0x020c2664 / 0x020c271c / 0x020c2440 / 0x020c2594 | hit | Sound: a match (2), a mismatch (1), a loss (4), a win (7). Cup: a win. |
| `mCredits` really means lives or coins | `Mg_LoseCredits` / `Mg_GainCredits` (symbols.txt) | r0 = amount | Cup (lives), Card (coins). |
| Is "bet" the right word for the coin animator | `MgCoins_PlaceBet` 0x020b63a0, `MgCoins_AddBetCoin` 0x020b6234 | r0 = count | Card, BSC and MCarlo: tap the top-left box. Memory: start of round. |
| The menu's "new" badge | `MiniGm_IsNew` 0x020c007c, `MiniGm_ClearNew` 0x020c0030 | r1 = row | Open a never-played game's page, play it once, and come back. |
| `IsAltVariant` = the second menu row of a game | `dScMgBase_c::IsAltVariant` 0x020ad660 (Amida overrides) | r0 on return | Play both menu entries of a duplicated game (Coin, Panel, Sound, Snowball). |
| Wiggler draw passes A and B | `Wiggler_DrawPassA` 0x020ea914, `Wiggler_DrawPassB` 0x020eac38 | which runs for which part | Hanachan; freeze a frame after each pass. |
| Trampoline2 item kinds 0..8 | `TrmpItem_SetMotion0..8` (0x020cdf20 is kind 0) | hit and kind | Trampoline2; note which item moves which way. |
| Pachinko's flyer, background scroller and hit bursts | `PachinkoFlyer_State0`, `PachinkoBg_Start`, `PachinkoFx_Spawn` (see ov006 symbols.txt) | hit timing | Pachinko: note what crosses the top of the board when the flyer starts, and what scrolls when the background starts. |
| Teresa's exclaim, found mark, rising icon and reveal sprites | `dScMgTeresa_c::Exclaim_Spawn`, `FoundMark_Spawn`, `RisingIcon_Spawn`, `Reveal_State0` (see ov006 symbols.txt) | hit timing | Teresa: find a Boo and note each sprite as it appears. |
| The graph-callback hooks (slots 22-25) | `dScMgBase_c::OnGraphCallback0..3`, or the arm9 dispatchers `func_02019390` / `func_02019404` / `func_02019144` / `func_02019100` | call order within a frame | Any game. Name the callbacks by frame phase: before draw, after the OAM flush, and so on. |

## Observed so far (melonDS 1.1, EU ROM, 2026-10-15)

Logged with `build/melonds/mgwatch.py` (local, not in the repo) plus window
screenshots, over two games of Wanted! (Luigi):
- `MiniGm_IsNew` polls menu rows 0-3 and 7 every frame; picking Wanted! ran `MiniGm_ClearNew(3)` once. The badge names are confirmed, and Wanted! is row 3.
- Banner 0xd, shown at (0x80, 0xa8) at each new level, is the "LEVEL n" text.
- State 0x12 shows banner 8, the "GAME OVER" text, with jingle 3.
- State 16's banner 7 at (0x80, 0x14) is the "HIGH SCORE" header over the record table.
- A new high score went through state 17. `OnNewRecord` fired 120 frames in, then banner 5 and jingle 1 played. A tied score went to state 16 with no `OnNewRecord`.
- `Mg_CallStartRound` passed the finished state's number (1). This confirms `StartRound(state)`.
- Banners 0 and 2, at (0x80, 0x6c) and (0x80, 0x9c) after the record table, are the PLAY AGAIN and QUIT buttons.
- Mario's page: the ladder game (Amida, menu row 0) and the propeller Shy Guy game (row 4) both open with banner 0xd "LEVEL 1" and end in state 0x12, then state 16. Banner 0xd is the shared level banner; each game places it (y 0xa8 in Wanted!, 0x60 in these two).
- Wario's slingshot game is Pachinko (`dScMgPachinko_c`, menu row 2). The flyer is Lakitu on his cloud crossing the top; `PachinkoFlyer_State0` runs every frame while its delay counts down. The four targets are the four flowers along the bottom: at game end `PachinkoFx_Spawn` fires at y 0xa0000 and x 0x38000/0x68000/0x98000/0xc8000, and the flowers are gone. It ends straight in state 16, then state 17 on a new record. Banner 5 is the big rainbow "HIGH SCORE".
- Snowball (Wario's page, menu row 6): banner 0xc is the red "START". At time up the game calls `Mg_SetNextBanner(0xe)` and then enters state 3: banner 0xe is "TIME IS UP" over PLAY AGAIN / QUIT. `IsAltVariant` is polled at start.
- The flower game "loves me, loves me not" (Yoshi's page, menu row 7): banners 0x10 and 0x13 alternate per petal and are "LOVES ME" (red) and "LOVES ME NOT" (purple). It ends in state 0xc, which shows PLAY AGAIN / QUIT side by side (banners 0 and 2 at x 0x40 and 0xc0) under the result. That state is now named `MG_STATE_RETRY_PROMPT_WIDE`; it was `CONTINUE_PROMPT`.
- The stub accepts one client per melonDS launch. A second attach after `D` timed out and left the game halted. Restart melonDS (and re-enable `[Instance0.Gdb]`, which melonDS can rewrite to false) before reattaching.
- State 1, explained: Luigi enters it only when `mLevel % 5 == 4`, so after every fifth level, and otherwise restarts the round directly. A four-level game never entered it. On screen it is the fade into level 6, with the poster halves split aside. `MG_STATE_NEXT_ROUND` is confirmed. A nine-level game showed it exactly once, between level 5's and level 6's banners.
- PLAY AGAIN after game over calls `Mg_CallStartRound` with 0x12, the finished state's number. Retry also goes through `StartRound(state)`.

## Recording the answer

For each row, put the observed meaning in names.md's row and raise or lower the
confidence. A changed name goes through the same in-place rename the pass used:
- symbols.txt, the manifests, attribution and `symbols/actor_renames.tsv` lineage
  rows for mangled members
- then a TU object compare (identical modulo the rename) and the usual gates
