# Ularn: Wild Edition — the plan

Experimental branch `claude/wonderful-edison-1g02v3`. The goal: bolt a pile of
deliberately off-the-wall features onto the Ularn 1.6.3a Android port while
keeping the core game playable and shippable as an APK.

This document is the brief every implementation agent works from. Read all of
**Part 1** before touching anything; then read your own feature in **Part 3**.

---

## Part 1 — Ground rules for every agent

### The repo in 60 seconds

* The game is upstream C in `app/src/main/cpp/ularn/`, running in a process
  forked from the app. It never draws; it writes a `UlarnShared` struct
  (`app/src/main/cpp/shared_state.h`) into shared memory and reads ASCII keys
  from a pipe. `compat/ularn_winandroid.c` implements the display API
  (`ularn_win.h`) on top of that struct.
* Kotlin (`app/src/main/java/com/ularn/android/`) renders the struct
  (`game/GameView.kt`), sends keys (`GameSession.kt`), and owns all menus
  (`MainActivity.kt`, `Settings.kt`). It never knows game rules, only tile ids
  and the fields the C side exports.
* Field offsets of `UlarnShared` are exported at runtime by
  `app/src/main/cpp/ularn_jni.c` (`layout()`) and consumed by
  `game/GameState.kt` (`object F`) — the two lists must stay in the same order.
* Tiles: `app/src/main/assets/ularn_tiles.png`, 32×32 px, 16 columns. Tile id
  `n` is at column `n % 16`, row `n / 16`. The original sheet is 23 rows
  (ids 0–367). **Never edit the PNG by hand** — see "Tiles" below.
* Saves are a raw binary dump (`savegame.c`); the Wild Edition adds a header
  and a trailer (see "Save format"), so **never change the size or order of
  anything written by `write_player/levels/store/monster_data/spheres`** except
  by appending new `AttributeType` entries *before* `ATTRIBUTE_COUNT`
  (the whole `c[]` array is written, so new attributes are saved for free).

### Hard rules

1. **Never touch the `original-backup` branch** (see `AGENTS.md`).
2. **Core game stays playable.** Every feature is behind a bit in the
   `ULARN_WILD` feature mask (see "Feature flags"). With your bit off, the game
   must behave exactly as before. With it on, nothing may crash, soft-lock a
   prompt, or corrupt a save.
3. **Only edit the files your feature owns** (listed in its spec) plus the
   explicitly listed "shared insertion points". Foundation already reserved
   every enum value, tile id, attribute slot, shared-struct field and settings
   key you need. If you believe you need something else, add it in the most
   additive way possible and call it out in your final report.
4. **No new dependencies.** Framework Android + Kotlin stdlib only, C99 only.
   No network. No new permissions except where a spec says so.
5. **Don't mention model names in commits.** Commit on your worktree branch
   with clear messages; don't push.
6. **Style:** match the surrounding code. C files use the upstream's 2-space
   style; Kotlin follows the existing files. Comments explain *why*.

### Feature flags

`compat/wild.h` defines one bit per feature:

| Bit | Macro | Feature |
|---|---|---|
| 1<<0 | `WILDF_RADIO` | Dungeon Radio (chiptune music + SFX) |
| 1<<1 | `WILDF_CARDS` | Monster trading cards |
| 1<<2 | `WILDF_VENDING` | Vending machines, junk food, rubber duck, Roomba |
| 1<<3 | `WILDF_HATS` | Hats |
| 1<<4 | `WILDF_COIN` | LarnCoin at the bank + the Tax Auditor |
| 1<<5 | `WILDF_GLITCH` | The Glitch Realm |
| 1<<6 | `WILDF_FISHING` | Fishing at fountains + the Fountain Kraken |
| 1<<7 | `WILDF_PETS` | Mystery egg → pet companion |
| 1<<8 | `WILDF_PERKS` | Level-up perks |
| 1<<9 | `WILDF_WEATHER` | Weather and day/night on the surface |
| 1<<10 | `WILDF_PHOTO` | Polaroid photo mode (Kotlin only) |
| 1<<11 | `WILDF_BOUNTY` | Town bounty board (added after the first wave; docs/wild/bounty.md) |

* C side: `wild_on(WILD_X)` returns non-zero when the bit is set. The mask
  arrives in the environment variable `ULARN_WILD` (decimal), read once in
  `wild_init()`. **Also stored in the save** (`c[WILD_FLAGS]`) so a save keeps
  the flags it was started with; `wild_init()` only applies the env mask to a
  new character.
* Kotlin side: `Settings.wildFlags` (Int) with per-feature `Boolean`
  properties `Settings.wild(bit)`; the "Wild features" section in
  `MainActivity.showSettings()` has one checkbox per feature. `GameSession`
  passes `ULARN_WILD=<mask>` in the env array. Kotlin-only features
  (Radio, Photo, Cards UI) read `settings.wild(...)` directly.
* Default: **all on**.

### Hooks (C) — `compat/wild.c`

`wild.c` is the single hub called from upstream code. Upstream edits are
one-liners (`wild_xxx(...)`) already in place; each hub function calls the
per-feature function in that feature's file. Features implement their
hooks in **their own file** (`compat/wild_<feature>.c`), whose stubs already
exist with empty bodies. Available hooks:

| Hook | Called from | When |
|---|---|---|
| `wild_init(void)` | `ularn.c: main()` after options are read, before the save is restored | once per process |
| `wild_new_character(void)` | `ularn.c` right after `makeplayer()` for a *new* game | once per new game |
| `wild_turn(void)` | `ularn.c` main loop, right after `regen()` | every player turn |
| `wild_new_level(int lev)` | `dungeon.c: newcavelevel()` after the level is in memory | each level change |
| `wild_make_level(int lev)` | `dungeon.c: makemaze()`, after the level and its objects are generated the first time | first visit of a level |
| `wild_kill(int monst, int x, int y)` | `monster.c: hitm()` when the player kills a monster | each kill |
| `wild_level_up(int newlevel)` | `player.c: raiseexperience()` once per level gained | each level-up |
| `wild_take(int obj, int arg)` | `player.c: take()` after the item is in the pack | each pickup |
| `wild_object(int obj)` | `object.c: lookforobject()` `default:` branch for ids ≥ `OWILD_FIRST` | stepping on a Wild object |
| `wild_wear(int slot)` / `wild_takeoff()` | `action.c: wear()` / `ularn.c: ACTION_REMOVE_ARMOUR` | when the item is a Wild object (returns 1 if handled) |
| `wild_player_hit(int dmg)` | `player.c: losehp()` | each time the player takes damage |
| `wild_save(FILE*)` / `wild_restore(FILE*)` | `savegame.c` | trailer, see below |

