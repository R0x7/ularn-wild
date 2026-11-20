# Part 00 — Recreating "Ularn for Android: Wild Edition" from scratch

This folder is a complete specification of the game in this repository, written so that
an agent with **no access to the source code or the assets** can rebuild it. It contains
no code and no asset files. Every rule, table, formula, screen, string and asset is
described in words and data tables.

This part is the entry point. It explains what the product is, how the specification is
organised, the recommended way to build it (including a different language and
architecture from the original), the assets that have to be created, a milestone
roadmap, and how to check the result. Part 16 lists suggested improvements.

## 1. How to use this specification

1. Read this part in full.
2. Read Part 09 section 2 (the display state) and Part 01 section 8 (the turn loop).
   Almost every other part refers to those two.
3. Build in the order of the roadmap (section 9). Each milestone names the parts it
   needs.
4. When two parts disagree, the part that **owns** the topic wins (the ownership column
   in section 6). Report the disagreement instead of averaging the two.
5. Anything marked "(unverified)" in a part could not be confirmed from the source.
   Treat it as a reasonable default, not a requirement.

The parts were written from the source by separate readers and then spot-checked against
the source. Their numbers are meant to be exact. Where a part says "quirk", the original
really behaves that way, however odd it looks.

## 2. The product

### 2.1 The game

Ularn is a turn-based, single-player roguelike from the Larn family (1986 onward). The
version specified here is Ularn 1.6.3a, which calls itself "The Addiction of Ularn".

- **Story and goal.** Your daughter has the disease dianthroritis. The only cure is a
  potion found at the bottom of a volcano. You win by carrying the potion into your home
  within 400 "mobuls" of game time (one mobul is 100 turns of the clock).
- **World.** 21 levels, each a grid of 67 by 17 squares: a walled-less town (level 0)
  with eight buildings, a 15-level dungeon entered from town, and a 5-level volcano
  reached by a volcanic shaft. Levels are generated once per game and then remembered.
  Roughly half the levels come from a maze generator and half from a file of hand-drawn
  maps.
- **Character.** One of eight classes, with six abilities, hit points, spell points,
  armour class, weapon class, 26 inventory slots and about thirty timed effects. Up to
  100 experience levels.
- **Content.** 56 ordinary monsters, seven demon lords, a demon prince and Lucifer
  (the Wild Edition adds four more monsters); 101 kinds of object in the base game
  and 17 Wild ones (weapons, armour, rings, artifacts, 35 potion kinds, 28 scroll
  kinds, books, chests, gems, drugs, traps, altars, thrones, fountains, statues, mirrors); 39
  spells cast by three-letter codes; spheres of annihilation.
- **Town.** A store, a bank (with a branch on dungeon level 8), a trading post, a
  college, a tax office, your home, and a drug dealer's pad in the dungeon. Winners owe
  tax in their next game and start it at a higher difficulty.
- **Feel.** Fast and lethal. There is no line of sight: you see the 3 by 3 block around
  you and remember what you have seen. Objects act when you step on them, through
  single-key prompts. Time pressure comes from the mobul limit.

### 2.2 The Android app around it

The app draws the game with 32 by 32 pixel tiles and adds phone-friendly controls and
quality-of-life features without changing the rules:

- a start screen with save slots, a scoreboard, settings and a control editor;
- a HUD (hit point bar, level and title, gold, stats, active effects), the tile map with
  pinch zoom, and the message lines;
- swipe, tap and long-press movement, and fully customisable on-screen buttons and
  D-pads with a key-macro notation;
- answer buttons that appear for every prompt, a context action button, a spell picker;
- a round minimap and a whole-level overview, a side panel on wide screens;
- danger cues at low hit points, statistics and 17 achievements, accessibility options;
- re-wrapping of the game's 80-column text pages to fit a narrow screen;
- checkpoints every 25 commands and whenever the app goes to the background.

### 2.3 The Wild Edition

Twelve optional features, each behind its own switch and all on by default. With every
switch off the game is the base game exactly.

