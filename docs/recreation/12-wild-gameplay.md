# Part 12 — Wild Edition gameplay (all the C-side Wild features)

This part specifies everything the "Wild Edition" adds to the game rules: the feature
mask, the hub that connects the features to the base game, the extra character
attributes, save data, monsters and objects, and each of the eleven gameplay features
(Dungeon Radio's game side, trading-card drops, vending machines, hats, LarnCoin, the
Glitch Realm, fishing, pets, level-up perks, weather and day/night, the bounty board),
plus the wizard-only Wild debug menu and every message the Wild code prints.

It leans on: Part 01 (turn loop and commands), Part 02 (attributes, `recalc`, taking
items, hit points), Part 03 (the known-list screen, score, save file body), Part 04
(monster table, movement and attacks), Part 05 (level creation), Part 06 (objects,
fountains, traps, scrolls), Part 08 (DND store, bank, trading post), Part 09 (the shared
state block and the event ring as a transport) and Part 13 (what the app does with the
exports and events: music, card album, glitch and weather drawing, side panel, ticker).
Tile pictures are in Part 15; here only tile ids are given.

Where the design brief (`docs/WILD_EDITION_PLAN.md`) or the user pages (`docs/wild/*.md`)
disagree with the code, the code is what is described here; the differences are listed
in section 19.

---

## 1. Conventions used in this part

* **rnd(n)** is a uniform integer 1..n; **rund(n)** is a uniform integer 0..n-1. Both
  come from the game's single pseudo-random stream (checked in the source).
* "x in 100" style odds are written as "rnd(100) ≤ x". All divisions are integer
  divisions (truncating) unless stated.
* **Level index**: 0 is the town ("H"); 1..15 are the dungeon levels; 16..20 are the
  volcano levels V1..V5. There are 21 levels in all. "The current level" means this
  index.
* The map is 67 columns (x = 0..66) by 17 rows (y = 0..16). y grows southwards.
* **Distance** between two cells always means Chebyshev distance: the larger of the
  horizontal and vertical differences.
* **The eight directions** are numbered 1..8 in this order: south, east, north, west,
  north-east, north-west, south-east, south-west. Where a rule says "directions 1..8"
  that order matters.
* **Turn counter** ("game time"): a counter that goes up by one per player action, by
  one per two actions while the player is hasted, and not at all while time is stopped
  (Part 02). The Wild per-turn hook still runs once per player action in all three
  cases, so rules below say either "per Wild turn" (every action) or "by the turn
  counter" (clock-based).
* **Free-cell helper** (used by many features to place things): try up to 400 times:
  pick x = rnd(65) (1..65) and y = rnd(15) (1..15); accept the cell if its object is
  "nothing" (bare floor), no monster stands on it and it is not the player's cell. If no
  try succeeds, the helper fails and the caller places nothing.
* **Standable test** (the base game's cell check, Part 05): the cell is inside the map,
  is not the level-1 exit cell (33,16), and is not a wall or a closed door. Two optional
  extra conditions are named where used: "no object" (the cell holds nothing) and "no
  monster".
* A message quoted below is printed on a new line unless it is described as continuing
  the current line. `<name>` stands for a substituted value.
* "Known" for a cell means the player's map memory for it is set (the cell is drawn).

---

## 2. The feature mask

### 2.1 Bits

| Bit | Value | Name | Feature |
|---|---|---|---|
| 0 | 1 | RADIO | Dungeon Radio (music is in the app; the game side is the cassette tapes) |
| 1 | 2 | CARDS | Monster trading cards drop from kills |
| 2 | 4 | VENDING | Vending machines, junk food, rubber duck, Roomba |
| 3 | 8 | HATS | Hats |
| 4 | 16 | COIN | LarnCoin, the ATM, the Tax Auditor |
| 5 | 32 | GLITCH | The Glitch Realm (scroll of segfault, null pointer) |
| 6 | 64 | FISHING | Fishing at fountains, the Fountain Kraken |
| 7 | 128 | PETS | Mysterious eggs and pets |
| 8 | 256 | PERKS | Level-up perks |
| 9 | 512 | WEATHER | Weather and day/night on the surface |
| 10 | 1024 | PHOTO | Photo mode. App only: the game never tests this bit |
| 11 | 2048 | BOUNTY | Town bounty board |

All twelve bits together are 4095 (hex fff).

### 2.2 How the mask reaches the game and where it lives

1. The app starts the game process with an environment variable named `ULARN_WILD`
   holding the mask as a decimal number (the app's settings default to all bits on).
2. At process start, after the options file is read and before any saved game is
   restored, the hub reads the variable. Missing or empty means 4095. Otherwise the
   decimal value is taken and masked with 4095.
3. For a **new character** only, the mask is copied into character attribute 101
   ("Wild flags") immediately after the character is created. All other Wild
   attributes (102..120) are zeroed, the per-species kill counts and all 64 extra save
   slots are zeroed, and then each enabled feature's "new character" handler runs.
4. A **restored** character keeps the mask stored in its save (the attribute array is
   saved whole). The environment variable is ignored for it. A character therefore
   keeps the feature set it was created with for its whole life.
5. "Feature X is on" always means: the bit is set in attribute 101 of the current
   character.

### 2.3 What "off" means

With a bit off, none of that feature's hub handlers run and each of its helper
functions returns its neutral value, so the base game is unchanged. The Wild code draws
no random numbers for a feature that is off. Per feature:

| Feature off | Result |
|---|---|
| RADIO | No cassette tapes are placed. A tape that exists anyway (wizard-made) is an ordinary take/ignore item and unlocks nothing. |
| CARDS | No card roll, no card event. (Kill counts are still kept; see 3.3.) |
| VENDING | No machines, no Roombas placed. A machine object underfoot gets the generic take/ignore prompt. Junk food is listed by the eat command but eating answers "You can't eat that!"; the duck is listed by the read command but answers "There's nothing on it to read." A Roomba that exists anyway eats nothing. |
| HATS | No hats on levels; the five hat rows of the DND store show blank and are "out of stock"; hats cannot be worn ("You can't wear that!"); no effect ever applies; the exported hat tile is 0. |
| COIN | No ATM; the bank menu and its accepted keys are the base ones; score unchanged; no auditor spawns; an auditor that exists anyway has a special attack that does nothing (the base code then treats it as a failed special attack and may land an ordinary blow). |
| GLITCH | No scroll of segfault is ever generated (and no random number is drawn for it); teleports never glitch; tiles and names are untouched; reading a segfault scroll that exists anyway prints the base "You read a scroll of" line with a blank name and does nothing. |
| FISHING | No rod on level 1/2; the store's rod row is blank/out of stock; the fountain prompt is the base one; fish and bottles are inert (unless PETS is on, which can still feed a fish to a pet); an old boot has no sale value; a kraken is not tethered. |
| PETS | No eggs; an egg is an inert item; no cell is ever "the pet". |
| PERKS | Level-ups grant no picks; all perk helpers return neutral values. |
| WEATHER | The weather attribute stays 0 (exported as hour 0, clear); no night effects, migrations or lightning. The Night Owl perk uses its fallback. |
| PHOTO | Nothing on the game side. |
| BOUNTY | No posters, no messages, nothing exported. |

With the whole mask 0 the per-turn hub returns at once. Three things still differ from
an unmodified Ularn even then, none of them a rule change: the save file always has the
Wild header and trailer (section 4.6), the hub still counts kills and emits the base
events of section 5.2, and the four Wild monster ids exist in the table (never created;
section 4.3).

---

## 3. The hub and its hooks

The base game calls one hub function at each insertion point; the hub calls the
per-feature handlers whose bit is on.

### 3.1 Fan-out orders

* **Fan-out order** (used by every "call them all" hook): radio, cards, vending, hats,
  coin, glitch, fishing, pets, perks, weather, bounty.
* **First-handler order** (used by wear, take-off, read, eat and dodge; the first
  enabled feature that returns "handled" wins and the rest are not asked): radio, cards,
  vending, hats, coin, glitch, **pets, fishing**, perks, weather, bounty. Pets come
  before fishing so that eating a fish next to a pet feeds the pet.
* The "stepped on a Wild object" hook uses the fan-out order but stops at the first
  feature that claims the object.

### 3.2 Hook table

| Hook | Where the base game calls it | What the hub does, and what is returned |
|---|---|---|
| Init | Process start: after the options file is read, before score-file setup and before any restore | Reads the mask from the environment, zeroes kill counts and extra slots, then runs every feature's init regardless of the mask (the mask is not known yet). Inits that do something: coin (tick counter, momentum and "moved" flag to 0), glitch (takes a copy of the original monster-name table), perks (clears the postponement time), bounty (clears the "town handled" flag). |
| New character | New game only: right after the character is made, before the town level is created | Section 2.2 step 3. Handlers that do something, in fan-out order: coin (price 100, holdings 0); glitch (names restored, scroll named); fishing (rod level = rnd(2)); pets (no pet, pet cell = (-1,-1)); perks (postponement cleared); weather (hour and clear sky stored, first weather change scheduled: this draws rnd(400)); bounty (flag cleared). |
| Turn | Main loop, once per time-consuming player action, right after the base regeneration step and before the base random-monster countdown. Also called once per waiting turn inside a fishing cast (12.3) and by the debug menu's "turns" option | Does nothing if the mask is 0; otherwise every enabled feature's turn handler in fan-out order. Note the order consequences: the Roomba eats, then hats are checked, the coin price moves, the glitch counts down, the kraken tether runs, the egg and pet act, then the perk prompt/magnet/Night Owl run, then the weather, then the bounty board. |
| Leaving level | Start of a level change, only if the level being left has been visited (always true except for the very first level creation of a new game), before that level is stored | Fan-out. Only pets act (13.7). |
| Make level | First visit of a level, after the maze, its objects and its monsters are made, after a wizard's or the town's whole-map reveal, after genocided monsters are removed and after the player is positioned | Fan-out. Placement order is therefore: cassette, vending machine then Roomba, hat, ATM, fishing rod, egg. Because it runs after the reveal, things placed here are only on the player's map if the feature marks them known itself. |
| New level | Every level change (first visit or revisit), once the level is in memory and the player positioned. The base code emits the "new level" base event right after it | Fan-out: glitch (re-syncs names), pets (follow/whine message, hand-over of a travelling pet), perks (Cartographer), weather (weather report on arriving in town), bounty (clears "town handled"). |
| Kill | In the base "damage a monster" routine when a monster's hit points run out: after "The <monster> died!" is printed, before experience is awarded, before the monster is removed and before its drops. Arguments: species, x, y | Adds 1 to that species' kill count (ids 1..69), emits base event KILL(species, x, y), then fan-out: cards, vending, coin, glitch, fishing, pets, perks (Vampiric), bounty act. Called for every death through that routine whoever caused it: the player's blows and spells, a pet's bite, storm lightning. |
| Annihilated | Scroll of annihilation, for each adjacent victim below demon-lord rank, while the monster is still on its cell (the base scroll removes victims without the damage routine) | Runs the Kill hook, then, if the victim was a Roomba, creates everything it had eaten around its cell (the normal death path would have dropped them). |
| Level up | Experience gain, once per character level gained, right after the level number goes up and before the level's hit point and spell gains | Base event LEVEL_UP(new level), then fan-out (perks: one more pick). |
| Take | Inside the base "put item in pack" routine, right after the item is stored in a slot, before the base item-specific effects and before "You pick up:" is printed | Base event TAKE(object, arg), then fan-out: radio (cassette), fishing (old boot), pets (egg). |
| Object | The base "what is under the player" routine, default case, for object ids 101 and above | Fan-out order, first claimer wins: radio claims the cassette; vending claims the machine, the three foods and the duck; coin claims the ATM; fishing claims the fish and the bottle. Anything unclaimed (hats, egg, fishing rod, old boot, and every Wild object whose feature is off) gets the base generic prompt: "You find <object name>", then " + N" if the cell's arg is positive or " N" if negative, then "Do you want to (t) take it, or (i) ignore it? ". Unlike fountains or altars these prompts are not suppressed by a monster standing next to the player. |
| Wear | Wear command, when the chosen item is not armour or a shield | First-handler order. Returns handled (hats) or not; not handled gives the base "You can't wear that!". |
| Take off | Take-off-armour command, before the base shield/armour logic | First-handler order. Handled means the base logic is skipped. Only hats act. |
| Read | Read command, when the chosen item is neither scroll nor book | First-handler: vending (duck), fishing (bottle). Not handled gives the base "There's nothing on it to read.". |
| Eat | Eat command, when the chosen item is not a fortune cookie | First-handler: vending (junk food), pets (fish to pet), fishing (fish). Not handled gives the base "You can't eat that!". |
| Dodge | Start of a monster's melee attack on the player, after the lemming exemption, before every other hit test and before any special attack | First-handler: hats (traffic cone), then perks (Nimble). If any returns true the attack ends with "The <monster> misses you!". |
| Player hit | First thing in the base "player loses hit points" routine, before the damage is subtracted | Base event PLAYER_HIT(damage), then fan-out (perks: Second Wind may raise hit points first). |
| Spell | Casting a known spell, after the spell point is spent | Base event SPELL(spell index). |
| Gold | Picking up a gold pile from the floor | Base event GOLD(amount). |
| Died | First thing in the base "game over" routine | Base event WON if the reason is "a winner", else DIED. This also fires for quitting, for save-and-exit, and for deaths the player then survives through life protection. |
| Save / restore | After the spheres are written / read | The trailer (4.6). Restore returns failure for a bad trailer; the base code then treats the save as corrupt. |
| Paint | Every time the front end repaints the map | Copies Wild state to the shared block (5.1), then fan-out paint handlers: hats (hat tile), coin (ticker), glitch (re-sync names), pets, weather, bounty. |
| Debug | Front end, command input, key `=` while in wizard mode | Section 18. |

Which item uses the pack commands offer is decided by a small table that is consulted
whether or not the features are on: the five hats are "wearable"; Fizz, pizza, energy
drink and fish are "edible"; the rubber duck and the bottle are "readable"; every other
Wild object can only be dropped. (The base `*` pick-lists for wear/read/eat do not list
Wild objects, but their letters are accepted.)

### 3.3 Hub state and helpers

* **Kill counts**: one counter per monster id (70 entries), counted by the Kill hook
  regardless of the mask, saved in the trailer, exported to the app.
* **Kills of a species** helper: the count including the kill being reported.
* **"Is this cell my pet?"** helper: true when PETS is on, a pet species is set, the
  pet's stored level is the current level, its stored cell is (x, y) and the monster
  there is of the pet's species. Used by the base monster mover (the pet is never moved
  or made to attack by the base AI), by player movement (swap places), by the lemming
  march and by lightning.
* **Free-cell helper**: section 1.

### 3.4 Feature helpers called directly from base code

| Helper (feature) | Called from | Behaviour when the feature is off |
|---|---|---|
| Worn-hat pack slot (hats) | Inventory line in the text list and in the side panel: appends " (on your head)" | No slot |
| Fly over (hats) | Pit and trap-door handling | Never |
| Extra spell regen (hats) | Base spell-point regeneration tick | Nothing |
| Clear confusion (hats) | Start of every player move; the "confused?" test before casting | Nothing |
| Hidden in store (hats, fishing) | DND store display and purchase | Hats / rod hidden |
| Price (hats, perks) | DND store display and purchase | Unchanged |
| Bank keys, bank line, bank key (coin) | Bank menu | Base menu |
| Coin value (coin) | Score calculation | 0 |
| Garnish (coin) | Monster special attack number 17 | Not called |
| New scroll is segfault? (glitch) | Random scroll generator | Never, no random draw |
| Read segfault, teleport check, disguise tile (glitch) | Scroll reading, the teleport routine, the map tile chooser | Nothing / unchanged tile |
| Can fish?, fish (fishing) | Fountain prompt | Base prompt |
| Item value (fishing) | Trading-post value routine (also used by the score) | No value |
| Swap with pet (pets) | Player movement into a monster | Never applies |
| Armour-class and weapon-class bonus (perks) | Base stat recalculation | 0 |
| Free spell? (perks) | Spell casting | Never |
| Lucky? (perks) | Monster death drops | Never |
| Sell value (perks) | Trading post | Unchanged |
| Perk names, pet text, bounty text | Known-list screen (section 17) | Nothing shown |
| "Is a Wild monster" test | Random monster pickers (section 4.3) | Always applies |

---

## 4. Data the Wild Edition adds

### 4.1 Character attributes

Appended to the end of the character attribute array (so the array, which is saved
whole, grows from 101 to 121 entries). All are zeroed for a new character except the
mask.

| Index | Name | Meaning |
|---|---|---|
| 101 | Wild flags | Feature mask (section 2) |
| 102 | Hat | Object id of the hat being worn, 0 = none |
| 103 | Perks | Bit n set = perk n owned (n = 0..10); bit 15 = "Night Owl bonus currently applied" |
| 104 | Perk picks | Level-ups whose perk has not been chosen yet |
| 105 | Egg | Turns left until a carried egg hatches; 0 = not counting |
| 106 | Pet species | Monster id of the pet, 0 = no pet |
| 107 | Pet hit points | Last known hit points of the pet |
| 108 | Coins | LarnCoin held |
| 109 | Coin price | Gold per LarnCoin |
| 110 | Glitch | Turns of glitch left, 0 = none |
| 111 | Duck | Times the rubber duck has been used (0..3) |
| 112 | Weather | Hour of day (0..23) in the low byte, weather state in the next byte |
| 113 | Fishing skill | Number of things landed by fishing |
| 114 | Last card | Species of the last card dropped (written, never read by the game) |
| 115..120 | Spare A..F | Unused, always 0 |

### 4.2 Extra save slots

64 signed 32-bit slots, all 0 for a new character, saved in the trailer.

| Slot | Owner | Meaning |
|---|---|---|
| 0, 1 | Pets | The pet's cell x, y; x = -1 while it is not placed |
| 2 | Pets | Level index + 1 of the level the pet is on; 0 = no pet placed; -1 = travelling with the player between levels |
| 3 | (vending) | Reserved for a "wobbling machine" flag but never used: each machine keeps its wobble in its own cell argument |
| 4, 5 | Fishing | Cell of the fountain the tracked kraken came out of |
| 6 | Glitch | Seed of the current name shuffle, 0 = no shuffle |
| 7 | Weather | Turn-counter value of the next weather change |
| 8 | Perks | Legacy: level index + 1 of the latest Second Wind use; converted to slot 14 and zeroed the first time Second Wind is tested |
| 9 | Radio | Bit t set = cassette track t found by this character |
| 10 | Bounty | Wanted species, 0 = no poster |
| 11 | Bounty | Reward in gold |
| 12 | Bounty | 1 = the wanted monster has been killed, reward not yet paid |
| 13 | Bounty | Turn-counter value when the poster went up |
| 14 | Perks | Second Wind: bit n set = already used on level index n |
| 15 | Pets | Monsters the pet has killed with its own bite |
| 24 | Fishing | Level index + 1 of the tracked kraken's fountain, 0 = no kraken tracked |
| 25 | Fishing | Dungeon level (1 or 2) that holds the free fishing rod |
| 16..23, 26..63 | — | Unused, 0 |

### 4.3 Monsters

Four monsters are inserted between the red dragon (id 56) and the first demon lord. The
demon lords, demon prince and God of Hellfire therefore have ids 61..69, and "the first
non-ordinary monster" boundary used by the base game is id 61. Final stat rows:

| Id | Name | Level | Armour class | Damage | Special attack | Intelligence | Gold | Hit points | Experience | Flags | Map character | Tile |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 57 | null pointer | 8 | 2 | 7 | none | 3 | 200 | 60 | 400 | flies, has infravision (no head) | `?` | 384 |
| 58 | fountain kraken | 7 | 4 | 6 | none | 3 | 300 | 70 | 300 | cannot be beheaded (no head) | `k` | 385 |
| 59 | tax auditor | 6 | 6 | 0 | 17 (garnish gold) | 12 | 0 | 40 | 50 | has a head | `$` | 386 |
| 60 | roomba | 2 | 8 | 1 | none | 3 | 0 | 25 | 30 | slow (acts on even turns only), cannot be beheaded (no head) | `@` | 387 |

Rules that keep them out of the base game's random choices (these apply whatever the
mask is):