Events to the app: `wild_event(type, a, b, c)` appends to a ring buffer in
`UlarnShared` (`ev[]`, `ev_seq`); Kotlin's `game/GameEvents.kt` dispatches
them to listeners. Event ids are in `shared_state.h` (`US_EV_*`). Foundation
already emits: `KILL(monst)`, `LEVEL_UP(lvl)`, `NEW_LEVEL(lev)`, `TAKE(obj)`,
`PLAYER_HIT(dmg)`, `GOLD(amount)`, `SPELL(spell)`, `DIED`, `WON`.
Features emit their own with ids in their reserved ranges.

### Save format (foundation, do not change)

```
"ULRNWILD" (8 bytes) | int32 version (=2) | <upstream dump unchanged> |
wild trailer: int32 'W','I','L','D' | int32 n_kill_counts | int32 kill_counts[n] |
              int32 n_extra | int32 extra[n_extra] | <upstream FileSum>
```
Old saves (no header) are refused with a friendly message and deleted; the
app then offers a new game. `extra[]` is 64 int32 slots: see each feature's
reserved indices (`WX_*` in `wild.h`). Most features need no extra slots
because their state lives in `c[]` attributes.

### Reserved identifiers (foundation created these; use, don't re-declare)

**Attributes** (`player.h`, appended before `ATTRIBUTE_COUNT`):
`WILD_FLAGS, WILD_HAT, WILD_PERKS, WILD_PERK_PICKS, WILD_EGG, WILD_PET_MON,
WILD_PET_HP, WILD_COINS, WILD_COIN_PRICE, WILDF_GLITCH, WILD_DUCK, WILDF_WEATHER,
WILD_FISH_XP, WILD_CARD_LAST, WILD_SPARE_A .. WILD_SPARE_F`.

**Monsters** (`monster.h`, inserted before `DEMONLORD`; never randomly
generated because `monstlevel[]` is unchanged — spawn them with
`createmonster()` or by writing `mitem[x][y].mon`):

| Id | Name | Tile | Owner |
|---|---|---|---|
| `NULLPOINTER` | "null pointer" | 384 | Glitch |
| `FOUNTAINKRAKEN` | "fountain kraken" | 385 | Fishing |
| `TAXAUDITOR` | "tax auditor" | 386 | LarnCoin |
| `ROOMBA` | "roomba" | 387 | Vending |

