# Minigame names: evidence and confidence

The cartridge carries class names (RTTI) but no method, field or helper names
for the minigame code. Every name on this page is coined in this tree. Each one
records:
- the evidence it rests on
- a confidence level
- for anything below **high**, what to watch at runtime to settle it (see
  [runtime-checks.md](runtime-checks.md))

Confidence levels:
- **high**: the body and every caller agree, and no other reading fits.
- **medium**: the role is clear from the code, but the exact meaning rests on
  something not yet observed, such as which banner graphic a file index shows.
- **low**: a best guess from one caller or one body. Rename freely when better
  evidence turns up.

A rename is checked by compiling every minigame TU before and after. The objects
must be identical apart from the renamed symbols: same section bytes, same
relocation offsets and types, same targets.

## dScMgBase_c virtual slots 18-35 (renamed 2026-10-14)

Until this rename, slots 18-30 carried `dActor_c` method names, copied across by
vtable slot index alone. `dActor_c` and `dScMgBase_c` share only `dBase_c`,
which declares no virtual, so a slot number means nothing between them.
- **Where they came from.** The actor names are themselves imports from an ASM-hack
  symbol file. An untooled "vtable-identity" pass (#877) wrote them onto the scene
  classes as `recovered from vtable slot identity` comments.
- **How they spread.** `tools/reconcile_names.py` (#944) turned those comments into
  `// recovered name:` lines. The slot campaign of 2026-08-31 (#2088-#2112) then made
  them symbols. Slots 31 and 32 got `Kill` and `AfterClsn` the same way, from
  `dBgActor_c` and `dPathLiftActor_c`.
- **Reach.** No other non-actor class has a vtable longer than 18 slots, so the
  transplant is confined to these 33 classes.

| Slot | Old name | New name | Confidence | Evidence |
|---|---|---|---|---|
| 18 | `OnYoshiTryEat(int)` | `StartRound(int)` | high (seen) | `dMgState_c` calls it with the finished state's number when a banner ends. Games call it with -1 to retry after a lost life (Cup's `StateFinish`). The overrides reset their round, and Memory's calls `ResetGame`. |
| 19 | `OnTurnIntoEgg(int)` | `CanEndState(int)` | high | Every `dMgState_c` behaviour handler polls it with `mState` and waits for nonzero. The overrides return 0 while their own reaction animation plays. |
| 20 | `Virtual50` | `OnNewRecord` | medium | Its one caller is state 17's behaviour, the results path taken when the new-record flag is set, 120 frames in. Cup and Sound play a win animation; 3DEsp and Teresa free banner slot 8. |
| 21 | `OnGroundPounded` | `OnDealDone` | medium | Its one caller is `Dealer_StateDeal` (unit020bfec0): the dealer's deal animation calls it on its last repeat. Memory and Memory2 then place their bet coins (`MgCoins_PlaceBet(mMaxMisses)`); BSC raises its bet from the score tier. |
| 22 | `OnAttacked1` | `OnGraphCallback1` | high (mapping) | `graphCallback_c::GraphCallback1` forwards to it. What the engine's callback 1 is for is not known. |
| 23 | `OnAttacked2` | `OnGraphCallback0` | high (mapping) | `GraphCallback0` forwards to it. The arm9 caller `func_02019390` skips its OAM reset and 3D flush when the callback returns 0. |
| 24 | `OnKicked` | `OnGraphCallback2` | high (mapping) | `GraphCallback2` returns its result. The base body saves or restores the screen on an edge of `mMenuOpen`. |
| 25 | `OnPushed` | `OnGraphCallback3` | high (mapping) | `GraphCallback3` forwards to it. The base returns `mMenuOpen == 0`. |
| 26 | `OnHitByCannonBlastedChar` | `GetDisplayMode` | high | It returns 0, 1 or 2 from Base, Single3DBase and D3DBase. `BeforeInitResources` enables the 3D engines for any nonzero value, and the sprite wrappers choose a screen on 2. |
| 27 | `OnHitByMegaChar` | `OpenMenu` | high | It sets the three menu items to their slide-in positions and sets `mMenuOpen = 1`. `BeforeBehavior` calls it on A, B, Select or Start with the stylus up. |
| 28 | `OnHitFromUnderneath` | `CloseMenu` | high | It clears `mMenuOpen` and resets the options panel. It runs from the resume item, or from a button press with nothing highlighted. |
| 29 | `OnAimedAtWithEgg` | `SaveScreenForMenu` | high | It saves POWCNT1, both DISPCNT BG bits, 0x400 bytes of palette and 0x2000 bytes of OBJ VRAM, then draws the menu graphics. It runs on the rising edge of `mMenuOpen`. |
| 30 | `OnAimedAtWithEggReturnVec` | `RestoreScreenFromMenu` | high | It is slot 29's exact mirror and runs on the falling edge. It returns nothing (the bytes settle `void`). |
| 31 | `Virtual7C` (was `Kill`) | `InitSubScreenBg` | high | It sets up sub-engine BG1 and loads the language's character file plus screen map 0x5b. |
| 32 | `Virtual80` (was `AfterClsn`) | `InitMainScreenBg` | high | It does the same for main-engine BG1 with screen map 0x67. |
| 33 | `Virtual84` | `InitEngines` | high | It sets graphics modes, VRAM banks, the BG/OBJ character files and palettes, and registers the graph callback. (`InitDisplay` was already taken by Trampoline's own method.) |
| 34 | `Virtual88(int,int,int,int)` | `PaintSquare` | high | It paints a size x size square of one 4bpp palette index into the brush's BG layer. The line rasteriser `Mg_DrawLine` calls it once per point. |
| 35 | `Virtual8C` | `IsAltVariant` | medium | It returns `(param1 & 0xff) != 0`, and Amida asks `== 1`. Coin, Panel, Sound and Snowball branch on it to pick their second variant's assets. Which menu rows set that byte has not been checked against the menu table. |

Still open for the graph callbacks: what the engine's four `dGraph_c::callback_c`
slots are for. The arm9 dispatchers are `func_02019390` (slot 0),
`func_02019404` (slot 1), `func_02019144` (slot 2) and `func_02019100` (slot 3).
They are named from their call order by the frame loop once the dispatchers are
named, and not before.

## ov004 runtime helpers and globals (renamed 2026-10-14)

These are free functions and data of the shared ov004 runtime
(`src/minigames/d_s_mg_base.cpp`). Each was named from its body, and its body's
comment in that file carries the detail. The `Mg_` prefix marks the minigame
runtime. `Ov004_` marks a thunk that only forwards to an arm9 routine.

**High confidence.** The body says exactly this, and the callers agree.

| Area | Names (old address) |
|---|---|
| Scene singleton | `gMgScene` (data_ov004_020beb68): set by the `dScMgBase_c` constructor, cleared by its destructor. `gMgMenuRow` (arm9 data_0209b308): the 0x34-byte menu row the select menu copies in. Word 2 is the scoring mode, words 5/6 the record defaults, word 11 the music mode. |
| Scores and records | `Mg_SubmitScore` (020adb1c), `Mg_GetSubmittedScore` (020adbc0), `Mg_SetSessionBest` (020ada40), `Mg_GetSessionBest` (020adafc), `Mg_IsNewRecord` (020adbe0), `Mg_ClearNewRecordFlag` (020ada20), `Mg_SetTopRecord`/`Mg_GetTopRecord` (020adc00/020adc1c), `Mg_GetSaveTable` (020adc3c), `Mg_FindRecordRank` (020ad6fc), `Mg_GetLastRecordRank` (020ad6f4), `Mg_RankSessionBest` (020ad940), `Mg_CommitRecords` (020ad90c), `Mg_SaveRecordPair` (020ad79c), `Mg_GetSavedRecord0/1` (020ad8b8/020ad878), `Mg_GetScoringMode` (020ae1a8) |
| Shared state machine | `Mg_SetSharedState` (020b0a54), `Mg_CallStartRound` (020b29a0) |
| Banners | `Mg_ShowBanner` (020b0cac), `gMgBanners` (data_ov004_020bf648, three 0x134-byte records) |
| Effect pool | `sMgFxPool` (0x40 records), `sMgFxUpdateTable`/`sMgFxDrawTable`, `Mg_SpawnFx`, `Mg_InitFx`, `Mg_UpdateFx`, `Mg_DrawFx` |
| Pause menu | `Mg_UpdateMenu` (020aeb24), `Mg_DrawMenu` (020ae858) |
| Drawing | `Mg_DrawSpriteList`, `Mg_DrawSpriteListEntry`, `Mg_CopySpriteList`, `Mg_DrawNumber` (8 px digits), `Mg_DrawBigNumber` (16 px digits), `Mg_DrawNumberRotated`, `Mg_DrawTally`, `Mg_DrawScoreTally`, `Mg_DrawRecordTable`, `Mg_DrawRecordRow`, `Mg_DrawPrompt`, `Mg_DrawLine`, `Mg_InitBrush`, `sMgSpriteTables` (per-language sprite lists) |
| Display | `Mg_ResetBgOffsets`, `Mg_DisableAllBanks`, `Mg_GetTopScreenYOffset`/`Mg_SetTopScreenYOffset` with `sMgTopScreenYOffset`, `Mg_ResetOverlays` |
| Music | `Mg_GetMusicMode`, `Mg_GetMusicTrack`, `Mg_StartMusic`, `Mg_InitMusic`, `Mg_RestoreMusic` |
| Files and memory | `Ov004_LoadFile`, `Ov004_LoadFileByPath`, `sMgWorkBuffer`/`Mg_GetWorkBuffer` (the 0x4000-byte buffer `BeforeInitResources` allocates) |

**Medium confidence.** The mechanics are certain, but the game-facing meaning is not observed yet.

| Name (old) | Why it is not high |
|---|---|
| `Mg_PlayJingle` (020ae274) | It starts sequence `id` on `Sound::PlaySub`'s player and sets the music volume target to 0x40. The callers pass 1, 3 and 4 when a banner goes up. "Jingle" is inferred; the IDs are not mapped to sounds. |
| `Mg_LoseCredits`, `Mg_GainCredits`, `Mg_SubCredits`, `Mg_AddCredits`, `Mg_DrawCredits`, `Mg_DrawCreditsIcon` | They work on `mCredits`, clamped to 0..9999, which `BeforeBehavior` eases `mCreditsShown` toward for display. Cup gives 2 and loses one per wrong pick before game over; Memory caps the value at 3. A wider "credits" reading is possible in the gambling games. |
| `Mg_ResumeSharedState` (020b0a38) | It clears `mStateController.unk_024`, which lets `BeforeBehavior`/`BeforeRender` step and draw the controller again. What sets the flag (state 6's behaviour) reads as "hand control back to the game". |
| `Mg_CheckBannerButton` (020b0b1c) | It hit-tests a touch against a shown banner whose graphic id is `arg` (0x40 x 0x13 box), or reports the banner's finish. States use ids 0, 1 and 2 as retry/quit buttons. |
| `Mg_DrawBlinkSprite` (020b0d8c) | It alternates sprite words 0x11/0x12 on the 40-frame counter. Which graphic that is has not been seen. |
| `Mg_DrawTopLeftCounter`, `Mg_DrawTopRightCounter`, `Mg_DrawTopCenterCounter`, `Mg_DrawStockCounter` | HUD numbers with a label sprite, named by screen position. The label graphics (sprite words 8, 4, 3 and 0/1) are not identified. |
| `Mg_RenderSub*`, `Mg_RenderMain*`, `Mg_RenderSpriteBoth*`, `Mg_RenderSpriteOn*` | OAM wrappers named by screen and arguments. `OAM::Render`'s first argument is read as main=0 / sub=1 from `RenderOamMainScreen` and `Hud_RenderSprite`; that reading has not been checked on hardware. |

**Low confidence.**
- `Mg_GetLevel` (020b1aec) returns `mLevel`, which the constructor copies from menu-row word 12. The name only describes where the value lives. Jump reads it `% 5`.

## dScMgBase_c fields (renamed 2026-10-14)

The compiler located every use for these: each field was renamed in the header,
and only the lines the compiler then rejected were changed.

| Offset | Old | New | Confidence | Evidence |
|---|---|---|---|---|
| 0x050 | `unk_050` | `mTallyShown` | high | `Mg_DrawTally` keeps the last tally it drew here and compares the new count against it. |
| 0x054 | `unk_054` | `mTallyAnimTimer` | high | It starts at 1 when the tally rises and drives the newest icon's blink and the close-up at five. |
| 0x060 | `unk_060` | `mRecordRevealTimer` | high | `Mg_DrawRecordTable` reveals a row every 0x14 frames as it counts, then loops it over 0x64..0x8b. `Mg_SetSharedState` zeroes it. |
| 0x064 | `unk_064` | `mLastRecordRank` | high | `Mg_RankSessionBest` stores the rank (0..4, or -1); `Mg_GetLastRecordRank` returns it. |
| 0x0a0 | `unk_0a0` | `mSubBgBank` | medium | Only `dScMgD3DBase_c` uses it: it saves `func_02053eb0()` and passes the value to `GX::SetBankForSubBG`. |
| 0x0a4 | `unk_0a4` | `mRunDuringFade` | high | `BeforeBehavior` skips the frame while it is 0 and the fader is busy. 3DEsp, Coin, Curling and Curling2 set it to 1. |
| 0x0a8 | `unk_0a8` | `mCredits` | medium | It is clamped to 0..9999 by `Mg_Add/SubCredits`, and 0 sends the shared states to game over. Cup uses it as lives (2); Card calls it the coin counter. |
| 0x0ac | `unk_0ac` | `mCreditsShown` | high | `BeforeBehavior` eases it toward `mCredits`, and `Mg_DrawCredits` draws it. |
| 0x0b8 | `unk_0b8` | `mBestHudScore` | high | Cup, BSC and Card raise it to `mHudScore` whenever that is higher. `BeforeInitResources` zeroes it. |
| 0x0bc | `unk_0bc` | `mLevel` | medium | The constructor seeds it from menu-row word 12. Wins increment it (capped at 9998) and a loss can reset it. Amida reads it as the level; the level banner (style 12) draws `mLevel + 1`. |
| 0x0c8 | `unk_0c8` | `mMusicTrack` | high | `BeforeInitResources` copies menu-row word 10 into it; `Mg_GetMusicTrack` returns it. |
| 0x462c | `unk_462c` | `mMenuScreenSaved` | high | `OnGraphCallback2` latches `mMenuOpen` into it after saving or restoring the screen. `BeforeBehavior` waits while it is set. |
| 0x4630 | `unk_4630` | `mMenuDisabled` | high | `OpenMenu` does nothing while it is set. `Mg_CheckBannerButton` sets it when a quit button (1 or 2) is touched. |
| 0x4648 | `unk_4648` | `mQuitFadeStarted` | high | The menu's quit item sets it once, after starting the fade and `func_02012dd0(0x3c)`. |
| 0x464c | `unk_464c` | `mSubmittedScore` | high | `Mg_SubmitScore` writes it; `Mg_GetSubmittedScore` returns it. |
| 0x4654 | `unk_4654` | `mSessionBest` | high | `Mg_SetSessionBest` and `Mg_GetSessionBest` write and read it. |
| 0x4658 | `unk_4658` | `mRankedScore` | high | `Mg_RankSessionBest` moves the session best here before clearing it; the record table blinks it. |
| 0x465c | `unk_465c` | `mNewRecord` | high | `Mg_SetSessionBest` sets it on beating the top record; `Mg_IsNewRecord` returns it. |

Left unnamed:
- `unk_0c2`: only the constructor writes it (1).
- The top record at +0x4650: still header padding, read raw (see themes.md, theme 1).

## dMgState_c: the shared banner states (renamed 2026-10-14)

Handlers that serve one state are named by state number (`State05_Enter`,
`State05_Update`). That much is certain from the enter table in `SetState` and
from which behaviour each enter handler installs. Shared handlers are named by
what they do:
- `UpdateRetryFadeOut`: the retry button's fade-out.
- `UpdateGameOverToMenu`: the Game Over banner, then a fade to the menu (scene 5).
- `UpdateWaitThenResume`
- `UpdateFadeThenResume`
- `UpdateAfterHidden`
- `UpdateRetryPrompt`: states 12/13, which show buttons 0 and 2.
- `DrawRecordTable`, `DrawScore`, `DrawScoreDigits`

What a state means to the player depends on which banner graphic its file index
shows, and that has not been observed. The table records the mechanics (certain)
and a guess (unconfirmed):
- **Banner** is the file index passed to `Mg_ShowBanner`. `*` means a game can
  override it once through `Mg_SetNextBanner`.
- **Jingle** is the `Mg_PlayJingle` id, or an `s` + `Sound::PlayBank2_2D` id.

| State | Enter: banner, jingle, timer | Then | Entered by | Guess |
|---|---|---|---|---|
| 0 | 3\*, 4, 180 | fade, restore music, `StartRound(0)` | a win in Cup, Amida, Teresa, Panel, Sound, Jump | round cleared, play on |
| 1 | none, none, 15 | fade, `StartRound(1)` | Hanachan, Luigi | silent next round |
| 2 | 5, 1, 180 | fade, restore music, `StartRound(2)` | none found | |
| 3 | 8\*, 3, 60 | retry (button 0) / quit (1, 2) buttons | Snowball, with banner 0xe | game over, retry? |
| 4 | 9\*, s0x136, 120 | `StartRound(4)` | a win in Memory, Card, BSC, MCarlo, Roulette | you win |
| 5 | 0xa, s0x137, 120 | game over at 0 credits, else `StartRound(5)` | a loss in the same games | you lose |
| 6 | 0xa, s0x137, 120 | hides the controller and hands back to the game | none found | |
| 7 | 0xb, none, 120 | `StartRound(7)` | Card | draw? |
| 8 | none, none, 0 | `StartRound(8)` at once | Roulette's tie | silent tie |
| 9 | 3\* on the top screen, 4, 180 | `StartRound(9)` | none found | |
| 10 | 8 (style 0xb), 3, 180 | retry / quit buttons | none found | |
| 11 | none, 1, 180, draws the score | fade, `StartRound(11)` | none found | |
| 12, 13 | none, none, 10 | retry / quit buttons | Flower (12) | continue? |
| 14 | 5, 1, 180, draws score digits | fade, `StartRound(14)` | none found | |
| 15 | 0xe, 3, 60 | fade, `StartRound(15)` | none found | |
| 16 | 7 at (0x80, 0x14), none, 120 | record table, touch to go on; `Mg_CommitRecords` | the timed games (Bomroom, Coin, Curling, Pachinko, Smartball); Wanted! after game over | results. **Seen:** banner 7 is "HIGH SCORE" |
| 17 | as 16, 300 frames | record table; banner 5, jingle 1 and `OnNewRecord` 120 frames in | 16 or 18/19, when `Mg_IsNewRecord` | results, new record. **Seen** on a new Wanted! high score |
| 18 | 8, 3, 180 | touch or timeout, then 16 or 17 | game over in Cup, Panel, Luigi, Teresa, Jump, Jump2, Slots, Trampoline/2, Hanachan | game over. **Seen:** banner 8 is "GAME OVER", jingle 3 |
| 19 | 3\*, 4, 180 | touch or timeout, then 16 or 17 | Snowball (banner 4 override) | finished, goal |

**The `MgSharedState` enum** (`include/dMgState_c.h`, 2026-10-15). Every
`Mg_SetSharedState(n)` call now passes a name. Each name describes the state's
mechanics from the table above, and the enum marks the confidence of its
meaning word per entry:
- **high:** `RETRY_PROMPT`, `LOSE`, `RESULTS`, `RESULTS_NEW_RECORD`.
- **medium:** `ROUND_CLEAR`, `NEXT_ROUND`, `WIN`, `TIE`, `CONTINUE_PROMPT`,
  `GAME_OVER`, `FINISHED`.
- **low:** the rest, which no caller enters or one game enters.

The runtime check on `Mg_SetSharedState` settles the medium and low words. A
corrected name is a plain enumerator rename; the values are fixed.

Medium-confidence helpers:
- `Mg_SetNextBanner` (020b67e8) sets the one-shot override `sMgNextBanner` (0x1d means none).
- `Mg_SetPerfectBanner` (020b67f8) is `Mg_SetNextBanner(6)`. Memory calls it before state 4 when there were no misses.
- `Mg_ResetBannerLayout` (020b8a8c) seeds every state's banner x/y.
- `Mg_PlaceBannersOnTopScreen` (020b682c) moves several of them by the top-screen offset.
- `Mg_ResetRecordBannerPos` (020b6808).
- `sMgBannerButtonsArmed` is set by the layout reset and cleared by `Banner_Press`; button touches are ignored while it is 0.

## The credit coins (unit020b4aa4) and the options panel (renamed 2026-10-14)

**The coins, medium confidence.** `unit020b4aa4` animates up to twenty coin icons, drawn
with the credits icon (`Mg_DrawCreditsIcon`), between the credits counter at
(12, 12) and the top screen. Each sequence moves `mCredits` one coin at a time:
- `Coin_Launch` and `Coin_Spend` take a credit.
- `Coin_MoveThenBank`, `Coin_MoveThenRefund` and `Coin_FlyToCounter` give one back.

The entry points are:
- `MgCoins_Reset`, `MgCoins_Update` and `MgCoins_Draw`.
- **Bets.** `MgCoins_AddBetCoin` runs on a touch in the top-left box, which `MgCoins_Draw` marks with a button. `MgCoins_PlaceBet`, `MgCoins_RaiseBet`.
- **Losses.** `MgCoins_StartLose` and `MgCoins_StartLoseOneMore` send coins away, up from x 128.
- **Spending.** `MgCoins_StartSpend`.
- **Refunds.** `MgCoins_StartRefund` returns coins to the counter.
- **Payouts.** `MgCoins_StartPayout` lays coins out five wide and flies them to the counter, then submits `mCredits` as the score in scoring mode 2.

Games watch `gMgCoinsStatus`: 1 when a sequence is done, 2-4 while one runs.
`gMgCoinsShowDelta` makes the sequences draw their signed count.

What is not settled is the word "bet". The touch button and the sounds (0x149-0x14e)
fit Card, BSC and MCarlo, but Memory and Memory2 also call `MgCoins_PlaceBet`
with their remaining misses.

**The options panel, high confidence.** Pass 2 (2026-10-15) made the
`dMgPsOpt_c` helpers real members: `dMgPsOpt_c::Open`, `Close`, `Update`, `Draw`
and `IsIdle` (formerly `PsOpt_*` on a `char *` view). `PsOpt_PlaceIcon` stays a
free function that takes the icon.

## dScMiniGm_c, the select menu (ov005, renamed 2026-10-14)

High confidence, from the bodies:
- **Drawing and loading:** `MiniGm_LoadGroupIcons` (the nine icons of the current group), `MiniGm_LoadIconPalette`, `MiniGm_DrawRecords` (the record under each icon, by scoring type), `MiniGm_DrawNumber`, `MiniGm_DrawNewBadges`.
- **Input:** `MiniGm_UpdateGroupTabs` (the four tabs down the left), `MiniGm_UpdatePageArrows`, `MiniGm_UpdateExitButton`, `MiniGm_UpdatePageScroll`, `dScMiniGm_c::UpdateGameSelect` (a touch on an icon copies its row into `gMgMenuRow` and fades to its scene).
- **Records and saving:** `MiniGm_HasSecondPage`, `MiniGm_IsUnlocked`, `MiniGm_GetSaveTable`, `MiniGm_GetRecord`/`MiniGm_SetRecord`, `MiniGm_SaveIfDirty`.
- **Tables:** `sMiniGmRows` (the 36-row menu table), `sMiniGmGroupBases`, and the icon, badge, digit and arrow tables.
- **arm9 globals:** `gMgGroupBase`, `gMgLastSelected`, `gMgMenuPage`, and `gMgSaveDirty` (set whenever a record changes; the menu saves while it is set).

Medium confidence:
- `MiniGm_IsNew` and `MiniGm_ClearNew` wrap a save flag. The menu draws a blinking badge over an unlocked game while it is set, and leaving a game clears it.
- "New" is the reading; a "played" or "cleared" flag would look the same. Check by watching a never-played game's icon.

## unit020bfec0: shared 3D components (renamed 2026-10-14)

This unit holds the 3D helper objects the Single3D games embed. None has RTTI, so
each one is named after the games that construct it and what it draws:

| Prefix | Built by | What it is | Confidence |
|---|---|---|---|
| `Camera_` | all | `Camera_UpdateMatrices` (already named), screen projection, and four eye/target presets: `Camera_InitFlowerView`, `Camera_InitPresenterView`, `Camera_InitSlotsView`, `Camera_InitDealerView` | high |
| `FlowerFace_`, `FallingPetal_` | Flower | The face that watches the player pull petals. It has idle and two reaction animations, and up to 22 falling petals with gravity and spin. | high |
| `Presenter_` | Cup, Sound | A character with 8 body, 8 texture-sequence and 8 texture-transform animations, whose head follows the stylus (`Presenter_TrackStylus`). | high (structure) |
| `Dealer_` | Card, BSC, MCarlo/2, Memory/2, Roulette | The card dealer: deal, collect, reveal, swap, win and lose animations, plus the table lamps that cycle colours. | medium |
| `Mascot_`, `MascotWalker_` | Slots; inside `Dealer_` | One small character model (files 0x205/0x206). Slots shows it turning its head toward a target. Behind the dealer it walks a waypoint path and pauses at random. | medium |

The `Presenter_PlayAnimN` / `Presenter_StateAnimN` names give the animation slot
index (N = 0..7), which is certain. What each animation looks like is not
observed yet:
- Cup's `OnNewRecord` comment calls slot 7 the win animation.
- Slot 7 plays with sound 3 and slot 2 with sound 0x1c.

The dealer's deal, collect, reveal and swap names follow the animation order
and the sounds (0x139-0x142) and are medium confidence.

## Per-game helpers (ov006)

Each game's helpers are named from their bodies and their pointer-to-member state
tables. The tables are resolved through the relocations of the records each
`__sinit` copies, which makes a state handler's index certain. A name is high
confidence unless listed below the game.

**Sound (`dScMgSound_c`).** This is the note-pair memory game. A touched note plays its
pitch, two matching notes clear, and a mismatch closes them again.
- **Notes:** `DealNotes`, `ResetRound`, `UpdateNotes`, `CheckPair`, `CheckRoundOver`, and `RevealUnmatched` on a loss.
- **Note states 0-4:** `Note_StateWait`, `Note_StateOpening`, `Note_StateOpen`, `Note_StateClosing`, `Note_StateScroll`.
- **Pops:** `SpawnPops` launches the burst sprites, and each of the 13 pop kinds has `PopNN_Update`, `PopNN_Start`, `PopNN_Fly` and `PopNN_Finish`. The kind number is the index in the dispatch table; the look of each kind is not observed.
- **Win and loss:** the win plays `Presenter_PlayAnim7` (a cheer, medium) and the two win sprites (`Start/Update/DrawWinSpriteA/B`); the loss plays `Presenter_PlayAnim4` (disappointment, medium).

**Teresa (`dScMgTeresa_c`, Hide and Boo Seek).**

The four state tables are named by index, which is certain:
- `BigBoo_State0..8`: the big Boo's flight.
- `Reveal_State0..4`: the reveal pair.
- `Boo_State0..7`: each of the 16 hiding Boos.
- `Scene_State0..4`: the scene.

Descriptive names, high confidence:
- **Boos:** `SpawnBoos`, `UpdateBoos`, `StepBoo`, `BounceBooOffWalls`, `MoveBoo`, `DrawBoos`.
- **The rub mechanic:** `UpdateRubbing` / `ReadRubTouch` (stylus lines drawn into BG0), `TouchTestBoo`, `CountRubbedTiles`.
- **The clock:** `StartCountdown` (3 seconds, or 2 from level 15), `UpdateCountdown`, `DrawCountdown`.
- **Round flow:** `CheckRoundEnd`, `CheckRevealDone`, `ResetRound`.

Medium confidence: the three popup pools above found Boos, whose looks are not observed.
- `RisingIcon_*` (+0x4a60): a delayed icon that rises, with sound 0x1bc.
- `FoundMark_*` (+0x4960).
- `Exclaim_*` (+0x48a0): spawned by the rub test, 0x18 px above the Boo.
- Also `Marker_*` (one sprite) and `StepBg3Scroll`.

Left unnamed: `func_ov006_0211cc90`. Both round endings call it, and it sets +0x4be5 when +0x4be0 is set; its role is unknown.

**Bomroom (`dScMgBomroom_c`, Bob-omb sorting).**

The seven member-pointer tables are named by index:
- `Round_State0..4` and `Round_SubState0..3`
- `Bomb_State0..6` and `Bomb_Roam0..4`
- `SlotA_State0..3`, `SlotB_State0..3` and `SlotB2_State0..3`

Descriptive names, high confidence:
- **Bombs:** `SpawnBombs`, `UpdateBombs`, `DrawBombs`, `ClearBombs`, `Bomb_Animate`, `Bomb_CheckGrab`, `Bomb_CheckSorted`, `Bomb_CheckWrongPen`, `Bomb_PushApart`, `OnBombExploded`, `HideExplodedBombs`.
- **Round flow:** `CountSortedBombs`, `CheckColorComplete`, `ResetRound`, `UpdateBg2Lift`.

Medium confidence:
- `SlotA_*`, `SlotB_*`, `SlotB2_*` and `Marker_*` keep the header's names for the UI slot records at +0x6260, +0x6280 and +0x62b0. What they show is not observed. `SlotB_Hit` bounces BG0 and `SlotA_Trigger` plays sound 0x1e2, which suggests the pen doors and their lamps.
- `DrawRemaining`.

Left unnamed: `func_ov006_020d8c88`.

**Pachinko (`dScMgPachinko_c`, Bob-omb Squad).** These helpers are C-linkage free
functions on a `char *` view of the scene, so each name carries the `Pachinko`
prefix.

Six tables, named by index:
- `PachinkoBg_State0..3`: the board scroll.
- `PachinkoFlyer_State0..4`: a crossing flyer that plays looped sound 0x189.
- `PachinkoPipe_State0..3`
- `PachinkoBall_State0..6`: the enemies that aim at a live target.
- `PachinkoBobOmb_State0..4`: grab, aim, fly, hit, remove.

The other subsystems are named by what they draw and update:
- `PachinkoTarget_*` (4 targets)
- `PachinkoFx_*` (30 effects)
- `PachinkoPopup_*` (score numbers)
- `PachinkoDeco_*` (3 records that scroll with the board)
- the aim line on sub BG2 (`Pachinko_DrawAimLine`, `Pachinko_ClearAimLine`, `Pachinko_PlotSubBg2Pixels`)

Medium confidence: "Flyer", "Deco" and "Ball" are role words from code alone; the
actors' looks are not observed.

Left unnamed: `func_ov006_020fea70`.

**Pachinko2 (`dScMgPachinko2_c`, Tamaire).** These are members, named after the
header's own `mCups` / `mBalls` / `mPaddles`.

Three tables, named by index:
- `Cup_State0..4`
- `Ball_State0..7`
- `Paddle_State0..14`

High confidence:
- **The aim line:** `DrawAimLine`, `ClearAimLine`, `PlotSubBg2Pixels`.
- **The clock:** `StartCountdown`, `UpdateCountdown`, `DrawCountdown`, `CheckCountdownEnd`.
- **The round:** `CheckRoundOver`, `StartRoundEnd`, `UpdateRoundEnd`, `ResetRound`.
- **Ball hits:** `Ball_HitAt`, `Ball_Bounce`.

Medium confidence: `Popup_*`, `Spark_*`, `CupSlot_*` and `ChooseLaneConfig`, named from the
record they touch; their looks are not observed.

**Panel (`dScMgPanel_c`, Puzzle Panic).**

Six tables, named by index:
- `Scene_State0..7`
- `Shake_State0..2`: the sub-BG0 shake.
- `Banner_State0..2`
- `IntroCard_State0..4`: the slide-in card on `mSlide`.
- `Input_State0..3`
- `Panel_State0..3`: one per panel.

High confidence: `FlipAt` (a panel and its neighbours), `DealPuzzle`, `CheckSolved`, `CountMatches`, `UpdateTouch`, `DrawPanels`, `Particle_*`, `Scroll_*`, `ResetRound`.

Medium confidence: `ScramblePanels`, `HasDuplicateRows`, `UpdateMoves`, `StartSolveCheck`, `EnableOverlayLayer`, `DrawCursor`, the slide-delay helpers, `CountFlipping`.

**Coin (`dScMgCoin_c`, Coincentration).**

Five tables, named by index:
- `Phase_State0..5`
- `Block_State0..5`
- `Coin_State0..2`
- `Caption_State0..4`
- `Bag_State0..3`: the moneybag.

High confidence: `LayOutCoins`, `SpawnBlocks`, `KickBlockUnderCoin`, `Block_ClampToWalls`, `Sparkle_*`, `Popup_*`, `ResetRound`.

Medium confidence: `DrawScorePlayer` and `DrawScoreRival` (the two result scores), `Caption_*`, `Bag_Release`.

**Hanachan (`dScMgHanachan_c`, Which Wiggler?).**

High confidence:
- **Phases:** `Phase_SetupRound`, `Phase_WaitIntro`, `Phase_Play`, `Phase_Result`, `Phase_WaitTouch`, `BeginPlay` (a 1200-frame clock), `BeginResult`.
- **Wigglers:** `Hana_SpawnWigglers`, `Hana_ChooseStarWiggler`, `Wiggler_HitTest` (any segment within 0x14 px), `Wiggler_ClampToScreen`, `Wiggler_Move`.
- **Popups:** the single star popup (`StarPopup_*`) and the sixteen `HanaPopup_*`.

Medium confidence:
- The movement modes: `Wiggler_StartPathA/B/C`, `Wiggler_StepSegmentsA/B/C`, `Wiggler_StartBounce`/`Wiggler_MoveBounce`, `Wiggler_StateMoveA/B`, `Wiggler_StateHold`, `Wiggler_StateWait`, `Wiggler_Retarget`, `Wiggler_OnTouched`, `Wiggler_StopIfIdle`, `Wiggler_UpdateSway` (sounds 0x1e7/0x1e8 as the sway crosses zero).
- `Wiggler_DrawPassA/B`: two draw passes; which part of the wiggler each draws is not observed.
- `Wiggler_IsStarVisible`.

**unit020cd744: the stylus-drawn trampolines.**

- **`TrmpLine_*`, high confidence.** The four line slots at `data_ov006_02140990`, each a 4x4 vertex patch, used by Trampoline and Trampoline2.
  - **Building:** `TrmpLine_Create` (from a stylus start and end point), `TrmpLine_Build`, `TrmpLine_ComputeNormals`.
  - **Drawing:** `TrmpLine_Draw`/`DrawAll`.
  - **Behaviour:** the three states `StateFade`, `StateSettle` and `StateFlex`, each with its setter; `TrmpLine_PushJumper` applies the line's spring to the jumper.
  - **Setup:** `TrmpLine_CreateStartLineT1/T2`, the line each game starts with.
- **`TrmpItem_*`, medium confidence.** The bouncing items of Trampoline2.
  - `TrmpItem_SetMotionN` / `TrmpItem_MotionN` are indexed by the item's kind byte (+0x98), which is certain.
  - What each kind looks like is not observed.

**Curling (`dScMgCurling_c`).** Named mostly from the bodies' own comments, high confidence.
- **Scene:** `Scene_StartRound`, `Scene_Play`, `Scene_Score`, `Scene_End`, `Scene_Idle`.
- **Stones:** `Stone_WaitRelease`, `Stone_Slide`, `Stone_Collide`, `Stone_Drag`, `SeparateStones`, `ResolveStoneCollision`, `SetStoneSpin`, `BringOutNextStone`, `ClearStones`, `ScoreStones`.
- **Aim:** `Aim_WaitTouch` and `Aim_Swing`.
- **Houses:** `PickHouse` and `ResetHouse`.
- **The falling bits (50 records):** `Bit_PickKind`, `Bit_RunKindA/B`, `Bit_State0..2`, `BitA_State0..3`, `BitB_State0..3`. These are named by index; what the bits are (snow?) is not observed.

**Roulette (`dScMgRoulette_c`, Mushroom Roulette).** These are C-linkage free helpers with a `Roulette` prefix.
- **`RouletteRacer_*`, high confidence:** the racers dealt onto the board, how they move to a tile, and the payout.
- **`RouletteBoard_*`, high confidence:** the spinning board. Its two states are confirmed by the records their setters store (`RouletteBoard_StateSpin`, `RouletteBoard_StateSlow`).
- **`RouletteStrip_*`, `RouletteResults_*`, medium confidence:** the per-player result strips.

**Snowball (`dScMgSnowball_c`).**

High confidence:
- **The ball:** `UpdateSnowball`, which runs 32 probes per frame through `HitsWall`, `HitsBreakable` and `IsInWater`.
- **The course:** `BuildCourse`, `DrawCourseColumn`, `RedrawCourse`.
- **Camera and reset:** `Snowball_UpdateCamera`, `ResetScene`.
- `dScMgSnowball_c_Construct`: the out-of-line constructor themes.md describes (it is not yet a real C1).

Medium confidence:
- `SnowSpray_*`: the 32 moving records at +0xba14.
- The falling-bit machine, which has the same shape as Curling's and the same names (`Bit_*`, `BitA/B_State*`).

**Cup, Jump2, Slots, Trampoline2 (one batch).**

- **Cup.** High confidence: `CupFx_*` (the 32-sprite effect pool), `Cup_DrawOamList`, `Cup_UpdateAnim`, `Cup_PlayAnimSound`. Medium confidence: `Cup_PickShuffleSpeed`, which reads the round row's speed nibbles.
- **Jump2.**
  - A linked list of score popups: `Jump2Popup_*`, plus the members `Elem::Spawn/Tick/DrawNumber` and `Node::Link/Unlink`.
  - Phases: `BeginIntro`, `State_Intro`, `BeginPlay`, `State_Play`, `BeginEnd`, `State_End`, `State_Result`, `State_Idle`. These are medium confidence, from the PMF records the transitions store.
- **Slots.**
  - `Slot3_State0..7`: Slot3's state table by index. States 3 and 4 share a handler, hence `Slot3_State3and4`.
  - `SlotLamp_*` and `SlotBlink_*`: the blink helpers the file's own comments describe.
  - Medium confidence: `SlotBet_*`, `SlotReel_SetSpeed`, `SlotReels_CheckStopped`, `Slot1_InitHud`.
- **Trampoline2.** All medium confidence:
  - `Trmp2_ColumnForKind`, `Trmp2_OccupancyBits` and `Trmp2_PickFaceA/B`: the face-row dealer.
  - `Trmp2Spinner_*`: a spinning decoration pool.
  - `TrmpItem_Construct`, `Trmp2_DestructJumperModels`.

**Small leftovers (one batch).**

High confidence:
- **Smartball:** the hand-written constructors of the twelve RTTI-named object classes (`cMgSmartball_wing_c_Construct` ... `cMgSmartball_object_c_Construct`), and `SmartballBall_UpdatePhysics`.
- **Jump:** the phases (`BeginIntro`, `State_Intro`, `BeginPlay`, `State_Play`, `BeginEnd`, `State_End`, `BeginFinal`, `State_Final`, `State_Result`, `State_Idle`). Each `Begin*` stores the record of the state named after it.
- **D3DBase:** the dual-3D display helpers (`D3D_InitDualScreen`, `D3D_CaptureToSub`, `D3D_CaptureToMain`, which write DISPCAPCNT) and the panned-sound thunks.
- **Elsewhere:**
  - Trampoline: `TrmpFx_*`, a linked effect list.
  - Amida: `Amida_Save/RestoreBgChars`.
  - Flower: `UpdatePetals`, `DealPetals`.
  - Hanachan: `Wiggler_Construct`.
  - Curling2: `Curling2_ResolveStoneCollision`.
  - Single3DBase: `LoadCard2SubBg3`.
  - The empty element constructors and destructors.

Medium confidence:
- `Puff_*` (unit020c8a30): three rising, growing sprites spawned at a jumper's x.
- `Luigi_PrepareScanlines` / `Luigi_PushScanlineRecord`.

Left unnamed (role unknown): `func_ov006_020d8c88` (Bomroom), `func_ov006_0211cc90` (Teresa), `func_ov006_020fea70` (Pachinko).

**dMgJump3DMario_c (Jump, Jump2).** The member names are checked against the state records each setter stores: `StartWindUp` → `StateWindUp`, `StartRiseOut` → `StateRiseOut`, `StartFallOut` → `StateFallOut`, `HideAndIdle` → `StateIdle`. The pool helpers are `Jump3D_*`. Medium confidence: `Jump3D_DisableInput`/`EnableInput` (a flag the scenes toggle) and `Jump3D_SetLowGravity`/`StopGravity` (tuning words).

**dMg3DHeyhoObjAdapter_c (the Shy Guys of the Jump games).**
- `Heyho_MotionN` / `Heyho_StartMotionN` (N = 0..14) pair each motion handler with the setter that stores its state record. The pairing is certain; what each motion looks like (walk, hop, bob, path, spin) is only partly read, and Motion11 is the spin taken when a jumper hits one (sound 0x1c8).
- Pool and object helpers: `Heyho_InitPool`, `Heyho_SpawnAll`, `Heyho_ReactAll`/`Heyho_React`, `Heyho_SeparateAll`, `Heyho_CheckHitA/B`, `Heyho_Update`, `Heyho_Draw`, `Heyho_Construct`/`Heyho_Destruct`, `Heyho_LoadResources`.
- Medium confidence: `Heyho_Settle`, `Heyho_SettleAll`, `Heyho_AllIdle`, `Heyho_IsChasing`.

**dMgTrmpln2Mario_c / dMgTrmpln3DMario_c (the trampoline Marios).** The two classes mirror each other, so their helpers carry parallel names.

High confidence:
- **States:** `Trmp2Mario_State0..7` and `Trmp3DMario_State0..12`, named in address order.
- **Setters:** each `EnterStateN` stores the record of its state, checked through ov006 relocs.
- **Getters:** `GetPos`, `GetPrevPos` and `GetVelocity` return the +0x24/+0x30/+0x3c (2Mario) and +0x1c/+0x28/+0x34 (3DMario) vectors, which the Update/Matrix bodies read as position, previous position and velocity.
- **Lifecycle:** the destructors and constructors the file banner identifies.
- **Object adapters:** the `TrmpObjAdapter_*` and `TrmpRingObjAdapter_*` constructors, which store those classes' vtables.

Medium confidence: `BounceOffLineA/B` (the reflection off the drawn line), `EnterByKind`, `PickIdleAnim`, `MoveToStart`, `PlaceAtStart`, `CountFinished`, `SetSpawnHeight`.

Left unnamed: `func_ov006_020ca840`.

## Member-pointer records (`pmf_*`, renamed 2026-10-15)

630 8-byte `{function, adjustment}` records in ov004/ov006 `.data` were address
names (`data_ov006_0213cb84`). Each is now named for the function its first word
relocates to: `pmf_<Class>_<Method>`, or `pmf_<Function>` for a free one. A
`_2`/`_3` suffix marks a second record pointing at the same function. This is certain:
the name is read off the relocation. So `mState = pmf_dScMgJump_c_State_Play;` says
which state is installed.

## Pass 2 names: types and record fields (2026-10-15)

These come from the shape pass. Each field is named from the functions that
read and write it, and every header change was checked against the object
compare (all 45 TUs identical).

**Scene base.**

| Name | Where | Confidence | Evidence |
|---|---|---|---|
| `gMgScene` typed `dScMgBase_c *` | `dScMgBase_c.h` | high | The `dScMgBase_c` constructor stores `this` and the destructor clears it. |
| `mTopRecord` | `dScMgBase_c` +0x4650 | high | `Mg_SetSessionBest` compares scores against it, and the constructor seeds it from the save table. |
| `dMgPsOpt_c::Open/Close/Update/Draw/IsIdle` | ov004 | high | The former `PsOpt_*`; each casts its argument back to `dMgPsOpt_c`. |

**Teresa (`dScMgTeresa_c`), confidence by field.**

- **`SmallBoo mBoos[16]`** (0x4660):
  - **high:** `mX`/`mY`, `mHeading` (indexes the sine table; its sign picks `mAnim`), `mFrame`/`mAnimTimer`, `mFade` (nonzero draws semi-transparent), `mVisible`, `mPriority` (passed as the OAM priority), `mState`.
  - **medium:** `mSpeed` (0xc00 adjusted by `mKind`), `mInPlay`, `mKind`, `mFound` (set by the touch test), `mFoundOrder`.
  - **unknown:** `unk_12`, `unk_19`, `unk_21`.
- **`mExclaims`, `mFoundMarks`, `mRisingIcons`** (16 each): **medium**. The record names come from their Spawn/Update/Draw functions. What each sprite looks like on screen is not observed.
- **`mReveal[1]`, `mMarker`** (0x4ba0, 0x4bc0):
  - **medium:** the field mechanics, including the bounce `phase` and `angle` (the `OAM::Render` rotation).
  - **low:** the one-element array. It is inferred from handlers that take an index while the dispatcher passes 0.
- **`mFoundCount`, `mLastSettledBoo`, `mBooCount`, `mFirstFoundBoo`** (0x4c21..0x4c26): **medium**, named from their writers.

**Pachinko (`dScMgPachinko_c`).**

- **`mFx[30]`:** **medium**. The hit bursts; `kind` 2 is the long animation.
- **`mTarget[4]`:** **medium**. `sprite` is high: it indexes the target sprite table.
- **`mFlyer[1]` and `mBg[1]`:**
  - **medium:** the fields. `vy` is high: `Flyer_State0` seeds it and `State1` adds it to y.
  - **low:** the one-element arrays, inferred the same way as Teresa's `mReveal`.
  - **seen:** the flyer is Lakitu and the targets are the four flowers (runtime-checks.md, 2026-10-15). "background" is still medium.
- **0x5418..0x5958:** no reconstructed function touches it.