* The random monster generator's level table is unchanged and ends at the red dragon,
  so it never picks ids 57..60.
* When the generator, or the "create a monster next to the player" routine, steps
  forward past a genocided species and reaches id 57, it jumps straight to id 61 (the
  type I demon lord), exactly as the base game would arrive there after the red dragon.
* Polymorph picks rnd(68) (ids 1..68) and re-rolls while the result is genocided or is
  one of ids 57..60.
* A mimic's disguise (re-chosen whenever the turn counter is a multiple of 10 while a
  mimic is being drawn) is rnd(61), i.e. an id 1..61, re-rolled while it is the
  invisible stalker or one of ids 57..60. Id 61, the type I demon lord, is a legal
  disguise, as it is in the base game.
* The genocide list shows ids 1..60, so the four Wild monsters can be genocided by
  number. Each feature copes: a genocided null pointer is not spawned, a genocided
  kraken does not surface and stops being tracked.
* Apart from the special rules in their feature sections, all four are ordinary
  monsters to the base AI: they move, attack, take damage, give experience and drop
  their gold (rnd(gold) + gold pieces when the gold column is positive) like any other.
  None of them has an entry in the base "drops an extra object" list.

### 4.4 Objects

Appended after the last base object ("some cocaine", id 100). "First Wild object" is id
101; everything from there up goes to the Object hook when stepped on.

| Id | Name (as printed) | Map character | Tile | Pack use | Notes |
|---|---|---|---|---|---|
| 101 | a humming vending machine | `&` | 368 | — | Fixture (a floor object you stand on); cell arg 0 = steady, 1 = wobbly |
| 102 | a can of Fizz | `!` | 369 | eat | |
| 103 | a slice of pizza | `!` | 370 | eat | |
| 104 | an energy drink | `!` | 371 | eat | |
| 105 | a rubber duck | `*` | 372 | read | |
| 106 | a traffic cone | `[` | 373 | wear | Hat |
| 107 | a propeller beanie | `[` | 374 | wear | Hat |
| 108 | a pointy wizard hat | `[` | 375 | wear | Hat |
| 109 | a party hat | `[` | 376 | wear | Hat |
| 110 | a tinfoil hat | `[` | 377 | wear | Hat |
| 111 | a mysterious egg | `*` | 378 | — | |
| 112 | a fishing rod | `*` | 379 | — | Only needs to be in the pack |
| 113 | a fish | `*` | 380 | eat | |
| 114 | an old boot | `*` | 381 | — | arg 0 = not yet shaken out, 1 = searched |
| 115 | a message in a bottle | `*` | 382 | read | |
| 116 | a cassette tape | `*` | 383 | — | arg = track index 0..2 |
| 117 | a LarnCoin ATM | `&` | 388 | — | Fixture |

Other properties:

* **Weight**: every Wild object counts 1 towards pack weight (the base default).
* **Enchantment display**: Wild objects never show a "+N" in inventory lines, whatever
  their arg. (The generic floor prompt does show it: a searched boot dropped on the
  floor reads "You find an old boot + 1", and a cassette for track 1 or 2 met with
  RADIO off reads "a cassette tape + 1" / "+ 2".)
* **Store price and stock** (DND store; price in gold; these six rows are appended to
  the store list as entries 93..98, which makes the store 99 entries long, and they
  appear on the fourth and last page as letters p, q, r, s, t, u): traffic cone 600
  (stock 1), propeller beanie 1200 (1), pointy wizard hat 2500 (1), party hat 400 (1),
  tinfoil hat 800 (1), fishing rod 250 (stock 2). The stock counts are saved with the
  rest of the store. Hat rows are hidden while HATS is off, the rod row while FISHING
  is off: a hidden row is drawn blank and buying its letter gives the base "out of
  stock" answer.
