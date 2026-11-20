# Part 04 — Monsters: the table, spawning, movement AI, attacks, drops

This part specifies everything the monster module does: the static monster table, how a
monster is chosen for a dungeon level, how monsters are created and stored on the map,
how they move, how they attack the player, how the player's blows and spells are applied
to them, what happens when they die, and the helpers other modules call (teleport a
monster, polymorph, genocide, the save-file block). It leans on Part 02 for the player's
attributes and timers (armour class, weapon class, levels, experience, the timed
effects), Part 05 for level generation (which calls into the spawning routines described
here), Part 06 for objects, traps and the detailed item rules, Part 07 for the spells
themselves and spheres of annihilation, Part 09/11/14 for how tiles reach the screen, and
Part 12 for the behaviour of the four Wild Edition monsters and every Wild hook named
below.

## 1. Conventions used in this part

* **Random helpers.** `rnd(n)` is a uniformly distributed integer from 1 to n inclusive.
  `rund(n)` is a uniformly distributed integer from 0 to n−1 inclusive. Both are a plain
  remainder of the C library generator, which is seeded once from the wall clock when a
  game starts (see Part 01). All divisions in this part are integer divisions that
  truncate.
* **Map.** A level is 67 columns (x = 0..66) by 17 rows (y = 0..16); y grows down the
  screen. Level 0 is the home (town) level, 1..15 are the dungeon (15 is the dungeon
  bottom), 16..20 are the volcano V1..V5 (20 is the volcano bottom). The dungeon exit is
  the square (33, 16) of level 1.
* **Direction table.** Eight directions are numbered 1..8 and always tried in this order
  when the text says "direction order": 1 = (0, +1), 2 = (+1, 0), 3 = (0, −1),
  4 = (−1, 0), 5 = (+1, −1), 6 = (−1, −1), 7 = (+1, +1), 8 = (−1, +1), each given as
  (x offset, y offset). Index 0 is "no movement".
* **Difficulty** means the game's hardness setting (0 for a first game; Part 01/02).
* **Player armour class (AC)** is ascending: a higher number is better protection.
  **Monster armour class** is used the other way round: a *higher* monster AC makes the
  monster *easier* to hit, and very negative values make it nearly unhittable.
* **Messages** are quoted exactly. A leading `\n` means the message starts a new line in
  the message log; `%s` is replaced by the monster's name (see "the name used in
  messages" in section 4.3), `%d` by a number.

## 2. The monster table

### 2.1 Fields

Every monster kind has one row with these fields:

| Field | Meaning |
|---|---|
| name | Text used in messages and on the scoreboard. |
| level | Added to its melee damage; used in the charm check. |
| armour class | Added to the player's to-hit number (higher = easier to hit). |
| damage | Base melee damage. |
| attack | Special attack number 0..17 (0 = none), see section 8.4. |
| intelligence | Decides smart versus dumb movement (section 7.4). |
| gold | Base gold dropped on death (0 = none). |
| hit points | Hit points every new instance starts with. |
| experience | Experience awarded for the kill. |
| flags | Bit mask, see below. |

There is a single flags column; "defence" properties and the genocide marker are bits in
it:

| Bit value | Letter used below | Meaning |
|---|---|---|
| 1 | (G) | Genocided. Never set in the static table; set at run time by genocide and by loading a save. |
| 2 | H | Has a head (can be beheaded by the Vorpal Blade). |
| 4 | N | Cannot be beheaded even though it has a head. |
| 8 | S | Slow: only moves on turns when the game-time counter is even. |
| 16 | F | Flies: unaffected by pits and trapdoors. |
| 32 | P | Spirit: damage halved by spirit protection. |
| 64 | U | Undead: damage halved and special attack cancelled by undead protection. |
| 128 | I | Infravision: can see an invisible player (never "misses wildly"). |

Two parallel arrays give each monster a text-mode map character and a tile number. The
Android build draws tiles only; the map characters are used only by the diagnostic dump,
but they are listed so the table is complete. Tile numbers refer to the tile sheet in
Part 14 (Wild tiles in Part 15).

### 2.2 The rows, in id order

The id is the row index and is what is stored on the map, in save files and used in
arithmetic (several rules add to or compare ids), so the order is part of the behaviour.

| Id | Name | Char | Tile | Lvl | AC | Dmg | Att | Int | Gold | HP | Exp | Flags |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | unseen attacker | (space) | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | none |
| 1 | lemming | l | 0 | 1 | 0 | 0 | 0 | 3 | 0 | 0 | 1 | H |
| 2 | gnome | G | 1 | 1 | 10 | 1 | 0 | 8 | 30 | 2 | 2 | H I |
| 3 | hobgoblin | H | 2 | 1 | 13 | 2 | 0 | 5 | 25 | 3 | 2 | H S |
| 4 | jackal | J | 3 | 1 | 7 | 1 | 0 | 4 | 0 | 1 | 1 | H |
| 5 | kobold | K | 4 | 1 | 15 | 1 | 0 | 7 | 10 | 1 | 1 | H I |
| 6 | orc | O | 5 | 2 | 15 | 3 | 0 | 9 | 40 | 5 | 2 | H |
| 7 | snake | S | 6 | 2 | 10 | 1 | 0 | 3 | 0 | 3 | 1 | H |
| 8 | giant centipede | c | 7 | 2 | 13 | 1 | 4 | 3 | 0 | 2 | 2 | H |
| 9 | jaculi | j | 8 | 2 | 9 | 1 | 0 | 3 | 0 | 2 | 1 | H |
| 10 | troglodyte | t | 9 | 2 | 10 | 2 | 0 | 5 | 80 | 5 | 3 | H S |
| 11 | giant ant | A | 10 | 2 | 8 | 1 | 4 | 4 | 0 | 5 | 4 | H |
| 12 | floating eye | E | 11 | 3 | 8 | 2 | 0 | 3 | 0 | 7 | 2 | F |
| 13 | leprechaun | L | 12 | 3 | 3 | 0 | 8 | 3 | 1500 | 15 | 40 | H |
| 14 | nymph | N | 13 | 3 | 3 | 0 | 14 | 9 | 0 | 20 | 40 | H |
| 15 | quasit | Q | 14 | 3 | 5 | 3 | 0 | 3 | 0 | 14 | 10 | H |
| 16 | rust monster | R | 15 | 3 | 5 | 0 | 1 | 3 | 0 | 18 | 20 | H |
| 17 | zombie | Z | 16 | 3 | 12 | 3 | 0 | 3 | 0 | 9 | 7 | H U |
| 18 | assassin bug | a | 17 | 4 | 4 | 3 | 0 | 3 | 0 | 23 | 13 | H |
| 19 | bugbear | b | 18 | 4 | 5 | 4 | 15 | 5 | 40 | 24 | 33 | H I |
| 20 | hell hound | h | 19 | 4 | 5 | 2 | 2 | 6 | 0 | 20 | 33 | H |
| 21 | ice lizard | i | 20 | 4 | 11 | 3 | 10 | 6 | 50 | 19 | 23 | H S |
| 22 | centaur | C | 21 | 4 | 6 | 4 | 0 | 10 | 40 | 25 | 43 | H |
| 23 | troll | T | 22 | 5 | 9 | 5 | 0 | 9 | 80 | 55 | 250 | H |
| 24 | yeti | Y | 23 | 5 | 8 | 4 | 0 | 5 | 50 | 45 | 90 | H |
| 25 | white dragon | d | 24 | 5 | 4 | 5 | 5 | 16 | 500 | 65 | 1000 | H |
| 26 | elf | e | 25 | 5 | 3 | 3 | 0 | 15 | 50 | 25 | 33 | H I |
| 27 | gelatinous cube | g | 26 | 5 | 9 | 3 | 0 | 3 | 0 | 24 | 43 | none |
| 28 | metamorph | m | 27 | 6 | 9 | 3 | 0 | 3 | 0 | 32 | 40 | S |
| 29 | vortex | v | 28 | 6 | 5 | 4 | 0 | 3 | 0 | 33 | 53 | none |
| 30 | ziller | z | 29 | 6 | 15 | 3 | 0 | 3 | 0 | 34 | 33 | H |
| 31 | violet fungus | F | 30 | 6 | 12 | 3 | 0 | 3 | 0 | 39 | 90 | none |
| 32 | wraith | W | 31 | 6 | 3 | 1 | 6 | 3 | 0 | 36 | 300 | H F U |
| 33 | forvalaka | f | 32 | 6 | 3 | 5 | 0 | 7 | 0 | 55 | 270 | H I |
| 34 | lama nobe | l | 33 | 7 | 14 | 7 | 0 | 6 | 0 | 36 | 70 | H |
| 35 | osequip | o | 34 | 7 | 4 | 7 | 16 | 4 | 0 | 36 | 90 | H |
| 36 | rothe | r | 35 | 7 | 15 | 5 | 0 | 3 | 100 | 53 | 230 | H I |
| 37 | xorn | X | 36 | 7 | 6 | 7 | 0 | 13 | 0 | 63 | 290 | I |
| 38 | vampire | V | 37 | 7 | 5 | 4 | 6 | 17 | 0 | 55 | 950 | H F U |
| 39 | invisible stalker | (space) | 38 | 7 | 5 | 6 | 0 | 5 | 0 | 55 | 330 | H S |
| 40 | poltergeist | p | 39 | 8 | 1 | 8 | 0 | 3 | 0 | 55 | 430 | P |
| 41 | disenchantress | q | 40 | 8 | 3 | 1 | 9 | 3 | 0 | 57 | 500 | H |
| 42 | shambling mound | s | 41 | 8 | 13 | 5 | 0 | 6 | 0 | 47 | 390 | none |
| 43 | yellow mold | y | 42 | 8 | 12 | 4 | 0 | 3 | 0 | 37 | 240 | none |
| 44 | umber hulk | U | 43 | 8 | 6 | 7 | 11 | 14 | 0 | 67 | 600 | H I |
| 45 | gnome king | k | 44 | 9 | −1 | 10 | 0 | 18 | 2000 | 120 | 3000 | H I |
| 46 | mimic | M | 45 | 9 | 9 | 7 | 0 | 8 | 0 | 57 | 100 | none |
| 47 | water lord | w | 46 | 9 | −10 | 15 | 7 | 20 | 0 | 155 | 15000 | H N |
| 48 | bronze dragon | D | 47 | 9 | 5 | 9 | 3 | 16 | 300 | 90 | 4000 | H F |
| 49 | green dragon | D | 48 | 9 | 4 | 4 | 10 | 15 | 200 | 80 | 2500 | H F |
| 50 | purple worm | P | 49 | 9 | −1 | 13 | 0 | 3 | 100 | 130 | 15000 | H |
| 51 | xvart | x | 50 | 9 | −2 | 14 | 0 | 13 | 0 | 100 | 1000 | H S |
| 52 | spirit naga | n | 51 | 10 | −20 | 15 | 12 | 23 | 0 | 100 | 20000 | H N F P I |
| 53 | silver dragon | D | 52 | 10 | −4 | 10 | 3 | 20 | 700 | 110 | 10000 | H F |
| 54 | platinum dragon | D | 53 | 10 | −7 | 15 | 13 | 22 | 1000 | 150 | 25000 | H F |
| 55 | green urchin | u | 54 | 10 | −5 | 12 | 0 | 3 | 0 | 95 | 5000 | none |
| 56 | red dragon | D | 55 | 10 | −4 | 13 | 3 | 19 | 800 | 120 | 14000 | H F |
| 57 | null pointer (Wild) | ? | 384 | 8 | 2 | 7 | 0 | 3 | 200 | 60 | 400 | F I |
| 58 | fountain kraken (Wild) | k | 385 | 7 | 4 | 6 | 0 | 3 | 300 | 70 | 300 | N |
| 59 | tax auditor (Wild) | $ | 386 | 6 | 6 | 0 | 17 | 12 | 0 | 40 | 50 | H |
| 60 | roomba (Wild) | @ | 387 | 2 | 8 | 1 | 0 | 3 | 0 | 25 | 30 | S N |
| 61 | type I demon lord | 1 | 56 | 12 | −40 | 20 | 3 | 20 | 0 | 150 | 50000 | H N F I |
| 62 | type II demon lord | 2 | 56 | 13 | −45 | 25 | 5 | 22 | 0 | 200 | 75000 | H N F I |
| 63 | type III demon lord | 3 | 57 | 14 | −50 | 30 | 9 | 24 | 0 | 250 | 100000 | H N F I |
| 64 | type IV demon lord | 4 | 58 | 15 | −55 | 35 | 11 | 26 | 0 | 300 | 125000 | H N F I |
| 65 | type V demon lord | 5 | 59 | 16 | −60 | 40 | 13 | 28 | 0 | 350 | 150000 | H N F I |
| 66 | type VI demon lord | 6 | 60 | 17 | −65 | 45 | 13 | 30 | 0 | 400 | 175000 | H N F I |
| 67 | type VII demon lord | 7 | 61 | 18 | −70 | 50 | 6 | 32 | 0 | 450 | 200000 | H N F I |
| 68 | demon prince | 9 | 62 | 19 | −90 | 80 | 6 | 40 | 0 | 1000 | 500000 | H N F I |
| 69 | God of Hellfire (Lucifer) | 0 | 63 | 20 | −120 | 100 | 6 | 100 | 0 | 32767 | 1000000 | H N F I |