`MAXMONST` is now `#define MAXMONST DEMONLORD` so genocide/mimic logic still
covers "ordinary monsters" (including the new ones), and `MAXMONST+8 ==
LUCIFER` still holds. Stats rows are placeholders — tune them in your feature.
Only their features make these monsters: code that picks a monster at random
from `1..MAXMONST-1` (the step past a genocided species in `makemonst()` and
`createmonster()`, polymorph, a mimic's disguise) leaves them out with
`is_wild_monst()` (`monster.h`).

**Objects** (`itm.h`, appended before `OCOUNT`; `OWILD_FIRST == OVENDING`):

| Id | Name | Tile | Owner |
|---|---|---|---|
| `OVENDING` | a humming vending machine | 368 | Vending |
| `OFIZZ` | a can of Fizz | 369 | Vending |
| `OPIZZA` | a slice of pizza | 370 | Vending |
| `OENERGYDRINK` | an energy drink | 371 | Vending |
| `ORUBBERDUCK` | a rubber duck | 372 | Vending |
| `OHATCONE` | a traffic cone | 373 | Hats |
| `OHATPROPELLER` | a propeller beanie | 374 | Hats |
| `OHATWIZARD` | a pointy wizard hat | 375 | Hats |
| `OHATPARTY` | a party hat | 376 | Hats |
| `OHATTINFOIL` | a tinfoil hat | 377 | Hats |
| `OEGG` | a mysterious egg | 378 | Pets |
| `OFISHINGROD` | a fishing rod | 379 | Fishing |
| `OFISH` | a fish | 380 | Fishing |
| `OOLDBOOT` | an old boot | 381 | Fishing |
| `OBOTTLE` | a message in a bottle | 382 | Fishing |
| `OCASSETTE` | a cassette tape | 383 | Radio |
| `OCOINATM` | a LarnCoin ATM | 388 | LarnCoin |

All of them already pick up as generic items (name, tile, map char `*`,
weight 1, value 0, context action "Take"). Your feature adds behaviour.

**Tiles** (ids; every one is a file in `tools/tiles/src/`):

| Range | Owner | Content |
|---|---|---|
| 368–372 | Vending | machine, fizz, pizza, energy drink, duck |
| 373–377 | Hats | the five hats as *items on the floor* |
| 378–383 | Pets / Fishing / Radio | egg, rod, fish, boot, bottle, cassette |
| 384–387 | Glitch / Fishing / Coin / Vending | null pointer, kraken, auditor, roomba |
| 388–391 | LarnCoin | ATM, coin icon, up-arrow, down-arrow |
| 392–396 | Hats | hat *overlays* drawn on the player (transparent except the hat, sitting on the top ~12 rows) |
| 397–399 | Pets | heart overlay, egg-crack frame 1, 2 |
| 400–407 | Weather | rain 1–2, snow 1–2, night-sky stars 1–2, sun, moon |
| 408–415 | Glitch | 8 "corruption" tiles (noise / scanlines / inverted blocks) |
| 416–431 | Cards | card back, 3 frame variants (common/rare/holo) as 32×32 corner pieces are *not* used — cards are drawn in Kotlin; **this row is spare** |
| 432–447 | Spare | ask before using |

**Shared struct fields** (`shared_state.h`, already exported to Kotlin as
`GameState.hatTile`, `glitch`, `weather`, `coinPrice`, `petX/petY`,
`killCount(monst)`, `wildFlags`, and the event ring):
`int32 hat_tile; int32 glitch; int32 weather; int32 coin_price; int32 pet_x,
pet_y; int32 wild_flags; int32 kill_counts[80]; int32 ev_seq; int32 ev[64][4]`.

### Tiles — how to author pixel art

Agents can't paint, but they can write pixels. Each tile is a text file
`tools/tiles/src/<id>_<name>.txt`:

```
# palette: one char per colour, '.' is transparent
# k=#000000 w=#ffffff r=#c42020 ...
# then exactly 32 lines of exactly 32 chars
................................
...
```
`python3 tools/tiles/build_sheet.py` validates every file and rebuilds
`app/src/main/assets/ularn_tiles.png` = original sheet (`tools/tiles/base_sheet.png`)
+ all authored tiles, growing the sheet by whole rows. Run it after adding or
editing a tile. **Style guide:** the original tiles are chunky 32×32 sprites
with 1px black outlines, 3–4 shades per material, a slightly desaturated
palette, no anti-aliasing, drawn on a transparent (floor shows through) or
solid-black background. Look at `tools/tiles/base_sheet.png` tiles 64–162
(objects) and 0–63 (monsters) — `python3 tools/tiles/dump_tile.py <id>` prints
one as text so you can imitate the look. Objects should leave 2–3 px margin.

### Verifying your work (no device needed)

1. **C compiles for the host:** `tools/host/check.sh` syntax-checks every game
   source with gcc using the Android flags.
2. **Host harness plays the game:** `tools/host/play.sh` runs the real game
   (upstream core + Android front end) from a key script and prints messages,
   status, events and prompts after every step; `tools/host/README.md` has the
   details. The essentials: `--wizard` starts as a wizard, and then the key
   `=` opens the **Wild debug menu**: *(o)* place any object by id under you
   (the game then offers to take it), *(m)* create a monster by id next to
   you, *(g)* +1000 gold, *(x)* gain a level, *(t)* run N `wild_turn()` ticks,
   *(h)* heal, *(f)* dump the Wild attributes. `tools/host/ids.py <filter>`
   prints object / monster / attribute ids. `--seed N` makes a run
   reproducible, `--wild MASK` sets the feature mask (`--wild 0` = vanilla),
   `--home DIR` lets you save in one run and restore in the next. Driver
   commands `!map`, `!inv`, `!tiles` dump the level, the inventory and the
   tile ids around you. A crash keeps the temp dir and prints `ularn.log`.
   Example:
   ```
   tools/host/play.sh --wizard --seed 3 --quiet '=' 'o' '101\n' '0\n' 't' 'i' '\s'
   ```
3. **APK compiles:** `./gradlew --no-daemon assembleDebug -Pularn.abis=x86_64`
   (one ABI, ~1-3 min warm). `ANDROID_HOME=/opt/android-sdk` is set in
   `local.properties`. Kotlin unit tests: `./gradlew --no-daemon testDebugUnitTest`.
   Run Gradle builds sparingly (other agents share the machine): once when
   your code is done, once more if you had to fix something.
4. **Tiles:** `python3 tools/tiles/build_sheet.py` must pass, then
   `python3 tools/tiles/dump_tile.py <id>` to eyeball yours.
5. **Regression:** `tools/host/smoke.sh` (new game → dungeon → level 2 → save →
   restore) must still pass with your changes.

### Definition of done (per feature)

* Behaviour works in the host harness with the flag on; nothing changes with
  it off.
* `check.sh`, `build_sheet.py`, `assembleDebug -Pularn.abis=x86_64` all pass.
* README section written (see "Docs" in your spec) in `docs/wild/<feature>.md`
  (2–10 lines: what it is, how to find/use it, the setting that disables it).
* Committed on your worktree branch. Final report lists files touched, any
  shared-file edits, anything you could not finish, and how you tested.

---

## Part 2 — Foundation (done before agents start)

Owner: the orchestrator. Everything in Part 1's "Reserved identifiers" plus:

* `app/build.gradle.kts`: `applicationId "com.ularn.android.wild"` (coexists
  with the normal app, separate saves), app name "Ularn Wild", versionCode
  100, versionName `3.0-wild (Ularn 1.6.3a)`, `-Pularn.abis=` property.
* `compat/wild.h`, `compat/wild.c`, `compat/wild_<feature>.c` stubs added to
  CMake; hooks wired into upstream (one line each).
* Save header/trailer; refusal path for old saves; `SaveSlots.kt` recognises
  the header so an old slot shows "(old version)".
* Event ring + `GameEvents.kt` + `GameState` accessors.
* `tools/tiles/` and `tools/host/`.
* `Settings` wild flags + Settings UI section + `GameSession` env.
* `ularn_android_panel.c`: monsters classified by `mitem`, not tile id.
* CI: branch pushes publish a `wild-latest` prerelease with `ularn-wild.apk`.

---

## Part 3 — Features

Each spec: *Pitch · Design · Files you own · Shared insertion points · Tiles ·
Settings · Test script · Docs.* Sizes are for one Opus agent.

### 3.1 Dungeon Radio — procedural chiptune + SFX (Kotlin) — size M

**Pitch.** The dungeon has a soundtrack now: an NES-style synth generated in
code (no audio assets), with the track picked by where you are, and
square-wave SFX for hits, kills, pickups, stairs, spells, level-ups and death.
Find **cassette tapes** in the dungeon to unlock bonus tracks.

**Design.**
* `audio/Chiptune.kt`: an `AudioTrack` (44.1 kHz, mono, 16-bit, streaming)
  fed by a mixer thread. Voices: pulse (12.5/25/50% duty), triangle, noise.
  Simple ADSR. A **sequencer** plays `Song` objects: tempo + per-voice
  patterns (note, duration) in a tiny text DSL, looped.
* `audio/Songs.kt`: at least 5 songs: *Town* (surface, major, bouncy),
  *Shallows* (levels 1–4), *Depths* (5–10, minor, slow), *Volcano* (V1–V5,
  fast, dissonant), *Shop* (jaunty), plus *Game Over* and *Victory* stingers.
  Bonus (cassette) tracks: *Elevator* (bank), *Surf* (fishing — if the
  Fishing agent's `US_EV_WILD_FISHING` events arrive; otherwise unused),
  *Boss* (a demon lord is visible — detect via tiles 56–63 on screen).
* `audio/Sfx.kt`: synthesised one-shots per `GameEvents` event; keep them
  under 150 ms; limit polyphony to 4.
* Track selection from `GameState`: `dlevel`, display mode (shop text pages
  → *Shop*), `endState`. Cross-fade 300 ms.
* `OCASSETTE` (tile 383): C side (`compat/wild_radio.c`) places one cassette
  on levels 2, 6, V2 in `wild_make_level()`; `wild_object(OCASSETTE)` picks it
  up with a message *"A cassette tape! Side A: '<song>'."* and emits
  `US_EV_WILD_RADIO(track_index)`. Kotlin stores unlocked tracks in
  `SharedPreferences` and adds a **"Radio"** entry to the in-game menu: now
  playing, next track, unlocked list, music/SFX volume sliders.
* Respect the system: pause when the activity pauses; honour
  `settings.noBeep` only for the old beep, not the radio.

**Files you own.** `audio/*.kt` (new), `compat/wild_radio.c`,
`docs/wild/radio.md`, tile `383_cassette.txt`.
**Shared insertion points.** `MainActivity.showMenu()` (+1 item),
`MainActivity.onPause/onResume` (start/stop), `Settings` (volumes).
**Settings.** `WILDF_RADIO`; music volume, SFX volume (0–100).
**Test.** Unit test the sequencer (note→frequency, pattern parsing, song
length). Host harness: cassette appears on level 2 (`ULARN_WILD` on), pickup
message, event emitted (`play.sh` prints events).

### 3.2 Monster Trading Cards (Kotlin + a little C) — size M

**Pitch.** Every monster you kill can drop a collectible card. Gotta kill 'em
all. A pixel-art card album with rarity, holo foil, and lore.

**Design.**
* C (`compat/wild_cards.c`): in `wild_kill()`, first kill of a species
  always drops a card; later kills 15%. Emit `US_EV_WILD_CARD(monst, holo)`
  where `holo` is 1 with 5% chance. Kill counts already live in
  `kill_counts[]` (saved).
* Kotlin `cards/CardAlbum.kt`: persistent set (SharedPreferences JSON) of
  owned cards `{monst, holo, count, firstSeenRun}` across all runs.
  `cards/CardRenderer.kt` draws a card to a Bitmap: frame colour by rarity
  (common = monster level 1–3, uncommon 4–6, rare 7–9, epic = dragons and
  uniques, legendary = demons), monster tile scaled ×4 with nearest-neighbour,
  name, stats (LV/AC/DAM/HP/XP taken from a Kotlin table copied from
  `monster.c` — put it in `cards/MonsterFacts.kt` with a comment that it
  mirrors `monster.c`), and a one-line lore quip per monster (write 70 of
  them; be funny). Holo = animated diagonal rainbow sheen (ColorMatrix /
  gradient shader with time) in the viewer.
* `cards/CardAlbumDialog.kt`: grid of owned cards (unknown = silhouette
  "???"), tap for full-screen card, "x/69 collected" progress, filter by
  rarity. Entry in the start panel and the in-game menu ("Card album").
* Toast with the card when one drops ("New card: Gnome King ★ HOLO").
* Achievement: all cards → hook into `stats/RunBook.kt` only if trivial;
  otherwise skip.

**Files you own.** `cards/*.kt`, `compat/wild_cards.c`, `docs/wild/cards.md`.
**Shared insertion points.** `MainActivity.showMenu()`, start panel item,
`GameEvents` listener registration.
**Settings.** `WILDF_CARDS`; "Card drop toasts".
**Test.** Unit test `CardAlbum` serialisation and rarity mapping. Host
harness: kill a monster (wizard: create a lemming next to you, attack) and
see the event.

### 3.3 Vending Machines, junk food, the rubber duck, and the Roomba (C) — size L

**Pitch.** Someone installed vending machines in the dungeon. Also a Roomba
got loose and it is eating the loot.

**Design.**
* `OVENDING` (368) placed in `wild_make_level()` on ~40% of levels 1–10 and
  V1–V3, in a floor cell not adjacent to stairs. Standing on it
  (`wild_object(OVENDING)`): prompt *"The machine hums. (b)uy for 50 gold,
  (k)ick it, or (i)gnore?"* using `get_prompt_input(..., "bki\033", 1)`.
  * **buy**: random weighted item: `OFIZZ` 35%, `OPIZZA` 30%,
    `OENERGYDRINK` 25%, `ORUBBERDUCK` 7%, a random potion 3% ("Huh, that's
    not what you pressed."). Deducts gold; "You don't have 50 gold" otherwise.
    Items appear at the player's feet (`createitem`) and the usual pickup
    prompt follows via `lookforobject()` next turn — or simply `take()` them
    directly, your call; be consistent with how cookies behave.
  * **kick**: 50% nothing ("THUNK."), 30% a free item falls out, 15% "The
    machine wobbles ominously" (sets a flag; next kick = 100% topple), 5%
    (or guaranteed after wobble) the machine **topples onto you**: 10–25 dmg
    (`losehp(DIED_WILD_VENDING, ...)` — add the death reason to `scores.h`
    `DiedReasonType` *at the end* and `whydead[]` "crushed by a vending
    machine") and the machine becomes `ONOTHING`.
  * A wall-mounted machine on the **home level** near the DND store would be
    a nice touch (place in `wild_make_level(0)`).
* Food items are used by **eating** (`ACTION_EAT_COOKIE`, key `e`) — extend
  `action.c`'s eat path via `wild_eat(obj)` if simpler than teaching
  `ocookie`; or by stepping on them (`wild_object`) with a prompt like
  potions. Effects:
  * Fizz: haste 30 turns (`c[HASTESELF] += 30`), then "You burp."
  * Pizza: heal 25% max HP, +1 CON permanently with 10% chance ("That was
    a *good* slice.").
  * Energy drink: `c[SPELLS] = min(SPELLMAX, SPELLS + 8)`, but
    `c[CONFUSE] += 5` ("Your hands are shaking.").
  * Rubber duck: not eaten. **Rubbing** it — reuse the brass-lamp pattern:
    stepping on it offers (r)ub/(t)ake; in inventory add key handling via
    `wild_use(obj)` from a new `ACTION_WILD_USE` bound to `x`? Keep it
    simple: rubbing happens by **reading** it (`r`, `readscr` path): "You
    explain your problem to the duck. The duck says nothing. You suddenly
    understand everything." → identifies every potion and scroll in the pack
    (set `potionknown/scrollknown` for carried ones). 3 charges
    (`c[WILD_DUCK]`), then "The duck has heard enough."
* **Roomba** (`ROOMBA`, tile 387): spawns on 20% of levels ≥ 3 awake. Stats
  ~ LV 4, AC 8, DAM 1, HP 25, XP 30, `FL_SLOW|FL_NOBEHEAD`. Behaviour in
  `wild_turn()`: if a Roomba is on the level and standing on an item that
  isn't a wall/stairs/feature, it **eats it** (stores it in `mitem[x][y].it[]`
  like a thief — up to 6 — and removes it from the floor); the upstream death
  code already drops stolen items back out. Moves like a dumb monster
  (upstream handles movement); message when seen eating: *"The roomba hums
  contentedly as it eats <item>."* Killing it: *"The roomba beeps sadly and
  coughs up its contents."*

**Files you own.** `compat/wild_vending.c`, tiles 368–372 and 387,
`docs/wild/vending.md`.
**Shared insertion points.** `scores.h/.c` (append death reason),
`monster.c` row for ROOMBA (stats only), `action.c` eat path (guarded by
`wild_on`), `compat/android_context.c` (vending → a "Use" context kind:
reuse `US_CTX_LAMP`-style; add `US_CTX_VENDING` and the label "Vending" in
`game/ContextAction.kt`).
**Settings.** `WILDF_VENDING`.
**Test.** Host harness: wizard-create `OVENDING` under you, buy with
gold, kick repeatedly until it topples (force RNG via many tries), eat each
food, rub the duck with unknown potions in the pack, create a Roomba next to
an item and watch it eat.

### 3.4 Hats (C + Kotlin render) — size M

**Pitch.** Five hats. They sit on your sprite. They are mostly ridiculous.

**Design.**
* Hats are objects 373–377 found in the dungeon (`wild_make_level`: 25%
  chance of one random hat per level, more often deep) and in the DND store
  (append to `itm[]` in `store.c` — the array size `DNDSIZE` must grow and
  `write_dnd_store/read_dnd_store` write `DNDSIZE` entries, which is fine
  for a new save format).
* Wearing: `W` (wear) on a hat → `wild_wear(slot)` returns 1 after setting
  `c[WILD_HAT] = obj`, remembering the slot in `ivenarg`? No — hats stack
  nothing; just set `c[WILD_HAT] = iven[slot]` and mark the slot as worn by
  appending " (on your head)" via `ularn_android_panel.c: describe()` and
  `show.c: show3()` (guarded). `T` (take off) with a hat on removes it.
  Dropping/selling a worn hat takes it off. Only one hat at a time.
* Effects (`recalc()`-safe — apply via `c[]` modifiers in `wild_wear`/
  `wild_takeoff`, like rings):
  * Traffic cone: +2 AC; monsters "swerve": 10% chance a melee attack on
    you misses outright (`wild_player_hit` can't cancel; instead hook
    `hitplayer()` via `wild_dodge()` returning 1 — add to hooks list).
  * Propeller beanie: you fly over pits and trap doors (treat as
    `c[WWAND]`-style check in `object.c` pit/trapdoor cases, guarded).
  * Wizard hat: spell regen doubled (`regen()`: extra `c[SPELLS]++` tick).
  * Party hat: +3 CHA; shopkeepers give 10% off at the DND store
    (`dnditem()` price × 0.9).
  * Tinfoil hat: immune to confusion and to the mind-flayer-style "stupid"
    attacks (`c[CONFUSE]` never increases while worn; clear it each turn).
* Render: `S->hat_tile` = overlay tile id (392–396) or 0; set it in
  `PaintMap()` (`ularn_winandroid.c`) from `c[WILD_HAT]`. `GameView.drawMap()`
  draws `s.hatTile` on the player's cell after the player tile. Also show
  the hat on the side panel if trivial.

**Files you own.** `compat/wild_hats.c`, tiles 373–377 (items) and 392–396
(overlays), `docs/wild/hats.md`.
**Shared insertion points.** `store.c` `itm[]`, `object.c` pit/trapdoor,
`player.c: regen()`, `monster.c: hitplayer()` (one `if (wild_dodge()) return;`
line), `show.c: show3()` suffix, `ularn_android_panel.c: describe()`,
`ularn_winandroid.c: PaintMap()` (set `hat_tile`), `GameView.kt` (draw it).
**Settings.** `WILDF_HATS`.
**Test.** Host harness: create each hat, wear, check status/AC, take off,
drop while worn; pit with the beanie; confusion with tinfoil.

### 3.5 LarnCoin and the Tax Auditor (C) — size M

**Pitch.** The Bank of Larn now trades a volatile cryptocurrency. Number go
up. Number go down. If you dodge your taxes, an auditor comes for you.

**Design.**
* Price: `c[WILD_COIN_PRICE]` in gold per coin, starts 100. In `wild_turn()`
  every 10 turns: random walk ±(1–7)% with momentum, floor 1; 1-in-400
  "news" event with a message and a ±30–60% jump (*"A famous gnome posts
  about LarnCoin."*, *"The LRS announces a LarnCoin tax."*, *"LarnCoin
  halving!"*, *"A whale dumps."*). If price > 1500: 1-in-50 chance per tick
  of a **rug pull**: price → 1, message *"LarnCoin's founder has moved to a
  volcano. The price collapses."*
* Bank menu (`store.c: obanksub()`): add options **(b)uy LarnCoin / (s)ell
  LarnCoin** showing price, holdings (`c[WILD_COINS]`) and their value; sell
  pays into the bank account. Extend the prompt string/switch minimally; keep
  the layout within 80 columns.
* `OCOINATM` (388): one ATM on the home level outside the bank
  (`wild_make_level(0)`), usable like the bank menu but **buy/sell only** with
  a 5% fee. Minimap category `US_MM_BUILDING` (add to
  `ularn_android_panel.c: object_class`).
* Coins count toward score at the end: `calc_score()` in `scores.c` adds
  `coins × price` (guarded).
* HUD: `S->coin_price` set each `PaintStatus()`; Kotlin shows a tiny ticker
  "Ⓛ 123 ▲" at the right end of the status line when holdings > 0 or the
  price moved this turn (use tiles 389–391 or just text; text is fine).
* **Tax Auditor** (`TAXAUDITOR`, 386): if `outstanding_taxes > 0` and the
  player is on the home level, each turn 1-in-150 chance to spawn an awake
  auditor near the entrance: LV 6, AC 6, DAM 0, HP 40, XP 50, special
  attack: on hit it **garnishes** 10% of carried gold (message *"The auditor
  issues you a receipt."*), vanishes when taxes are paid (check in
  `wild_turn`). Implement the attack via a new `spattack` case id (next
  unused number) in `monster.c`, guarded by `wild_on`.

**Files you own.** `compat/wild_coin.c`, tiles 386, 388–391, `docs/wild/coin.md`.
**Shared insertion points.** `store.c: obanksub()` menu, `scores.c:
calc_score()`, `monster.c: spattack()` case + stats row, `ularn_android_panel.c`,
`ularn_winandroid.c: PaintStatus()` (coin_price), `GameView.kt` ticker.
**Settings.** `WILDF_COIN`.
**Test.** Host harness: enter bank, buy, advance 100 turns with `.`, sell;
force taxes (wizard) and wait on the surface for the auditor.

### 3.6 The Glitch Realm (C + Kotlin render) — size M

**Pitch.** A scroll of segfault corrupts reality. For a while the dungeon
renders wrong, monsters wear the wrong names, and a MissingNo-style
"null pointer" hunts you — but it drops wonderful garbage.

**Design.**
* New scroll `SSEGFAULT` = 24 in `scroll.h` (within `MAXSCROLL 28`), name
  " segfault", add to `scprob[]` with low weight (3 slots) **only when
  `wild_on(WILDF_GLITCH)`** — `newscroll()` uses `scprob[rund(SCROLL_PROB_SIZE)]`,
  so instead wrap: in `read_scroll()` nothing; in `newscroll()` if wild and
  `rnd(40)==1` return `SSEGFAULT`. Bump `S_MAX` carefully (it bounds
  identification lists).
* Reading it: *"Segmentation fault (core dumped)... just kidding. Reality
  flickers."* → `c[WILDF_GLITCH] = 60 + rnd(60)` turns. Also 1% of the time
  on **any** teleport (`oteleport`) when wild.
* While glitched (`wild_turn()` decrements):
  * `S->glitch` = remaining turns (set in `PaintMap`). Kotlin: when > 0,
    `GameView` draws the map through a corruption pass: every frame
    (invalidate at 8 fps) 3–6 random visible tiles are drawn with a glitch
    tile (408–415) blended on top, random horizontal slices of the map are
    shifted ±4 px, and a `ColorMatrix` hue rotation of `(turn*37)%360°` is
    applied to the whole map. HUD text replaces 1-in-20 characters with
    `▓█▒░` glyphs. Keep it readable enough to play.
  * Monster names in messages are scrambled: `lastmonst` is set by upstream
    from `monster[].name`; in `wild_turn()` while glitched, temporarily
    swap `monster[k].name` pointers between two random ordinary monsters
    (and swap back when the glitch ends — keep a copy of the original table
    in `wild_glitch.c`). Yes, this means "The lemming hits you for 12".
  * Items look wrong: `GetTile()` in `ularn_winandroid.c` — when glitched,
    object tiles are drawn as `objtilelist[(obj*7 + gtime/5) % OCOUNT]`
    guarded to known-safe ranges (skip walls, doors, stairs).
  * 1 **null pointer** spawns awake when the glitch starts: LV 8, AC 2,
    DAM 7, HP 60, XP 400, `FL_FLY|FL_INFRAVIS`; it moves erratically (every
    other turn `teleportmonst` it within 3 cells). On death it drops 3
    random items via `something()` and a lot of gold; message *"The null
    pointer dereferences. Garbage spills everywhere."*
  * Glitch ends: *"Reality reasserts itself. You feel vaguely embarrassed."*
* `US_EV_WILD_GLITCH(start|end)` events for the radio.

**Files you own.** `compat/wild_glitch.c`, tiles 384 and 408–415,
`docs/wild/glitch.md`, `game/GlitchFx.kt` (new; called from GameView).
**Shared insertion points.** `scroll.h/.c` (SSEGFAULT), `dungeon_obj.c:
oteleport()` one line, `ularn_winandroid.c: GetTile()/PaintMap()`,
`GameView.kt` (3–6 lines calling `GlitchFx`), `monster.c` row.
**Settings.** `WILDF_GLITCH`; "Reduce glitch effects" (no slicing/hue, keeps
the tile swaps — for motion-sensitive players).
**Test.** Host harness: create+read the scroll, confirm the counter, names
swap, null pointer spawns and dies, the end message. Kotlin: unit test the
tile-substitution math stays in range.

### 3.7 Fishing (C) — size M

**Pitch.** Fountains contain fish. And boots. And one very annoyed kraken.

**Design.**
* `OFISHINGROD` (379): sold by the DND store (append) and 1 placed on level
  1 or 2 (`wild_make_level`). With a rod in the pack, the fountain prompt
  (`dungeon_obj.c: ofountain()`) gains **(f)ish**: pass the extended answer
  string only when `wild_on(WILDF_FISHING) && have_rod()`; handle `'f'` by
  calling `wild_fish()`.
* `wild_fish()`: takes 1–5 turns (loop `nap(300)` + message "…"); monsters
  still move: call `movemonst()` between waits like `ACTION_WAIT` does, and
  stop early if hit (`hitflag`). Catch table (skill `c[WILD_FISH_XP]`
  grows by 1 per catch, shifts odds toward the good stuff):
  `OFISH` 45%, nothing 20%, `OOLDBOOT` 15%, `OBOTTLE` 8%, a gold ring
  (`ORING` of a random kind via `something()`-style creation) 5%, a random
  potion 4% ("Someone threw this in"), **kraken** 3%. Fountain has 15%
  chance per catch of drying up (`ODEADFOUNTAIN`).
* `OFISH`: eaten (same path as the Vending agent's foods — coordinate via
  `wild_eat(obj)`; if the Vending agent's hook isn't there yet, implement
  `wild_eat` in `wild.c` and let the merge sort it out): heals 10 + level.
  Also drop it in front of a Roomba for fun (no code needed).
* `OOLDBOOT`: value 1 at the trading post (`item_value`), can be **worn**
  as armour AC 1 (hooks into `wear` via `wild_wear`)? Keep: just junk,
  sells for 1 gp, 20% it contains a small gem when picked up.
* `OBOTTLE`: reading it (`r`) shows a random fortune (`fortune.c`'s
  `fortune()` text) and vanishes.
* **Fountain kraken** (`FOUNTAINKRAKEN`, 385): LV 7, AC 4, DAM 6, ATT 0,
  HP 70, XP 300, `FL_NOBEHEAD`; spawns in the fountain's cell *replacing
  the fountain with floor* ("Something enormous surfaces. The fountain was
  deeper than it looked."). Cannot move more than 2 cells from where it
  spawned (clamp in `wild_turn()` by teleporting it back) — it's attached
  to the plumbing. Drops its own weight in gold.
* Events `US_EV_WILD_FISHING(cast|catch|kraken)`.

**Files you own.** `compat/wild_fishing.c`, tiles 379–382, 385,
`docs/wild/fishing.md`.
**Shared insertion points.** `dungeon_obj.c: ofountain()`, `store.c itm[]`
(rod), `store.c: item_value()` (boot), `action.c` read path (bottle) and eat
path (fish), `monster.c` row.
**Settings.** `WILDF_FISHING`.
**Test.** Host harness: create rod + fountain, fish 30 times (loop keys),
verify every outcome appears across runs with a fixed seed (the harness
supports `--seed`), kraken spawns and stays near.

### 3.8 Pets — the mysterious egg (C) — size L, highest risk

**Pitch.** Carry an egg long enough and it hatches into a loyal monster that
follows you and fights for you. Then you have to keep it alive.

**Design.**
* `OEGG` (378): 1 placed on levels 2–5 in `wild_make_level`, and Dealer
  McDope sells one (optional). Picking it up starts `c[WILD_EGG] = 300`.
  `wild_turn()` decrements while an egg is in the pack ("The egg wobbles."
  at 100, "You hear tapping." at 30). At 0 it hatches into a pet of a random
  species from `{JACKAL, SNAKE, GIANT ANT, HELL HOUND (rare), FLOATING EYE}`:
  `c[WILD_PET_MON]`, `c[WILD_PET_HP]`; egg removed.
* Representation: the pet is a real monster on the level: `mitem[x][y].mon =
  species`, with **pet marking** kept in `wild_pets.c` as the pet's current
  coordinates (`pet_x, pet_y` in `extra[]` slots WX_PET_X/Y, saved; also
  mirrored to `S->pet_x/pet_y`). Each turn in `wild_turn()`:
  1. Verify the monster at the stored coords is still our species; if not
     (it died, was teleported, polymorphed) → "Your pet is gone." and clear.
  2. **Move**: if not adjacent to the player, step one cell toward the
     player (8-dir, `cgood`-style walkable check, swap with nothing). Use
     `moved[][]` so upstream `movemonst()` doesn't also move it: set
     `moved[x][y] = 1` **before** `movemonst()` runs — i.e. mark it in
     `wild_turn()` for the *next* loop iteration; study `movemonst()` to see
     when `moved[][]` is cleared and pick the right spot. If that proves
     fragile, the alternative is making `movemt()` return early for the
     pet's coords (one guarded line).
  3. **Attack**: if a hostile monster is adjacent to the pet, deal
     `rnd(monster[species].damage + 2)` to it via the existing
     `hitm()`-equivalent for monsters (write a small `pet_hit(x,y,dmg)` that
     reduces `hitp[][]`, prints "Your <pet> bites the <monster>!", and
     handles death with the same drops as `hitm` — simplest is to call
     `hitm(x, y, dmg, 0)` which already does everything; check it doesn't
     assume the player is adjacent).
  4. Hostiles attack the pet: upstream monsters only target the player.
     Simplest believable version: in `wild_turn()`, each hostile adjacent to
     the pet has a 50% chance to hit it (`hitp[pet] -= rnd(dam)`); if ≤ 0 the
     pet dies: "Your <pet> dies. You feel a lump in your throat." and the
     player gets `c[WILD_PET_MON] = 0`. Keep `hitp` in sync with
     `c[WILD_PET_HP]` for saving (levels save `hitp` anyway).
  5. Player walking into the pet **swaps places** instead of attacking:
     `player.c: moveplayer()` → `if (wild_is_pet(x,y)) { swap; return; }`
     guarded one-liner.
  6. Spells/bolts: don't special-case; the player can kill their own pet
     with a fireball. That's roguelikes.
* Level changes: the pet follows if adjacent when you take stairs
  (`wild_new_level`: place it next to the player on the new level, remove
  it from the old one — the old level is already saved, so do it in
  `newcavelevel` *before* `savelevel()`: hook `wild_leaving_level()` there),
  otherwise it stays and you get "You hear a sad whine behind you."
* Render: `S->pet_x/pet_y`; GameView draws the heart overlay (397) above
  the pet's tile. Hatching animation: tiles 398–399 as `overlay` for two
  turns.
* Eating a fish while the pet is adjacent feeds it (+10 HP) — if the fish
  item exists after merge; otherwise skip.

**Files you own.** `compat/wild_pets.c`, tiles 378, 397–399,
`docs/wild/pets.md`.
**Shared insertion points.** `player.c: moveplayer()` (swap), `monster.c:
movemt()` or `movemonst()` (skip pet) — one guarded line, `dungeon.c:
newcavelevel()` (leaving-level hook), `GameView.kt` (heart).
**Settings.** `WILDF_PETS`.
**Test.** Host harness with `--seed`: wizard-create the egg, wait 300 turns
(`.` × 300 via the harness repeat syntax), confirm hatch, walk away and see
it follow, create a lemming next to it and see it attack, take stairs with
it adjacent and non-adjacent, save+restore mid-pet (`S` then restart with
the save) and confirm it is still there.

### 3.9 Level-up Perks (C) — size M

**Pitch.** Roguelite flavour: each level-up offers a choice of three perks.

**Design.**
* `wild_level_up()` increments `c[WILD_PERK_PICKS]`. At the next command
  prompt (`wild_turn()` start, when `input_mode` is about to be command and
  no monster is adjacent — or simply at the top of `wild_turn()`), if picks
  > 0: roll 3 distinct perks the player doesn't have, show
  *"LEVEL UP! Choose a perk: (a) Magnet Hands  (b) Thick Skin  (c) Lucky"*
  with one-line descriptions via `Print`, then `get_prompt_input(..., "abc", 1)`.
  Store as bits in `c[WILD_PERKS]`. Max 10 perks, pick list exhausted →
  "+5 max HP instead".
* Perks (implement each where it bites; every edit guarded by
  `wild_perk(PERK_X)`):
  1. **Magnet Hands** — gold within 1 cell is pulled to you each turn
     (`wild_turn`: scan 8 neighbours for gold piles, move them under you;
     `lookforobject` handles pickup).
  2. **Thick Skin** — +2 AC (`recalc()`).
  3. **Vampiric** — heal 1 HP per kill (`wild_kill`), +1 more if HP < 25%.
  4. **Bookworm** — spells cost 1 less (`spell.c: cast()` cost line), min 1.
  5. **Lucky** — item drops from monsters doubled (`monster.c:
     dropsomething()` call it twice).
  6. **Nimble** — 10% dodge (`hitplayer()` via the Hats agent's
     `wild_dodge()` hook — implement `wild_dodge()` in `wild.c` summing both
     sources; coordinate by making it OR together).
  7. **Haggler** — 10% off at the DND store and trading post pays 10% more.
  8. **Glass Cannon** — +4 weapon class (`c[WCLASS]` in `recalc`), −10% max
     HP once when taken.
  9. **Second Wind** — once per level (dungeon level), dying with ≥ 1 perk
     slot… no: when HP would drop to 0 and you have not used it on this
     level, HP becomes 1 instead (`losehp()` guard) — "Second wind!"
  10. **Night Owl** — +2 to all stats while on the surface at night (needs
      the Weather agent's `c[WILDF_WEATHER]` night flag; if absent, "+1 INT
      permanently" as fallback). Keep this one last and optional.
* Side panel / HUD: `inv_text`-style list isn't needed; show perks in the
  `\` (list spells/items) screen via a small `wild_show_perks()` appended to
  `show.c`'s known-items page, and in `GameState` as `wildFlags`? No —
  export `c[WILD_PERKS]` into `S->wild_flags` high 16 bits (foundation
  reserved `wild_flags`; low 16 bits are the feature mask, high 16 the perk
  bits) so `SidePanelView` can list them.

**Files you own.** `compat/wild_perks.c`, `docs/wild/perks.md`.
**Shared insertion points.** `player.c: recalc(), losehp()`, `spell.c:
cast()` cost, `monster.c: dropsomething()` call site, `store.c` prices,
`show.c`, `SidePanelView.kt` (perk list).
**Settings.** `WILDF_PERKS`.
**Test.** Host harness: wizard-raise level (the game's `^` ? no — use
`ACTION_BECOME_CREATOR`/wizard `+` XP command if present, else create many
kills), take each perk, verify its effect with before/after numbers.

### 3.10 Atmosphere — Polaroid photo mode + weather & day/night (Kotlin + a little C) — size M

**Pitch.** Take instant photos of your run; and the surface now has weather
and a day/night cycle, because why does the town never rain.

**Photo mode.**
* In-game menu item **"Take a photo"** and a `Settings`-assignable button
  action "Photo" (add to the control button actions in
  `controls/ControlLayout.kt` + `ControlsOverlay.kt` if the action enum is
  easy to extend; otherwise menu only).
* `photo/Polaroid.kt`: renders the *current map viewport* (`GameView`
  exposes `renderMapBitmap(scale)`) at 2× into a Polaroid frame bitmap:
  off-white border, thicker at the bottom, a hand-written-style caption
  (`Typeface.SERIF italic` is fine): *"<name> the <title> — level <n>,
  turn <t>"* and the date; a subtle vignette; 1–3 random pixel-art stickers
  from the tile sheet (a gem, a potion, the player's class tile). Shutter
  flash + camera click via Radio SFX if the Radio agent's `Sfx` exists —
  otherwise a `ToneGenerator` beep.
* Saves to `MediaStore.Images` (Pictures/Ularn Wild) — no permission needed
  on API 29+; on API 24–28 write with `WRITE_EXTERNAL_STORAGE` requested at
  runtime (add to the manifest with `maxSdkVersion="28"`). Then offers
  Share (`Intent.ACTION_SEND`).
* Also auto-snap on death and on winning (setting), shown in the stats
  dialog as the run's "last photo".

**Weather and day/night.**
* C (`compat/wild_weather.c`): `c[WILDF_WEATHER]` packs `time_of_day`
  (0–23 = `(gtime / 50) % 24`) and the weather state (clear / rain / snow /
  storm) that changes every 200–600 turns on the surface with messages
  (*"It starts to rain."*, *"Snow! In Larn!"*, *"Thunder rolls over the
  mountains."*). Exported each `PaintStatus()` to `S->weather` =
  `hour | weather << 8`. Gameplay: at night (hour 21–5) on the surface,
  `rmst` random-spawn interval halves (`ularn.c` guarded one-liner) and the
  **lemming migration** may happen: 1-in-300 per night turn, a line of 8–12
  awake lemmings appears at the east edge and marches west (upstream
  movement handles the marching; just place them) — *"You hear the
  pitter-patter of a thousand tiny feet."* Storms: 1-in-200 a lightning bolt
  hits a random visible monster for 20 (uses `omnidirect`-style damage or
  just `hitp[][] -= 20` and the kill path).
* Kotlin `game/WeatherFx.kt` called from `GameView.drawMap()` when `dlevel
  == 0` (home level): a day/night tint (multiply the map by a colour that
  goes warm at dawn/dusk and deep blue at night, 40% max darkness so it
  stays playable), rain streaks / snow flakes as tiles 400–403 randomly
  overlaid on floor cells drifting each frame (invalidate at 10 fps only
  while weather is active), occasional full-screen white flash for storm
  lightning, stars (404–405) in the black "unknown" cells at night, and a
  sun/moon (406/407) in the HUD next to the level name.

**Files you own.** `photo/*.kt`, `game/WeatherFx.kt`, `compat/wild_weather.c`,
tiles 400–407, `docs/wild/atmosphere.md`, `AndroidManifest.xml` (permission).
**Shared insertion points.** `MainActivity.showMenu()`, `GameView.kt`
(`renderMapBitmap`, WeatherFx call), `ularn.c` (rmst), `ularn_winandroid.c:
PaintStatus()` (weather), `stats/StatsDialog.kt` (last photo, optional).
**Settings.** `WILDF_WEATHER`, `WILDF_PHOTO`; "Auto-photo on death/victory";
"Reduce weather effects".
**Test.** Unit test the Polaroid layout math and hour/weather packing. Host
harness: advance 1200 turns on the surface, see weather messages and the
`weather` field change; see a lemming migration with `--seed` search.

---

## Part 4 — Orchestration

1. Foundation lands on the branch; `assembleDebug` passes; host harness
   plays a new game to level 2 and back.
2. **Wave 1** (parallel worktrees): 3.1 Radio, 3.2 Cards, 3.3 Vending,
   3.4 Hats. Merge, build, smoke-test, commit.
3. **Wave 2**: 3.5 LarnCoin, 3.6 Glitch, 3.7 Fishing, 3.8 Pets. Merge, build.
4. **Wave 3**: 3.9 Perks, 3.10 Atmosphere. Merge, build.
5. Integration: full 3-ABI `assembleDebug`, host harness regression
   (`tools/host/smoke.sh`: new game → surface walk → enter dungeon → level
   2 → save → restore), README update, push. CI publishes
   `ularn-wild.apk` on the `wild-latest` prerelease.

Merge-conflict policy: foundation pre-allocated everything, so conflicts
should be limited to `MainActivity.showMenu()`/`showSettings()` item lists,
`GameView.drawMap()` overlay calls, and `wild.c` hub bodies. Resolve by
keeping both sides; re-run the build after every merge.