* **Trading-post value** (also what the item adds to the score's net worth): the base
  rule gives a store item twice its store table price when its arg is not negative, so:
  cone 120, beanie 240, wizard hat 500, party hat 80, tinfoil hat 160, fishing rod 50.
  The old boot is worth exactly 1 while FISHING is on. Every other Wild object has no
  value: the trading post says "So sorry but we are not authorized to accept that
  item." and it adds nothing to the score.
* **Minimap class**: the ATM counts as a building; all the others (including the
  vending machine) count as plain items.
* **Context action** (Part 09): standing on a vending machine with VENDING on gives
  context kind 15 ("Vending"); on an ATM with COIN on, kind 16 ("ATM"); with the
  feature off both are the ordinary "take" kind. All other Wild objects are "take".

### 4.5 Other table additions

* **Death reason** appended as the last entry of the death-reason list: "crushed by a
  vending machine".
* **Scroll 24, "segfault"**. The scroll-name table's slot 24 is blank in the base
  game. While GLITCH is on, the glitch feature sets the name to " segfault" (with the
  leading space every scroll name has) on every new character, every Wild turn, every
  level change and every repaint. It is never in the base scroll probability table and
  is not sold by the store. "Highest defined scroll number" stays 24.
* **Monster special attack 17** (tax auditor): section 10.7.

### 4.6 Save file header and trailer

* **Header** (first bytes of every save and checkpoint, written through the checksummed
  writer): the 8 ASCII bytes `ULRNWILD`, then a 32-bit little-endian integer, the save
  version, 2. On restore, a file whose first 8 bytes or version differ is closed and
  deleted, the messages "That saved game is from a different version of Ularn and can't
  be loaded." and "It has been removed. Please start a new game." are shown for about 4
  seconds, gold and bank balance are zeroed and the game ends with the base "killed by
  an old save file" reason.
* **Body**: the base dump, unchanged in order (player, levels, store, monster data,
  spheres). The player part is longer because the attribute array has 121 entries, and
  the store part has 99 stock counts.
* **Trailer**, after the spheres and before the base checksum, each value a 32-bit
  little-endian integer written as its own separate checksummed write (the base
  checksum depends on how bytes are grouped into writes, so the grouping is part of the
  format):
  1. the magic number whose four bytes spell `WILD` (hex 44 4c 49 57 as a little-endian
     value 0x444c4957);
  2. the number of kill counts, 70;
  3. 70 kill counts, by monster id;
  4. the number of extra slots, 64;
  5. the 64 extra slots.
* **Restore** of the trailer: wrong magic, or a count below 0 or above 4096, fails the
  restore (base "corrupt save" path). Entries beyond the array sizes are read and
  discarded; missing ones stay 0.
* Not saved (rebuilt or reset at process start): the coin tick counter, momentum and
  "price moved" flag; the perk postponement time; the bounty "town handled" flag; the
  pet's "follows / left behind" note between the two level hooks.

---

## 5. Shared-state exports and events

### 5.1 Fields written on every map repaint

The hub first writes defaults, then the enabled features overwrite theirs.

| Field | Default written by the hub | Overwritten by |
|---|---|---|
| Wild flags (32 bits) | Low 16 bits: the feature mask. High 16 bits: the low 16 bits of the perks attribute (so perk n is bit 16+n, and the "Night Owl applied" marker is bit 31) | — |
| Hat tile | 0 | Hats: 392 + (hat id − 106) while a hat is worn, i.e. cone 392, beanie 393, wizard hat 394, party hat 395, tinfoil hat 396 |
| Glitch | Turns of glitch left | — |
| Weather | The weather attribute (hour, state × 256) | Weather: recomputed from the turn counter so the hour is current even between turns; also written immediately whenever the hour or state changes |
| Coin price | The price attribute | Coin: +price when the player holds at least one coin or the price changed during the latest Wild turn; otherwise −price (meaning "hide the ticker, but here is the value") |
| Pet cell x, y | −1, −1 | Pets: the pet's cell when it is on this level, the player is not blind and the cell is known; else −1, −1 |
| Pet info (6 integers) and pet level name (8 characters) | not touched by the hub | Pets: section 13.10 |
| Bounty info (4 integers) | all 0 | Bounty: section 16.6 |
| Kill counts (96 integers) | Entries 0..69 = the per-species kill counts | — |

### 5.2 The event ring

64 entries of four 32-bit integers (type, a, b, c) and a sequence counter. To emit: the
entry at index (sequence modulo 64) is written, then the sequence is incremented. The
app reads forward from the last sequence it consumed.

Base events (emitted by the hub whatever the mask):

| Type | Name | a, b, c | When |
|---|---|---|---|
| 1 | KILL | species, x, y | Kill hook |
| 2 | LEVEL_UP | new character level, 0, 0 | Level-up hook |
| 3 | NEW_LEVEL | level index, 0, 0 | After every level change |
| 4 | TAKE | object id, arg, 0 | Take hook |
| 5 | PLAYER_HIT | damage, 0, 0 | Player-hit hook (the full damage asked for, before Second Wind) |
| 6 | GOLD | amount, 0, 0 | Gold picked up from the floor |
| 7 | SPELL | spell index, 0, 0 | Spell cast |
| 8 | DIED | 0, 0, 0 | Game-over routine entered for any reason but winning |
| 9 | WON | 0, 0, 0 | Game-over routine entered as a winner |

Feature events:

| Type | Feature | Variants (a, b, c) |
|---|---|---|
| 100 | Radio | (track index 0..2, 0, 0): a tape was found |
| 110 | Cards | (species, holo 0 or 1, 0): a card dropped |
| 120 | Vending | (0, 0, 0) bought; (1, 0, 0) kicked; (2, 0, 0) machine toppled; (3, object id, 0) a Roomba ate that object |
| 130 | Hats | (hat object id, 0, 0) put on; (0, 0, 0) taken off or lost |
| 140 | Coin | (0, price, 0) ordinary price step; (1, price, story index 0..3) news; (2, 1, 0) rug pull; (3, 0, 0) auditor arrives; (3, 1, gold taken) garnish; (3, 2, 0) auditors leave |
| 150 | Glitch | (1, turns, 0) starts; (0, 0, 0) ends |
| 160 | Fishing | (0, 0, 0) cast; (1, object id, arg) something landed; (2, fountain x, fountain y) kraken surfaced; (3, x, y) treasure map placed gold there |
| 170 | Pets | (0, species, 0) hatched; (1, victim species, 0) pet attacks; (2, species, cause) pet lost, cause 0 = vanished / no longer there, 1 = killed by monsters, 2 = genocided, 3 = killed by the player; (3, species, 0) followed to a new level; (4, species, new stage 1..3) grew |
| 180 | Perks | (perk index 0..10, 0, 0) perk taken; (10, cells marked, 0) Cartographer marked stairs (cells ≥ 1); (8, 1, 0) Second Wind saved the player; (−1, hit points gained, 0) all perks owned, picks converted |
| 190 | Weather | (new state 0..3, hour, 0) weather changed (also while underground); (current state, hour, 0) dawn (hour 6) or nightfall (hour 21) announced in town; (3, hour, 1) lightning |

---

## 6. Dungeon Radio — game side (RADIO)

Three bonus tracks, indexed 0..2. The index is what the app receives and must match its
song list.

| Track | Title printed | Tape lies on level index | (The app plays it…) |
|---|---|---|---|
| 0 | Elevator Music | 2 | in the bank |
| 1 | Surf's Up | 6 | while fishing |
| 2 | Boss Fight | 17 (volcano V2) | when a demon lord is in sight |

* **Placement**: when one of those levels is first created, the free-cell helper picks a
  cell and a cassette tape with arg = track index is put there. If the helper fails the
  tape simply does not exist. The tape is not marked known (not even for a wizard).
* **Stepping on a tape**: no prompt. The tape is "unlocked" at once and removed from the
  floor (cell emptied and remembered as empty, arg cleared).
* **Unlock**: an out-of-range index is treated as 0; print "A cassette tape! Side A:
  '<title>'."; set bit (track) in extra slot 9; emit event 100 (track, 0, 0).
* **A tape entering the pack** by any other route (the Take hook) unlocks the same way
  each time it is picked up; the tape stays in the pack as a souvenir with no use.

Nothing else in the game reads slot 9. The app keeps its own permanent list of unlocked
tracks (Part 13).

---

## 7. Trading cards — game side (CARDS)

On every kill reported to the hub (species 1..69):

1. If this character has killed this species before (the kill count, which already
   includes this kill, is above 1), draw rnd(100); if it is above 15, no card (so a
   repeat kill drops a card 15% of the time). The first kill of a species always drops
   one and draws no number here.
2. Draw rnd(100); the card is a **holo** if the result is ≤ 5 (5%).
3. Store the species in attribute 114 and emit event 110 (species, holo, 0).

The game keeps no album; it only announces drops. Wild monsters and demons drop cards
like any other species. Kills by a pet, by lightning and by annihilation count.

---

## 8. Vending machines, junk food, the rubber duck and the Roomba (VENDING)

### 8.1 Placement of machines

"Exit objects" are: stairs up, stairs down, elevator up, elevator down, the volcanic
shaft down, the volcanic tube up, and the dungeon entrance. A cell is "next to an exit"
if it or any of its eight neighbours holds one.

* **Town (level 0), always**: find the DND store by scanning columns west to east and,
  within a column, rows north to south; use the first one found. For radius r = 1, 2, 3
  in turn, scan the whole square of that radius around the store row by row (north to
  south, west to east within a row); the first cell that is inside the map, bare floor,
  without a monster, not the player's cell and not next to an exit receives a steady
  machine. If no cell qualifies there is no town machine. The machine is marked known.
* **Dungeon levels 1..10 and volcano levels V1..V3 (indices 16..18)**: on first
  creation, if rnd(100) ≤ 40, try up to 20 times: get a cell from the free-cell helper
  (stop trying if it fails); skip it if it is next to an exit; otherwise place a steady
  machine there and stop. The machine is marked known only for a wizard.
* Levels 11..15 and V4, V5 never get a machine.

### 8.2 Using a machine

Stepping onto (or re-examining) a machine prints "There is a humming vending machine
here." after a blank line, then the prompt "The machine hums. (b)uy for 50 gold, (k)ick
it, or (i)gnore? ". Accepted keys: b, k, i, Escape. Anything but b or k echoes
"ignore.".

**Buy** (echo "buy."):

1. With less than 50 gold: "You don't have 50 gold." and nothing else happens.
2. Otherwise subtract 50 gold, emit event 120 (0,0,0), print "The machine whirrs and
   clunks.", then dispense a random product (below).

**Random product**: r = rnd(100).

| r | Odds | Product |
|---|---|---|
| 1..35 | 35% | a can of Fizz |
| 36..65 | 30% | a slice of pizza |
| 66..90 | 25% | an energy drink |
| 91..97 | 7% | a rubber duck |
| 98..100 | 3% | first "Huh, that's not what you pressed.", then a magic potion of a random kind chosen by the base new-potion routine |

Dispensing prints "The machine drops <object name> into the tray." and puts the item in
the pack through the base take routine (which prints its usual pick-up or "can't carry"
lines). If the pack is full the item is created on a free cell adjacent to the player
and "It rolls onto the floor." is printed.

**Kick** (echo "kick."): r = rnd(100) is drawn first; event 120 (1,0,0) is emitted.

* If the machine is steady and r ≤ 95:
  * r 1..50 (50%): "THUNK." Nothing else.
  * r 51..80 (30%): "Something falls out!" then a free random product as above.
  * r 81..95 (15%): "The machine wobbles ominously." The machine becomes wobbly (its
    cell arg becomes 1; this is stored with the level, so it survives saving and each
    machine wobbles on its own).
* Otherwise (r 96..100, 5%, or the machine was already wobbly, 100%): it topples. The
  machine is removed for good (cell emptied and remembered empty), event 120 (2,0,0),
  "The vending machine topples onto you!", a beep, and the player loses 9 + rnd(16)
  hit points (10..25) directly, ignoring armour, with the death reason "crushed by a
  vending machine".

### 8.3 Junk food

Eaten from the pack with the eat command (the item is consumed) or straight from the
floor. Found on the floor: after a blank line "You find <object name>.", then "Do you
(e) eat it, (t) take it, or (i) ignore it? " (keys e, i, t, Escape). "e" echoes "eat.",
removes it from the floor and applies the effect; "t" echoes "take." and takes it (it
leaves the floor only if the pack had room); anything else echoes "ignore.".

| Food | Messages, in order | Effect |
|---|---|---|
| Can of Fizz | "You crack open the Fizz and chug it. Everything speeds up!" … "You burp." | Haste-self counter +30 |
| Slice of pizza | "You eat the slice of pizza."; on the bonus also "That was a *good* slice." | Heal max(1, maximum hit points / 4), not above the maximum. Then if rnd(100) ≤ 10, constitution +1 permanently (no upper limit applied) |
| Energy drink | "You down the energy drink. Your mind races." … "Your hands are shaking." | Spell points +8, not above the maximum; confusion counter +5 |

After any of them the base stat recalculation and status refresh run.

### 8.4 The rubber duck

* Found on the floor: after a blank line "You find a rubber duck.", then "Do you want to
  (r) rub it, (t) take it, or (i) ignore it? " (keys r, i, t, Escape). "r" echoes "rub."
  and rubs it where it lies (the duck stays on the floor); "t" echoes "take."; else
  "ignore.".
* From the pack it is used with the read command (so, like all reading, not while
  blind); the duck is never used up.
* **Rubbing**: the character has 3 uses for the whole game, counted in attribute 111
  (per character, not per duck).
  1. If the count is already 3: "The duck has heard enough." Nothing else.
  2. Otherwise add 1 to the count and print "You explain your problem to the duck. The
     duck says nothing." and "You suddenly understand everything."
  3. Go through the pack slots a..z in order. Every potion whose kind is not yet
     identified becomes identified and a line of two spaces, the slot letter, ") a magic
     potion of", then the potion's name is printed (for example "  c) a magic potion of
     healing"). Scrolls likewise with ") a magic scroll of". Identification is by kind,
     so a second potion of a kind just identified prints no line.
  4. If nothing was identified: "...about everything you're carrying, which you already
     knew." (the use is still spent).
  5. If the count is now 3: "The duck looks tired."

### 8.5 The Roomba

* **Spawn**: when a level with index 3 or more (dungeon 3..15 and all volcano levels) is
  first created, if rnd(100) ≤ 20 and the free-cell helper finds a cell, one Roomba is
  put there with 25 hit points, carrying nothing, awake. (The roll is made after the
  vending-machine roll for the level.) Roombas never appear any other way.
* **Behaviour**: the base monster AI moves it and makes it attack like any slow,
  unintelligent monster. In addition, on every Wild turn, unless time is stopped, every
  Roomba on the level that stands on an edible object and carries fewer than 6 things
  eats that object:
  1. the object and its arg are added to the Roomba's carried-items list (the same list
     thieving monsters use; capacity 6; a full Roomba just drives over things);
  2. the cell is emptied; if the player knew the cell, it is remembered as empty and
     redrawn;
  3. event 120 (3, object id, 0);
  4. if the player is not blind and the Roomba is within 6 columns and 6 rows of the
     player: "The roomba hums contentedly as it eats <object name>."
* **Edible** means: any object id from the plain gold pile (19) through the sapphire
  (74) except the chest — that is, all four kinds of gold pile, the Eye of Larn, all
  armour, weapons and rings, the belt, scrolls, potions, books, the orbs, amulets,
  scarab, cube, device, brass lamp, Hand of Fear, talisman, wand, staff, and gems — plus
  the fortune cookie, the five drugs, and these Wild objects: Fizz, pizza, energy drink,
  rubber duck, the five hats, fishing rod, fish, old boot, bottle and cassette. Never:
  dungeon features, buildings, traps, chests, the egg, vending machines or the ATM.