| Feature | In one line |
|---|---|
| Dungeon Radio | Synthesised chiptune music and sound effects; cassette tapes unlock bonus tracks |
| Trading cards | Kills drop collectible monster cards into a permanent album |
| Vending machines | Junk food, a rubber duck that identifies, a loot-eating Roomba |
| Hats | Five wearable hats with effects, drawn on the player |
| LarnCoin | A volatile currency at the bank and an ATM, and a Tax Auditor monster |
| Glitch Realm | A scroll of segfault corrupts the display and names for a while |
| Fishing | Fish at fountains; the Fountain Kraken |
| Pets | An egg hatches into a companion that fights and grows |
| Perks | Choose one of three perks at each level-up |
| Weather | A day and night clock and weather in town, with effects |
| Photo mode | A Polaroid-style screenshot with caption and stickers |
| Bounty board | A WANTED poster in town with a gold reward |

## 3. Credits and licence

The game is the work of Noah Morgan (Larn, 1986), Phil Cordier (Ularn), David Richerby,
Josh Brandt and Josh Bressers (Ularn 1.5) and Julian Olds (Ularn 1.6). The original
tiles include work by Craig Spencer. Larn was released on the condition that nobody
makes a profit from it or from work derived from it, and the 1.6 sources carry the GNU
GPL version 2.

A recreation built from this specification is a derived work in every sense that
matters. It must:

- keep the credit lines the game shows (Part 01 section 1) and credit these authors in
  its own documentation;
- be shared free of charge and non-commercially only;
- not claim copyright in the game design, text or original art.

New art, music and text written for the recreation belong to whoever writes them.

## 4. Fidelity policy

"As close to the original as possible" is defined in three tiers. Every part's "quirks"
section lists what falls in tier 2.

| Tier | What | Rule |
|---|---|---|
| 1. Rules and content | Every table, formula, probability, prompt, message and ordering | Reproduce exactly |
| 2. Quirks | Original bugs and oddities that change what the player experiences (for example the dexterity spell that becomes permanent) | Reproduce by default. Each one must be a single, named switch in the code so it can be turned into the fix proposed in Part 16 without a hunt |
| 3. Implementation artefacts | The forked process, shared memory, raw-memory save bytes, reads of unset memory, C integer widths, the platform's random generator | Do not reproduce. Keep only the observable behaviour |

Three consequences of tier 3 need stating:

- **Random numbers.** The original uses the C library's generator, so its exact
  sequences were never portable. Only the distributions matter. Use the two helpers
  everywhere with their exact ranges: rnd(n) is uniform over 1..n and rund(n) is uniform
  over 0..n-1. Note that the original helpers are macros, so a loop bound written with
  one is rolled again on every pass (Part 06 section 19 gives the resulting odds); the
  specification states the effective odds wherever this matters.
- **Integer arithmetic.** All divisions truncate toward zero unless a part says
  otherwise. Gold and experience fit in a signed 64-bit integer; most other values in a
  signed 32-bit integer; map grids in small integers.
- **Undefined behaviour.** A few original code paths read memory that was never set.
  The parts describe the outcome seen in practice. Implement that outcome.

## 5. Conventions used in every part

- **Coordinates.** x runs 0..66 left to right, y runs 0..16 top to bottom. A level's
  outer ring is wall on dungeon levels.
- **Levels.** 0 is the town ("H"), 1..15 the dungeon, 16..20 the volcano ("V1".."V5").
- **Directions.** Eight, entered with the keys h j k l y u b n (west, south, north,
  east, north-west, north-east, south-west, south-east). Capitals run.
- **Time.** The clock counts time units from 0. 100 units are a mobul. The limit is
  40000 units and is tested only at home.