Notes on the table:

* Id 0 is a placeholder ("no monster" on the map); its name is what the scoreboard shows
  when the player is killed while blind.
* Lucifer's row is named "God of Hellfire"; that is the text the player sees.
* Type I and type II demon lords share tile 56. Lemming and id 0 both map to tile 0, and
  lemming and lama nobe share the character `l`.
* The invisible stalker's character is a space until a potion of see invisible is drunk,
  which changes it to `I` for the rest of that process (it is not saved).
* **Named id boundaries used by the rules.** "Ordinary monsters" are ids 1..56. "Wild
  monsters" are ids 57..60. "Demons" are ids 61..69 (the code tests "id ≥ 61"). The
  constant called the ordinary-monster limit equals 61 (the id of the type I demon lord):
  genocide, the mimic's disguise roll and the save block all use it. Lucifer is that
  limit plus 8.
* The Wild rows are inserted between the last ordinary monster and the demons, so every
  rule that picks "a random monster below the limit" must explicitly skip ids 57..60;
  each such rule says so below. The Wild Edition's own features create and tune these
  four monsters; see Part 12.

### 2.3 Difficulty scaling of the table

Once at start-up (after a new character is made or a save is loaded) the whole table,
ids 0..69, is rescaled if difficulty k is not zero:

* hit points = ((6 + k) × hit points + 1) / 6, capped at 32767;
* damage = ((6 + k) × damage + 1) / 5, capped at 127 (note the divisor is 5, not 6);
* gold = (10 × gold) / (10 + k);
* armour class = armour class − k, floored at −127;
* experience = (7 × experience) / (7 + k) + 1, and at least 1.

All later references to table values mean the scaled values. Monsters already standing
on saved levels keep whatever hit points they had.

## 3. Choosing a monster for a level

### 3.1 The per-level range table

A twelve-entry table gives the highest ordinary id for each "monster level" 1..12:

| Monster level | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Highest id | 5 | 11 | 17 | 22 | 27 | 33 | 39 | 42 | 46 | 50 | 53 | 56 |

For monster levels 1..4 the lowest id is 1. For monster level L ≥ 5 the lowest id is one
more than the highest id of level L−3. The resulting ranges:

| Monster level | Id range | Monsters |
|---|---|---|
| 1 | 1..5 | lemming to kobold |
| 2 | 1..11 | lemming to giant ant |
| 3 | 1..17 | lemming to zombie |
| 4 | 1..22 | lemming to centaur |
| 5 | 12..27 | floating eye to gelatinous cube |
| 6 | 18..33 | assassin bug to forvalaka |
| 7 | 23..39 | troll to invisible stalker |
| 8 | 28..42 | metamorph to shambling mound |
| 9 | 34..46 | lama nobe to mimic |
| 10 | 40..50 | poltergeist to purple worm (water lord excluded) |
| 11 | 43..53 | yellow mold to silver dragon (water lord excluded) |
| 12 | 47..56 | bronze dragon to red dragon (water lord excluded) |

### 3.2 The selection procedure

"Pick a monster for level L" (L is whatever the caller passes; callers often pass the
current level plus 1 to 4) works as follows:

1. Clamp L to 1..12. So the home level behaves like level 1, and dungeon levels 12..15
   and the whole volcano use the level-12 range.
2. Pick an id uniformly from the range for L. If it is the water lord (47), pick again,
   repeating until it is not. The water lord therefore never comes from this routine.
3. Genocide skip: while the chosen id is genocided and is below 61, add 1 to it; if that
   lands on a Wild id (57..60), jump straight to 61. So a genocided monster is replaced
   by the next non-genocided id above it, which may be outside the level's range, may be
   the water lord, and, if everything from the chosen id up to the red dragon is
   genocided, is the type I demon lord. (This is the "Wild exclusion helper": a test
   "57 ≤ id < 61" used here, in monster creation, in polymorph and in the mimic's
   disguise.)
4. Lemming override: if the *current* level (not L) is 15 or less, which includes the
   home level, then with probability 9 in 100 (`rnd(100) < 10`) the result is replaced
   by the lemming (id 1). This happens even if lemmings are genocided.
5. Return the id.

## 4. Monsters on the map

### 4.1 The per-square record

There is no list of monsters. A monster exists only as the contents of a map square, and
its identity is its position. For each square of the current level the game keeps:

| Per-square datum | Meaning |
|---|---|
| monster id | 0 = no monster, otherwise an id from the table. Stored in one signed byte. |
| hit points | The monster's current hit points (16-bit signed). |
| carried count | How many stolen or picked-up items the monster holds, 0..6. |
| carried items | Up to six (item id, item argument) pairs. |
| stealth flags | Bit 1 = "seen" (the player has seen or knows of the monster on this square); bit 2 = "awake". |
| moved flag | Scratch flag used during one monster-movement pass. |
| path value | Scratch number used by the smart path-finder. |

The monster id, hit points and carried items are saved with each level (Part 03/05). The
stealth flags, moved flags and path values are *not* saved and are *not* stored per
level: there is a single working copy. Consequences that are part of the behaviour:

* After loading a saved game every monster is unseen and asleep.
* On the *first* visit to a level all stealth flags are cleared. On a *return* visit
  they are not: the flags left over from the level just departed are reused by position,
  so some monsters on the re-entered level are "awake" or "seen" by coincidence.
* The seen bit belongs to the square, not the monster: when a monster moves, the
  destination square gets "awake" added to whatever it already held, and the source
  square's flags are left unchanged.

### 4.2 When a monster is drawn

A square shows its monster only if all of these hold: the player is not blind; the
player's knowledge of that square is not "unknown"; the square is flagged seen or awake.
Then:

* a **mimic** is drawn with the tile of its current disguise (below);
* an **invisible stalker** is drawn only while the see-invisible timer is non-zero;
  otherwise the square shows the remembered object as if empty;
* a **demon** (id 61..69) is drawn only while the player carries the Eye of Larn;
  otherwise the square shows the remembered object;
* any other monster is drawn with its own tile.