* **Death**: when a Roomba that carries at least one thing is killed, "The roomba beeps
  sadly and coughs up its contents." is printed (before the drop); the base death code
  then creates every carried thing around its cell. A Roomba destroyed by a scroll of
  annihilation gets the same message and its contents are created around its cell by
  the hub (3.2). A Roomba has no gold of its own.

---

## 9. Hats (HATS)

### 9.1 Finding hats

* **On levels**: when a dungeon or volcano level (index ≥ 1) is first created, with
  chance = min(60, 25 + 2 × level index) percent (27% on level 1, 55% on level 15, 57%
  on V1, 59% on V2, 60% on V3..V5): if rnd(100) ≤ chance and the free-cell helper finds
  a cell, one hat is placed there, each of the five kinds equally likely (id 106 +
  rund(5)), arg 0. Not marked known. Never in town.
* **DND store**: section 4.4.
* A hat on the floor uses the generic take/ignore prompt.

### 9.2 Wearing rules

* Attribute 102 holds the object id of the hat worn. A hat **counts as worn** only while
  HATS is on, attribute 102 holds a hat id and at least one object of that id is in the
  pack. The inventory marks the first pack slot holding that kind with " (on your
  head)".
* **Wear command on a hat**:
  1. First the "lost hat" check of 9.4 runs.
  2. If a hat is already recorded as worn: if the chosen slot is the one marked as on
     the head, "You are already wearing your <hat>."; otherwise "You already have a
     <worn hat> on your head. One hat at a time!". Nothing changes.
  3. Otherwise record the hat as worn; if that slot was the wielded weapon, nothing is
     wielded any more; apply the hat's attribute modifiers (9.3); recalculate stats;
     emit event 130 (hat id, 0, 0); print the hat's line:
     * traffic cone: "You put the traffic cone on your head. Monsters will have to
       swerve."
     * propeller beanie: "You put on the propeller beanie. The propeller spins
       hopefully."
     * wizard hat: "You put on the pointy wizard hat. You feel extremely magical."
     * party hat: "You put on the party hat. Everyone loves a party animal!"
     * tinfoil hat: "You put on the tinfoil hat. Your thoughts are your own again."
* **Take-off command**:
  1. The "lost hat" check runs. If no hat is worn, the base command carries on (shield
     first, else armour, else "You aren't wearing anything.").
  2. If body armour or a shield is also worn, ask "Take off your (h)at or your (a)rmor?
     " (keys h, a, Escape). "a" hands over to the base command; Escape does nothing;
     "h" continues.
  3. Clear the worn hat, undo its modifiers, emit event 130 (0,0,0), print "You take
     off your <hat>.".
* The short names used in these messages are: "traffic cone", "propeller beanie",
  "wizard hat", "party hat", "tinfoil hat".

### 9.3 Effects

| Hat | Effect | Exactly where it applies |
|---|---|---|
| Traffic cone | +2 armour class | Added to the character's permanent "extra defence" modifier when put on, subtracted when it comes off (the base recalculation starts armour class from that modifier). |
| | 10% of melee attacks swerve | In the Dodge hook: if the cone is worn, rnd(100) ≤ 10 makes the attack miss ("The <monster> misses you!"). The roll is only made while a cone is worn. A dodged attack also skips the monster's special attack. |
| Propeller beanie | Fly over pits and trap doors | Pit: after the base game has decided the player would fall and after its own "wand of wonder" float check, the beanie prints "Your propeller whirs and you fly right over the pit." and nothing happens. Trap door (including a hidden one that has just been revealed): checked first, "Your propeller whirs and you fly right over the trap door." |
| Pointy wizard hat | Double spell-point regeneration | Each time the base regeneration gives back a spell point, one more is added if the points are still below the maximum. |
| Party hat | +3 charisma | Added through the base ability adjuster when put on; −3 when it comes off (the adjuster never lets an ability go below 3). |
| | 10% off at the DND store | Shown and charged price = price − price / 10. Applied before the Haggler perk's own 10% (14.3), so both together give price′ = price − price/10, then price′ − price′/10. |
| Tinfoil hat | Immune to confusion | Whenever the check runs with the hat worn and the confusion counter non-zero, the counter is set to 0 and "Your tinfoil hat crackles. Nobody is messing with YOUR head." is printed. The check runs every Wild turn, at the start of every player move, and when a spell is about to be cast. |

### 9.4 Losing the hat

Every Wild turn (and before wear/take-off): if attribute 102 names a hat but no object
of that id is in the pack any more (dropped, sold, stolen, eaten by nothing — any way),
the hat is taken off as in 9.2 step 3 of take-off, but the message is "Your <hat> is no
longer on your head.". The effects that test "worn" (dodge, flying, regeneration,
discount, confusion immunity, the drawn hat) stop the moment the hat leaves the pack;
the two attribute modifiers (cone armour class, party-hat charisma) are undone at this
check, i.e. on the next Wild turn. Because both modifiers live in saved attributes, a
restored game needs nothing re-applied.

---

## 10. LarnCoin and the Tax Auditor (COIN)

### 10.1 Constants

| Constant | Value |
|---|---|
| Starting price | 100 gold |
| Price floor / ceiling | 1 / 99,999 |
| Most coins a character can hold | 9,999,999 |
| Wild turns between ordinary price steps | 10 |
| Odds of news on a Wild turn | 1 in 400 |
| Rug-pull threshold and odds | price above 1,500; 1 in 50 per price step |
| ATM fee | 5%, rounded up |
| Auditor odds | 1 in 150 per Wild turn in town |
| Cap on any gold sum produced by coin trading | 2,000,000,000 |

### 10.2 The price, per Wild turn

Three values exist only in memory (reset to 0 when the process starts, not saved): a
tick counter, a momentum (+1, −1 or 0) and a "price changed this turn" flag.

1. Clear the "changed" flag. If the stored price is below 1 (for example a missing
   value), set it to 100.
2. If rnd(400) = 1: **news** (10.3). The tick counter is not advanced on a news turn.
3. Otherwise add 1 to the tick counter; when it reaches 10, reset it to 0 and:
   * if the price is above 1,500 and rnd(50) = 1: **rug pull** (10.4);
   * otherwise an **ordinary step**:
     1. Direction: with momentum 0, up if rnd(2) = 1, else down. With momentum set,
        draw rnd(3): 1 or 2 keeps the momentum's direction, 3 reverses it.
     2. Momentum becomes the direction just chosen.
     3. Percent = rnd(7). Step = price × percent / 100, at least 1. New price = price ±
        step, clamped to 1..99,999.
     4. Emit event 140 (0, new price, 0).
4. Run the auditor logic (10.6).

Whenever the price actually changes value, the "changed" flag is set.

### 10.3 News

Pick story = rund(4):

| Index | Text | Direction |
|---|---|---|
| 0 | A famous gnome posts about LarnCoin. | up |
| 1 | The LRS announces a LarnCoin tax. | down |
| 2 | LarnCoin halving! | up |
| 3 | A whale dumps. | down |

Momentum becomes the story's direction. Percent = 29 + rnd(31) (30..60); step = price ×
percent / 100, at least 1; price moves by the step in the story's direction and is
clamped. Print "LarnCoin news: <text>" and, continuing the same line, " LarnCoin is up
to <price> gp." or " LarnCoin is down to <price> gp.". Emit event 140 (1, price, story
index).

### 10.4 Rug pull

Momentum becomes 0, the price becomes 1, "LarnCoin's founder has moved to a volcano. The
price collapses." is printed with a beep, and event 140 (2, 1, 0) is emitted.

### 10.5 Trading

**Value of n coins** = n × price, capped at 2,000,000,000. **Fee on g gold** = (g × 5 +
99) / 100, i.e. 5% rounded up.

**Number entry quirk** (base number prompt, Part 09): pressing Escape, `*`, or Enter with
no digits returns the prompt's default. The buy prompt's default is the maximum
affordable and the sell prompt's default is everything held, so Escape at "How many
LarnCoin" buys the maximum or sells all. Typing 0 cancels.

**Buying** (bank: `atm` = no; ATM: `atm` = yes):

1. Funds = carried gold (ATM), or bank balance + carried gold capped at 2,000,000,000
   (bank).
2. Maximum: bank: funds / price. ATM: funds × 100 / (price × 105). Then not more than
   9,999,999 minus the coins already held; then lowered one at a time while the cost
   (count × price, plus the fee on that at the ATM) exceeds the funds; never below 0.
3. Ask "How many LarnCoin (<price> gp each, up to <maximum>)? " and read a number with
   the maximum as default.
4. A number ≤ 0: nothing happens (silently). A number above the maximum: "  You can't
   afford that many." (two leading spaces), a pause of about 2 seconds, nothing bought.
5. Cost = number × price, plus the fee at the ATM. ATM: taken from carried gold. Bank:
   taken from the bank balance if it covers the whole cost; otherwise the balance goes
   to 0 and the rest is taken from carried gold.
6. Holdings increase. Print "  You buy <n> LarnCoin for <cost> gp. Number go up." and
   pause about 1 second.

**Selling**:

1. With no coins: "  You don't have any LarnCoin.", pause about 2 seconds.
2. Ask "How many LarnCoin (<price> gp each, you have <held>)? " with the holdings as
   default. ≤ 0: nothing. More than held: "  You don't have that many.", pause about 2
   seconds.
3. Proceeds = value of the coins; at the ATM minus the fee on that value; not below 0.
4. Holdings decrease. ATM: proceeds are added to carried gold; bank: to the bank
   balance (both capped at 2,000,000,000).
5. Print "  You sell <n> LarnCoin for <proceeds> gp in cash." (ATM) or "  You sell <n>
   LarnCoin for <proceeds> gp, paid into your account." (bank); pause about 1 second.

**In the bank** (base bank screen, Part 08): each time the menu is drawn, on row 20,
before the base "Your wish?" line, this line is printed: "LarnCoin @ <price> gp. You
hold <coins> (<value> gp): (b)uy, (c)ash out" with the letters b and c highlighted. The
menu accepts b and c in addition to its own d, w, s and Escape. "b" echoes "buy" and
runs a bank purchase; "c" echoes "cash out" and runs a bank sale; then the menu is
redrawn. Since the base bank refuses customers who owe taxes, bank trading is
unavailable then.

**The ATM** (object 117): placed when the town is created — find the Bank of Larn (the
main bank object, scanning columns west to east and rows north to south within a
column); for radius 1, 2, 3 scan the square around it row by row; the first cell that is
inside the map, bare floor, without a monster and not the player's cell becomes the ATM,
marked known. Stepping on it prints, after a blank line, "There is a LarnCoin ATM
here.", then "LarnCoin is <price> gp. You hold <n> coins." ("coin" without the s when n
is exactly 1), then the prompt "The ATM blinks (5% fee). (b)uy, (s)ell, or (i)gnore? "
(keys b, s, i, Escape). "b" echoes "buy." and runs an ATM purchase; "s" echoes "sell."
and runs an ATM sale; anything else echoes "ignore.". The ATM works while taxes are
owed.

**Score**: the value of all coins held at the current price (capped as above) is added
to the character's net worth in the score calculation, alongside gold, bank balance and
the trade-in value of the pack (and is then subject to the base 10% reduction for
non-winners like the rest).

### 10.6 The Tax Auditor's appearance and departure

Run every Wild turn, only while the player is in town:

1. If no taxes are outstanding: every tax auditor on the level is removed from the map
   (cells redrawn if known). If at least one was removed, print "The tax auditor stamps
   your file PAID and leaves." and emit event 140 (3, 2, 0).
2. Otherwise, unless time is stopped: if rnd(150) = 1 and there is no auditor on the
   level already, spawn one: find the dungeon entrance (same scan order as above); for
   radius 1..4 scan the square around it row by row; the first cell inside the map that
   is bare floor, without a monster and not the player's cell gets a tax auditor with 40
   hit points, carrying nothing, awake and already seen; the cell is drawn; "A tax
   auditor from the LRS arrives, clipboard in hand." is printed with a beep; event 140
   (3, 0, 0). (The rnd(150) draw happens before the "already one here" test.)

"Outstanding taxes" is the base game's figure from the winners' scoreboard (Part 03).

### 10.7 The garnish (special attack 17)

The auditor has damage 0, so the base attack routine computes its blow as rnd(1) + its
level = 7 and, like every monster with a special attack, tries the special attack when
7 + (difficulty + 1) + 8 exceeds the player's armour class or on a 1-in-(armour class)
chance. With COIN on, special attack 17:

1. With no carried gold: the message is "The tax auditor finds no gold to garnish and
   makes a note of it." (the monster's displayed name is substituted, so "monster" when
   blind).
2. Otherwise amount = carried gold / 10, at least 1. The whole amount is removed from
   carried gold. If taxes are outstanding, the base "pay taxes" routine is called with
   the amount (it reduces the tax bill by at most what is owed; any excess is simply
   lost). Event 140 (3, 1, amount). Message: "The tax auditor garnishes <amount> gp of
   your gold. The auditor issues you a receipt."
3. In both cases there is a beep and the attack ends there: no blow follows.

The base "cancellation" effect blocks special attacks; then (and whenever the special
attack is not attempted) the base code may still land the auditor's ordinary blow of 7
minus armour class.