- **Difficulty.** A non-negative integer ("hard game"), 0 for a first game.
- **Identifiers.** Monsters, objects, spells, potions, scrolls and attributes are
  numbered. The numbers matter: some rules do arithmetic on them (for example "add 8 to
  the monster number"), the list screens sort by them, and tile lookups use them. Keep
  the numbering given in Parts 02, 04, 06 and 07.
- **Keys.** The game core is driven only by 7-bit ASCII key codes. Everything the app
  does (gestures, buttons, menus that act on the game) ends up as key codes. Keep this:
  the control editor's macros depend on it.
- **Text.** Quoted strings are exact. The mark for a line break inside a quoted message
  is defined in each part.

## 6. The parts

| Part | File | Owns |
|---|---|---|
| 00 | README.md | This overview, architecture, assets, roadmap, verification |
| 01 | 01-game-flow-and-commands.md | Start-up, options, the turn loop, every command, difficulty, time, help and fortune files |
| 02 | 02-player.md | The attribute array, classes, levels, regeneration and timers, armour and weapon class, inventory |
| 03 | 03-screens-scores-saves.md | Item naming, list screens, scoreboard, score, causes of death, end of game, save contents |
| 04 | 04-monsters.md | The monster table, selection by depth, movement, attacks both ways, drops, genocide and polymorph |
| 05 | 05-dungeon.md | Level structure, the town, maze generator, pre-made maps, object and monster placement, level changes |
| 06 | 06-objects.md | The object table, every dungeon feature and trap, potions, scrolls, books, chests, drugs |
| 07 | 07-spells-and-spheres.md | The 39 spells, casting, projectiles, the resistance table, spheres of annihilation |
| 08 | 08-stores-and-buildings.md | Store stock and prices, bank, trading post, college, tax office, home, dealer |
| 09 | 09-native-front-end.md | The display and input contract between game and app, tile choice per square, animation timing, checkpoints, context action, events |
| 10 | 10-app-shell.md | Start screen, menus, settings, save slots, statistics and achievements, accessibility |
| 11 | 11-game-view-and-controls.md | Rendering, HUD, gestures, text re-wrapping, minimap, side panel, on-screen controls and their editor |
| 12 | 12-wild-gameplay.md | All game-side rules of the twelve Wild features |
| 13 | 13-wild-app-features.md | Synthesiser, songs, sound effects, cards, photo mode, weather and glitch rendering |
| 14 | 14-tile-art-base.md | The base tile sheet (ids 0 to 367) and every tile mapping |
| 15 | 15-wild-art-build-tooling.md | Wild tiles (368 to 415), icon, resources, build, CI, the test harness, the asset inventory |
| 16 | 16-suggested-improvements.md | Optional changes: architecture, bug fixes, usability, design |

Parts 01 to 08 are the base game. Parts 09 to 11 are the app. Parts 12 and 13 are the
Wild Edition. Parts 14 and 15 are art and tooling.

## 7. Architecture

### 7.1 How the original is built

The original is two programs in one app. The game core is about 23,000 lines of C from
2004 that keeps all state in globals, blocks on input from deep inside its call stack,
and ends by calling "exit". The app therefore runs each game as a forked copy of its
own process. The game writes what should be on screen into a block of shared memory and
reads key codes from a pipe. A Kotlin app (about 11,000 lines) draws that block and
sends keys. Part 09 describes all of this.

None of that plumbing is worth copying. What is worth copying is the **shape**:

1. a game core that knows nothing about the screen;
2. a **display state** that the core fills in (map tiles, status values, messages, a
   text page, what input is expected, the valid answers, a context action, events);
3. an app that only renders the display state and sends key codes.

That separation is why the original could be tested without a device, and it is what
lets the same game run on a phone, in a browser and in a test harness.

### 7.2 Recommended design for the recreation

**Language: Kotlin throughout, with the game core as a platform-neutral Kotlin
Multiplatform module.** Reasons:

- One language for rules, app and tests removes the C-to-Kotlin bridge, the forked
  process, shared memory, the hand-kept list of field offsets, and a whole class of
  portability traps (the original broke on ARM because of unsigned characters).
- The core's "block in the middle of an action and wait for a key" style maps directly
  onto suspending functions. The game runs as one coroutine; the app feeds it keys. No
  thread, process or re-entrant state machine is needed.
- The core compiles for the JVM, so the whole game runs in fast unit tests and a
  command-line harness on any development machine.
- The same core can later target the browser (the original also ships a WebAssembly
  build), again with no threads.

A sound alternative is a Rust core with thin Kotlin and web shells. It gives the same
separation with stronger guarantees, at the cost of a bridge layer and an explicit
state machine or a dedicated thread for input. TypeScript with a web view shell is a
third option if the browser is the main target. The rest of this section is written
for the Kotlin design but the constraints apply to any of them.

**Modules.**

| Module | Contents | May depend on |
|---|---|---|
| core | All rules of Parts 01 to 08 and 12, the display state and event definitions of Part 09, save and load | Nothing but the standard library and coroutines |
| data | The tables (monsters, objects, spells, potions, scrolls, classes, titles, store stock, courses) and the three text data files (help, fortunes, maps), kept as data, not scattered through code | core types |
| harness | A command-line driver that plays the core from a key script and prints state (Part 15) | core |
| app | Everything in Parts 10, 11 and 13 | core, the Android framework |
| tools | The tile sheet builder and tile dumper (Part 15) | Nothing |

**Constraints on the core.**

1. **No globals.** One object owns a whole game. Starting a new game is constructing a
   new object, which replaces the forked process.
2. **Injected randomness.** One generator object, seedable, owned by the game. Every
   random draw goes through rnd and rund on it. This makes runs reproducible from a
   seed, which the harness and the tests rely on.
3. **Suspending input.** Five input requests, as in Part 09 section 5: a command key, a
   key from a set of valid answers, a direction, a number with a default, and a
   password. Each publishes what it is waiting for, then suspends.
4. **Published snapshots.** After each batch of changes, and before every wait and
   every delay, the core publishes an immutable snapshot of the display state. The app
   observes the latest snapshot. This replaces the shared block and its sequence
   counter and removes torn reads.
5. **Delays are requests.** Animation pauses (Part 09 gives every duration) are
   suspending delays made after publishing. A "no delays" setting scales them to zero.
   The harness always runs with zero delay.
6. **Typed results.** A game ends by returning a result (died with a cause, won, saved,
   quit, could not load) instead of ending a process. The original app learns about
   kills and the final score by matching printed text; do not copy that, emit events.
7. **Typed events.** The event list of Part 09 section 10 is the single channel for
   "something happened" (kill, level-up, new level, pick-up, hit taken, gold, spell,
   died, won, and the Wild events). Sound, cards, statistics and danger cues all listen
   to it.
8. **Explicit save format.** Save every field that Part 03 section 9 lists, in a
   versioned, self-describing format, with the Wild feature mask and Wild state. Do not
   dump memory. The byte layout of the original is not a requirement; what is and is
   not saved is.
9. **Keep the text page.** Buildings, lists and help write to an 80 by 25 character
   page with six text styles. Keep that model for the base build, because the screen
   layouts in Parts 03 and 08 are specified in those terms. Part 16 proposes a
   structured alternative.
10. **Keep the key protocol.** Section 5.

**Constraints on the app.**

1. The app never contains a game rule. It may know tile ids, the meaning of display
   fields and event ids, nothing else. The two original exceptions are the spell
   picker's description texts and the card stat table (Parts 10 and 13); generate both
   from the data module instead of keeping second copies.
2. All drawing of the map, HUD and controls is custom canvas drawing at exact sizes
   given in Part 11. Either a classic custom view or a canvas in a declarative toolkit
   is fine. Dialogs and settings can use standard widgets.
3. Game files live in app-private storage: one folder per save slot holding the save,
   the checkpoint and a small metadata file; one shared scoreboard (Part 10 section 8).

### 7.3 What changes for the player if the recommended design is followed

Nothing that the specification calls a rule. Three things behave slightly better and
are called out in Part 16 so that the choice is deliberate: saves from older versions
can be migrated instead of refused, a game can be reproduced from its seed, and the
end of a game is reported directly instead of being read off the score page.

## 8. Assets to create

The original ships very few asset files. Everything else (music, sound, card art,
photo frames, HUD) is produced by code from the recipes in Parts 11 and 13. There are
no audio files, no font files and no images other than the tile sheet and the icon.

| Asset | Form | Size | Described in | Notes |
|---|---|---|---|---|
| Tile sheet | One PNG, 32 by 32 pixel tiles, 16 per row, with transparency | 512 by 896 pixels, 448 cells | Parts 14 and 15 | Ids 0 to 367 are the base game: monsters, objects and features, player figures for 8 classes and 2 genders, cursor, floor, unknown, 40 directional bolt frames, sparkle frames, about 90 spell animation frames, 16 wall shapes. Ids 368 to 415 are the Wild tiles. Ids 416 to 447 are spare. Tile id n sits at column n mod 16, row n div 16 |
| Help text | Plain text, 23-line pages with style markers | A welcome page and 11 help pages | Part 01 section 11 | Must be rewritten from the page-by-page description |
| Fortunes | Plain text, one per line | 39 lines, at most 64 characters | Part 01 section 12 | Dry one-line hints and jokes about the game; write new ones in the same vein |
| Pre-made maps | Plain text grids | 21 maps of 17 lines by 67 characters | Part 05 section 8 | Hand-drawn vault and corridor layouts using a small set of symbols for walls, doors, monsters, treasure and the two unique items. Draw new ones to the described style and symbol budget |
| Launcher icon | Vector foreground and background | Adaptive icon | Part 15 | |
| Songs | Data in the song notation | 7 standard tracks and 3 unlockable ones | Part 13 sections 3 and 4 | Compose new tracks to the given tempo, key, length and character |
| Sound effects | Synthesis recipes | About a dozen | Part 13 section 6 | Fully specified; nothing to author |
| Card texts | One flavour line per card | 69 lines | Part 13 section 7 | Format, tone and examples are given; the rest must be written |
| Spell descriptions | One short line per spell | 39 | Part 10 section 9 | Listed in full |
| Interface strings | Text | Several hundred | Parts 10, 11, 13 | Listed in each part's string index |

The Wild tiles are authored as text files, one character per pixel with a small palette
header, and composed onto the sheet by a tool (Part 15 sections 2 and 3). Use the same
method for the whole sheet: it lets an agent that cannot paint produce pixel art,
review it as text, and keep it under version control. Part 14 section 4 is the style
guide. In short: chunky, shaded sprites with no anti-aliasing, drawn on an opaque
background of the floor colour (a muted teal, #426b6b) with a hard black cast shadow
to the lower right, and mostly without outlines. Only the cursor, the sparkle frames
and the Wild overlay tiles (hats on the player, the pet heart, weather, glitch) have
transparent pixels. Because the floor is painted into every tile, a monster or item
is never drawn over a separate floor tile.

## 9. Roadmap

Each milestone ends with something that can be run and checked. Milestones 1 to 6 need
no device: they are driven through the harness. The art track and the app track can
start as soon as milestone 1 has fixed the identifiers and the display state.

| # | Milestone | Build | Parts | Done when |
|---|---|---|---|---|
| 0 | Skeleton | Project, modules, continuous integration, the random helpers, the identifier lists, the harness with its step notation and state printer | 00, 15 | The harness starts an empty game, accepts a key script and prints the display state; a fixed seed gives identical output twice |
| 1 | Contract and world | The display state and input requests; the level store; the town; the maze generator; pre-made maps; stairs, entrance and shaft; walking, running and map reveal | 09, 05, 01 | A scripted character walks the town, enters the dungeon, goes down and up, and each level is the same on return; generated levels match the structural checks in section 10 |
| 2 | Player and objects | Classes and creation; the attribute array; inventory; every object interaction; potions, scrolls, books; item naming and list screens; timers and regeneration | 02, 06, 03 | Every object can be placed by a debug key and behaves as specified; each class starts with the right kit; the inventory screen matches the layout rules |
| 3 | Monsters and combat | The table; selection by depth; placement and wandering spawns; movement; attacks both ways; special attacks; drops; experience and levels | 04, 02 | Scripted fights against each attack type give outcomes inside the specified ranges; kill rewards and drops match their tables over many seeded trials |
| 4 | Spells and spheres | Casting, all 39 spells, projectiles and their animation requests, the resistance table, spheres | 07 | Each spell can be cast from a script and its effect verified; animation requests appear in the display state in the specified order |
| 5 | Town and endings | Store, bank, trading post, college, tax office, home, dealer; the scoreboard; scoring; every ending; save, load and checkpoint; difficulty carry-over and tax | 08, 03, 01 | A scripted game can be saved and resumed mid-level with identical state; a scripted win and a scripted death produce the right screens and scoreboard entries; **the base game is complete** |
| 6 | Wild rules | The feature mask and hooks, then each feature. The features are independent of each other and can be built in parallel once the hooks exist | 12 | Each regression scenario of Part 15 passes; with the mask at zero, a seeded script gives the same output as before this milestone |
| 7 | Tiles | The full sheet, authored as text tiles | 14, 15 | The builder validates every tile; a contact sheet has been reviewed against the descriptions |
| 8 | Game view | The map, HUD, messages, text pages, answer buttons, gestures, keyboard, zoom | 11, 09 | A complete game can be played on a device with touch only |
| 9 | Shell | Start screen, slots, settings, menus, scoreboard, lifecycle, checkpoint on background | 10 | Kill the app mid-game and resume from the checkpoint; export and import a slot |
| 10 | Controls | The on-screen control model, default layouts, play mode, the editor, profiles | 11 | Layouts survive restart and rotation; a macro button casts a spell |
| 11 | Comforts | Context action, spell picker, minimap and overview, side panel, text re-wrapping, danger cues, statistics and achievements, accessibility | 10, 11 | The unit tests listed in section 10 pass; a screen reader names every control |
| 12 | Wild app features | Synthesiser, songs, sound effects, radio, cards and album, photo mode, weather and glitch rendering, pet and bounty panels | 13 | Each feature can be switched off independently and leaves no trace when off |
| 13 | Release | Signing, versioning, the published build, the developer documentation | 15 | A fresh install plays; an update keeps saves |

**Working in parallel.** Milestones 0 and 1 must be done by one owner, because they fix
the identifiers, the display state and the event list that everything else shares.
After that the safe splits are: one owner each for Parts 02 with 06, 04, 07, and 08
with 03; one per Wild feature; one for the tile sheet; one for the game view; one for
the shell and controls. Give every owner a fixed list of files and reserve identifiers,
tile ids and settings keys before they start. This is how the original Wild Edition
was built, and Part 12 shows the reservation tables.

## 10. Verification

A recreation cannot be compared with the original line by line, so correctness has to
come from checks that follow from the specification.

1. **Table checks.** Unit tests that count and total every data table: 70 monster rows
   (a placeholder row, 56 ordinary monsters, 4 Wild ones, 7 demon lords, the demon
   prince and Lucifer), 118 object rows (101 base, 17 Wild), 121 attributes, 39
   spells, 35 potion kinds and 28 scroll kinds with their weight tables, 99 store
   rows (93 base, 6 Wild), 8 classes, the level titles and the experience table, 21
   maps of 17 by 67. A wrong table is the commonest error and
   the cheapest to catch.
2. **Scenario scripts.** Key scripts run through the harness with a fixed seed and a
   debug menu that can place any object or monster (Part 15 lists the menu and nine
   scenarios with their expected outcomes: new game to save and restore, the Second
   Wind perk, treasure maps, annihilation kills, pet growth, the pet and bounty panels,
   Wild monster exclusion, the Cartographer perk). Add one scenario for every "done
   when" in the roadmap.
3. **Distribution checks.** For each probability table (drops, scatter, fishing,
   vending kicks, perk choices, fountain and altar outcomes), run the routine many
   thousands of times with seeded randomness and compare the frequencies with the
   stated odds within a tolerance.
4. **Structural checks on levels.** Every generated level has its required stairs, the
   outer wall is intact, counts of scattered objects are inside their ranges, unique
   items appear exactly once on their levels, and every level that should have a way
   down has one. Note that the original does not guarantee that every square is
   reachable without digging; do not test for more than Part 05 promises.
5. **Screen checks.** Golden text for the fixed screens: class selection, the four
   store pages, the bank, the college, the tax office, the scoreboard, the help pages.
6. **Save round trip.** Save, load, and compare the entire game state, for seeded games
   stopped at random turns.
7. **Flags-off check.** With all Wild switches off, seeded scripts give the same
   output before and after each Wild feature is added.
8. **App unit tests.** The original has unit tests for the text re-wrapper, the danger
   tracker, the song parser, the synthesiser, the track picker, the card album, the
   glitch, weather and photo geometry, the pet and bounty status decoders, the run
   statistics and the difficulty setting. Parts 10, 11 and 13 describe their cases.
   Rebuild all of them.
9. **Device checklist.** Portrait and landscape, a narrow phone and a tablet, rotation
   during a prompt, background and resume, process death and checkpoint resume, a
   hardware keyboard, a screen reader, font scale at its extremes.

## 11. Traps for the implementer

- **Stepping on things is the interface.** There are no "pray", "drink" or "sit"
  commands. The square under the player is examined at the start of every turn and
  asks its question then. Resting one turn asks again. Get Part 01 section 8.2 exactly
  right before building any object.
- **Monsters move at the start of the next turn**, after the object check, and not at
  all on a turn in which the player was just attacked.
- **Identifier arithmetic.** See section 5. The Wild monsters were inserted before the
  demon lords, which shifted some results; Part 16 discusses it.
- **Awake and seen flags for monsters are not stored per level** and are not saved.
  Part 04 section 13 and Part 05 section 17 explain the visible effect.
- **Escape is not always "cancel".** At number prompts it accepts the default, which
  is often "everything". Only lower-case y means yes in the buildings.
- **Timed boosts are not symmetric.** Several effects add one amount and remove
  another. Parts 02 and 06 list each.
- **The text page is load-bearing.** The store and bank layouts, the re-wrapping
  heuristics of Part 11 and the track picker of Part 13 (which recognises the shop and
  the bank by words on the page) all depend on the exact text.
- **Wild features follow two masters.** Rules use the mask stored with the character.
  Sound and cards use the live app setting. Part 13 section 14 lists the mismatches.