A monster's square is marked seen whenever that single square is redrawn while a monster
is on it, whenever it lies in the area revealed around the player each turn (the 3×3
block, or 7×7 with awareness; Part 02/11), by magic mapping and enlightenment scrolls
(Part 06), and for every square of a newly created level when in wizard mode or on the
home level. Because a monster that has moved is always awake, any monster that has moved
is visible wherever it stands on explored ground; there is no line-of-sight test.

**Mimic disguise.** There is one disguise value shared by all mimics. It starts as the
mimic's own tile and is not saved. Every time a visible mimic's square is drawn while
the game-time counter is a multiple of 10, a new disguise id is rolled: `rnd(61)`,
re-rolled while it is the invisible stalker (39) or a Wild id (57..60). The possible
disguises are therefore ids 1..38, 40..56 and 61 (the type I demon lord's tile); the
mimic can also "disguise" itself as a mimic. The disguise changes only the tile:
messages always use the name "mimic", and a mimic fights, moves and dies like any other
monster. Hitting a disguised or sleeping monster has no special rule beyond waking it.

**Invisibility and names.** The invisible stalker and unseen demons are fully solid: the
player attacks them by walking into them, and all messages name them correctly.

### 4.3 The name used in messages

Before most combat messages the game records "the current monster name": if the player
is blind it is the word "monster" and the "last attacker" id is set to 0; otherwise it
is the table name of the monster on the square and the last attacker id is that monster.
The last attacker id is what the death screen reports ("killed by a ..."), so dying
while blind is recorded as killed by an "unseen attacker".

### 4.4 Creating a monster next to the player

Used by scrolls, spells, altars, thrones, fountains, treasure rooms and the debug
command (the callers are listed in section 4.7). Given a monster id:

1. If the id is below 1 or above 69: beep, print `\ncan't createmonst(%d)\n` with the
   id, pause three seconds and stop.
2. Apply the same genocide skip as step 3 of section 3.2 (advance past genocided ids,
   jumping from 57 to 61). An id that is itself a Wild id and not genocided is *not*
   skipped: asking for 57..60 creates that Wild monster.
3. Pick a starting direction `rnd(8)` and try the eight directions in cyclic direction
   order (after 8 comes 1). The candidate square is the player's square plus the offset.
   It is acceptable if it is on the map, is not the level-1 exit square, holds neither a
   wall nor a closed door, and has no monster. Objects on the square do not matter (a
   monster can be created on top of an item or a trap).
4. On the first acceptable square: set the monster id, set hit points to the table
   value, clear both stealth flags (unseen, asleep). If the monster is a rothe, a
   poltergeist or a vampire, set it awake.
5. If no adjacent square is acceptable nothing is created and nothing is printed.

The carried-item count of the square is not reset by creation (see the quirk list in
section 13).

### 4.5 Placing a monster somewhere on the level

Used for level population and for monsters that appear over time. Given a monster id
(no genocide skip here; the caller normally used the level-selection routine):

1. Up to 10 attempts: x = `rnd(65)` (1..65), y = `rnd(15)` (1..15).
2. The square is acceptable if it holds no object at all (empty floor), no monster, and
   is not the player's square.
3. On success set the id, clear both stealth flags and set hit points to the table
   value; report success. After 10 failures report failure and create nothing.

Rothes, poltergeists and vampires placed this way are asleep like everything else.

### 4.6 Level population (summary; Part 05 owns level creation)

* **First visit** to any level other than home: all stealth flags are cleared, then
  `rnd(12) + 2 + level/2` monsters are placed with section 4.5, each chosen with
  section 3.2 for the current level. Then, on levels 11..15, `level − 10` demon lords
  are placed, each of a random type (id 61 + `rund(7)`), retrying placement until it
  succeeds; on volcano levels, `level − 15` demon princes are placed the same way.
* **Return visit** to a level other than home: `level/2 + 1` extra monsters are placed
  the same way (no extra demons).
* The home level gets no monsters when created or re-entered (but see section 5).
* After either kind of arrival every monster on the level whose kind is genocided is
  removed.
* Pre-made maps and treasure rooms place additional monsters with full table hit points
  directly or through section 4.4 (Part 05): the demon prince guarding the Eye of Larn
  on level 15, Lucifer on the bottom volcano level, and level-appropriate guards.
* Wild Edition hooks run when a level is created, entered and left (Part 12).

Monsters never follow the player between levels. A monster that falls through a
trapdoor or rides an elevator is simply removed from the game.

### 4.7 Who creates monsters next to the player

For cross-reference (the probabilities belong to the named parts): the scroll of create
monster (level + 1); the create monster spell (level + 1, **then 8 is added to the
id**); praying at an altar without paying (level + 2), an insultingly small offering
(demon prince), desecrating an altar (level + 3, **then 8 is added to the id**),
ignoring an altar (level + 2); prying at or sitting on a throne (gnome king); washing in
a fountain (water lord); vaporising an altar (demon princes); treasure rooms (level + 2,
or level + 4 at difficulty 3 and above); the wizard-mode debug command.

Because of the "add 8" rule and the Wild insertion, those two callers can produce the
four Wild monsters (selection results 49..52 become 57..60), demon lords I..IV (53..56
become 61..64) and, when the selection overflowed to the type I demon lord because of
genocide, Lucifer (61 becomes 69).

## 5. Monsters appearing over time

A one-byte countdown starts at 80 for a new game and is saved in the save file. After
each player turn, unless time is stopped:

1. Subtract 1 from the countdown.
2. If it is now 0 or less: set it to `120 − 4 × level` (120 on the home level, 116 on
   level 1, 60 on level 15, 40 on the bottom of the volcano) and place one monster
   (section 4.5) chosen for the current level (section 3.2).

This runs on every level including the home level (where the level-1 list is used, with
the lemming override). The same countdown step is also performed for each step of a
"run" and for each turn the player loses while asleep, held or otherwise out of action.
If placement fails after its 10 attempts no monster appears and the countdown is still
reset.

## 6. When monsters get to move

The monster-movement pass ("one monster turn") is invoked:

* once at the top of every main-loop iteration, before the player's command, unless the
  player was hit during the previous command (a flag set by any monster attack, used so
  that monsters which already acted during a run or a lost turn do not act again);
* after every step of a run;
* once for every turn the player loses (sleep, self-inflicted web and so on);
* **twice** in each of those places while the haste-monster timer is non-zero.

Part 01 owns the loop; the pass itself is this part. One pass does the following, in
order:

1. If the time-stop timer is non-zero, do nothing at all.
2. If the haste-self timer is non-zero and even, do nothing at all. (The timer counts
   down each turn, so a hasted player gets two turns for each monster turn.)
3. Move every sphere of annihilation (Part 07). Spheres therefore still move while
   monsters are held.
4. If the hold-monster timer is non-zero, stop here.
5. Work out the **activity window** around the player:
   * normally columns playerx−5 .. playerx+5 and rows playery−3 .. playery+3 (11 × 7),
     with a path depth limit of 17;
   * while the aggravate-monsters timer is non-zero, columns playerx−10 .. playerx+10
     and rows playery−5 .. playery+5 (21 × 11), with a path depth limit of 40.
   On the home level the window is clipped to the whole map; on every other level it is
   clipped to the interior (columns 1..65, rows 1..15).
6. Clear the moved flag of every square in the window.
7. **The monster the player last damaged goes first.** The game remembers the square of
   the last monster the player hit (with a weapon or a spell, including spells the
   monster resisted). If that position is on the map: clear its moved flag and, if a
   monster is there, give it a move (section 7), wherever on the level it is; then set
   the remembered position to "the place the most recent monster move ended at". That
   last value is only updated when some monster actually moves or teleports (it is
   "nowhere" when the mover was destroyed), so if the remembered monster did not move
   the remembered position becomes a stale one. The remembered position is reset to
   "nowhere" on every level change.
8. Scan the window row by row from the top row to the bottom row, and within a row from
   left to right. For each square holding a monster whose moved flag is clear:
   * if the aggravate timer is non-zero, or the stealth timer is zero: give it a move,
     whether it is awake or not;
   * otherwise (the player is stealthy and not aggravating): give it a move only if it
     is flagged awake.

A monster that moves sets the moved flag of its destination, and a monster that attacks
sets the moved flag of its own square, so nothing acts twice in one pass even if it
moves "ahead" of the scan. A monster outside the window never acts (except the
last-damaged monster in step 7).

So "asleep" only matters while the player has stealth: without stealth, every monster
within the 11 × 7 window homes in on the player. A monster becomes awake when it moves,
when it takes any damage from the player, or when it is created next to the player as a
rothe, poltergeist or vampire.

## 7. One monster's move

### 7.1 Order of decisions

For the monster on a given square:

1. Wild Edition hook: if this monster is the player's pet it is skipped here; the pet
   moves itself (Part 12).
2. If the monster is slow (flag S) and the game-time counter is odd, it does nothing
   this pass. (While monsters are hasted both passes of a turn see the same counter, so
   a slow monster moves twice on even turns and not at all on odd turns.)
3. **Fear.** Start with fear = 1 if the player carries the hand of fear, else 0.
   * If the scare-monster timer is non-zero, add 1 (so 1 without the hand, 2 with it).
   * Otherwise, if fear is 1 (hand only), then with probability 6 in 10 (`rnd(10) > 4`)
     set fear to 0. The hand alone therefore frightens a given monster on 40% of its
     moves.
   * If the monster's id is above the type I demon lord (ids 62..69) or it is the
     platinum dragon: fear becomes 0 unless it was 2, in which case it becomes 1 with
     probability 5 in 10 (`rnd(10) > 5`) and 0 otherwise. The type I demon lord itself
     is treated as an ordinary monster here.
4. If fear is non-zero the monster makes a **scared move** (7.2). Otherwise, if its
   intelligence is greater than `10 − difficulty` it makes a **smart move** (7.4);
   otherwise a **dumb move** (7.3).

At difficulty 0 the smart monsters are: white dragon, elf, xorn, vampire, umber hulk,
gnome king, water lord, bronze dragon, green dragon, xvart, spirit naga, silver dragon,
platinum dragon, red dragon, tax auditor and all demons. Each point of difficulty lowers
the threshold by one (the centaur becomes smart at difficulty 1, and so on).

No monster kind is stationary: floating eyes, molds and fungi all move by the dumb rule.

### 7.2 Scared move

"Fleeing" is a random stagger, not a retreat:

1. Target x = x + `rnd(3)` − 2 and target y = y + `rnd(3)` − 2 (each offset is −1, 0 or
   +1), clamped to the map.
2. If the target is a legal square for this monster (7.5) and holds no monster, perform
   the move (7.6). Otherwise do nothing this pass.

The target may be the monster's own square (then nothing happens) or the player's
square, in which case the "frightened" monster attacks. Scared monsters do not avoid
traps.

### 7.3 Dumb move

1. Consider the 3 × 3 block centred on the monster, but drop the column on the side away
   from the player (if the monster is left of the player drop the left column; if right
   of the player drop the right column; if in the same column keep all three) and
   likewise drop the row on the side away from the player. Clip to the map.
2. Walk the remaining squares column by column from left to right, and within a column
   from top to bottom. A square is a candidate if it is legal for this monster (7.5) and
   holds no monster (so the monster's own square is never a candidate; the player's
   square is, since no monster stands there).
3. Each candidate scores the squared straight-line distance to the player:
   (playerx − x)² + (playery − y)².
4. Choose the candidate with the smallest score; ties go to the earliest in the walk
   order. If there is no candidate the monster stays put. Otherwise perform the move
   (7.6) to that square.

A dumb monster always moves if any candidate exists, even if every candidate is farther
from the player than where it stands, so dumb monsters shuffle sideways along walls
rather than waiting. Dumb monsters do not avoid traps.

### 7.4 Smart move (scent / path-finding)

Smart monsters follow a shortest path computed by flooding outward from the player
through a numeric grid. For the monster at (x, y):

1. **Mark the grid.** Take the activity window of section 6 step 5 and grow it by two
   squares on the low sides and by three on the high sides (the window's upper bounds
   are exclusive, so the marked region is columns playerx−7 .. playerx+8 and rows
   playery−5 .. playery+6 normally, or playerx−12 .. playerx+13 and playery−7 ..
   playery+8 when aggravating), clipped to the map. For each square in it:
   * if the square is not legal for this monster (7.5): mark it 127 (blocked);
   * else, for the demon prince and Lucifer: mark it 0 (open), whatever it contains;
   * else by content: an up or down elevator, a visible arrow trap, a visible dart trap
     or a visible teleport trap is marked 127; a pit or a visible trapdoor is marked 127
     unless the monster flies, in which case 0; anything else is marked 0.
   Invisible traps are not avoided (and, see 7.7, do not affect monsters).
2. Mark the player's square 1.
3. **Flood.** The flood works inside the region one square smaller than the marked
   region on every side. For distance d = 1, 2, 3, ... while d is below the path depth
   limit (17, or 40 when aggravating) and the monster's square has not been reached:
   scan the flood region row by row, left to right; for every square whose mark equals
   d, look at its eight neighbours in direction order; each neighbour that is on the map
   and marked 0 is marked d + 1; if that neighbour is the monster's own square, the path
   is found (the scan for this d still completes).
   Other monsters do not block the flood. The monster must therefore be within 16 steps
   (39 when aggravating) of the player along open squares inside the region.
4. **Step.** If the path was found, let t be the monster's mark minus 1. Examine the
   monster's eight neighbours in direction order. A neighbour is usable if it is on the
   map (on the home level anywhere; elsewhere only the interior, columns 1..65 and rows
   1..15), its mark equals t, and no monster stands on it. Move (7.6) to the first
   usable neighbour and stop. If none is usable the monster does not move this pass.
   When t is 1 the neighbour is the player's square and the move is an attack.
5. If no path was found the monster does not move. There is no fallback to the dumb
   rule.

Diagonal steps are allowed everywhere, including around corners. Because the monster's
own square is marked like any other, a smart monster standing on a square it would avoid
(for example one created on top of a visible trap or an elevator) is never reached by
the flood and stays motionless until frightened. A smart monster far outside the region
(possible only for the last-damaged monster) also does not move.

### 7.5 Legal squares: walls, doors and special cases

A square is illegal for a monster if any of these is true:

* it is the level-1 exit square (33, 16) while on level 1;
* it holds a closed door (even if the player is standing in the doorway);
* it holds a wall, unless the player is standing on that square (a player walking
  through walls can be attacked inside the wall);
* the monster is a vampire and the square holds a mirror.

For the demon prince and Lucifer, walls and closed doors are not obstacles at all (they
pass through them without altering them), but the exit square and the path-finder's
"interior only" rule still apply.

Monsters never open, close or break doors; an open door is ordinary floor to them. A
closed door stops every monster except those two, and a player standing on a closed-door
square cannot be attacked by anything else.

### 7.6 Performing the move

Given a source and a destination square:

1. If the destination is the player's square the monster attacks instead (section 8);
   its own square is flagged moved and recorded as where the move ended. Stop.
2. Otherwise remember what object lies on the destination, then transfer the monster:
   copy its carried items and count, its id and its hit points to the destination; add
   "awake" to the destination's stealth flags; clear the source's id, carried count and
   hit points; flag the destination moved.
3. **Lemming:** with probability 2 in 100 (`rnd(100) ≤ 2`) a new lemming with the same
   hit points appears on the square just vacated.
4. **Leprechaun:** if the destination object is any gold pile (all four gold sizes) or a
   gem (diamond, ruby, emerald, sapphire), the leprechaun takes it: if it carries fewer
   than six items the object and its argument are added to its carried items; in every
   case the object is removed from the floor (a full leprechaun destroys it).
5. **Troll:** if the game-time counter is even and its hit points are below the table
   value, it regains 1 hit point. (Only when it actually moves, not when it attacks.)
6. Apply traps and spheres on the destination (7.7).
7. Record where the move ended (the destination, or "nowhere" if the monster was
   destroyed).
8. If the player is blind, stop: nothing is printed or redrawn.
9. If a trap message was produced and the destination square is known to the player,
   print it and beep.
10. If the mover is a demon and the player does not carry the Eye of Larn, stop without
    redrawing. Otherwise redraw the source and the destination squares if each is known
    to the player (which also marks the monster seen).

Monsters walk over ordinary items, stairs, fountains, altars, thrones, statues, mirrors
(except vampires) and buildings with no effect. Apart from the leprechaun (and the Wild
roomba, Part 12) no monster picks anything up.

### 7.7 Traps, spheres and elevators as monsters meet them

The effect depends on the object that was on the destination square:

| Object | Effect on the monster | Message (printed only if not blind and the square is known) |
|---|---|---|
| Sphere of annihilation, mover is a demon | If the player does *not* carry the talisman of the sphere, or the player does carry it and the mover is Lucifer and `rnd(10) > 7` (3 in 10): the sphere is removed. Otherwise the demon is destroyed. | `\nThe %s dispels the sphere!` or `\nThe %s is destroyed by the sphere of annihilation!` |
| Sphere of annihilation, any other monster | The monster is destroyed (no experience is awarded). | `\nThe %s is destroyed by the sphere of annihilation!` |
| Visible arrow trap | Loses `rnd(10) + level` hit points (level = current dungeon level number). | `\nAn arrow hits the %s.` or, if that kills it, `\nAn arrow hits and kills the %s.` |
| Visible dart trap | Loses `rnd(6)` hit points. | `\nA dart hits the %s.` or `\nA dart hits and kills the %s.` |
| Visible teleport trap | Non-demons are teleported (section 11.1). Demons are unaffected. | `\nThe %s gets teleported.` |
| Pit | Non-flying monsters are removed from the game. | `\nThe %s fell into a pit.` |
| Visible trapdoor | Non-flying monsters are removed from the game (they do not arrive on the level below). | `\nThe %s fell through a trapdoor.` |
| Elevator up or down | Non-demons are removed from the game. | `\nThe %s is carried away by an elevator!` |

Traps are not used up. Invisible (undiscovered) arrow, dart, teleport and trapdoor traps
have no effect on monsters. A monster killed by an arrow or dart gives no experience and
drops nothing. For the teleport trap the message is shown only if the square the monster
arrives on is known to the player. The `%s` in these messages is always the true table
name.

## 8. Monster attacks on the player

### 8.1 Sequence

When a monster's move targets the player's square:

1. The monster becomes the "last attacker". If its square is unknown to the player the
   square is revealed.
2. Flags are set that stop a run, flush typed-ahead input and suppress the next monster
   pass (Part 01); any repeat count is cancelled.
3. The current monster name is recorded (section 4.3; this resets the last attacker to
   "unseen" if the player is blind).
4. **Lemmings never attack:** if the monster is a lemming, stop here with no message.
5. Wild Edition hook: a hat or perk may make the blow miss, printing
   `\nThe %s misses you!` (Part 12). Stop if so.
6. **Invisible player:** if the monster lacks infravision and the player's invisibility
   timer is non-zero, then with probability 19 in 33 (`rnd(33) < 20`) print
   `\nThe %s misses wildly!` and stop.
7. **Charm:** if the monster is not a demon and not the platinum dragon, and the charm
   timer is non-zero, then if `rnd(30) + 5 × monster level − player charisma < 30` print
   `\nThe %s is awestruck by your magnificence!` and stop.
8. **Damage roll:** damage = D + `rnd(max(D, 1))` + monster level, where D is the table
   damage.
9. **Slayer:** if the monster is a demon and the player is wielding Slayer, damage =
   (`rnd(5)` × damage) / 10, i.e. one tenth to one half.
10. **Spirit protection:** if the player has a scarab of negate spirit or the
    spirit-protection timer is non-zero, and the monster is a spirit (poltergeist,
    spirit naga), halve the damage.
11. **Undead protection:** if the player has a cube of undead control or the
    undead-protection timer is non-zero, and the monster is undead (zombie, wraith,
    vampire), halve the damage.
12. Let bias = difficulty + 1 and chance = max(player AC, 1).
13. **Special attack:** if the monster's attack number is above 0, and either
    damage + bias + 8 > player AC or `rnd(chance)` = 1 (the second roll is made only if
    the first test fails): perform the special attack (8.4). If it reports "finished"
    stop here. Otherwise note that the monster connected (so no "missed" line will be
    printed) and reduce bias by 2.
14. **Ordinary hit:** if damage + bias > player AC or `rnd(chance)` = 1: print
    `\n  The %s hit you.`; the damage taken is damage − player AC, not below 0; if it is
    above 0 the player loses that many hit points.
15. If neither step 13 nor step 14 connected, print `\n  The %s missed.`

Losing hit points to a monster can kill the player (`\nAlas, you have died.\n`, then the
death sequence with the last attacker as the cause; Part 03). A Wild Edition hook runs
whenever the player loses hit points (Part 12).

### 8.2 Things that cancel a special attack before it happens

Checked in this order at the start of every special attack (including the ones the
spirit naga borrows):

1. **Cancellation timer non-zero:** for the demon prince and Lucifer the attack is
   cancelled only with probability 6 in 100 (`rnd(100) ≥ 95`); for every other monster
   it is always cancelled.
2. **Staff of power in the pack:** for any demon except Lucifer, and for wraiths and
   vampires, the attack is cancelled with probability 74 in 100 (`rnd(100) < 75`).
3. **Undead control:** if the monster is undead and the player has a cube of undead
   control or an active undead-protection timer, the attack is cancelled.

A cancelled special attack prints nothing and reports "not finished", so the ordinary
hit of step 14 is still tried with the reduced bias.

### 8.3 What happens after the special attack's effect

If the attack set a message it is printed with the monster's name after the effect has
been applied, the player's derived statistics are recalculated and the status display is
refreshed. Attacks marked "beep" below sound the alert beep. "Finished" means no
ordinary hit follows.

### 8.4 The special attacks

Hit-point losses below are applied only if the computed number is above 0; AC is the
player's armour class.

| No. | Used by | Effect | Message | Beep | Finished |
|---|---|---|---|---|---|
| 1 | rust monster | Rusts armour, see 8.5. | see 8.5 | only if something rusted | no |
| 2 | hell hound | Lose `rnd(15) + 8 − AC` hit points. | `\nThe %s breathes fire at you!`, or with fire resistance `\nThe %s's flame doesn't faze you!` | unless fire resistant | no |
| 3 | bronze, silver and red dragons, type I demon lord | Lose `rnd(20) + 25 − AC` hit points. | same two messages as no. 2 | unless fire resistant | no |
| 4 | giant centipede, giant ant | If strength is above 3: strength −1. | `\nThe %s stung you!  You feel weaker.` or, at strength 3 or less, `\nThe %s stung you!` | only if weakened | no |
| 5 | white dragon, type II demon lord | Lose `rnd(15) + 18 − AC` hit points. | `\nThe %s blasts you with his cold breath.` | yes | no |
| 6 | wraith, vampire, type VII demon lord, demon prince, Lucifer | Level drain, see 8.6. | `\nThe %s drains you of your life energy!` | yes | no |
| 7 | water lord | Lose `rnd(15) + 25 − AC` hit points. | `\nThe %s got you with a gusher!` | yes | no |
| 8 | leprechaun | Steal gold, see 8.7. | see 8.7 | yes | yes |
| 9 | disenchantress, type III demon lord | Disenchant an item, see 8.8. | see 8.8 | if an item was hit | no |
| 10 | ice lizard, green dragon | Lose `rnd(25) − AC` hit points. | `\nThe %s hit you with its barbed tail.` | yes | no |
| 11 | umber hulk, type IV demon lord | Confusion timer += 10 + `rnd(10)`. No effect at all in wizard mode. | `\nThe %s has confused you.` | yes | no |
| 12 | spirit naga | Performs one of attacks 1, 2, 3, 5, 6, 8, 9, 11, 13, 14 chosen uniformly (`rund(10)` into that list), with all the checks of 8.2 applied again; finished exactly when the borrowed attack is. | the borrowed attack's | as borrowed | as borrowed |
| 13 | platinum dragon, type V and type VI demon lords | Lose `rnd(15) + 30 − AC` hit points. | `\nThe %s flattens you with it's psionics!` | yes | no |
| 14 | nymph | Steal an item, see 8.9. | see 8.9 | if it stole | if it stole |
| 15 | bugbear | Lose `rnd(10) + 5 − AC` hit points. | `\nThe %s bit you!` | yes | no |
| 16 | osequip | Lose `rnd(15) + 10 − AC` hit points. | `\nThe %s bit you!` | yes | no |
| 17 | tax auditor (Wild) | Wild Edition hook: if the coin feature is on, the auditor garnishes gold and the attack is finished; otherwise nothing. See Part 12. | from Part 12 | yes if on | yes if on |

Fire resistance changes only the message and the beep for attacks 2 and 3: **the fire
damage is applied in full either way.**

### 8.5 Rust (attack 1)

1. If a shield is worn and its enchantment is 0 or more, reduce the shield's enchantment
   by 1. Nothing else rusts this time.
2. Otherwise, if body armour is worn and it is one of the rustable kinds below, and its
   enchantment is above that kind's floor, reduce it by 1.

| Armour | Floor (stops rusting at) |
|---|---|
| studded leather armor | −2 |
| ring mail | −4 |
| chain mail | −5 |
| splint mail | −6 |
| plate mail | −8 |
| plate armor | −9 |

A shield stops rusting at −1. Leather armor, stainless plate armor and elven chain never
rust.

3. Message: if something rusted, beep and print
   `\nThe %s hit you -- your armor feels weaker.` Otherwise the message depends on the
   body armour worn: leather armor
   `\nThe %s hit you -- you are lucky you have leather on.`; stainless plate armor
   `\nThe %s hit you -- you are fortunate to have stainless steel armor!`; elven chain
   `\nThe %s hit you -- you are very lucky to have such strong elven chain!`; anything
   else, including no armour, `\nThe %s hit you.`
4. Recalculate the player's armour class.

Since the attack is not "finished", the rust monster's ordinary hit follows and can
print a second "hit you" line.

### 8.6 Level drain (attack 6)

1. If an amulet of life preservation is anywhere in the pack the attack does nothing and
   prints nothing (it still counts as having connected).
2. Otherwise the player loses one experience level (Part 02: experience drops to one
   point below the current level's threshold, with the associated loss of maximum hit
   points and spell points; no effect at level 1).
3. If the attacker is the demon prince, maximum and current spell points each drop by 1
   more (not below 0).
4. If the attacker is Lucifer: a second level is lost, maximum and current spell points
   each drop by 2, and strength, intelligence, wisdom, constitution and dexterity each
   drop by 1 (charisma is spared).

### 8.7 Steal gold (attack 8)

1. If the player has a device of theft prevention, nothing happens (not finished; the
   ordinary hit follows).
2. If the player carries gold: print `\nThe %s hit you.  Your purse feels lighter.`; if
   the gold is above 32767 it is halved, otherwise it is reduced by
   `rnd(1 + gold/2)`; it cannot go below 0.
3. If the player has no gold: print `\nThe %s couldn't find any gold to steal.`
4. In both cases the monster then vanishes from the game entirely (it is not teleported,
   the stolen gold cannot be recovered, and no experience is given). Beep.

### 8.8 Disenchant (attack 9)

Up to 50 times, pick a random pack slot (`rund(26)`). The first slot that holds an item
which is not a scroll and not a potion and whose argument (enchantment or charge value)
is above 0 is hit: its argument is reduced by 3, or to 0 if it was below 3; the item's
inventory line is printed; beep; message
`\nThe %s hits you with a spell of disenchantment! ` (with the trailing space; note the
item line appears before this message). If all 50 picks fail the message is
`\nThe %s nearly misses.`

### 8.9 Steal an item (attack 14)

1. If the player has a device of theft prevention, nothing happens (not finished).
2. If the pack holds nothing except what is wielded, worn as armour or carried as the
   shield: print `\nThe %s couldn't find anything to steal.` (not finished).
3. Otherwise print `\nThe %s picks your pocket and takes:` and attempt the theft:
   * if the monster already carries six items the theft fails;
   * otherwise up to 100 times pick a random pack slot; the first that holds an item
     which is not the wielded weapon, the worn armour or the shield is taken: its
     inventory line is printed, the player's statistics are adjusted for losing it, it
     is appended to the monster's carried items and removed from the pack, and the
     level's stolen-item counter is incremented.
   If the theft failed print `  nothing`.
4. Teleport the monster (section 11.1), beep, finished.

The stolen items come back when the thief is killed (section 10.3).

### 8.10 Demon lords' attack numbers

Type I: 3 (fire). Type II: 5 (cold). Type III: 9 (disenchant). Type IV: 11 (confusion).
Type V and VI: 13 (psionics). Type VII, demon prince and Lucifer: 6 (drain).

## 9. Player attacks on monsters

### 9.1 Melee: walking into a monster

Walking into a square that holds a monster attacks it instead of moving (Part 01/02; the
Wild pet swap happens before this). The attack:

1. If the time-stop timer is non-zero nothing happens at all: monsters cannot be struck
   while time is stopped.
2. Record the current monster name (section 4.3).
3. To-hit number = monster AC + player level + player dexterity + weapon class / 4 − 12
   − difficulty.
4. The blow lands if `rnd(20)` is less than the to-hit number, or failing that if
   `rnd(71) < 5` (a 4 in 71 last chance, so even an "unhittable" monster is hit about
   5.6% of the time).
5. On a hit print `\nYou hit`; base = the result of "one full hit" (9.2); if base is
   below 9999 the damage is `rnd(base) + 1`, otherwise it is base. On a miss print
   `\nYou missed`. Either way finish the line with ` the %s.`
6. **Weapon dulling** (9.3) if the blow landed on a rust monster, a disenchantress or a
   gelatinous cube.
7. If the blow still counts as a hit, apply the damage as weapon damage (9.4). Then, if
   the monster is a demon, the player wields the lance of death and the monster is still
   alive, print `\nYour lance of death tickles the %s!`
8. **Metamorph:** if the monster struck at was a metamorph and its hit points are now
   above 0 and below 25, it turns into a monster with id 48 + `rund(9)` (bronze dragon,
   green dragon, purple worm, xvart, spirit naga, silver dragon, platinum dragon, green
   urchin or red dragon) keeping its current hit points, and its square is redrawn. Only
   melee triggers this.
9. **Lemming:** if the monster now on the square is a lemming (it survived, hit or
   miss), then with probability 40 in 100 (`rnd(100) ≤ 40`) a new lemming is created
   next to the player (section 4.4).

There is no bonus for striking a sleeping, unseen or disguised monster.

### 9.2 "Full hits"

"n full hits" (used by melee with n = 1 and by the sleep and web spells with n = 2..5)
is:

* 0 if n is below 0 or above 20;
* 10000 if the lance of death is wielded;
* otherwise n × (weapon class / 2 + strength + extra strength − difficulty − 12 + extra
  damage); if that is below 1 the result is n.

Weapon class, extra strength and extra damage are derived player statistics (Part 02).

### 9.3 Weapon dulling

Applies only when a blow lands on a rust monster, disenchantress or gelatinous cube and
something is wielded. Let E be the wielded item's argument (its enchantment).

* The "dullable list" is: Slayer, dagger, spear, flail, battle axe, longsword, 2-handed
  sword, lance of death, Bessman's flailing hammer, the Vorpal Blade and the belt of
  striking.
* If (the item is on the list and E > −10) or E > 0: print
  `\nYour weapon is dulled by the %s.`, beep, and reduce E by 1.
* Otherwise, if E ≤ −10: print `\nYour weapon disintegrates!`; the item is destroyed
  (statistics adjusted for losing it, nothing wielded) and the blow does **not** count
  as a hit after all, although "You hit the ..." was already printed.
* Otherwise nothing happens.
* The player's weapon class is recalculated.

So listed weapons dull all the way to −10 and are destroyed by the next landed blow;
unlisted things (sword of slashing, sunsword, or any non-weapon) only lose positive
enchantment. Because the rule looks only at the argument, a wielded scroll or potion has
its argument, which is its *kind*, reduced by one.

### 9.4 Applying damage to a monster

This is the single routine through which melee, spells, the Wild pet and Wild lightning
damage monsters. Inputs: the square, an amount, and whether the damage is from a spell.
It relies on the caller having recorded the current monster name.

1. If the player's half-damage curse (sickness) timer is non-zero, halve the amount. If
   the amount is now 0 or less it becomes 1.
2. Remember this square as "the monster the player last damaged" (section 6 step 7).
3. Flag the monster awake.
4. If the hold-monster timer is above 0 set it to 1, so the hold ends at the next turn's
   timer step: any damage to any monster breaks hold monster.
5. **Dragon slaying:** if the player holds at least one orb of dragon slaying and the
   monster is a white, red, green, bronze, platinum or silver dragon, triple the amount
   (this applies to spells too).
6. Weapon effects, skipped for spell damage:
   * **Vorpal Blade:** if the wielded item is the Vorpal Blade *and it is not in the
     first pack slot*, and `rnd(20)` = 1, and the monster has a head (flag H) and is not
     protected (flag N): print `\nThe Vorpal Blade beheads the %s!` and set the amount
     to the monster's current hit points. Protected: water lord, spirit naga, all
     demons, fountain kraken, roomba. Headless: see the table.
   * **Demons:** if the monster is a demon, then if the lance of death is wielded the
     amount becomes 300, and (checked second, so it wins) if Slayer is wielded the
     amount becomes 10000.
7. If the monster's hit points are less than or equal to the amount, it dies (section
   10) and the routine reports the hit points it had. Otherwise subtract the amount and
   report the amount originally passed in (before halving and tripling; 1 if step 1
   forced it to 1). The reported number is what a ray spell subtracts from its remaining
   strength (Part 07).