Killing an auditor prints "The auditor's clipboard clatters to the ground. The LRS will
hear of this." (no further consequence). Another may arrive later while taxes remain.

### 10.8 The ticker export

See 5.1: positive price while coins are held or on a turn the price moved, negative
otherwise.

---

## 11. The Glitch Realm (GLITCH)

### 11.1 Triggers

* **Scroll of segfault**: every time the base game generates a random scroll kind (floor
  scrolls, chests, monster drops, anything using the random scroll routine), GLITCH on
  makes it draw rnd(40) first; on a 1 the scroll is a segfault scroll (kind 24),
  otherwise the base table is used as usual.
* **Reading it**: the base code identifies it and prints "You read a scroll of
  segfault."; then, if a glitch is already running, "Segmentation fault (core
  dumped)... again. Reality flickers harder.", otherwise "Segmentation fault (core
  dumped)... just kidding. Reality flickers."; then a glitch of 60 + rnd(60) turns
  (61..120) is started.
* **Teleporting**: at the end of every use of the base teleport routine (teleport traps,
  the teleportation scroll and anything else that goes through it), if no glitch is
  running and rnd(100) = 1: "You arrive slightly out of bounds. Reality flickers." and
  a glitch of 60 + rnd(60) turns starts. (No draw is made while a glitch is running.)

### 11.2 Starting

"Start a glitch of T turns": if the remaining count is below T it becomes T. If a glitch
was already running that is all (no new monster, no event). On a fresh start:

1. Shuffle seed (extra slot 6) = rnd(30000).
2. The monster names are shuffled (11.4).
3. Event 150 (1, turns, 0).
4. A null pointer is spawned (11.6).
5. The whole map is redrawn (item disguises show at once).

### 11.3 Each Wild turn

1. If the count is above 0, subtract 1.
   * If it reached 0: the glitch ends — seed set to 0, names restored, "Reality
     reasserts itself. You feel vaguely embarrassed.", event 150 (0,0,0), whole map
     redrawn.
   * Otherwise: if the remaining count is a multiple of 10, a new seed is made: (old
     seed modulo 29,999) + 1 + rnd(30000) (never 0). And if the turn counter is a
     multiple of 5, the whole map is redrawn (disguises change every 5 turns).
2. The name table is brought in line with the count and seed (shuffled with the current
   seed while glitched, original otherwise).
3. Null pointers hop (11.6).

### 11.4 Name scrambling

While glitched, the **names** of the ordinary monsters — ids 1 (lemming) through 56 (red
dragon) — are permuted among themselves. Nothing else about the monsters changes. The
four Wild monsters, the demons and the "unseen attacker" keep their names. Every message
that prints a monster name therefore lies ("The lemming hits you"), including the pet's
name, the bounty poster and the genocide list.

The permutation is a pure function of the seed, always applied to the original table:

1. A private generator (separate from the game's random stream) holds a 32-bit unsigned
   state, initially the seed. Each draw: state = state × 1103515245 + 12345 (modulo
   2^32); result = (state / 65536) modulo 32768.
2. For i from 56 down to 2: j = 1 + (draw modulo i); swap the names of monsters i and
   j.

The table is re-derived whenever the seed in force differs from the saved seed, on every
Wild turn, level change and repaint, so a restored game shows the same scrambled names
before anything is displayed.

### 11.5 Item disguises

While glitched, the map tile for a cell that is showing its remembered object (not a
monster, not the player, not an unknown cell) is replaced if the object is in the
**disguise set**: object ids 19..70 (the gold piles through the amulet of life
preservation, which includes the Eye of Larn, armour, weapons, rings, scrolls, potions,
books, chests and the magic items), 71..74 (gems), 94 (fortune cookie), 96..100 (drugs)
and 102..116 (all Wild objects except the vending machine and the ATM). That is 77
object ids. Walls, doors, stairs, fountains, buildings, traps and other features never
change.

Let L be the list of those 77 ids in ascending order, and k = (object id × 7 + turn
counter / 5) modulo 77. The object is drawn with the normal tile of L[k]; if L[k] is the
object itself, with the tile of L[(k + 1) modulo 77]. So each kind of object has one
disguise at a time, and all disguises advance every 5 turns of the turn counter. Only
the picture changes: names in prompts, the context action and behaviour are the real
object's.

### 11.6 The null pointer

* **Spawn** (on a fresh glitch start, unless the species is genocided): look for a cell
  near the player — up to 40 tries of x = player x + (rnd(11) − 6), y likewise (offsets
  −5..+5), keeping only cells at distance ≥ 2, inside the map, not the player's cell,
  standable and without a monster. If none is found, use the free-cell helper. If that
  fails too there is no monster. The null pointer gets 60 hit points, carries nothing
  and is awake. "Something that should not exist blinks into being." is printed.
* **Hopping**: on every Wild turn whose turn-counter value is even, unless time is
  stopped, up to 8 null pointers on the level (found scanning columns then rows) each
  try to jump: up to 40 tries of a cell with offsets −3..+3 in x and y (rnd(7) − 4), at
  distance ≥ 1, inside the map, not the player's cell, standable and without a monster.
  On success the monster moves there with its hit points and carried items, is made
  awake, and both cells are redrawn if they are known. This happens whether or not a
  glitch is still running: a null pointer that outlives the glitch keeps hopping.
* Between hops the base AI moves it and makes it attack as a level-8, damage-7 flying
  monster.
* **Death**: "The null pointer dereferences. Garbage spills everywhere."; then three
  calls of the base "create something for this level" routine around its cell (each
  makes at least one level-appropriate random object) and a gold pile of 250 + rnd(250)
  × (level index + 1) gold created around its cell. After that the base death code adds
  its normal gold drop of rnd(200) + 200.

### 11.7 Persistence and export

The remaining count is a saved attribute and the seed a saved extra slot, so a glitch
continues across save and restore with the same names. The count is exported to the app
(5.1), which draws the corruption effects (Part 13).

---

## 12. Fishing and the Fountain Kraken (FISHING)

### 12.1 The rod

* For a new character, rnd(2) decides which dungeon level (1 or 2) holds a free rod.
  When that level is first created the free-cell helper picks its cell (marked known
  only for a wizard). It uses the generic take/ignore prompt.
* The DND store sells two for 250 gold each.
* Fishing only requires a rod somewhere in the pack. It is never used up.

### 12.2 The prompt

When the player stands on a live fountain with no monster adjacent, the base game prints
"There is a fountain here." and asks. With FISHING on and a rod in the pack the question
is "Do you (d) drink, (w) wash yourself, (f) fish, or (i) ignore it? " (keys d, w, f, i,
Escape); otherwise it is the base question without the fish option. "f" echoes "fish.".

### 12.3 A cast

1. Event 160 (0,0,0); print "You cast your line into the fountain" (no full stop).
2. Turns to wait = rnd(5). Clear the "player was attacked" flag.
3. For each waiting turn: print "." on the same line; pause about 300 ms; the monsters
   move (twice if "haste monsters" is active); the base regeneration step runs (so the
   turn counter advances); the Wild per-turn hook runs. If a monster attacked the player
   during this turn (any melee attempt sets the flag, including one that missed, was
   dodged, or came from a harmless lemming): print "You drop your line!" and stop —
   nothing is caught, the fountain is unchanged.
4. If the player is somehow no longer on a live fountain, stop.
5. Choose the catch (12.4) and resolve it (12.5).
6. Unless a kraken surfaced: if rnd(100) ≤ 15, print "The fountain's bubbling slowly
   quietens." and the fountain becomes a dead fountain (also on the player's map). This
   roll is made after every completed cast, including "nothing bites".

### 12.4 The catch table

Skill s = min(10, fishing skill / 3), where fishing skill (attribute 113) is the number
of things the character has ever landed. Weights:

| Outcome | Weight | s = 0 | s = 5 | s = 10 |
|---|---|---|---|---|
| Nothing | 20 − s | 20 | 15 | 10 |
| Fish | 45 | 45 | 45 | 45 |
| Old boot | 15 − s/2 | 15 | 13 | 10 |
| Bottle | 8 + s/2 | 8 | 10 | 13 |
| Ring | 5 + s/2 | 5 | 7 | 10 |
| Potion | 4 + s/2 | 4 | 6 | 9 |
| Kraken | 3 | 3 | 3 | 3 |
| Total | | 100 | 99 | 100 |

(s/2 is integer division, so the total is 100 for even s and 99 for odd s.) Draw r =
rnd(total) and walk the outcomes in the order nothing, fish, boot, bottle, ring, potion,
kraken, taking the first whose cumulative weight reaches r.

### 12.5 Outcomes

"Landing" an object means: fishing skill +1; event 160 (1, object id, arg); the object
goes into the pack through the base take routine; if the pack is full it is created on a
free cell adjacent to the player and "It flops onto the floor next to you." is printed.

| Outcome | Message | Result |
|---|---|---|
| Nothing | "Nothing bites." | — |
| Fish | "A tug! You reel in a fish." | Land a fish |
| Old boot | "A heavy tug! You reel in... an old boot." | Land an unsearched old boot (arg 0) |
| Bottle | "You reel in a corked bottle with a note inside." | Land a message in a bottle |
| Ring | "Something glints on the hook: a ring!" | Land one of eight rings, equally likely: regeneration, protection, energy, dexterity, strength, cleverness, increase damage, extra regeneration; its enchantment (arg) is rnd(level index / 4 + 1) |
| Potion | "You fish out a potion. Someone threw this in." | Land a magic potion of a kind chosen by the base new-potion routine |
| Kraken | see 12.7 | If no kraken can surface: "Something big brushes the line, then lets go." |

### 12.6 Using the catch

* **Fish.** Eating it (eat command, item consumed): "You eat the fish. Raw, but
  surprisingly good."; heal 10 + character level hit points, not above the maximum. If
  PETS is on and the pet is adjacent, the eat command feeds the pet instead (13.6).
  Found on the floor: after a blank line "You find a fish.", then "Do you (e) eat it,
  (t) take it, or (i) ignore it? " (keys e, i, t, Escape) with echoes "eat." / "take." /
  "ignore."; eating from the floor always feeds the player.
* **Old boot.** The first time an unsearched boot enters the pack (from the hook or from
  the floor): the first unsearched boot in the pack is marked searched (arg 1); then if
  rnd(100) ≤ 20: "Something rattles in the boot. A small gem falls out!" and a gem is
  created on a free cell adjacent to the player — ruby, emerald or sapphire with equal
  chance, with arg (size) rnd(5) + 2, i.e. 3..7. A searched boot never rolls again. The
  boot sells for 1 gold at the trading post and has no other use.
* **Message in a bottle.** Read from the pack (read command; the bottle leaves the pack
  first) or from the floor. Found on the floor: after a blank line "You find a message
  in a bottle.", then "Do you (r) read it, (t) take it, or (i) ignore it? " (keys r, i,
  t, Escape). "r" while blind prints "You can't read anything when you're blind!" and
  leaves the bottle; "r" otherwise echoes "read.", removes it from the floor and reads
  it; "t" echoes "take."; else "ignore.".
  Reading:
  1. "You uncork the bottle and unroll the note."
  2. **Treasure map?** Only below ground (level index ≥ 1; in town no roll is made): if
     rnd(100) ≤ 25 and the free-cell helper finds a cell, the note is a map. Gold = 40 ×
     level index + rnd(60 × level index) (41..100 on level 1; 281..700 on level 7). An
     ordinary gold pile of that amount is placed on the cell, the cell is marked known
     and drawn, event 160 (3, x, y) is emitted, and, continuing the line, " It is a map
     of this very level!" then "An X marks a spot to the <direction>. You mark it on
     your own map." are printed. The pile is an ordinary gold pile from then on (nothing
     else is remembered about it).
     Direction: let dx, dy be the cell's offset from the player and ax, ay their
     absolute values. The east/west part is used if 2 × ax ≥ ay (west if dx < 0, east if
     dx > 0); the north/south part is used if 2 × ay ≥ ax (north if dy < 0, south if dy
     > 0). The words are "north", "south", "east", "west", "north-east", "north-west",
     "south-east", "south-west".
  3. Otherwise a fortune: a random line from the base fortune file (the same source as
     fortune cookies) is printed after " It says:" and a line break; if no fortune is
     available, " The ink has washed out." instead.
  4. Finally "The bottle slips from your fingers and is gone."
* **Rings and potions** are ordinary base items.

### 12.7 The Fountain Kraken

**Surfacing** (the catch came up "kraken"). It does not surface — and the "brushes the
line" message is shown instead — if krakens are genocided, if a kraken is already being
tracked (extra slot 24 is non-zero; only one at a time), or if neither placement below
is possible. Let F be the fountain cell (where the player stands).

1. Starting from a random direction (rnd(8)) and going round all eight, look for a
   knock-back: let A be the cell one step from F in that direction and B the cell two
   steps away. If A is standable with no monster and B is standable with no monster and
   no object, the player lands on B. Otherwise, if A is standable with no monster and
   no object, the player lands on A.
2. If a landing cell was found: print "Something enormous surfaces. The fountain was
   deeper than it looked." and "The surge throws you aside!"; the fountain is removed
   (F becomes bare floor, remembered as such); the player is moved to the landing cell;
   a kraken with 70 hit points, carrying nothing, awake and seen, is put on F.
3. If not: starting from a random direction, find a cell adjacent to F that is standable
   with no monster. If there is one: print only the first of the two messages, remove
   the fountain under the player, and put the kraken on that adjacent cell. If there is
   none, nothing surfaces.
4. On surfacing: record F in extra slots 4 and 5 and (level index + 1) in slot 24; emit
   event 160 (2, F.x, F.y); beep; set the "player was attacked" flag so that the
   monsters skip their move before the player's next action. The cast ends here (no
   drying-up roll).

**Tether** (every Wild turn while a kraken is tracked):

1. If krakens are genocided, or the recorded cell is outside the map, stop tracking
   (slots 4, 5, 24 to 0).
2. If the tracked kraken's level is not the current level, do nothing.
3. Find the fountain kraken on this level nearest to the recorded cell. If there is
   none, stop tracking.
4. If it is within distance 2 of the recorded cell, do nothing.
5. Otherwise, among the cells within 2 of the recorded cell (the 5 × 5 block, scanned
   column by column, north to south within a column) that are standable, hold no
   monster and are not the player's cell, take the one nearest to where the kraken is
   now (the first such in scan order on ties). If none exists, try again next turn.
   Move the kraken there with its hit points, carried items and awake/seen state; redraw
   both cells if known; and if the player is not blind and the new cell is within
   distance 6 of the player print "The fountain kraken is yanked back by its plumbing."

So the base AI moves and fights with the kraken freely, but it is pulled back at the end
of any turn in which it strayed 3 or more cells from its old fountain.

**Killing it**: if the dead monster is a fountain kraken, a kraken is tracked on this
level and the death cell is within distance 4 of the recorded cell: "Its tentacles
slither back down the drain." and tracking stops. It drops the base gold for a monster
with 300 in the gold column: rnd(300) + 300 pieces.

---

## 13. Pets (PETS)

### 13.1 The egg

* **Placement**: when each of dungeon levels 2, 3, 4 and 5 is first created, the
  free-cell helper picks a cell for one mysterious egg (marked known only for a wizard).
  It uses the generic take/ignore prompt.
* **Picking it up** (every time one enters the pack): if the hatch timer (attribute 105)
  is 0 or less it is set to 300; "The egg is warm. Something inside it moves." is
  printed.
* **Incubation**, every Wild turn: only while at least one egg is in the pack and the
  character has no pet. If the timer is 0 or less it is set to 300. Subtract 1. If it
  is now exactly 100: "The egg wobbles." If exactly 30: "You hear tapping from inside
  the egg." If 0 or less: try to hatch.
  A dropped egg pauses (the timer keeps its value). With a pet alive the timer does not
  run at all.

### 13.2 Hatching

1. Find a cell near the player (13.3). If there is none, set the timer to 1 so the
   attempt is repeated next turn.
2. Remove the first egg in the pack. Choose the species: if rnd(100) ≤ 6, a hell hound
   (6%); otherwise rund(4) picks jackal, snake, giant ant or floating eye (23.5% each).
3. Timer = 0; the pet's kill count = 0; the pet is placed on the cell with full hit
   points (13.4), awake and seen, and recorded as the pet (species, hit points, cell,
   current level).
4. Print "The egg hatches! A baby <species> blinks up at you." and "It seems to think
   you are its mother."; event 170 (0, species, 0).
5. Unless the player is blind, a two-frame animation plays on the pet's cell: overlay
   tile 398 for about 250 ms, overlay tile 399 for about 250 ms, then the cell is
   redrawn normally.

### 13.3 "A cell near the player"

A cell the pet may stand on is: standable, without a monster, and not the player's cell.
The search looks at the ring of 8 cells at distance 1, then the ring of 16 cells at
distance 2. Each ring is walked clockwise starting from its north-west corner (top edge
west to east, east edge north to south, bottom edge east to west, west edge south to
north), beginning at a random offset rund(8) along it, and the first suitable cell is
taken.

### 13.4 Hit points, growth and bite

* **Stage**: 0 "baby", 1 "young" at 5 kills, 2 "grown" at 15 kills, 3 "fearsome" at 30
  kills. Only kills made by the pet's own bite count (13.5).
* **Maximum hit points** = 10 + 3 × (the species' table hit points) + 8 × stage.
* **Bite damage** = rnd(species' table damage + 2 + 2 × stage).

| Species | Table HP / damage | Max HP at stage 0 / 1 / 2 / 3 | Bite roll at stage 0 / 1 / 2 / 3 |
|---|---|---|---|
| jackal | 1 / 1 | 13 / 21 / 29 / 37 | 1..3 / 1..5 / 1..7 / 1..9 |
| snake | 3 / 1 | 19 / 27 / 35 / 43 | same as jackal |
| giant ant | 5 / 1 | 25 / 33 / 41 / 49 | same as jackal |
| floating eye | 7 / 2 | 31 / 39 / 47 / 55 | 1..4 / 1..6 / 1..8 / 1..10 |
| hell hound | 20 / 2 | 70 / 78 / 86 / 94 | same as floating eye |

* **Growing**: when a bite kill raises the stage, the pet immediately gains 8 hit points
  per stage gained, "Your <species> has grown! It is a <stage name> <species> now:
  tougher, with a nastier bite." is printed and event 170 (4, species, new stage) is
  emitted. The kill count stops rising at 1,000,000.

### 13.5 The pet's turn (every Wild turn, after the egg logic)

The pet is an ordinary monster on the map; what makes it the pet is only the recorded
species, level and cell. The base AI never moves it and it never attacks the player.
Note that nothing here checks for stopped time: the pet keeps acting while time is
stopped.

1. **Genocide**: if the pet's species has been genocided: if it is on this level it is
   taken off the map; "Your <species> fades out of existence."; event 170 (2, species,
   2); the pet record is cleared.
2. **Arrival**: if the pet is recorded on this level but not yet placed (it came with
   the player): its hit points are made at least 1, and if a cell near the player
   (13.3) exists it is placed there with those hit points, awake and seen. The turn ends
   either way (it retries next turn if there was no room).
3. **Verify**: the monster on the recorded cell must be of the pet's species, with hit
   points above 0 and not above the last known value (monsters never regain hit points
   by themselves, so more would mean a different monster). If not: "Your <species> is
   gone.", event 170 (2, species, 0), record cleared, turn over. If so, the last known
   hit points are refreshed.
4. **Attack**: look at the eight neighbours of the pet starting from a random direction
   (rund(8)) and going round in direction order. The first one holding any monster
   (asleep or awake, any species) is bitten: print "Your <species> bites the
   <monster>!" ("glares at" instead of "bites" for a floating eye; `<monster>` is the
   monster's displayed name, "monster" when the player is blind); event 170 (1, victim
   species, 0); roll the bite damage and apply it through the base "damage a monster"
   routine as a non-weapon hit. For the duration of the bite the player's half-damage
   curse is ignored and the player's hold-monsters effect is left as it was. If the
   victim dies it is the player's kill in every respect (death message, experience,
   drops, Kill hook), and if the pet is still verified afterwards its own kill count
   goes up (13.4).
5. **Hunt** (only if nothing was bitten): candidates are monsters, other than the pet,
   inside the 7 × 7 block centred on the player that are within distance 2 of the
   player or of the pet. Take the one nearest to the pet, and only if that distance is
   at most 2 (scan rows north to south, columns west to east; first found wins ties).
   If there is one: compute step distances from the target to every cell by a
   breadth-first flood over the eight directions in which walls, closed doors, the
   level-1 exit cell and the player's cell block and monsters do not. Then go through
   the pet's neighbours in direction order 1..8 and move to the first one that the pet
   may stand on, that is within distance 2 of the player, and whose flood distance is
   lower than the pet's current one.
6. **Follow** (only if it neither bit nor hunted): if the pet is more than 1 away from
   the player, flood step distances from the player's cell in the same way and move to
   the neighbouring standable cell with the lowest distance, if that is lower than the
   pet's current one (first in direction order on ties; an unreachable cell counts as
   10,000 plus its straight distance to the player).
7. **Verify** again (the bite may have had side effects).
8. **Being attacked**: unless time is stopped or hold-monsters is active, go through the
   pet's eight neighbours in direction order. Each awake monster there whose table
   damage is above 0 hits the pet if rnd(100) ≤ 50, for rnd(its table damage + 2) hit
   points; "The <monster name> hits your <species>." is printed ("Something hits your
   <species>." when the player is blind). If the pet's hit points reach 0: "Your
   <species> dies. You feel a lump in your throat.", event 170 (2, species, 1), the pet
   is removed from the map, the record cleared, and the turn ends.
9. **Healing**: if the turn counter is a multiple of 15 and the pet is below its maximum
   hit points, it regains 1.

Moving the pet always marks it awake and redraws the old and new cells if they are
known.

### 13.6 Player interactions

* **Swapping places**: when the player moves into the cell of their pet, the pet is
  moved onto the player's old cell and the player's move then proceeds normally (no
  attack). If the player's own cell is one the pet cannot stand on (the player is
  inside a wall, on a closed door…), "Your <species> is in the way." is printed and the
  move is blocked.
* **Feeding**: using the eat command on a fish while the pet is verified and adjacent
  (distance ≤ 1) consumes the fish, heals the pet by 10 up to its maximum and prints
  "You toss the fish to your <species>. It gobbles it up happily.". (If the pet is not
  adjacent, the player eats the fish when FISHING is on.)
* **Harming it**: the player's spells and area effects treat it as any monster. If the
  player's action kills it (Kill hook reports the pet's species on the pet's cell):
  "You killed your own <species>! You feel terrible.", event 170 (2, species, 3), record
  cleared.
* Lightning never picks the pet, and a pet is never marched off as a migrating lemming.

### 13.7 Changing level

* **Leaving a level** (hook before the level is stored), if the pet is recorded on the
  level being left and is not genocided:
  * not yet placed there: it keeps travelling with the player ("follows");
  * otherwise it is verified (13.5 step 3; failure ends the matter). If it is adjacent
    to the player (distance ≤ 1) and carries nothing, its hit points are noted, it is
    taken off the map and it travels with the player ("follows"); otherwise it stays on
    that level ("left behind").
* **Arriving** (hook after the new level is in memory): if the level index differs from
  the one just left and there is a pet: "Your <species> follows you." with event 170
  (3, species, 0) in the "follows" case, or "You hear a sad whine behind you." in the
  "left behind" case. A travelling pet is recorded as on the new level, not yet placed;
  the next Wild turn places it next to the player (13.5 step 2).
* A pet left behind stays on its level with its hit points and is still the pet when
  the player returns to it (subject to the verify rule).

### 13.8 Ways a pet is lost

Killed by monsters; killed by the player; its species genocided; or it fails
verification ("is gone") because it was teleported, polymorphed, destroyed or otherwise
changed by something the pet logic did not do. Clearing the record also resets the kill
count, so the next pet starts as a baby; a carried egg then starts or resumes
incubating.

### 13.9 Save data

Species and last known hit points (attributes 106, 107), the hatch timer (105), the
cell, level and kill count (extra slots 0, 1, 2, 15). The monster itself is saved with
its level like any other.

### 13.10 Side-panel export

Written every Wild turn, every level arrival and every repaint. With no pet all values
are 0 and the level name is empty. Otherwise the six integers are: species; last known
hit points; maximum hit points; stage (0..3); the pet's own kills; the kill count at
which it next grows (5, 15 or 30), or 0 when fully grown. The level name is empty when
the pet is on the player's level or travelling; when it was left on another level it is
that level's display name without leading spaces ("H", "3", "V1").

---

## 14. Level-up perks (PERKS)

### 14.1 The list

The index is the bit number in attribute 103 and is part of the save and of the app's
perk list.