### 9.5 Named weapons and items in combat, in one place

| Item | Rule in this part |
|---|---|
| lance of death (wielded) | Every landed blow on a non-demon does 10000 (5000 while sick), which kills anything. Against demons it does exactly 300 and prints the "tickles" line if the demon survives. Sleep and web "full hits" are also 10000. |
| Slayer (wielded) | Every landed blow on a demon does 10000 (Lucifer, at 32767 hit points, needs four). Demons' melee damage against the player is cut to between one tenth and one half. |
| the Vorpal Blade (wielded) | 1 in 20 beheading on each landed blow, as in 9.4. Never triggers from pack slot "a". |
| orb of dragon slaying (carried) | Triples all damage to the six dragons. |
| Eye of Larn (carried) | Lets the player see demons. It has no effect on being able to hit them or be hit by them. |
| staff of power (carried) | 74% cancel of special attacks by demon lords, the demon prince, wraiths and vampires; no effect on Lucifer. |
| amulet of life preservation (carried) | Blocks every level drain. |
| device of theft prevention (carried) | Blocks gold and item theft. |
| scarab of negate spirit (carried) or spirit protection (timed) | Halves poltergeist and spirit naga melee damage. |
| cube of undead control (carried) or undead protection (timed) | Halves zombie, wraith and vampire melee damage and cancels the wraith's and vampire's drain. |
| hand of fear (carried) | Fear, as in 7.1. |
| talisman of the sphere (carried by the player) | Demons no longer dispel spheres they walk into (Lucifer still does 30% of the time); they are destroyed instead. |
| stainless plate armor, elven chain, leather armor | Immune to rust. |

No monster requires a special weapon to be damaged; demons are simply protected by very
low armour class, and the lance of death is deliberately weak against them.

## 10. Killing a monster

When the damage routine kills a monster, in order:

1. Print `\nThe %s died!` (the front end also counts kills from this line; Part 09).
2. Wild Edition hook: the kill is reported to the Wild features (kill counts, bounties,
   cards, perks, the roomba's contents; Part 12).
3. Award the monster's table experience (which may raise the player's level and print
   `\nWelcome to level %d!`; Part 02).
4. Remove the monster from its square and redraw the square if known.
5. Drops, one of two branches:
   * **The monster carried items** (stolen or picked up): each carried item is recreated
     on a free square adjacent to the monster's square (10.4); the level's stolen-item
     counter is reduced by the number of items but not below 1; the carried count is
     cleared; then gold is dropped (10.1). No random item is dropped in this branch.
   * **The monster carried nothing:** gold is dropped (10.1); then the random drop for
     its kind (10.2); then, Wild Edition hook, the Lucky perk repeats the random drop
     once more (Part 12).
6. Redraw the monster's square and the area around the player, recalculate the player's
   statistics, refresh the status display, and zero the square's hit points.

Monsters removed by any other means (traps, pits, elevators, spheres, the scroll of
annihilation, genocide, the leprechaun's vanishing act) drop nothing. Of those, only the
scroll of annihilation awards experience (Part 06/07).

### 10.1 Gold

If the monster's table gold G is above 0 the amount is `rnd(G) + G` (between G + 1 and
2G). It is dropped **around the player**, not around the monster: on one of the eight
squares adjacent to the player, starting from a random direction and trying all eight in
cyclic direction order. Amounts above 250 are created as the "hundreds" kind of gold
pile whose stored argument is amount / 100 (the remainder is lost); amounts of 250 or
less are an ordinary pile holding the exact amount. If the first square tried already
holds gold the new gold is merged into it under the pile-merging rules (Part 06).

### 10.2 Random drops by kind

| Monster | Drop |
|---|---|
| orc, nymph, elf, troglodyte, troll, rothe, violet fungus, platinum dragon, gnome king, red dragon | One "random item for the current dungeon level" (10.3), created adjacent to the monster's square. |
| leprechaun | With probability 27 in 101 (`rnd(101) ≥ 75`) one gem adjacent to the player (below). Then with probability 1 in 5 (`rnd(5)` = 1) the whole leprechaun drop is repeated (so the chain continues with probability 1/5 each time). |
| everything else, including the lemming, all demons and the Wild monsters | Nothing. |

A gem is chosen with `rnd(4)`: 1 diamond (j = 50), 2 ruby (j = 40), 3 emerald (j = 30),
4 sapphire (j = 20); its argument (value) is `rnd(j) + j/10`.

### 10.3 The random item table

"A random item for level L" creates one item and then, with probability 7 in 101
(`rnd(101) < 8`), another, repeating while that roll succeeds. Each item is chosen by
rolling `rnd(32)` if L ≤ 3, `rnd(35)` if L ≤ 6, otherwise `rnd(37)`, on this list:

| Roll | Item | Argument |
|---|---|---|
| 1–4 | scroll | a random scroll kind (Part 06) |
| 5–8 | potion | a random potion kind (Part 06) |
| 9–12 | gold pile | `rnd((L + 1) × 10) + L × 10 + 10` gold |
| 13–16 | book | L |
| 17–19 | dagger | a pick from {0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 5} |
| 20–22 | leather armor | a pick from {0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7}; only the first 12 entries when difficulty is non-zero |
| 23 | ring of regeneration | `rund(L/3 + 1)` |
| 24 | ring of protection | `rnd(L/4 + 1)` |
| 25 | energy ring | `rund(L/4 + 1)` |
| 26 | ring of dexterity | `rnd(L/4 + 1)` |
| 27 | ring of strength | `rnd(L/2 + 1)` |
| 28 | spear | `rund(L/3 + 1)` |
| 29 | belt of striking | `rund(L/2 + 1)` |
| 30 | ring mail | `rund(L/2 + 1)` |
| 31 | studded leather armor | `rund(L/2 + 1)` |
| 32 | shield | `rund(L/3 + 1)` |
| 33 | flail | `rund(L/2 + 1)` |
| 34 | chain mail | a pick from {0, 0, 0, 1, 1, 1, 2, 2, 3, 4} |
| 35 | 2-handed sword | `rund(L/3 + 1)` |
| 36 | plate mail | a pick from {0, 0, 0, 0, 1, 1, 2, 2, 3, 4}; only the first 3 entries when difficulty is non-zero |
| 37 | longsword | a pick from {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 3}; only the first 6 entries when difficulty is non-zero |

"A pick from" means a uniformly random entry of the list. The same table is used for
objects scattered at level creation (Part 05/06).

### 10.4 Where dropped items land

A non-gold item is placed on one of the eight squares adjacent to the given centre,
starting from a random direction (`rnd(8)`) and trying all eight in cyclic direction
order; the first square that is on the map, is not the level-1 exit, is not a wall or
closed door and holds no object is used (a monster or the player may be standing there).
If none of the eight qualifies the item is silently lost.

Carried gold given back by a dead leprechaun goes through the gold-creation path, which
treats the stored argument as a number of gold pieces; a pile of the "hundreds" kind
that the leprechaun had picked up is therefore recreated with its argument divided by
100 again (nearly worthless), while the other gold kinds and gems are returned intact.

### 10.5 Special death and life effects by kind

* **Lemming:** 2% chance to duplicate on each step; 40% chance to spawn another lemming
  each time one survives the player's swing; never attacks; worth 1 experience; has 0
  table hit points so any damage kills it.
* **Leprechaun:** picks up gold and gems; vanishes after its theft attempt; gem drops.
* **Nymph:** teleports after stealing; returns the items when killed.
* **Troll:** regenerates 1 hit point on even turns when it moves.
* **Metamorph:** changes into a high-level monster when wounded below 25 in melee.
* **Mimic:** changing disguise (display only).
* **Invisible stalker / demons:** hidden without see invisible / the Eye of Larn.
* **Vampire:** will not step onto a mirror; starts awake when summoned.
* **Rothe, poltergeist:** start awake when created next to the player.
* **Rust monster, disenchantress, gelatinous cube:** dull the weapon that hits them.
* **Demon prince, Lucifer:** walk through walls and closed doors; ignore traps when
  path-finding.
* **Water lord:** never generated at random; appears from fountains.
* **Xorn and troll:** take 200 points of spell-type damage from vaporise rock (Part 07).
* **Disenchantress:** cancels (detonates) a sphere of annihilation that reaches her
  (Part 07).

## 11. Other monster helpers

### 11.1 Teleporting a monster

Used by the teleport trap, the nymph and the teleport-away spell. Up to 10 attempts:
x = `rnd(65)`, y = `rnd(15)`; the square must hold no object, no monster and not be the
player's square. On success the monster and its carried items are moved there, **its hit
points are reset to the full table value**, the place is recorded as where the move
ended, and the old square is redrawn. The new square is not redrawn and its stealth
flags are left as they were (the monster is not woken). If all 10 attempts fail the
monster stays where it is.

### 11.2 "Does the monster here carry item X"

Reports whether the monster on a square (if any) has a given item among its carried
items. The sphere code uses it for the talisman of the sphere: a monster that stole the
talisman is immune to spheres and sphere explosions (Part 07).

### 11.3 Polymorph (the polymorph spell)

After the spell's own checks and the resistance test (11.5), the target becomes a new
kind: roll `rnd(68)` (ids 1..68, so any ordinary monster, a demon lord or the demon
prince, never Lucifer), re-rolling while the result is genocided or a Wild id (57..60).
Its hit points become the new kind's table value; its carried items and stealth flags
are unchanged; the square is redrawn. If the target resisted it is still remembered as
the last-damaged monster.

### 11.4 Genocide

Genocide (the spell; Part 07 owns the casting rules and costs) works on the table's
genocided flag:

1. Prompt `\nEnter the number of the monster to genocide [* for list]: ` and read a
   number.
2. If the list is requested: switch to the text screen, print `Monster numbers` and list
   ids 1..60 in three columns 28 characters apart starting on row 3, each as the id
   left-justified in two columns, `) ` and the name (so the four Wild monsters are
   listed and can be chosen). The code intends to print `Genocided` in place of the name
   for genocided kinds, but it tests the wrong row, so in practice names are always
   shown. Then ask again.
3. If the number is 1..60 and that kind is not already genocided (or in wizard mode):
   set its genocided flag, print `\n  There will be no more %ss.` (the name with an "s"
   appended), then re-enter the current level, which removes every monster of a
   genocided kind from it and, as on any return visit, adds `level/2 + 1` fresh monsters
   and forgets the last-damaged monster. Done.
4. If the number is 1..60 but the kind is already genocided (not wizard): ask again.
5. Any other number: print `\n  You sense failure!` and finish.

Effects of the flag elsewhere: the selection and creation routines skip genocided kinds
(3.2, 4.4); every arrival on a level removes monsters of genocided kinds; polymorph
never produces one. Demons cannot be genocided. Placement by explicit id (4.5), the
lemming override and the metamorph's transformation ignore the flag, so a genocided kind
can still appear on the current level and lasts until the level is next entered.

### 11.5 Spell resistance by monster

Spells that target monsters consult a monster-by-spell table before taking effect. If
the entry is non-zero the spell fails on that monster, a new line and the entry's
message are printed, and the monster is remembered as the last-damaged monster (so it
comes for the player). The messages:

| Code | Message |
|---|---|
| 1 | `the web had no effect on the %s` |
| 2 | `the %s changed shape to avoid the web` |
| 3 | `the %s isn't afraid of you` |
| 4 | `the %s isn't affected` |
| 5 | `the %s can see you with his infravision` |
| 6 | `the %s vaporizes your missile` |
| 7 | `your missile bounces off the %s` |
| 8 | `the %s doesn't sleep` |
| 9 | `the %s resists` |
| 10 | `the %s can't hear the noise` |
| 11 | `the %s's tail cuts it free of the web` |
| 12 | `the %s burns through the web` |
| 13 | `your missiles pass right through the %s` |
| 14 | `the %s sees through your illusions` |
| 15 | `the %s loves the cold!` |
| 16 | `the %s loves the water!` |
| 17 | `the demon is terrified of the %s!` |
| 18 | `the %s loves fire and lightning!` |

The table, listing only non-zero entries (spell abbreviations: MLE magic missile, SLE
sleep, CHM charm monster, SSP sonic spear, WEB web, PHA phantasmal forces, INV
invisibility, BAL fireball, CLD cone of cold, PLY polymorph, CKL cloud kill, DRY
dehydration, LIT lightning, DRL drain life, FLO flood, FGR finger of death, SCA scare
monster, HLD hold monster, TEL teleport away, MFI magic fire, MKW make wall, GEN
genocide):

| Monster | Resistances (spell = message code) |
|---|---|
| gnome, kobold, bugbear, forvalaka | INV=5 |
| floating eye | SLE=8, SSP=10, FGR=4 |
| zombie | SLE=8, SSP=4, PHA=4, CKL=4, DRY=4, DRL=4, FGR=4 |
| hell hound | MLE=6, WEB=12 |
| ice lizard | WEB=11, CLD=15 |
| troll | MLE=7, DRY=4 |
| yeti | CLD=15 |
| white dragon | PHA=14, CLD=15, DRY=4 |
| elf | PHA=14, INV=5 |
| gelatinous cube | MLE=13, SLE=8, SSP=10, WEB=2, CKL=4, FGR=4 |
| metamorph | MLE=13, WEB=2, CKL=4, DRY=4, FGR=4 |
| vortex | MLE=13, SSP=10, WEB=1, CKL=4, DRY=4, FLO=4, FGR=4 |
| violet fungus | SLE=8, FGR=4 |
| wraith | MLE=13, SLE=8, SSP=4, PHA=14, CKL=4, DRY=4, DRL=4, FGR=4 |
| rothe, umber hulk | MLE=7, INV=5 |
| xorn | MLE=7, SLE=8, INV=5, DRY=4, FLO=4, FGR=4 |
| vampire | SSP=4, WEB=2, PHA=14, CKL=4, DRL=4, FGR=4 |
| invisible stalker | WEB=1 |
| poltergeist | MLE=13, SLE=8, SSP=4, WEB=1, CLD=4, CKL=4, DRY=4, DRL=4, FLO=4, FGR=4 |
| disenchantress | SLE=8 |
| shambling mound | SSP=10 |
| yellow mold | SLE=8, SSP=10, WEB=1, CKL=4, FGR=4 |
| gnome king | MLE=7, CHM=3, INV=5, PLY=9 |
| mimic | WEB=2 |
| water lord | MLE=13, SLE=8, CHM=3, SSP=4, WEB=1, PLY=9, CKL=4, FLO=16, FGR=4 |
| bronze dragon | MLE=7 |
| green dragon | MLE=7, WEB=11 |
| xvart | MLE=13, FLO=4, FGR=4 |
| spirit naga | MLE=13, SLE=8, CHM=3, SSP=4, WEB=1, PHA=14, INV=5, CLD=4, PLY=9, CKL=4, DRY=4, DRL=4, FLO=4, FGR=4 |
| silver dragon | MLE=6, SLE=9, WEB=12, PLY=9 |
| platinum dragon | MLE=7, SLE=9, WEB=11, PHA=14, PLY=9 |
| red dragon | MLE=6, WEB=12 |
| type I–V demon lords | MLE=13, SLE=8, CHM=3, SSP=10, WEB=1, PHA=14, INV=5, PLY=9, CKL=4, DRY=4, DRL=9, FLO=4, FGR=4, SCA=3, TEL=9, MFI=4, MKW=9 |
| type VI and VII demon lords | as type I–V except PLY=4 and no DRL entry (drain life works on them) |
| demon prince | as type I–V except PLY=4 and DRL=4 |
| Lucifer | MLE=13, SLE=8, CHM=3, SSP=10, WEB=1, PHA=14, INV=5, BAL=18, PLY=9, CKL=4, DRY=4, LIT=18, DRL=4, FGR=4, SCA=4, HLD=4, TEL=9, MFI=4, MKW=9, GEN=17 (no FLO entry: flood works on Lucifer) |

Every monster not listed (including the lemming, all the other ordinary monsters and the
four Wild monsters) resists nothing. Only the spells that actually test the table matter:
sleep, web, phantasmal forces, dehydration, drain life, finger of death and summon demon
(single adjacent target); magic missile, sonic spear, fireball, cone of cold and
lightning (rays; the scroll of pulverization's 150-point ray is cast as lightning and
uses the lightning column); cloud kill,
flood and magic fire (all adjacent squares); teleport away; polymorph. The entries under
charm, invisibility, scare, hold, make wall and genocide are never consulted.

Two further demon rules sit in the ray code (Part 07): a ray that reaches Lucifer always
bounces back toward the caster, and a ray that reaches any other demon bounces back with
probability 9 in 100, printing a blank line and then
`\nthe %s returns your puny missile!`. The scroll of annihilation removes every adjacent
non-demon (awarding experience and printing `\nYou hear loud screams of agony!`) but
only halves a demon's hit points (new = old / 2 + 1), printing
`\nThe %s barely escapes being annihilated!`.

### 11.6 Spell and missile damage

Spells do not have their own damage routine: once the resistance test passes they call
the damage routine of 9.4 with the "spell" marker, so they wake the monster, break hold
monster, are halved by the sickness curse, are tripled against dragons by the orb, never
behead, are never replaced by the lance or Slayer amounts, and kill, award experience
and drop loot exactly like a weapon blow. The sleep and web spells do not put a monster
to sleep or immobilise it: they deal 2 to 4 (sleep) or 3 to 5 (web) "full hits" (9.2) as
immediate damage.

## 12. Timed effects and items: summary of what each does to monsters

| Effect | What it changes |
|---|---|
| Time stop | No monster pass, no sphere movement, no random monster countdown; the player cannot melee. |
| Haste self | The monster pass is skipped whenever the timer is even, i.e. every other turn. |
| Haste monster | The monster pass runs twice per turn. |
| Hold monster | No monster moves or attacks (spheres still move). Broken by damaging any monster. |
| Stealth | Only monsters flagged awake act (unless aggravated). |
| Aggravate monsters | Activity window 21 × 11 instead of 11 × 7, path depth 40 instead of 17, and sleeping monsters act even if the player is stealthy. |
| Scare monster / hand of fear | Random staggering instead of pursuit, as in 7.1. |
| Charm monster | Non-demon, non-platinum-dragon attackers may be "awestruck" and skip their attack (8.1 step 7). No effect on movement. |
| Invisibility | Monsters without infravision miss 19 times in 33. No effect on movement: they still home in. |
| Cancellation | Blocks special attacks (8.2). |
| Fire resistance | Changes the fire-breath message only. |
| Spirit / undead protection | Halve damage from spirits / undead; undead protection also cancels undead special attacks. |
| See invisible | Shows invisible stalkers. |
| Blindness | No monsters are drawn; all monsters are called "monster"; a death is credited to an "unseen attacker"; trap messages for monsters are suppressed. |
| Slow (flag S) | Hobgoblin, troglodyte, ice lizard, metamorph, invisible stalker, xvart and roomba act only on even game-time. |

## 13. Quirks that are part of the behaviour

1. Fire resistance does not reduce fire-breath damage (8.4).
2. The hand of fear alone works 40% of the time although the source comment says 60%.
3. The type I demon lord can be frightened like an ordinary monster; types II and above
   need both the hand and the spell.
4. A frightened monster next to the player can still attack (one chance in nine per
   move).
5. The Vorpal Blade never beheads when it sits in the first pack slot.
6. Teleporting a monster heals it fully.
7. The leprechaun does not teleport after stealing: it ceases to exist, with the gold.
8. Slayer does 10000 per hit to demons but Lucifer has 32767 hit points.
9. A weapon that "disintegrates" does so after "You hit the ..." has been printed, and
   that blow does no damage.
10. Creating a monster does not clear the square's carried-item count, and the removal
    paths other than the normal kill and the sphere do not clear it either (leprechaun
    vanishing, pit, trapdoor, elevator, trap death). A monster later *created* on such a
    square inherits the old carried items and drops them when killed; a monster that
    merely moves onto it does not.
11. The stolen-item counter of a level doubles as its "visited" marker, which is why it
    is never allowed to fall below 1 (Part 05).
12. The last-damaged monster's remembered position can become stale (section 6 step 7).
13. A smart monster standing on a square it would avoid, or with all downhill neighbours
    occupied, stands still; a dumb monster never stands still if any forward square is
    free.
14. Random level-appropriate selection can return the type I demon lord once enough
    high-level kinds are genocided, and "selection plus 8" (create monster spell, altar
    desecration) can then produce Lucifer; with the Wild table layout the same "plus 8"
    can produce the four Wild monsters.
15. Lemmings are immune to genocide in practice: the 9% override ignores the flag, and
    such lemmings last until the level is re-entered.
16. Sleeping matters only under stealth; after a save and reload, or on first arrival,
    all monsters are asleep, but they act anyway unless the player is stealthy.
17. If no armour is worn when a rust monster's attack finds nothing to rust, the choice
    of message reads an unset value; the intended and practical result is the plain
    `\nThe %s hit you.`

## 14. The monster block of the save file

Within the save file (Part 03 gives the overall layout and checksumming) the monster
module writes, in this order:

1. one byte: the random-monster countdown;
2. 61 bytes, one for each id 0..60 in order: 1 if that kind is genocided, else 0.

On loading, each non-zero byte sets the genocided flag of that kind (flags are never
cleared by loading; the table starts with none set in every process). Demons have no
entry. The mimic's disguise, the invisible stalker's map character, the last-damaged
position, the stealth flags and the scaled table values are not saved; scaling is
reapplied from the saved difficulty at start-up.

## 15. Every message printed by the monster module

Rust: `\nThe %s hit you -- you are lucky you have leather on.` ·
`\nThe %s hit you -- you are fortunate to have stainless steel armor!` ·
`\nThe %s hit you -- you are very lucky to have such strong elven chain!` ·
`\nThe %s hit you.` · `\nThe %s hit you -- your armor feels weaker.`

Special attacks: `\nThe %s's flame doesn't faze you!` · `\nThe %s breathes fire at you!`
· `\nThe %s stung you!  You feel weaker.` · `\nThe %s stung you!` ·
`\nThe %s blasts you with his cold breath.` · `\nThe %s drains you of your life energy!`
· `\nThe %s got you with a gusher!` · `\nThe %s hit you.  Your purse feels lighter.` ·
`\nThe %s couldn't find any gold to steal.` ·
`\nThe %s hits you with a spell of disenchantment! ` · `\nThe %s nearly misses.` ·
`\nThe %s hit you with its barbed tail.` · `\nThe %s has confused you.` ·
`\nThe %s flattens you with it's psionics!` ·
`\nThe %s couldn't find anything to steal.` · `\nThe %s picks your pocket and takes:` ·
`  nothing` · `\nThe %s bit you!`

Monster movement: `\nThe %s dispels the sphere!` ·
`\nThe %s is destroyed by the sphere of annihilation!` · `\nThe %s gets teleported.` ·
`\nThe %s fell into a pit.` · `\nThe %s fell through a trapdoor.` ·
`\nThe %s is carried away by an elevator!` · `\nAn arrow hits and kills the %s.` ·
`\nAn arrow hits the %s.` · `\nA dart hits and kills the %s.` · `\nA dart hits the %s.`

Creation error: `\ncan't createmonst(%d)\n`

Player attacking: `\nYou hit` · `\nYou missed` · ` the %s.` ·
`\nYour weapon is dulled by the %s.` · `\nYour weapon disintegrates!` ·
`\nYour lance of death tickles the %s!` · `\nThe Vorpal Blade beheads the %s!` ·
`\nThe %s died!`

Monster attacking: `\nThe %s misses you!` (Wild dodge) · `\nThe %s misses wildly!` ·
`\nThe %s is awestruck by your magnificence!` · `\n  The %s hit you.` ·
`\n  The %s missed.`

Name substitutes: `monster` (when blind) and the table names, including
`unseen attacker` and `God of Hellfire`.

Messages printed by neighbouring modules on behalf of monsters and quoted above:
`\nAlas, you have died.\n`, `\nWelcome to level %d!`, `\nYou went down to level %d!`
(Part 02); the genocide prompts, the resistance messages, the ray-return and
annihilation lines (Part 07).