| Index | Name | Description shown at the prompt | Exact effect |
|---|---|---|---|
| 0 | Magnet Hands | gold next to you slides over to you | 14.4 |
| 1 | Thick Skin | +2 armor class | +2 added to armour class in every base stat recalculation |
| 2 | Vampiric | heal 1 HP per kill (2 when below 1/4 HP) | On every Kill hook (blow, spell, pet bite, lightning, annihilation): heal 2 if current hit points are below maximum / 4, else 1; never above the maximum |
| 3 | Bookworm | 1 cast in 4 costs no spell point | When a known spell is cast: draw rnd(100); if ≤ 25, print " (free!)" on the same line and do not spend the spell point |
| 4 | Lucky | monsters drop their loot twice | When a killed monster carried no stolen items, the base "species drops an extra object" step is performed twice. The gold drop is not doubled |
| 5 | Nimble | 10% of blows aimed at you miss | Dodge hook: rnd(100) ≤ 10 makes a melee attack miss. Tested after the traffic cone, so both together avoid 19% |
| 6 | Haggler | 10% off at the DND store, +10% at the trading post | Store price = price − price / 10 (after the party hat's discount). Trading-post offer = value + value / 10 for values above 0 (not applied to the score) |
| 7 | Glass Cannon | +4 weapon class, but -10% max HP | Once, when taken: maximum hit points −= maximum / 10 (not below 1) and current hit points lowered to the new maximum if above it. Then +4 weapon class in every stat recalculation |
| 8 | Second Wind | once per dungeon level, a deadly blow leaves you at 1 HP | 14.5 |
| 9 | Night Owl | +2 to all stats at night in town (with WEATHER off: "+1 intelligence, permanently") | 14.6 |
| 10 | Cartographer | the stairs of every level you enter go on your map | 14.7 |

### 14.2 Getting and choosing perks

* Each character level gained adds 1 to the pending picks (attribute 104).
* Every Wild turn (so after the player's command and before the monsters' next move):
  a negative pick count is reset to 0; if the stored postponement time is more than 10
  turns in the future (the clock went backwards) it is cleared; then, while picks remain
  and the turn counter has reached the postponement time, one pick is offered; a
  postponed offer stops the loop. Several pending picks are thus offered one after the
  other in the same turn.
* **An offer**:
  1. List the perks not yet owned (bits 0..10 clear).
  2. If there are none: all pending picks are converted at once — maximum and current
     hit points both rise by 5 × picks, picks become 0, "LEVEL UP! You have every perk
     already: +<n> max HP instead." is printed and event 180 (−1, n, 0) is emitted.
  3. Otherwise choose up to three distinct perks at random from the list (each chosen by
     rund over those remaining) and label them a, b, c in the order drawn (fewer labels
     when fewer than three perks remain).
  4. Print "LEVEL UP! Choose a perk:", then one line per option: "(a) <name>:
     <description>". Then the prompt "Which perk? (Esc: decide later) ". Accepted keys:
     the option letters and Escape.
  5. Escape: the offer is postponed until the turn counter is 10 higher; "You can choose
     your perk in a little while." is printed; the pick is kept. The postponement time
     is not saved: a restored game with a pick pending asks again after the first
     action.
  6. A letter: picks −1 and the perk is granted: its bit is set, event 180 (index, 0,
     0), "You gain the <name> perk!", its immediate effect (Glass Cannon, Night Owl,
     Cartographer; below), then a stat recalculation.

### 14.3 Price order

DND store: price′ = party-hat price of (table price × 10), then Haggler's reduction of
that. Both the displayed price and the gold check/charge use the same figure.

### 14.4 Magnet Hands

Every Wild turn, after the perk offers: if the player owns the perk and stands on bare
floor, the eight neighbours are examined row by row (north-west, north, north-east,
west, east, south-west, south, south-east). The first one holding any of the four kinds
of gold pile has the pile moved under the player (the old cell is emptied and, if it
was known, remembered as empty and redrawn) and "The gold slides over to you." is
printed. Only one pile moves per turn. The base game then picks the gold up at the start
of the next loop iteration, before the monsters move.

### 14.5 Second Wind

Tested in the Player-hit hook, i.e. before any loss of hit points that goes through the
base "lose hit points" routine (monster blows, traps, the vending machine, poisonous
water, debug damage…). It saves the player when all hold: the perk is owned; the damage
is above 0; the blow would leave 0 or fewer hit points; and the bit for the current
level index in extra slot 14 is not set. Then: the bit is set, current hit points are
set to damage + 1 (so the blow leaves exactly 1), "Second wind! You cling to life." is
printed and event 180 (8, 1, 0) is emitted.

Tracking is per level for the whole game: bits 0..20 for the town, dungeon 1..15 and V1
to V5. Coming back to a level does not re-arm it. (Legacy: if extra slot 8 holds a value
1..21 it is converted to the corresponding bit of slot 14 and zeroed the first time the
test runs.) Deaths that do not go through that routine are not prevented.

### 14.6 Night Owl

* **WEATHER off**: taking the perk gives +1 intelligence permanently and prints,
  continuing the line, " You feel wiser in the dark." Nothing else ever happens.
* **WEATHER on**: "night in town" is true when the player is on level 0 and the hour
  stored in the low byte of the weather attribute is 21 or more, or 5 or less. Every
  Wild turn (and at the moment the perk is taken) the perk compares that with bit 15 of
  the perks attribute ("bonus applied"):
  * it became true: each of the six abilities (strength, intelligence, wisdom,
    constitution, dexterity, charisma) gets +2, bit 15 is set, "The night air sharpens
    your senses." is printed;
  * it became false (dawn, or the player left town): each ability gets −2 (never below
    3), bit 15 is cleared, "The night owl in you settles down." is printed.
  Because the perk handler runs before the weather handler within a Wild turn, it sees
  the hour stored by the previous turn. Bit 15 is saved, so a game saved at night
  restores consistently.

### 14.7 Cartographer

On every arrival on a dungeon or volcano level (index ≥ 1; first visit or not), and at
the moment the perk is taken for the level the player is on (followed by a full map
redraw): every cell holding stairs up, stairs down, the volcanic tube up or the volcanic
shaft down that the player does not know yet is marked known. If at least one cell was
marked: "You sense the way through this level." and event 180 (10, cells marked, 0).
Elevators are not included. (A wizard already knows a new level whole, so nothing is
marked for one.)

---

## 15. Weather and day/night (WEATHER)

### 15.1 Clock and state

* **Hour** = (turn counter / 50 + 8) modulo 24. An hour is 50 turns, a day 1,200 turns,
  and a new game starts at 08:00.
* **Night** is hour ≥ 21 or hour < 6 (21:00 to 05:59).
* **Weather states**: 0 clear, 1 rain, 2 snow, 3 storm.
* The weather attribute holds hour + 256 × state and is what the app reads.
* A new character starts with clear weather and the first change scheduled at turn 200 +
  rnd(400).

### 15.2 Every Wild turn

1. If no next change is scheduled (slot 7 ≤ 0), schedule one at turn counter + 200 +
   rnd(400).
2. Note the previously stored hour, then store the current hour with the current state
   (and publish it to the app).
3. **Weather change**: if the turn counter has reached the scheduled time: pick a new
   state different from the current one by repeating r = rnd(100) → clear if r ≤ 40,
   rain if r ≤ 70, snow if r ≤ 85, storm otherwise, until it differs. Store and publish
   it, schedule the next change 201..600 turns ahead, emit event 190 (new state, hour,
   0). If the player is in town, announce it:
   * rain: "It starts to rain."
   * snow: "Snow! In Larn!"
   * storm: "Thunder rolls over the mountains."
   * clear: "The snow stops and the sky clears." if it had been snowing, otherwise "The
     clouds part. The sky clears."
   The weather keeps changing while the player is underground, silently.
4. Everything below happens only in town (level 0).
5. **Dawn and dusk**: if the hour differs from the previously stored one: when it is now
   6, "Dawn breaks over Larn."; when it is now 21, "Night falls. Things stir in the
   dark."; either emits event 190 (state, hour, 0). (An hour that turned while the
   player was underground is not announced.)
6. **At night**: if time is not stopped and the base random-monster countdown is above
   1, it is reduced by 1 (the base loop reduces it again right afterwards, so wandering
   monsters arrive twice as often; it is never taken to 0 here). Then if rnd(300) = 1, a
   lemming migration starts (15.3).
7. Lemmings march (15.4) — every turn in town, day or night.
8. **Storm**: if the state is storm and rnd(200) = 1, lightning strikes (15.5).

**Arriving in town** (New-level hook for level 0) prints the current weather if it is
not clear: "It is raining." / "It is snowing. In Larn!" / "A storm rages over the town."

### 15.3 Lemming migration

1. Nothing happens if lemmings are genocided or if 4 or more lemmings are already on
   the level.
2. Count n = 7 + rnd(5) (8..12). Top row y0 = rnd(17 − n) − 1 (0 .. 16 − n).
3. For each of the n consecutive rows from y0: try columns 66, 65, 64, 63 in that order
   and put an awake lemming (with the table's hit points, carrying nothing) on the first
   cell that is standable, holds no monster and is not the player's cell; redraw it if
   known. A row with no such cell gets none.
4. If at least one was placed: "You hear the pitter-patter of a thousand tiny feet."

### 15.4 The march

On turns whose turn-counter value is even, unless time is stopped or hold-monsters is
active: scan the map column by column from west to east (rows north to south within a
column). Every lemming that is not the pet and is outside the base monster-movement
window around the player (the window is player x − 5 .. player x + 5 and player y − 3 ..
player y + 3; with "aggravate monsters" active it is ± 10 and ± 5) is handled:

* in column 0 it leaves the map (removed, cell redrawn if known);
* otherwise it steps to the first free cell among west, north-west, south-west (free =
  standable, no monster, not the player's cell), keeping its hit points, becoming awake.
  If none is free it stays.

Lemmings inside the window are left to the base AI (they come for the player). Because
the scan goes west to east, no lemming moves twice in one march.

### 15.5 Lightning

1. If the player is not blind, consider every monster within 7 columns and 7 rows of the
   player that is not the pet and stands on a cell the player knows; choose one of them
   uniformly at random (reservoir style: the k-th candidate replaces the choice when
   rnd(k) = 1).
2. Emit event 190 (3, hour, 1) in every case.
3. With no target (or blind): "Lightning splits the sky. The thunder is deafening."
4. Otherwise: "A bolt of lightning strikes the <monster>!" and 20 damage is applied
   through the base "damage a monster" routine as a non-weapon hit (so the player's
   half-damage curse halves it, and a kill is the player's kill with experience, drops
   and the Kill hook).

### 15.6 Export

The weather field (hour in the low byte, state in the next) is published whenever it is
stored and again on every repaint, recomputed from the turn counter without changing the
stored attribute (so dawn and dusk are still detected by the next Wild turn, for example
right after a restore).

---

## 16. The bounty board (BOUNTY)

### 16.1 State

Extra slots 10..13: wanted species (0 = none), reward, claimed flag, turn posted. One
unsaved flag, "this town visit has been handled", cleared at process start, for a new
character and on every level change.

### 16.2 When the board acts

On a Wild turn in town with the flag clear: set the flag, run the town visit (16.3),
republish the side-panel data. So the board speaks on the first Wild turn after arriving
in town (including the first turn of a new game and the first turn after restoring a
game saved in town).

### 16.3 The town visit

1. **Poster up and claimed**: carried gold += reward; "The town crier counts out your
   bounty: <reward> gold for the <species>."; the poster is cleared; go to step 5.
2. **Poster up, not claimed, and either the species is genocided or more than 2,500
   turns have passed since it was posted**: "The faded poster for the <species> has been
   taken down."; cleared; go to step 5.
3. **Poster up otherwise**: "Still WANTED: one <species>. Reward: <reward> gold." and
   stop (no new poster).
4. **No poster**: go to step 5.
5. **Post a new one** (16.4).

### 16.4 Choosing the target and reward

1. Depth d: the deepest dungeon level (1..15) the character has visited, or 0 if none;
   if any volcano level has been visited, d is set to 12 instead. Add 1, and cap at 12.
2. Up to 20 times, ask the base random-monster generator for a monster of depth d
   (while standing in town this generator also returns a lemming about 9% of the time).
   Accept the first result that is an ordinary monster id (1..60), is not a lemming and
   is not genocided. If none is accepted, no poster goes up (retried at the next town
   visit).
3. Reward = 100 + 25 × d × d gold (125 for d = 1, 3,700 for d = 12). Record species,
   reward, not claimed, posted = current turn counter.
4. Print "The town crier nails up a poster: WANTED, one <species>. Reward: <reward>
   gold."

### 16.5 Claiming

On any Kill hook for the wanted species while the poster is unclaimed (a blow, a spell,
the pet's bite, lightning, annihilation): the claimed flag is set and "That <species>
was WANTED! Collect <reward> gold from the town crier." is printed. If the kill happened
in town, the "handled" flag is cleared so the payout and the next poster come on the
next Wild turn. Otherwise the reward is paid at the next town visit. A claimed poster
never expires.

Species names in all bounty messages are the current table names, so they are scrambled
during a glitch.

### 16.6 Side-panel export

Four integers, republished on every repaint and whenever the poster changes; all 0 when
there is no poster or the feature is off: wanted species; reward; 1 if claimed, else 0;
and, for an unclaimed poster, the turn-counter value after which it will be taken down
at the next town visit (posted + 2,500), or 0 if the species is genocided (meaning "comes
down at the next visit whatever the turn"); 0 when claimed.

---

## 17. Additions to the `\` known-list screen

The base screen lists known spells, then known scrolls, then known potions in three
columns of 26-character cells with the base paging (Part 03). The segfault scroll, once
read or identified, appears among the scrolls as "segfault". After the potions the Wild
Edition appends up to three blocks, each skipped entirely when it has nothing to show.
Each block first finishes any half-filled row of the block before it.

1. **Perks** (if at least one is owned and PERKS is on): the heading "Your perks are:"
   between blank lines, then the names of the owned perks in index order, one per
   26-character cell, three per row.
2. **Pet** (if there is one and PETS is on): the heading "Your pet:" between blank
   lines, then one line: "A <stage name> <species>: <hit points> of <maximum> hit
   points, <kills> kill(s)" followed by " (it grows at <next threshold>)." or, when
   fully grown, ".". ("kill" is singular for exactly 1.) Example: "A baby jackal: 13 of
   13 hit points, 4 kills (it grows at 5)."
3. **Bounty** (if a poster is up and BOUNTY is on): the heading "The town bounty board:"
   between blank lines, then "WANTED: one <species>. Reward: <reward> gold." or, when
   claimed, "<species>: done! Collect <reward> gold in town."

These blocks are not shown in the short form of the screen used while choosing a spell
to cast.

Elsewhere in text lists: the inventory line of the worn hat ends with " (on your
head)"; Wild objects never show plusses in the inventory.

---

## 18. The wizard-only Wild debug menu

Available at the command prompt only while the character is in wizard mode: the key `=`
opens it. It prints the prompt "Wild debug: (o)bject here, (m)onster next to you,
(g)old, (x)p level, (t)urns, (h)eal, (d)amage, (f)lags, (w)izard off? " and accepts o,
m, g, x, t, h, d, f, w and Escape. Number inputs use the base number prompt with a
default of 0. Only "o" and "m" consume a turn (they act as a "wait" command, so the
game immediately looks at what is underfoot and the monsters move); every other option
returns to the command prompt without time passing.

| Key | Action |
|---|---|
| o | Asks " Item: " then " Arg: ". An id outside 1..117 gives "No such object." Otherwise the object with that arg is put on the player's cell (replacing whatever was there) and marked known; "Placed <object name> under you."; then a turn passes, so the object's prompt appears |
| m | Asks " Monster: ". An id outside 1..69 gives "No such monster." Otherwise the base "create monster next to the player" routine is called for it (which steps past genocided species) and "Created <table name>." is printed; a turn passes |
| g | Carried gold +1000; "+1000 gold." |
| x | Raises the character one level through the base routine (so a perk pick is granted); "Level raised." |
| t | Asks " Turns: " and, that many times, adds 1 to the turn counter and runs the Wild per-turn hook only (no monster movement, no regeneration); then "<n> turns passed (wild_turn only)." |
| h | Hit points and spell points set to their maximums; "Healed." |
| d | Asks " Damage: ". For a value above 0: "You take <n> damage." and that many hit points are removed through the base "lose hit points" routine with the reason "hit by his own magic" (so Second Wind can trigger and it can kill) |
| f | Prints one line: "wild flags 0x<mask in hex> hat <id> perks 0x<bits in hex> egg <timer> pet <species> coins <n> price <n> glitch <n> duck <n> weather 0x<value in hex>" |
| w | Wizard mode is switched off for the rest of the game; "You are no longer a wizard." (the menu is no longer reachable) |
| Escape | Nothing |

---

## 19. Where the design brief and user pages differ from the code

The code (as described above) wins in each case.

| Topic | Brief / user page says | Code does |
|---|---|---|
| Clock | Hour = (turns / 50) modulo 24 | Adds 8 hours: a new game starts at 08:00 |
| Night spawns | The spawn interval is halved | One extra tick of the countdown per night turn in town |
| Lemming migration | Just place them; base movement marches them | A dedicated march moves those outside the base movement window west and off the map |
| Bank keys | (b)uy / (s)ell | (b)uy / (c)ash out in the bank ((s) is the base "sell a stone"); (b)uy / (s)ell only at the ATM |
| Roomba stats | Level 4 | Level 2 |
| Hat placement | 25% per level | 25% + 2% per level index, capped at 60% |
| Bookworm | Spells cost 1 less, minimum 1 | 25% of casts are free |
| Glitch names | Swap two random monsters' names | Full seeded shuffle of all 56 ordinary names, reshuffled every 10 turns |
| Glitch items | Tile of object (id × 7 + turn / 5) modulo object count | Same idea, but restricted to a 77-entry list of real items, never the object itself |
| Kraken | "Drops its own weight in gold" | Base gold drop for 300: 301..600 |
| Old boot | (considered wearable) | Junk worth 1 gold, 20% gem |
| Fountain drying | "After a catch" | Rolled after every completed cast, including an empty hook |
| Egg | Dealer McDope may sell one | Not sold anywhere |
| Kill counts export | 80 entries | 96 entries (70 used) |
| Vending reserve | A wobble flag in the extra slots | Wobble kept in the machine's own cell; the slot is unused |
| Perk count | "Max 10" in places | 11 perks |
| Second Wind | (first version: latest level only) | Once per level for the whole game (bitmask) |
| "Weight 1, value 0" for all Wild objects | | Weight 1 holds; hats and the rod have store-derived values and the boot is worth 1 |

---

## 20. Appendix: every message printed by the Wild code

Grouped by feature; substitutions in angle brackets. Echo words printed after a prompt
answer are listed with their prompt. Messages marked (same line) continue the current
line; all others start a new line; "(blank line first)" means an empty line precedes.

**Hub / debug menu**

* Wild debug: (o)bject here, (m)onster next to you, (g)old, (x)p level, (t)urns, (h)eal, (d)amage, (f)lags, (w)izard off? — sub-prompts (same line): " Item: ", " Arg: ", " Monster: ", " Turns: ", " Damage: "
* No such object.
* Placed <object name> under you.
* No such monster.
* Created <monster name>.
* +1000 gold.
* Level raised.
* <n> turns passed (wild_turn only).
* Healed.
* You take <n> damage.
* wild flags 0x<hex> hat <n> perks 0x<hex> egg <n> pet <n> coins <n> price <n> glitch <n> duck <n> weather 0x<hex>
* You are no longer a wizard.
* (In the base save code, for the Wild header) That saved game is from a different version of Ularn and can't be loaded. / It has been removed. Please start a new game.
* (Death reason) crushed by a vending machine

**Radio**

* A cassette tape! Side A: '<title>'. — titles: Elevator Music, Surf's Up, Boss Fight

**Cards**: none.

**Vending**

* (blank line first) There is a humming vending machine here.
* The machine hums. (b)uy for 50 gold, (k)ick it, or (i)gnore? — echoes: "buy.", "kick.", "ignore."
* You don't have 50 gold.
* The machine whirrs and clunks.
* The machine drops <object name> into the tray.
* It rolls onto the floor.
* Huh, that's not what you pressed.
* THUNK.
* Something falls out!
* The machine wobbles ominously.
* The vending machine topples onto you!
* (blank line first) You find <object name>.
* Do you (e) eat it, (t) take it, or (i) ignore it? — echoes: "eat.", "take.", "ignore."
* You crack open the Fizz and chug it. Everything speeds up!
* You burp.
* You eat the slice of pizza.
* That was a *good* slice.
* You down the energy drink. Your mind races.
* Your hands are shaking.
* (blank line first) You find a rubber duck.
* Do you want to (r) rub it, (t) take it, or (i) ignore it? — echoes: "rub.", "take.", "ignore."
* The duck has heard enough.
* You explain your problem to the duck. The duck says nothing.
* You suddenly understand everything.
* (two spaces)<slot letter>) a magic potion of <potion name>
* (two spaces)<slot letter>) a magic scroll of <scroll name>
* ...about everything you're carrying, which you already knew.
* The duck looks tired.
* The roomba hums contentedly as it eats <object name>.
* The roomba beeps sadly and coughs up its contents.

**Hats**

* You are already wearing your <hat>.
* You already have a <hat> on your head. One hat at a time!
* You put the traffic cone on your head. Monsters will have to swerve.
* You put on the propeller beanie. The propeller spins hopefully.
* You put on the pointy wizard hat. You feel extremely magical.
* You put on the party hat. Everyone loves a party animal!
* You put on the tinfoil hat. Your thoughts are your own again.
* Take off your (h)at or your (a)rmor?
* You take off your <hat>.
* Your <hat> is no longer on your head.
* Your propeller whirs and you fly right over the pit. / …over the trap door.
* Your tinfoil hat crackles. Nobody is messing with YOUR head.
* (Inventory suffix) " (on your head)"
* (Base attack code, for a dodge by cone or Nimble) The <monster> misses you!

**LarnCoin**

* LarnCoin news: <story> — stories: "A famous gnome posts about LarnCoin.", "The LRS announces a LarnCoin tax.", "LarnCoin halving!", "A whale dumps."
* (same line) " LarnCoin is up to <price> gp." / " LarnCoin is down to <price> gp."
* LarnCoin's founder has moved to a volcano. The price collapses.
* How many LarnCoin (<price> gp each, up to <max>)?
* (two leading spaces) You can't afford that many.
* (two leading spaces) You buy <n> LarnCoin for <cost> gp. Number go up.
* (two leading spaces) You don't have any LarnCoin.
* How many LarnCoin (<price> gp each, you have <n>)?
* (two leading spaces) You don't have that many.
* (two leading spaces) You sell <n> LarnCoin for <gp> gp in cash. / …for <gp> gp, paid into your account.
* (Bank line, no line break before it) LarnCoin @ <price> gp. You hold <n> (<value> gp): (b)uy, (c)ash out — echoes: "buy", "cash out"
* (blank line first) There is a LarnCoin ATM here.
* LarnCoin is <price> gp. You hold <n> coin(s).
* The ATM blinks (5% fee). (b)uy, (s)ell, or (i)gnore? — echoes: "buy.", "sell.", "ignore."
* A tax auditor from the LRS arrives, clipboard in hand.
* The tax auditor stamps your file PAID and leaves.
* The <monster> finds no gold to garnish and makes a note of it.
* The <monster> garnishes <n> gp of your gold. The auditor issues you a receipt.
* The auditor's clipboard clatters to the ground. The LRS will hear of this.

**Glitch**

* Segmentation fault (core dumped)... just kidding. Reality flickers.
* Segmentation fault (core dumped)... again. Reality flickers harder.
* You arrive slightly out of bounds. Reality flickers.
* Something that should not exist blinks into being.
* Reality reasserts itself. You feel vaguely embarrassed.
* The null pointer dereferences. Garbage spills everywhere.
* (Scroll name) segfault

**Fishing**

* (Base fountain prompt, extended) Do you (d) drink, (w) wash yourself, (f) fish, or (i) ignore it? — echo: "fish."
* You cast your line into the fountain — followed by one "." per waiting turn
* You drop your line!
* A tug! You reel in a fish.
* A heavy tug! You reel in... an old boot.
* You reel in a corked bottle with a note inside.
* Something glints on the hook: a ring!
* You fish out a potion. Someone threw this in.
* Something big brushes the line, then lets go.
* Nothing bites.
* It flops onto the floor next to you.
* The fountain's bubbling slowly quietens.
* Something enormous surfaces. The fountain was deeper than it looked.
* The surge throws you aside!
* The fountain kraken is yanked back by its plumbing.
* Its tentacles slither back down the drain.
* You eat the fish. Raw, but surprisingly good.
* You uncork the bottle and unroll the note.
* (same line) " It is a map of this very level!"
* An X marks a spot to the <direction>. You mark it on your own map.
* (same line) " It says:" then the fortune on the next line
* (same line) " The ink has washed out."
* The bottle slips from your fingers and is gone.
* (blank line first) You find a fish. — prompt: Do you (e) eat it, (t) take it, or (i) ignore it? — echoes: "eat.", "take.", "ignore."
* (blank line first) You find a message in a bottle. — prompt: Do you (r) read it, (t) take it, or (i) ignore it? — echoes: "read.", "take.", "ignore."
* You can't read anything when you're blind!
* Something rattles in the boot. A small gem falls out!

**Pets**

* The egg is warm. Something inside it moves.
* The egg wobbles.
* You hear tapping from inside the egg.
* The egg hatches! A baby <species> blinks up at you.
* It seems to think you are its mother.
* Your <species> bites the <monster>! / Your <species> glares at the <monster>!
* Your <species> has grown! It is a <young | grown | fearsome> <species> now: tougher, with a nastier bite.
* The <monster> hits your <species>. / Something hits your <species>.
* Your <species> dies. You feel a lump in your throat.
* Your <species> is gone.
* Your <species> fades out of existence.
* You killed your own <species>! You feel terrible.
* Your <species> follows you.
* You hear a sad whine behind you.
* Your <species> is in the way.
* You toss the fish to your <species>. It gobbles it up happily.
* (Known list) A <baby | young | grown | fearsome> <species>: <hp> of <max> hit points, <n> kill(s) (it grows at <n>). — or ending "." when fully grown

**Perks**

* LEVEL UP! Choose a perk:
* (<letter>) <perk name>: <description> — names and descriptions in 14.1
* Which perk? (Esc: decide later)
* You can choose your perk in a little while.
* You gain the <perk name> perk!
* (same line) " You feel wiser in the dark."
* LEVEL UP! You have every perk already: +<n> max HP instead.
* The gold slides over to you.
* The night air sharpens your senses.
* The night owl in you settles down.
* You sense the way through this level.
* Second wind! You cling to life.
* (same line, after the spell name) " (free!)"
* (Known list heading) Your perks are:

**Weather**

* It starts to rain.
* Snow! In Larn!
* Thunder rolls over the mountains.
* The snow stops and the sky clears.
* The clouds part. The sky clears.
* Dawn breaks over Larn.
* Night falls. Things stir in the dark.
* It is raining. / It is snowing. In Larn! / A storm rages over the town.
* You hear the pitter-patter of a thousand tiny feet.
* Lightning splits the sky. The thunder is deafening.
* A bolt of lightning strikes the <monster>!

**Bounty**

* The town crier nails up a poster: WANTED, one <species>. Reward: <n> gold.
* The town crier counts out your bounty: <n> gold for the <species>.
* The faded poster for the <species> has been taken down.
* Still WANTED: one <species>. Reward: <n> gold.
* That <species> was WANTED! Collect <n> gold from the town crier.
* (Known list) WANTED: one <species>. Reward: <n> gold. / <species>: done! Collect <n> gold in town.
* (Known list headings) Your pet: / The town bounty board:
