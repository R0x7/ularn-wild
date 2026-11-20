# Part 06 — Objects: item table, dungeon features, traps, potions, scrolls, books, chests

This part covers everything that can occupy a map cell other than a monster: the complete object table, what happens when the player stands on each kind of object (prompts, choices, outcomes, every number), all traps and how they are hidden, and the full rules for potions, scrolls, books, chests, cookies, the brass lamp, drugs and gems. It leans on Part 02 (the player record, timers, inventory, armour and weapon numbers), Part 03 (how item names are printed, death and scoring), Part 04 (monsters and how they are created), Part 05 (where objects are placed when a level is built), Part 07 (spells, the directional bolt, spheres), Part 08 (what happens inside buildings, prices), Part 09 (the Android context button that answers these prompts), Part 12 (Wild Edition objects) and Part 14 (tile art). Where a routine belongs to another part but an object needs it to be understood, it is summarised here and the owner is named.

## 1. Conventions used in this part

### 1.1 Random helpers

- **rnd(n)** is a uniformly distributed whole number from 1 to n inclusive (the C library random number modulo n, plus 1).
- **rund(n)** is a uniformly distributed whole number from 0 to n−1 inclusive.

Both are textual macros, so wherever one appears in the *condition* of a loop it is rolled again on every pass. This matters in two places below (gems from a throne, items from a chest), which use "repeat while the pass counter is less than rnd(4)". The resulting number of passes is 1 with probability 1/4, 2 with 3/8, 3 with 9/32 and 4 with 3/32 (never 0, never more than 4). This is referred to below as the **re-rolled rnd(4) count**.

Tests are written exactly as coded, for example "rnd(101) < 40", which is true for 39 of 101 equally likely values.

### 1.2 Map, levels and cells

- A level is 67 columns (x = 0..66) by 17 rows (y = 0..16).
- Level 0 is the town. Levels 1..15 are the dungeon; level 15 is the dungeon bottom. Levels 16..20 are the volcano (shown as V1..V5); level 20 is the volcano bottom.
- Every cell holds one **object id** (a single byte, from the table in section 2), one signed 16‑bit **argument** whose meaning depends on the object, a **known** object id (what the player believes is there, drawn on the map), and optionally a monster. A cell can hold only one object.
- "Remove the object" below means: set the cell's object to "nothing" and also set the known object to "nothing".

### 1.3 Player numbers and timers

Ability scores are, in this fixed order, strength, intelligence, wisdom, constitution, dexterity, charisma. "Adjust an ability by n, floor 3" means add n and then raise the result to 3 if it is lower. Timers such as blindness, confusion, haste‑self, haste‑monsters, aggravate, half‑damage ("sickness"), itching, clumsiness, awareness, giant strength, heroism, altar protection and the cocaine timer are counters that tick down by one per game turn; what they do while running is Part 02 and Part 04. Only the amounts added and the expiry side effects that belong to objects are stated here.

"Raise the player one experience level" means: if the player is below level 100, grant exactly enough experience to reach the next level threshold (Part 02 describes what a level‑up gives).

"Lose n hit points" means subtract n from current hit points and, if the result is zero or less, the player dies with the stated cause (section 1.5). "Raise maximum hit points by n" raises both maximum and current hit points by n. "Lower maximum hit points by n" lowers both, each with a floor of 1.

### 1.4 Messages and prompts

Message strings are quoted exactly. Inside a quoted string, `\n` stands for a line break in the message window and `N` or `%s` style placeholders are explained next to it. `[s]` means the letter "s" is printed unless the number is exactly 1 (so 0 and negative numbers take the "s"). Many messages start with one or two line breaks and some end with one; these are part of the string.

A **prompt** prints its text and then waits for one key from a fixed set of accepted keys. Any other key is ignored (the game keeps waiting). Escape is accepted only where it is listed. After the key, the game usually echoes a word such as `take.`; these echoes are also quoted exactly, including the inconsistent spacing and punctuation of the original.

The shared tail `, or (i) ignore it? ` and the shared reply `ignore.\n` are used by many prompts. Where a prompt is said to be built from pieces, the pieces are printed one after another on the same line.

"Pause n ms" means flush the display and sleep for n milliseconds. "Beep" is the alert sound.

### 1.5 Deaths caused by objects, and life protection

The causes of death that originate in this part, with the text shown on the scoreboard:

| Cause | Scoreboard text | Can a life‑protection charge save the player? |
|---|---|---|
| Arrow trap | "shot by an arrow" | yes |
| Dart trap | "hit by a dart" | yes |
| Pit (ordinary) | "fell into a pit" | yes |
| Pit on a bottom level | "fell into a pit to HELL" | no |
| Trap door (ordinary) | "fell through a trap door" | yes |
| Trap door on a bottom level | "fell through a trap door to HELL" | no |
| Elevator down on a bottom level | "took an elevator straight to HELL" | no |
| Fountain water (drink or wash) | "drank some poisonous water" | yes |
| Fountain dreadful sleep | "fell into the dreadful sleep" | yes |
| Trapped door shock | "fried by an electric shock" | yes |
| Volcanic shaft | "slipped in a volcano shaft" | yes |
| Exploding chest | "killed by an exploding chest" | yes |
| Brass lamp genie | "wasted by an annoyed genie" | yes |
| Sphere of annihilation | "annihilated by a sphere" | yes |
| Teleport into rock | "trapped in solid rock" | yes |

A life‑protection charge (granted by the scroll of life protection, section 15) is consumed at the moment of a saveable death: the charge count drops by one, hit points are restored to maximum, constitution drops by one, the message `\nYou feel wiiieeeeerrrrrd all over! ` is printed, there is a beep and a 4000 ms pause, and the routine that killed the player simply continues from where it was (for example a fall still moves the player to the next level). Death itself is Part 03.

## 2. The object table

The table is in id order. Ids are stored in save files and in map cells, and the Wild Edition ids must stay at the end. The **glyph** is the legacy text‑mode character; the Android front end never draws it and always uses the **tile** number (Part 14; tile 191 is plain floor, tile 175 is the "unexplored" tile). The **name** is the description string used in "You find …" messages and inventory lines (the two markers have empty names).

Kind codes: **M** marker, **F** fixed dungeon feature (cannot be carried), **G** gold (collected automatically, never carried as an item), **I** item that can be carried in the pack, **B** building or entrance (fixed), **T** trap (fixed), **W** Wild Edition object (Part 12 says which of these are carried and which are fixed).

| Id | Internal name | Glyph | Tile | Name string | Kind | Meaning of the cell argument |
|---|---|---|---|---|---|---|
| 0 | unknown | space | 175 | (empty) | M | — (only ever used as a "known" value: cell not yet seen) |
| 1 | nothing | space | 191 | (empty) | M | — (empty floor) |
| 2 | altar | A | 64 | a holy altar | F | unused |
| 3 | throne | T | 65 | a handsome, jewel-encrusted throne | F | unused |
| 4 | throne, king already summoned | T | 142 | a handsome, jewel-encrusted throne | F | unused |
| 5 | dead throne (jewels gone) | T | 144 | a massive throne | F | unused |
| 6 | pit | P | 67 | a pit | T | unused |
| 7 | stairs up | % | 68 | a staircase leading upwards | F | unused |
| 8 | elevator up | ~ | 69 | an elevator going up | F | unused |
| 9 | fountain | F | 70 | a bubbling fountain | F | unused |
| 10 | statue | & | 71 | a great marble statue | F | unused |
| 11 | teleport trap (visible) | ^ | 72 | a teleport trap | T | unused |
| 12 | mirror | = | 74 | a mirror | F | unused |
| 13 | stairs down | % | 76 | a staircase going down | F | unused |
| 14 | elevator down | ~ | 77 | an elevator going down | F | unused |
| 15 | dead fountain | f | 80 | a dead fountain | F | unused |
| 16 | open door | O | 82 | an open door | F | unused |
| 17 | closed door | D | 83 | a closed door | F | door trap code (section 7.1) |
| 18 | wall | # | 84 | a wall | F | wall‑shape variant used only for drawing (Parts 05 and 14) |
| 19 | gold pile | * | 81 | gold | G | amount in gold pieces |
| 20 | gold pile ×10 | * | 133 | gold | G | amount divided by 10 |
| 21 | gold pile ×1000 | * | 134 | gold | G | amount divided by 1000 |
| 22 | gold pile ×100 | * | 135 | gold | G | amount divided by 100 |
| 23 | Eye of Larn | o | 85 | The Eye of Larn | I | unused (0) |
| 24 | plate mail | [ | 86 | plate mail | I | enchantment (+n / −n) |
| 25 | chain mail | [ | 87 | chain mail | I | enchantment |
| 26 | leather armor | [ | 88 | leather armor | I | enchantment |
| 27 | ring mail | [ | 123 | ring mail | I | enchantment |
| 28 | studded leather armor | [ | 124 | studded leather armor | I | enchantment |
| 29 | splint mail | [ | 125 | splint mail | I | enchantment |
| 30 | plate armor | [ | 126 | plate armor | I | enchantment |
| 31 | stainless plate armor | [ | 127 | stainless plate armor | I | enchantment |
| 32 | shield | [ | 131 | a shield | I | enchantment |
| 33 | elven chain | [ | 155 | elven chain | I | enchantment |
| 34 | sword of slashing | ( | 89 | a sword of slashing | I | enchantment |
| 35 | Bessman's flailing hammer | ( | 90 | Bessman's flailing hammer | I | enchantment |
| 36 | sunsword | ( | 91 | a sunsword | I | enchantment |
| 37 | two‑handed sword | ( | 92 | a two-handed sword | I | enchantment |
| 38 | spear | ( | 93 | a spear | I | enchantment |
| 39 | dagger | ( | 94 | a dagger | I | enchantment |
| 40 | battle axe | ( | 120 | a battle axe | I | enchantment |
| 41 | longsword | ( | 121 | a longsword | I | enchantment |
| 42 | flail | ( | 122 | a flail | I | enchantment |
| 43 | lance of death | ( | 128 | a lance of death | I | enchantment |
| 44 | Vorpal Blade | ( | 153 | the Vorpal Blade | I | enchantment |
| 45 | Slayer | ( | 154 | Slayer | I | enchantment |
| 46 | ring of extra regeneration | = | 95 | a ring of extra regeneration | I | enchantment |
| 47 | ring of regeneration | = | 96 | a ring of regeneration | I | enchantment |
| 48 | ring of protection | = | 97 | a ring of protection | I | enchantment |
| 49 | energy ring | = | 98 | an energy ring | I | enchantment |
| 50 | ring of dexterity | = | 99 | a ring of dexterity | I | enchantment |
| 51 | ring of strength | = | 100 | a ring of strength | I | enchantment |
| 52 | ring of cleverness | = | 101 | a ring of cleverness | I | enchantment |
| 53 | ring of increase damage | = | 102 | a ring of increase damage | I | enchantment |
| 54 | belt of striking | ) | 103 | a belt of striking | I | enchantment |
| 55 | scroll | ? | 104 | a magic scroll | I | scroll type id (section 15) |
| 56 | potion | ! | 105 | a magic potion | I | potion type id (section 14) |
| 57 | book | B | 106 | a book | I | level the book was made for (section 16) |
| 58 | chest | C | 107 | a chest | I | level the chest was made for (section 11) |
| 59 | orb of enlightenment | o | 66 | an orb of enlightenment | I | unused |
| 60 | amulet of invisibility | " | 108 | an amulet of invisibility | I | enchantment |
| 61 | orb of dragon slaying | o | 109 | an orb of dragon slaying | I | unused |
| 62 | scarab of negate spirit | . | 110 | a scarab of negate spirit | I | unused |
| 63 | cube of undead control | . | 111 | a cube of undead control | I | unused |
| 64 | device of theft prevention | . | 112 | a device of theft-prevention | I | unused |
| 65 | brass lamp | . | 148 | a brass lamp | I | unused |
| 66 | Hand of Fear | . | 149 | The Hand of Fear | I | unused |
| 67 | Talisman of the Sphere | . | 150 | The Talisman of the Sphere | I | unused |
| 68 | wand of wonder | / | 151 | a wand of wonder | I | unused |
| 69 | staff of power | / | 152 | a staff of power | I | enchantment |
| 70 | amulet of life preservation | " | 162 | an amulet of life preservation | I | unused |
| 71 | diamond | < | 113 | a brilliant diamond | I | gem size (value number) |
| 72 | ruby | < | 114 | a ruby | I | gem size |
| 73 | emerald | < | 115 | an enchanting emerald | I | gem size |
| 74 | sapphire | < | 116 | a sparkling sapphire | I | gem size |
| 75 | College of Larn | M | 73 | the College of Larn | B | unused |
| 76 | DND store | S | 75 | the DND store | B | unused |
| 77 | bank, 8th branch | $ | 79 | the 8th branch of the Bank of Larn | B | unused |
| 78 | bank | $ | 78 | the Bank of Larn | B | unused |
| 79 | dungeon entrance | E | 117 | the dungeon entrance | B | unused |
| 80 | volcanic shaft down | V | 118 | a volcanic shaft leaning downward | B | unused |
| 81 | volcanic shaft up | V | 119 | the base of a volcanic shaft | B | unused |
| 82 | home | H | 132 | your home | B | unused |
| 83 | trading post | T | 140 | the local trading post | B | unused |
| 84 | Larn Revenue Service | L | 145 | the Larn Revenue Service | B | unused |
| 85 | Dealer McDope's Pad | P | 161 | Dealer McDope's Pad | B | unused |
| 86 | arrow trap, hidden | space | 191 | an arrow trap | T | unused |
| 87 | arrow trap, visible | ^ | 129 | an arrow trap | T | unused |
| 88 | dart trap, hidden | space | 191 | a dart trap | T | unused |
| 89 | dart trap, visible | ^ | 136 | a dart trap | T | unused |
| 90 | trap door, visible | ^ | 138 | a trapdoor | T | unused |
| 91 | trap door, hidden | space | 191 | a trapdoor | T | unused |
| 92 | teleport trap, hidden | space | 191 | a teleport trap | T | unused |
| 93 | sphere of annihilation | s | 143 | a sphere of annihilation | T | unused (spheres are tracked separately, Part 07) |
| 94 | fortune cookie | c | 146 | a fortune cookie | I | unused |
| 95 | golden urn | u | 147 | a golden urn | I | unused; never generated anywhere, has no special behaviour |
| 96 | speed | : | 156 | some speed | I | unused |
| 97 | LSD | : | 157 | some LSD | I | unused |
| 98 | hashish | : | 158 | some hashish | I | unused |
| 99 | magic mushrooms | : | 159 | some magic mushrooms | I | unused |
| 100 | cocaine | : | 160 | some cocaine | I | unused |
| 101 | vending machine | & | 368 | a humming vending machine | W | Part 12 |
| 102 | can of Fizz | ! | 369 | a can of Fizz | W | Part 12 |
| 103 | slice of pizza | ! | 370 | a slice of pizza | W | Part 12 |
| 104 | energy drink | ! | 371 | an energy drink | W | Part 12 |
| 105 | rubber duck | * | 372 | a rubber duck | W | Part 12 |
| 106 | traffic cone (hat) | [ | 373 | a traffic cone | W | Part 12 |
| 107 | propeller beanie (hat) | [ | 374 | a propeller beanie | W | Part 12 |
| 108 | wizard hat | [ | 375 | a pointy wizard hat | W | Part 12 |
| 109 | party hat | [ | 376 | a party hat | W | Part 12 |
| 110 | tinfoil hat | [ | 377 | a tinfoil hat | W | Part 12 |
| 111 | mysterious egg | * | 378 | a mysterious egg | W | Part 12 |
| 112 | fishing rod | * | 379 | a fishing rod | W | Part 12 |
| 113 | fish | * | 380 | a fish | W | Part 12 |
| 114 | old boot | * | 381 | an old boot | W | Part 12 |
| 115 | message in a bottle | * | 382 | a message in a bottle | W | Part 12 |
| 116 | cassette tape | * | 383 | a cassette tape | W | Part 12 |
| 117 | LarnCoin ATM | & | 388 | a LarnCoin ATM | W | Part 12 |

The count of objects is 118 (ids 0..117). "First Wild object" is id 101; every id from 101 upward is routed to the Wild Edition handler when stood on (section 17).

Notes on the table:

- The four gold piles differ only in the multiplier applied to the argument: ×1 (id 19), ×10 (id 20), ×1000 (id 21), ×100 (id 22). Note the id order is not the multiplier order.
- Thrones 3 and 4 look identical to the player in text (same name) but have different tiles (65 and 142).
- The four hidden traps have the same tile and glyph as empty floor. Their name strings are the same as their visible partners.
- The object table itself has only these columns (glyph, tile, name). Armour class, weapon class, weight and prices live in other tables (Parts 02 and 08); section 13 summarises them for convenience.

## 3. When objects act: the "look for object" step

There is no "pick up", "climb" or "enter" command. Instead, at the **start of every pass of the main turn loop**, before the monsters move and before the player's next command is read, the game runs a "look for object" step on the cell the player is standing in. Consequences:

- Stepping onto an object produces its message or prompt immediately after the move.
- Staying on the same cell and spending another turn (resting, casting, reading, etc.) runs the step again: the prompt is asked again, a visible arrow trap shoots again, a hidden trap gets another chance to trigger, and so on.
- The step is skipped for exactly one turn when the "just dropped" flag is set. The flag is set by: dropping an item or gold, the open‑door command, the close‑door command, the open‑chest command, and the wizard create‑object command. It is cleared by the skipped step.

The step itself:

1. If the time‑stop timer is running, do nothing at all.
2. If the cell holds "nothing", do nothing.
3. Reveal the cells around the player as for a normal move (Part 01/09: radius 1, radius 3 with awareness, only the player's own cell when blind) and cancel any pending command repeat count.
4. Act according to the object, as described in sections 5 to 12 and 17.

**Adjacent‑monster rule.** For the following objects the step silently does nothing (no message, no prompt) if any monster occupies any cell of the 3×3 block centred on the player (clipped at the map edge): altar, both live thrones (ids 3 and 4), orb of enlightenment, wand of wonder, Hand of Fear, fountain, dead fountain, statue, mirror, College, both banks, DND store, trading post, home, Dealer McDope's Pad, Larn Revenue Service. All other objects act even with monsters adjacent (note the dead throne, id 5, is *not* in the list).

Movement interplay (Part 01 owns movement): walking onto empty floor or onto any of the four hidden traps does not interrupt a run or a repeat count; walking onto any other object does. Walls block movement unless the walk‑through‑walls timer is running. With the "enhanced interface" option on, the player cannot walk onto a closed door at all.

## 4. Picking things up (hand‑off to the inventory)

All "take" choices call the same routine (owned by Part 02):

1. The carry limit is 15 + (experience level ÷ 2, rounded down), capped at 26 slots.
2. The item goes into the lowest‑numbered free slot below the limit, keeping its argument. If there is none: `\nYou can't carry anything else.`, the take fails and the object stays on the floor.
3. On success, pick‑up side effects are applied (section 13.2), then `\nYou pick up:` is printed followed by the inventory line for the new slot (Part 03 gives the line format, for example a letter, the name, " of <type>" for identified potions and scrolls, and "+n" for enchanted gear).
4. If the item was the Eye of Larn and the player is not blind: `\nYour sight fades for a moment...`, pause 2000 ms, full redraw, `\nYour sight returns, and everything looks crystal-clear!`.
5. Wild Edition hook: a "took item" notification, see Part 12.

When a take succeeds the floor object is removed.

## 5. The generic "find item" prompt

Used for every carryable object that has no special handling: the Eye of Larn, all armour, weapons, rings, the belt, both amulets, both orbs, scarab, cube, device, Hand of Fear, Talisman, wand of wonder, staff of power, gems, the golden urn, and Wild objects when their feature does not claim them.

1. Print `\n\nYou find ` followed by the object's name string.
2. For gems, the scarab, the orb of dragon slaying, the orb of enlightenment, the Hand of Fear, the wand of wonder, the cube and the device: print `.` and never show the argument.
3. For everything else: if the argument is positive print ` + N`; if negative print ` N` (which shows as, for example, ` -2`); if zero print nothing (so there is no full stop).
4. Print `\nDo you want to (t) take it` and the shared tail `, or (i) ignore it? `. Accepted keys: i, t, Escape.
5. On t: print `take.` and take the item (section 4). On anything else: print `ignore.\n`.

Three of these items (orb of enlightenment, wand of wonder, Hand of Fear) are subject to the adjacent‑monster rule; the rest are not.

## 6. Gold, stairs, entrances, shafts, elevators

### 6.1 Gold piles (ids 19–22)

Automatic, no prompt: amount = argument × 1, 10, 100 or 1000 according to the pile type (section 2). Print `\nYou find N gold piece[s].`, add the amount to the player's gold, refresh the status line and set the cell to "nothing". Wild Edition hook: a "gold gained" event, see Part 12.

Piles are created in the ×1 form up to 32,767 pieces and in a larger form above that (dropping gold, Part 02: up to 327,670 uses ×10, up to 3,276,700 uses ×100, above that ×1000 with the argument capped at 32,767; amounts are rounded down to the multiplier). Gold "dropped" by the game itself is described in section 16.4.

### 6.2 Staircases (ids 7 and 13)

Message `\n\nThere is a circular staircase here.` Then `\nDo you (s) stay here  or  ` followed by `(u) go up?  ` on an up staircase or `(d) go down?  ` on a down staircase. Accepted keys: s, u, d, i, Escape (both u and d are always accepted).

- s, i or Escape: `stay here.`
- u: `go up.` If this is a down staircase: `\nThe stairs don't go up!`. Otherwise, if the current level is 2 or more and is not 16 (V1): go to level − 1 and place the player on that level's down staircase. Otherwise: `\nThe stairs lead to a dead end!`.
- d: `go down.` If this is an up staircase: `\nThe stairs don't go down!`. Otherwise, if the current level is not 0, 15 or 20: go to level + 1 and place the player on that level's up staircase. Otherwise: `\nThe stairs lead to a dead end!`.

Placement on arrival: the level change itself first moves the player to the nearest empty, monster‑free cell to their old coordinates (Part 05); then the new level is scanned column by column (x from 0 upward, and within a column y from 0 upward) for the matching staircase and the player is put on the first one found. If a monster is standing on that staircase, the player is instead put on the nearest cell that has no object and no monster (nearest by straight‑line distance; ties go to the lowest x, then lowest y). The map is then fully redrawn.

### 6.3 Dungeon entrance (id 79, town only)

Print `\nYou have found ` + `the dungeon entrance`, then `\nDo you (g) go inside` + `, or (i) ignore it? `. Accepted keys: g, i, Escape.

- g: go to level 1, put the player at x = 33, y = 15, clear any object and any monster from the exit cell (x = 33, y = 16), redraw. (Walking onto that exit cell on level 1 returns the player to the town next to the entrance; Part 01/05.)
- otherwise: `ignore.\n`.

### 6.4 Volcanic shaft going down (id 80, town only)

Print `\nYou have found ` + `a volcanic shaft leaning downward`, then `\nDo you (c) climb down` + `, or (i) ignore it? `. Accepted keys: c, i, Escape.

- i or Escape: `ignore.\n`.
- c: if the current level is not 0: `\nThe shaft only extends 5 feet downward!` and nothing else. Otherwise compare the pack weight (section 13.3) with 45 + 3 × (strength + extra strength):
  - heavier: `\nYou slip and fall down the shaft.`, beep, lose 30 + rnd(20) hit points;
  - otherwise: `climb down.`
  
  In both cases (if still alive): pause 3000 ms, go to level 16 (V1), choose a random position x = rnd(65), y = rnd(15), move to the nearest free cell, redraw.

### 6.5 Volcanic shaft going up (id 81, on V1)

Print `\nYou have found ` + `the base of a volcanic shaft`, then `\nDo you (c) climb up` + `, or (i) ignore it? `. Accepted keys: c, i, Escape.

- i or Escape: `ignore.\n`.
- c: if pack weight is greater than 40 + 5 × (dexterity + strength + extra strength): `\nYou slip and fall down the shaft.`, beep, lose 15 + rnd(20) hit points, and the player stays where they are. Otherwise `climb up.`, pause 3000 ms, go to level 0, find the downward shaft (scanning row by row from the top, left to right), stand the player on it and then move them to the nearest free cell, redraw.

### 6.6 Elevators (ids 8 and 14)

There is no choice; standing on an elevator uses it.

**Up** (`\n\nYou have found an express elevator going up.`):

- On level 0: `,\nunfortunately, it is out of order.` and nothing happens.
- Otherwise: random position x = rnd(65), y = rnd(15); pause 2000 ms. In the dungeon (levels 1..15) the destination is rund(level), that is any level from 0 (the town) to one above the current, equally likely. In the volcano the destination is 15 + rund(level − 15); a result of 15 means the town (level 0).

**Down** (`\n\nYou have found an express elevator going down.`):

- On level 15 or 20: pause 2000 ms, `\nand it leads straight to HELL!`, beep, pause 3000 ms, death ("took an elevator straight to HELL").
- Otherwise: random position as above; pause 2000 ms. Below level 15 the destination is level + rnd(15 − level) (any deeper dungeon level down to 15). In the volcano it is level + rnd(20 − level).

After travelling the map is redrawn; the level change moves the player to the nearest free cell to the random position. Monsters that walk into an elevator are removed (Part 04).

## 7. Doors and walls

### 7.1 The door‑opening roll and door traps

Every attempt to open a closed door, by any route, does this:

1. If rnd(11) < 7 (6 chances in 11) the door stays shut and its trap, if any, goes off according to the door's argument:
   - 6: aggravate timer += rnd(400) (no message);
   - 7 or 8: `\nYou are jolted by an electric shock!`, lose rnd(20) hit points;
   - 9: `\nYou suddenly feel weaker!`, strength − 1 unless it is already 3 or less;
   - anything else: nothing.
   
   After any failed attempt the door's argument is set to 0, so a door trap fires at most once (on the first failed attempt) and is gone afterwards.
2. Otherwise (5 in 11) the door becomes an open door and the cell is redrawn.

Door arguments are assigned when the level is built (Part 05): doors read from the pre‑made maps get rnd(30); the door of an ordinary treasure room gets rnd(9); the door of the treasure rooms on levels 15 and 20 gets rnd(3) + 6 (always trapped: 7, 8 or 9). Closing a door by command always sets its argument to 0.

### 7.2 Walking onto a closed door (classic interface only)

1. Print `\n\nYou find a closed door`, then `\nDo you (o) try to open it` + `, or (i) ignore it? `. Accepted keys: o, i, Escape.
2. i or Escape: `ignore.\n`; the player is moved back to the cell they came from (and that door cell becomes their "previous position").
3. o: `open.`, then the roll of section 7.1. If the door is still closed the player is moved back exactly as in step 2; if it opened they remain standing in the doorway.

### 7.3 Door and chest commands

These commands belong to Part 01 but their messages concern doors:

- **Open door** (enhanced interface only; asks for a direction): if time is stopped `\nNothing can be moved while time is stopped!`. If the target is off the map or is neither kind of door: `\nYou see no door there.` (or `\nYou cannot feel any door there.` when blind). If it is an open door: `\nThat door is already open.`. If closed: make the roll of section 7.1, then `\nThe door opens.` or `\nThe door resists.` (any trap message comes first).
- **Close door**: if time is stopped, the same time‑stop message. Enhanced interface (asks for a direction): `\nThat door is already closed.`, or the "no door" messages as above, or the door becomes closed with argument 0 and `\nThe door closes.`. Classic interface: the player must be standing on an open door, otherwise `\nThere is no open door here.`; else `\nThe door closes.` and the cell under the player becomes a closed door with argument 0 (the next turn's look step is skipped, but the one after will ask whether to open it).
- **Open chest** (enhanced interface only): `\nThere isn't a chest to open here.` unless standing on a chest, in which case the chest‑opening roll of section 11 is made.

### 7.4 Open doors and walls

- Open door: `\nThere is an open door here.` every turn the player stands in it. Nothing else.
- Wall (only reachable while walking through walls): nothing, no message.
- A bolt spell of 40 or more remaining damage destroys a closed door (`  The door is blasted apart.`), Part 07.

## 8. Buildings

All building entrances obey the adjacent‑monster rule. Each prints a message, asks one question, and either calls the building's own screen (Part 08) or prints a short refusal. Unless noted the question is `\nDo you (g) go inside, or (i) stay here? ` with accepted keys g, i, Escape; g enters and anything else refuses.

| Object | Message | Refusal echo |
|---|---|---|
| College of Larn | `\n\nYou have found the College of Ularn.` | ` stay here.` |
| Bank of Larn | `\n\nYou have found the bank of Ularn.` | ` stay here.` |
| 8th branch of the bank (on dungeon level 8) | `\n\nYou have found a branch office of the bank of Ularn.` | ` stay here.` |
| DND store | `\n\nThere is a DND store here.` | ` stay here.` |
| Larn Revenue Service | `\n\nThere is an LRS office here.` | ` stay here.` |
| Trading post | `\nYou have found the Ularn trading Post.` | `stay here.` (no leading space) |
| Home | `\nYou have found your way home.` | `stay here.` (no leading space) |
| Dealer McDope's Pad | `\nYou have found Dealer McDope's Hideout!` | `forget it.` |

The Pad uses its own question, `\nDo you (c) check it out, or (i) ignore it? `, accepted keys c, i, Escape; c enters.

## 9. Altars, thrones, fountains and inert features

### 9.1 Altar (id 2)

Message `\n\nThere is a holy altar here.` then the prompt `\nDo you (p) pray  (d) desecrate, or (i) ignore it? `, accepted keys p, d, i, Escape.

**Ignore (i or Escape).** Echo ` ignore`. If rnd(100) < 30: a monster is created next to the player, of the kind appropriate to (current level + 2) (Part 04's level‑based monster picker), and the aggravate timer gains rnd(450). Otherwise `\nNothing happens.`

**Desecrate (d).** Echo ` desecrate`. Rolls are made in sequence, each only if the previous failed:

1. rnd(100) < 60: create a monster whose id is 8 higher than the one the level‑based picker returns for (level + 3); aggravate += 2500.
2. rnd(100) < 5: raise the player one experience level.
3. rnd(101) < 30: `\nThe altar crumbles into a pile of dust before your eyes.` and the altar is removed.
4. Otherwise `\nNothing happens.`

Overall chances: 59% monster, about 1.6% level, about 11.3% crumble, about 28.1% nothing.

**Pray (p).** Echo ` pray`, then `\nDo you (m) give money or (j) just pray? `, accepted keys m and j only (Escape is not accepted).

*Just pray (j).* Roll p = rund(100):

| p | Chance | Result |
|---|---|---|
| 0–11 | 12% | a monster for (level + 2) appears next to the player |
| 12–16 | 5% | altar weapon enchantment (section 9.2) |
| 17–21 | 5% | altar armour enchantment (section 9.2) |
| 22–26 | 5% | altar protection (below) |
| 27–99 | 73% | `\nNothing happens.` |

*Give money (m).* Print `\nHow much do you donate? ` and read a number. Entering nothing, entering `*`, or pressing Escape all mean "everything I carry". Then:

1. A negative amount: the whole altar prompt starts again from "pray / desecrate / ignore".
2. More than the player has: ` You don't have that much!`, pause 1001 ms, and the altar prompt starts again.
3. Otherwise subtract the donation k from the player's gold, then judge it (R = gold remaining after the subtraction, so R + k is what the player had):
   - **Insult.** If k < R ÷ 10 (rounded down) and rnd(60) < 30 and the game is not in wizard mode: ` Cheapskate! The Gods are insulted by such a tiny offering!`, the altar is removed, a demon prince is created next to the player, aggravate += 1500, and k is subtracted from the player's gold a second time.
   - **Stingy.** Otherwise, if (k < (R + k) ÷ 10, or k < rnd(50)) and not wizard mode: a monster for (level + 2) appears, aggravate += 500, and k is subtracted a second time. There is no message. (The rnd(50) roll is only made when the first test fails.) Because of the second subtraction the player's gold can go negative here, for example by donating 30 of 40 gold and losing the rnd(50) roll.
   - **Accepted.** Otherwise roll p = rund(16): 0–3 (25%) ` Thank you.`; 4–5 (12.5%) altar armour enchantment twice; 6–7 (12.5%) altar weapon enchantment twice; 8–15 (50%) altar protection.
4. Refresh the status line.

A donation of 0 is always judged insulting or stingy (outside wizard mode). To be sure of acceptance the donation must be at least one tenth (rounded down) of the gold carried before donating and at least 50.

**Altar protection.** `\nYou have been heard!`. If the altar‑protection timer is currently 0, the player's bonus armour class gains 3. The timer then gains 800. When the timer runs out the bonus 3 is removed.

### 9.2 Enchanting armour and weapons (shared by altars and scrolls; owned by Part 02)

**Armour.**

1. If neither body armour nor a shield is in use: beep, `\nYou feel a sense of loss.`, nothing else.
2. Choose the shield slot or the body‑armour slot with equal chance; if the chosen slot is empty use the other.
3. If that item's enchantment is already 9 or more:
   - from an altar: `\nYour <name> glows briefly.` and nothing changes;
   - from a scroll: with probability 9/10, `\nYour <name> vibrates violently and crumbles into dust!` and the item is destroyed (its slot is emptied, it is no longer worn, armour class is recalculated); with probability 1/10 nothing happens and nothing is printed.
4. Otherwise `\nYour <name> glows for a moment.` and its enchantment rises by 1.

`<name>` is the item's name string from section 2 (for example "Your a shield glows for a moment.").

**Weapon.**

1. If nothing is wielded: beep, `\nYou feel depressed.`
2. If the wielded item is a scroll or a potion: nothing happens and nothing is printed.
3. If the enchantment is 9 or more and rnd(10) ≤ 9 (9 in 10):
   - from an altar: `\nYour weapon glows a little.` and nothing changes;
   - from a scroll: `\nYour weapon vibrates violently and crumbles into dust!` and the weapon is destroyed (its carried bonuses are removed, weapon class recalculated).
4. Otherwise (always below +9, and 1 time in 10 at +9 or above): `\nYour weapon glows for a moment.` and the enchantment rises by 1. So, unlike armour, a weapon can pass +9.

### 9.3 Thrones (ids 3, 4, 5)

**Live thrones.** Message `\n\nThere is a handsome, jewel-encrusted throne here.` then `\nDo you (p) pry off jewels, (s) sit down, or (i) ignore it? `, accepted keys p, s, i, Escape. "King not yet summoned" means the object is id 3; id 4 is the same throne after the gnome king has been called.

- **p**: echo ` pry off`; roll k = rnd(101).
  - k < 25 (24 in 101): gems pop off. Create a random gem (section 16.3) next to the player the re‑rolled rnd(4) count of times, and the throne becomes a dead throne (id 5).
  - else if k < 40 and the king has not been summoned (15 in 101): a gnome king is created next to the player and the throne becomes id 4.
  - else `\nNothing happens.`
- **s**: echo ` sit down`; roll k = rnd(101).
  - k < 30 and the king has not been summoned (29 in 101): gnome king, throne becomes id 4.
  - else if k < 35 (5 in 101 on an id‑3 throne; 34 in 101 on an id‑4 throne): `\nZaaaappp!  You've been teleported!\n`, beep, safe teleport (section 10.6).
  - else `\nNothing happens.`
- **i or Escape**: echo ` ignore`.

**Dead throne.** No adjacent‑monster check. Message `\n\nThere is a massive throne here.` then `\nDo you (s) sit down, or (i) ignore it? `, accepted keys s, i, Escape.

- **s**: echo ` sit down`; k = rnd(101). k < 5 (4 in 101): raise the player one experience level. Else k < 25 (20 in 101): `\nZaaaappp!  You've been teleported!\n`, beep, safe teleport. Else `\nNothing happens.`
- **i or Escape**: echo ` ignore`.

### 9.4 Fountain (id 9)

Message `\n\nThere is a fountain here.` then `\nDo you (d) drink, (w) wash yourself, or (i) ignore it? `, accepted keys d, w, i, Escape. Wild Edition hook: when the player carries a fishing rod and the fishing feature is on, the prompt is instead `\nDo you (d) drink, (w) wash yourself, (f) fish, or (i) ignore it? ` with f also accepted; f echoes `fish.` and runs the fishing routine (Part 12).

**Ignore**: echo ` ignore`.

**Drink**: echo `drink`.

1. If rnd(1501) < 4 (3 in 1501): `\nOH MY GOD!! You have caught the *dreadful sleep*!`, beep, pause 3000 ms, death ("fell into the dreadful sleep"); nothing further happens this visit even if a life‑protection charge saves the player.
2. Otherwise roll x = rnd(100):

| x | Chance | Result |
|---|---|---|
| 1 | 1% | raise the player one experience level |
| 2–10 | 9% | damage d = rnd(4 × level + 2); `\nBleah! The water tasted like stale gatorade!  You lose d hit point[s]!`; lose d hit points |
| 11–13 | 3% | half‑damage (sickness) timer += 200 + rnd(200); `\nThe water makes you vomit.` |
| 14–16 | 3% | `\n  You now have incredible bulging muscles!`; exactly the giant‑strength effect of the potion (section 14, id 17) |
| 17–44 | 28% | `\nNothing seems to have happened.` |
| 45–100 | 56% | an attribute change: upward if rnd(3) ≠ 2 (2 in 3), otherwise downward |

3. Then, whatever the result of step 2, if rnd(12) < 3 (1 in 6): `\nThe fountains bubbling slowly quietens.` and the fountain becomes a dead fountain.

*Attribute change.* Print a line break and roll rnd(9):

| Roll | Text printed first | Upward | Downward |
|---|---|---|---|
| 1 | `Your strength` | ` went up by one!`, +1 | if the score is above 3: ` went down by one!`, −1; otherwise ` remained unchanged!` |
| 2 | `Your intelligence` | same | same |
| 3 | `Your wisdom` | same | same |
| 4 | `Your constitution` | same | same |
| 5 | `Your dexterity` | same | same |
| 6 | `Your charm` (charisma) | same | same |
| 7 | — | j = rnd(level + 1); `You gain j hit point[s]!`; raise maximum hit points by j | `You lose j hit point[s]!`; lower maximum hit points by j |
| 8 | — | j = rnd(level + 1); `You just gained j spell[s]!`; maximum and current spell points +j | `You just lost j spell[s]!`; maximum and current spell points −j, floor 0 each |
| 9 | — | j = 5 × rnd((level + 1)²); `You just gained j experience point[s]!`; gain j experience | `You just lost j experience point[s]!`; lose j experience (levels may be lost, Part 02) |

**Wash**: echo `wash yourself.` Rolls in sequence, each only if the previous failed:

1. rnd(100) < 11 (10%): d = rnd(4 × level + 2); `\nThe water burns like acid!  You lose d hit point[s]!`; lose d hit points (cause "drank some poisonous water").
2. rnd(100) < 29 (28%): `\nYou are now clean.`; if the itching timer is running it is set to 1 so that it ends on the next turn.
3. rnd(100) < 31 (30%): `\nThis water needs soap -- the dirt didn't come off.`
4. rnd(100) < 34 (33%): a water lord is created next to the player (no message).
5. Otherwise `\nNothing seems to have happened.`

Overall: 10% acid, about 25.2% clean, 19.4% soap, 15.0% water lord, 30.4% nothing. Washing never dries the fountain up.

### 9.5 Dead fountain, statue, mirror

All three obey the adjacent‑monster rule, print one line and do nothing else:

- Dead fountain: `\n\nThere is a dead fountain here.`
- Statue: `\n\nYou stand before a statue.`
- Mirror: `\n\nThere is a mirror here.`

## 10. Traps

### 10.1 How traps are hidden and revealed

Arrow traps, dart traps, trap doors and teleport traps each exist as two object ids: a hidden one and a visible one. Pits are always visible. Levels are generated with the *hidden* arrow, dart, trap‑door and teleport ids (Part 05); visible ones only arise by revelation.

- A hidden trap is an ordinary object whose tile is the plain floor tile, so it looks like empty floor even after the cell has been seen, and even after a scroll of enlightenment or magic mapping (those reveal the hidden id, which still draws as floor). The map side panel and the Android context button also treat hidden traps as floor.
- Each time the look step runs on a hidden trap it first rolls to see whether the trap goes off. If not, nothing at all happens and the trap stays hidden. If so, the cell is changed to the visible id and the visible trap's effect follows immediately. Trigger chances: arrow, dart and trap door trigger unless rnd(17) < 13, that is 5 times in 17; a teleport trap triggers unless rnd(11) < 6, that is 6 times in 11.
- There is no search command. The "identify traps" command (key ^, Part 01) only reports *visible* things: it prints a line break and then, for each cell of the 3×3 block around the player (clipped to the map) holding a visible trap door, visible dart trap, visible arrow trap, visible teleport trap, elevator up or elevator down, prints `\nIt's ` followed by the object's name string; if there were none it prints `\nNo traps are visible.`
- Walking onto a hidden trap does not interrupt running.

### 10.2 Arrow trap

`\nYou are hit by an arrow!`, beep, lose rnd(10) + level hit points. Repeats every turn the player remains on it.

### 10.3 Dart trap

`\nYou are hit by a dart!`, beep, lose rnd(5) hit points, then strength − 1 with a floor of 3.

### 10.4 Trap door

In order:

1. Wild Edition hook: a worn propeller beanie flies the player over it (Part 12).
2. If a wand of wonder is anywhere in the pack: `\nYou escape a trap door.` and nothing else.
3. On level 15 or 20: `\nYou fall through a trap door leading straight to HELL!`, beep, pause 3000 ms, death ("fell through a trap door to HELL").
4. Otherwise: `\nYou fall through a trap door!`, beep, lose rnd(5 + level) hit points, pause 2000 ms, go to level + 1 (arriving at the nearest free cell to the same coordinates), redraw.

### 10.5 Pit

Message `\n\nYou're standing at the top of a pit.` every turn, then:

1. If rnd(101) > 81 (20 in 101): nothing more.
2. The player starts to fall if rnd(70) > 9 × dexterity − pack weight, or failing that if rnd(101) < 5 (4 in 101). Otherwise nothing more.
3. If a wand of wonder is in the pack: `\nYou float right over the pit.` and nothing more.
4. Wild Edition hook: propeller beanie (Part 12).
5. On level 15 or 20: `\nYou fell into a pit leading straight to HELL!`, beep, pause 3000 ms, death ("fell into a pit to HELL").
6. Otherwise: if rnd(101) < 20 (19 in 101) the damage is 0 and the message is `\nYou fell ino a pit!  A poor monster cushions your fall!\n` (the misspelling is original). Else damage d = rnd(3 × level + 3), reduced to the player's current hit points if larger, with `\nYou fell into a pit!  You suffer d hit point[s] damage.` Lose d hit points (damage equal to current hit points is still fatal), pause 2000 ms, go to level + 1, redraw.

Dropping an item or gold while standing on a pit makes it vanish down the pit (Part 02).

### 10.6 Teleport trap and the teleport routine

Visible teleport trap (or a hidden one that has just triggered): the cell becomes known, `\nZaaaappp!  You've been teleported!\n`, beep, pause 3000 ms, then a safe teleport.

**The teleport routine** (also used by thrones, the teleportation scroll, and the level‑10 innate teleport command):

1. *Unsafe* teleports only (the innate teleport command, Part 01): if rnd(151) < 3 and the walk‑through‑walls timer is not running, the player dies "trapped in solid rock" and the routine ends there.
2. Outside wizard mode, set the "teleported" flag, which makes the level indicator show as unknown until cleared (Part 02).
3. Choose the destination level. On level 0: stay on level 0. In the dungeon (1..15): rnd(5) + level − 3, limited to the range 0..15 (so two up to two down, and the town is possible from levels 1 and 2). In the volcano (16..20): rnd(4) + level − 2, limited to at most 20, and any result below 16 becomes level 0.
4. Choose a random position x = rnd(65), y = rnd(15).
5. If the level changed, go to it (arriving at the nearest free cell); otherwise move to the nearest cell with no object and no monster.
6. Wild Edition hook: a small chance of a "glitch" event (Part 12). Redraw everything.

### 10.7 Sphere of annihilation as an object

A sphere occupies its cell as object id 93 (Part 07 covers how spheres are made and move). When the look step finds the player on one:

- With the Talisman of the Sphere in the pack: `\nThe Talisman of the Sphere protects you from annihilation!`
- Otherwise the player dies ("annihilated by a sphere").

### 10.8 Monsters and traps (summary; Part 04 owns)

Monsters only react to *visible* traps. Monsters below demon‑prince rank avoid elevators, visible arrow, dart and teleport traps, and (unless they fly) pits and visible trap doors when choosing a path. A monster that nevertheless enters one: arrow, rnd(10) + level damage; dart, rnd(6) damage; teleport trap, the monster is teleported (not demon lords or above); pit or trap door, a non‑flying monster is removed; elevator, the monster is removed (not demon lords or above); sphere, the monster is destroyed unless it is a demon able to dispel it.

## 11. Chests (id 58)

Message `\n\nThere is a chest here.` then `\nDo you (t) take it, (o) try to open it, or (i) ignore it? `, accepted keys o, t, i, Escape.

- **t**: echo ` take`; take the chest with its argument.
- **i or Escape**: `ignore.\n`
- **o**: echo ` open it.` and make the opening roll.

**Opening roll.** k = rnd(101).

- If k is 40 or more (62 in 101): `\nNothing happens.` The chest stays and can be tried again every turn.
- If k < 40 (39 in 101) the chest explodes, which is the only way it ever opens:
  1. `\nThe chest explodes as you open it.`, beep.
  2. Damage d = rnd(10), reduced to the current hit points if larger; `\nYou suffer d hit point[s] damage!`; lose d hit points (cause "killed by an exploding chest").
  3. Roll rnd(10): on 1, itching timer += rnd(1000) + 100, `\nYou feel an irritation spread over your skin!`, beep; on 2, clumsiness timer += rnd(1600) + 200, `\nYou begin to lose hand-eye co-ordination!`, beep; on 3, half‑damage (sickness) timer += rnd(1600) + 200, `\nYou suddenly feel sick and BARF all over your shoes!`, beep; on 4–10 nothing.
  4. The chest is removed.
  5. If rnd(100) < 69 (68%): one random gem appears next to the player (section 16.3).
  6. Gold appears next to the player: rnd(110 × chest argument + 200) pieces (section 16.4).
  7. The random‑item routine (section 16.1) is run for item level (chest argument + 2), the re‑rolled rnd(4) count of times (1 to 4 runs, each of which can itself make more than one item).

The chest argument is the level it was generated for. A carried chest cannot be opened; it must be put down first. A carried chest weighs 30 + its argument.

## 12. Things that are eaten, rubbed, snorted or smoked

### 12.1 Fortune cookie (id 94)

Message `\n\nYou find a fortune cookie.` then `\nDo you (e) eat it, (t) take it` + `, or (i) ignore it? `, accepted keys e, i, t, Escape.

- **e**: `eat.\nThe cookie tasted good.`; the cookie is removed. If the player is not blind and a fortune is available (Part 01 describes the fortune list): `  A message inside the cookie reads:\n` followed by one random fortune.
- **t**: `take.` and take it.
- **i or Escape**: `ignore.\n`

Eaten from the pack with the eat command (Part 01) the wording differs: `\nThe cookie was delicious.` and, if not blind and a fortune is available, `  Inside you find a scrap of paper that says:\n` plus the fortune.

### 12.2 Brass lamp (id 65)

Message `\nYou find a brass lamp.` then `\nDo you want to (r) rub it, (t) take it, or (i) ignore it? `, accepted keys r, i, t, Escape. The lamp can only be rubbed while it lies on the floor; a carried lamp does nothing.

- **t**: `take.` and take it.
- **i or Escape**: `ignore.` (no line break).
- **r**: roll i = rnd(100).
  1. If i > 90 (10%): `\nThe magic genie was very upset at being disturbed!`, lose (current hit points ÷ 2, rounded down) + 1 hit points, beep. Go to step 4.
  2. Otherwise, if rnd(100) + (experience level ÷ 2, rounded down) > 80: `\nA magic genie appears!`, then the prompt `\n  What spell would you like? : ` reads a three‑letter spell code (letters a–z; Escape aborts and prints `aborted.`; a capital D as the first key shows the list of known spells and re‑prints the prompt, Part 07). A line break is printed. If the three letters match one of the 39 spell codes, that spell's "known" counter is increased by one, `\nSpell "<code>":  <name>\n<description>` is printed, then `\nThe genie prefers not to be disturbed again.`, and the lamp is removed. If the code matches nothing (including an aborted entry): `\nThe genie has never heard of such a spell!` then `\nThe genie prefers not to be disturbed again.` and the lamp is removed. In both cases processing stops here.
  3. Otherwise: `\nnothing happened.`
  4. After step 1 or step 3: if rnd(100) < 15 (14%), `\nThe genie prefers not to be disturbed again!`, the lamp is removed, and the "a lamp has been created" flag is cleared so that another lamp may be generated on a later new level (Part 05).

Any spell at all may be requested, including ones far beyond the player's level.

### 12.3 Drugs (ids 96–100)

Each drug prints a message and a three‑way prompt. Taking prints `take.` and picks it up; ignoring (i or Escape) prints `ignore.` Drugs bought at Dealer McDope's Pad (Part 08) arrive in the pack; like found ones they can only be consumed from the floor, so the player must drop one and act on it the following turn. These files contain no addiction, tolerance or withdrawal rules: each dose simply applies the effects below, every time.

| Drug | Message | Prompt and accepted keys | Use echo and message | Effects |
|---|---|---|---|---|
| Speed | `\nYou find some speed.` | `\nDo you (s) snort it, (t) take it, or (i) ignore it? ` — s, t, i, Esc | `snort!` then `\nOhwowmanlikethingstotallyseemtoslowdown!` | haste‑self timer += 200 + experience level; half‑damage timer += 300 + rnd(200); intelligence, wisdom, constitution, dexterity and strength each −2 (floor 3) |
| Magic mushrooms | `\nYou find some magic mushrooms.` | `\nDo you (e) eat them, (t) take them, or (i) ignore them? ` — e, t, i, Esc | `eat!` then `\nThings start to get real spacey...` | haste‑monsters timer += rnd(75) + 25; confusion timer += 30 + rnd(10); wisdom +2; charisma +2 |
| LSD | `\nYou find some LSD.` | `\nDo you (e) eat it, (t) take it, or (i) ignore it? ` — e, i, t, Esc | `eat!` then `\nYou are now frying your ass off!` | confusion += 30 + rnd(10); wisdom +2; intelligence +2; awareness timer += 1500; aggravate timer += 1500; every monster on the level is restored to its type's full hit points |
| Hashish | `\nYou find some hashish.` | `\nDo you (s) smoke it, (t) take it, or (i) ignore it? ` — s, t, i, Esc | `smoke!` then `\nWOW! You feel stooooooned...` | haste‑monsters += rnd(75) + 25; intelligence +2; wisdom +2; constitution −2; dexterity −2 (floors 3); half‑damage += 300 + rnd(200); clumsiness += rnd(1800) + 200 |
| Cocaine | `\nYou find some cocaine.` | `\nDo you want to (s) snort it, (t) take it, or (i) ignore it? ` — s, i, t, Esc | `snort!` then `\nYour nose begins to bleed!` | dexterity −2, constitution −2 (floors 3), charisma +3; then all six abilities +33; cocaine timer += 10 |

When the cocaine timer reaches zero, all six abilities are reduced by 34 (floor 3 each), so each dose leaves a net permanent −1 on every ability on top of the dexterity, constitution and charisma changes. A second dose taken while the timer is still running adds another +33 and 10 more turns, but the expiry still removes only 34 once, so stacked doses leave a net permanent +32 on every ability for each extra dose. In every case the drug is removed from the floor and the status line refreshed.

## 13. Gems, the Eye of Larn and special carried items

### 13.1 Gems and the two quest objects

- **Gems** (ids 71–74) use the generic find prompt and have no effect when carried. The argument is the gem's size and is what its sale value is computed from: the trading post's base value is 20 × argument and the bank offers 100 × argument (both computed in the store code; the exact sale rules are Part 08). No price or selling‑value table lives in the object, potion or scroll files themselves. Sizes at creation: gems popped from thrones and chests are given in section 16.3; gems placed at level build are Part 05. A scroll of gem perfection doubles the argument of every carried gem. Leprechauns pick gems and gold off the floor (Part 04).
- **The Eye of Larn** (id 23) is placed only on dungeon level 15, guarded by a demon prince (Part 05). It uses the generic find prompt. While carried it sets the "has Eye" flag, which lets the player see demon lords, the demon prince and Lucifer (they are otherwise drawn as whatever object is under them). Picking it up shows the "sight" messages of section 4. Losing it (dropped, stolen, sold) clears the flag and, if not blind, prints `\nYour sight fades for a moment...`, pauses 2000 ms, redraws, and prints `\nYour sight returns but everything looks dull and faded.` The treasure‑finding potion reveals it. Its sale is Part 08.
- **The potion of cure dianthroritis** (potion type 21) is the object that wins the game. It is never produced by the random potion generator; exactly one is placed on volcano level 20, guarded by Lucifer (Part 05). On the floor it behaves like any potion (section 14.1). Drinking it wastes it: the only effect is the message `\n  You don't seem to be affected.` Winning requires carrying it into the player's home (Part 08).

### 13.2 What special items do while carried (summary; Parts 02, 04 and 07 own the formulas)

Effects marked "on pick‑up" are applied when the item enters the pack by any route and are reversed when it leaves (dropped, stolen, destroyed); after a removal every ability below 3 is raised to 3.

| Item | Effect |
|---|---|
| Ring of protection | armour class + (enchantment + 1) while carried |
| Ring of increase damage | weapon class + (enchantment + 1) while carried |
| Belt of striking | weapon class + (2 × enchantment + 2) while carried; base weapon class 7 if wielded |
| Ring of regeneration | hit‑point regeneration + (enchantment + 1) |
| Ring of extra regeneration | hit‑point regeneration + 5 × (enchantment + 1) |
| Energy ring | spell‑point regeneration bonus + (enchantment + 1) |
| Ring of dexterity | on pick‑up, dexterity + (enchantment + 1) |
| Ring of strength | on pick‑up, extra strength + (enchantment + 1) |
| Ring of cleverness | on pick‑up, intelligence + (enchantment + 1) |
| Bessman's flailing hammer | on pick‑up, dexterity +10, extra strength +10, and intelligence −10 with a floor of 3 (the previous intelligence is remembered and restored exactly when the hammer leaves the pack); weapon class 35 |
| Sword of slashing | on pick‑up, dexterity +5; weapon class 30 |
| Slayer | on pick‑up, intelligence +10; weapon class 30; when wielded, a hit on a demon lord or higher does 10,000 damage and their damage to the player is cut to between one tenth and one half |
| Staff of power | on pick‑up, wisdom +10; weapon class 10; while carried, 75% of special attacks by demon lords and higher (except Lucifer), wraiths and vampires are cancelled |
| Lance of death | when wielded, kills ordinary monsters outright (10,000 damage) and does 300 to demon lords and higher; weapon class 20 |
| Vorpal Blade | weapon class 22; when wielded, 1 chance in 20 per hit of beheading a monster that has a head (Part 04; note the code requires the blade to be in a pack slot other than the first) |
| Orb of enlightenment | on pick‑up, the "orb" count +1 and the awareness timer +1; while the count is above zero the awareness timer never decreases, giving permanent expanded awareness |
| Orb of dragon slaying | on pick‑up, a "slaying" count +1; while above zero, damage to the six dragon kinds is tripled |
| Scarab of negate spirit | on pick‑up, a count +1; while above zero, damage from spirit monsters is halved |
| Cube of undead control | on pick‑up, a count +1; while above zero, undead monsters' special attacks do nothing and their damage is halved |
| Device of theft prevention | on pick‑up, a count +1; while above zero, stealing attacks fail |
| Amulet of invisibility | each one carried adds (enchantment + 1) × 128 turns to the invisibility spell; while one is carried a running see‑invisible timer never expires |
| Amulet of life preservation | while carried, drain‑life attacks have no effect |
| Hand of Fear | while carried monsters may be frightened (Part 04); the scare‑monster spell's timer is tripled when cast |
| Talisman of the Sphere | protects from spheres of annihilation (section 10.7 and Part 07) |
| Wand of wonder | the player never falls into pits or through trap doors (sections 10.4, 10.5) |
| Elven chain | body armour, armour class 15 |
| Golden urn | nothing |

Base armour class by type (plus the item's enchantment): shield 2, leather 2, studded leather 3, ring mail 5, chain mail 6, splint mail 7, plate mail 9, plate armor 10, stainless plate 12, elven chain 15. Base weapon class (plus enchantment): dagger 3, spear 10, flail 14, battle axe 17, longsword 22, two‑handed sword 26, sunsword 32; the others are in the table above.

### 13.3 Pack weight

Used by the volcanic shafts and the pit. Weight = (gold carried ÷ 1000, rounded down) plus, for each carried item: stainless plate or plate armor 40; plate mail 35; Bessman's hammer 30; splint mail 26; chain mail, battle axe or two‑handed sword 23; longsword, staff of power, sunsword, ring mail or flail 20; elven chain, sword of slashing, lance of death, Slayer or studded leather 15; leather armor or spear 8; shield 7; orb of dragon slaying, orb of enlightenment or belt of striking 4; chest 30 + its argument; anything else 1.

## 14. Potions

### 14.1 Finding a potion

Message `\n\nYou find a magic potion`, then ` of <name>` if that potion type is already identified, then `.`. Prompt `\nDo you (d) drink it, (t) take it, or (i) ignore it? `, accepted keys d, i, t, Escape.

- **d**: `drink.\n`; the potion is removed from the floor first, then drunk.
- **t**: `take.\n` and take it.
- **i or Escape**: `ignore.\n`

Carried potions are drunk with the quaff command (Part 01); choosing something that is not a potion gives `\nYou wouldn't want to quaff that, would you? `, and an empty slot gives `\nYou don't have item <letter>!`.

### 14.2 Identification

- There are 35 potion type slots (0..34); 24 are defined (0..23) and the rest have blank names.
- All types start unidentified (Part 02 notes any class exceptions).
- A type becomes identified when: a potion of that type is drunk (the flag is set before the effect, so even a fatal one counts); a scroll of identify is read while one of that type is in the pack; one is bought at the DND store (Part 08); or the player reaches experience level 100 (everything is identified).
- An unidentified potion is shown as "a magic potion"; an identified one as "a magic potion of <name>". Unidentified potions cannot be sold (Part 08).

### 14.3 Random potion weights

A random potion is chosen by picking one of 41 equally likely entries:

| Id | Name | Entries (of 41) |
|---|---|---|
| 0 | sleep | 2 |
| 1 | healing | 3 |
| 2 | raise level | 1 |
| 3 | increase ability | 2 |
| 4 | wisdom | 2 |
| 5 | strength | 2 |
| 6 | raise charisma | 2 |
| 7 | dizziness | 2 |
| 8 | learning | 1 |
| 9 | gold detection | 3 |
| 10 | monster detection | 3 |
| 11 | forgetfulness | 2 |
| 12 | water | 2 |
| 13 | blindness | 1 |
| 14 | confusion | 1 |
| 15 | heroism | 1 |
| 16 | sturdiness | 1 |
| 17 | giant strength | 1 |
| 18 | fire resistance | 1 |
| 19 | treasure finding | 2 |
| 20 | instant healing | 2 |
| 21 | cure dianthroritis | 0 (never random) |
| 22 | poison | 2 |
| 23 | see invisible | 2 |

### 14.4 Effects when drunk

Every drink first identifies the type and prints `\nYou drink a potion of <name>.` Then:

| Id | Name | Message | Effect |
|---|---|---|---|
| 0 | sleep | `\n  You fall asleep...` then afterwards `\n.. you wake up.` | The player sleeps for rnd(11) + 1 − (constitution ÷ 4, rounded down) turns (none if that is zero or less). In each sleeping turn the monsters move (twice if haste‑monsters is running), the random monster arrival check runs, the player regenerates and timers tick, and there is a 1000 ms pause. |
| 1 | healing | `\n  You feel better.` | If hit points are already at maximum: maximum (and current) hit points +1. Otherwise current hit points += rnd(20) + 20 + experience level, capped at maximum. |
| 2 | raise level | `\n  You feel much more skillful!` | Raise the player one experience level, then maximum hit points +1. |
| 3 | increase ability | `\n  You feel strange for a moment.` | One of the six abilities, chosen with equal chance, +1. |
| 4 | wisdom | `\n  You feel more self-confident!` | Wisdom += rnd(2). |
| 5 | strength | `\n  Wow!  You feel great!` | If strength is below 12 it becomes 12; otherwise strength +1. |
| 6 | raise charisma | `\n  You feel charismatic!` | Charisma +1. |
| 7 | dizziness | `\n  You become dizzy!` | Strength −1, floor 3. |
| 8 | learning | `\n  You feel clever!` | Intelligence +1. |
| 9 | gold detection | `\n  You feel greedy...` | Pause 2000 ms, then every cell on the level holding a ×1 or ×100 gold pile becomes known and is drawn (×10 and ×1000 piles are not detected). Nothing is revealed while blind. |
| 10 | monster detection | (none) | Every cell holding a monster becomes known and its monster is marked as seen. Nothing while blind. |
| 11 | forgetfulness | `\n  You stagger for a moment...` | Every cell of the level becomes unknown; pause 2000 ms; redraw. |
| 12 | water | (none) | Nothing. |
| 13 | blindness | `\n  You can't see anything!` | Blindness timer += 250. |
| 14 | confusion | `\n  You feel confused.` | Confusion timer += 20 + rnd(9). |
| 15 | heroism | `\n  WOW!  You feel fantastic!` | If the heroism timer is 0, all six abilities +11. Timer += 250. When the timer expires all six abilities −10 (floor 3), leaving a permanent +1 each. |
| 16 | sturdiness | `\n  You feel healthier!` | Constitution +1. |
| 17 | giant strength | `\n  You now have incredible bulging muscles!` | If the giant‑strength timer is 0, extra strength +21. Timer += 700. When it expires extra strength −20, leaving a permanent +1. |
| 18 | fire resistance | `\n  You feel a chill run up your spine!` | Fire‑resistance timer += 1000. |
| 19 | treasure finding | `\n  You feel greedy...` | Pause 2000 ms, then every cell holding a diamond, ruby, emerald, sapphire, the Eye of Larn, or a ×1 or ×100 gold pile becomes known and is drawn. Nothing while blind. |
| 20 | instant healing | (none) | Hit points set to maximum, then the remove‑curse effect (section 15.5, scroll 20). |
| 21 | cure dianthroritis | `\n  You don't seem to be affected.` | Nothing (the potion is used up). |
| 22 | poison | `\n  You feel a sickness engulf you!` | Half‑damage (sickness) timer += 200 + rnd(200). |
| 23 | see invisible | `\n  You feel your vision sharpen.` | See‑invisible timer += rnd(1000) + 400; invisible stalkers become visible (Part 04). |
| 24–34 | (blank) | (none) | Nothing (never generated). |

The status and effects displays are refreshed afterwards.

## 15. Scrolls

### 15.1 Finding a scroll

Message `\n\nYou find a magic scroll`, then ` of <name>` if identified, then `.`. The prompt is built as `\nDo you ` + (only if not blind) `(r) read it, ` + `(t) take it` + `, or (i) ignore it? `. Accepted keys: i, r, t, Escape.

- **r**: if blind, nothing happens and nothing is printed (the scroll stays). Otherwise `read.`, the scroll is removed from the floor, and it is read.
- **t**: `take.` and take it.
- **i or Escape**: `ignore.\n`

Carried scrolls are read with the read command (Part 01), which refuses while blind with `\nYou can't read anything when you're blind!`; choosing an unreadable item gives `\nThere's nothing on it to read.`

### 15.2 Identification

There are 28 scroll type slots (0..27); 24 are defined (0..23), slot 24 is the Wild Edition scroll, the rest are blank. The rules mirror potions: a type is identified when read (before its effect), by a scroll of identify while one is carried, by buying one at the DND store, or at experience level 100. Display is "a magic scroll" or "a magic scroll of <name>".

### 15.3 Random scroll weights

Wild Edition hook: with the glitch feature on there is first a small chance that the result is scroll 24 (Part 12). Otherwise one of 81 equally likely entries is chosen:

| Id | Name | Entries (of 81) |
|---|---|---|
| 0 | enchant armor | 4 |
| 1 | enchant weapon | 5 |
| 2 | enlightenment | 6 |
| 3 | blank paper | 5 |
| 4 | create monster | 3 |
| 5 | create artifact | 5 |
| 6 | aggravate monsters | 5 |
| 7 | time warp | 4 |
| 8 | teleportation | 3 |
| 9 | expanded awareness | 4 |
| 10 | haste monsters | 4 |
| 11 | monster healing | 3 |
| 12 | spirit protection | 3 |
| 13 | undead protection | 4 |
| 14 | stealth | 2 |
| 15 | magic mapping | 2 |
| 16 | hold monsters | 3 |
| 17 | gem perfection | 2 |
| 18 | spell extension | 2 |
| 19 | identify | 3 |
| 20 | remove curse | 4 |
| 21 | annihilation | 1 |
| 22 | pulverization | 3 |
| 23 | life protection | 1 |

### 15.4 Effects when read

Every read first identifies the type and prints `\nYou read a scroll of <name>.` Then:

| Id | Name | Effect |
|---|---|---|
| 0 | enchant armor | The scroll form of armour enchantment (section 9.2). |
| 1 | enchant weapon | The scroll form of weapon enchantment (section 9.2). |
| 2 | enlightenment | Every cell in a block around the player becomes known and any monster in it is marked seen: 50 columns from 25 to the left of the player to 24 to the right, and 14 rows from 7 above to 6 below (clipped to the map; the block is one short on the right and bottom). Pause 2000 ms, redraw. |
| 3 | blank paper | Nothing. |
| 4 | create monster | A monster appropriate to (level + 1) appears next to the player. |
| 5 | create artifact | The random‑item routine (section 16.1) for the current level, dropping next to the player. |
| 6 | aggravate monsters | Aggravate timer += 800. |
| 7 | time warp | See below. |
| 8 | teleportation | Safe teleport (section 10.6); no "Zaaaappp" message. |
| 9 | expanded awareness | Awareness timer += 1800. |
| 10 | haste monsters | Haste‑monsters timer += rnd(55) + 12; `  You feel nervous.` (same line). |
| 11 | monster healing | Every monster on the level is restored to its type's full hit points; `  You feel uneasy.` (same line). |
| 12 | spirit protection | Spirit‑protection timer += 300 + rnd(200). |
| 13 | undead protection | Undead‑protection timer += 300 + rnd(200). |
| 14 | stealth | Stealth timer += 250 + rnd(250). |
| 15 | magic mapping | Every cell of the level becomes known and every monster is marked seen; pause 2000 ms; redraw. Hidden traps remain drawn as floor. |
| 16 | hold monsters | Hold‑monsters timer += 30. |
| 17 | gem perfection | The argument of every diamond, ruby, emerald and sapphire in the pack is doubled (stored as a signed 16‑bit number; a non‑zero argument whose double is not positive becomes 2550). |
| 18 | spell extension | The remaining time of each of these timers is doubled: protection spell, dexterity spell, strength spell, charm, invisibility, cancellation, haste self, globe of invulnerability, scare monster, hold monster, time stop. |
| 19 | identify | Every potion type and scroll type currently in the pack becomes identified. Nothing else is identified. |
| 20 | remove curse | Each of these timers that is non‑zero is set to 1, so it expires on the next turn with its normal expiry message: blindness, confusion, aggravate, haste monsters, itching, laughing, drain strength, clumsiness, enfeeblement, half‑damage (sickness). |
| 21 | annihilation | The eight cells around the player flash; every monster there below demon‑lord rank is destroyed and its experience awarded (`\nYou hear loud screams of agony!` if any experience was gained); demon lords and higher print `\nThe <monster> barely escapes being annihilated!` and are left with half their hit points plus 1. (Part 07 owns this routine.) |
| 22 | pulverization | Asks for a direction and fires the directional bolt of the lightning spell with 150 damage, the hit text `The ray hits the <target>.`, the lightning graphic and a 10 ms step delay. If the confusion timer is running the scroll is wasted instead: ` You can't aim your magic!` and a beep. (Part 07 owns the bolt.) |
| 23 | life protection | The life‑protection charge count +1 (section 1.5). |
| 24 | segfault (Wild) | Wild Edition hook, Part 12. The name is blank unless the glitch feature is on, when it reads "segfault". |
| 25–27 | (blank) | Nothing. |

**Time warp.** Compute i = (rnd(1000) − 850) ÷ 100, discarding the fraction toward zero; if i is 0 it becomes 1. The outcomes are +1 with probability 25%, each of −1 to −7 with probability 10%, and −8 with probability 5%. The game clock moves by 100 × i turns (one "mobul" is 100 turns) and is not allowed below 0. The message is `\nYou go forward in time by N mobul[s]` or `\nYou go backward in time by N mobul[s]` with N the size of i; the plural test looks at the signed number, so one mobul backward reads "1 mobuls". (The message is printed even when the clock could not go back that far.) Then every non‑zero timer in the timed‑effects list (Part 02) has 100 × i subtracted from it, which for a backward warp *lengthens* all running effects, good and bad, and for a forward warp shortens them; any timer that would drop to zero or below is set to 1. Finally one normal regeneration/timer tick is run.

## 16. Books

### 16.1 Finding and reading

Message `\n\nYou find a book.` then the same built prompt as a scroll (the "(r) read it, " piece is omitted when blind). Accepted keys i, r, t, Escape.

- **r**: if blind, nothing. Otherwise `read.`, the book is read, then removed.
- **t**: `take.` and take it (keeping its argument).
- **i or Escape**: `ignore.\n`

Carried books are read with the read command and are consumed.

### 16.2 What a book teaches

A book's argument is the level it was made for: the dungeon or volcano level number for books placed at level build, the item level passed to the random‑item routine otherwise. The highest spell number obtainable at each argument is:

| Argument | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15–20 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Highest spell number | 1 | 4 | 7 | 11 | 15 | 20 | 24 | 28 | 30 | 32 | 33 | 34 | 35 | 36 | 37 | 38 |

Reading:

1. If the argument is 3 or less, the spell learned is a random number from 0 up to the highest for that argument, all equally likely (argument 0: spells 0–1; 1: 0–4; 2: 0–7; 3: 0–11).
2. If the argument is 4 or more (values above 20 are treated as 20), the spell is a random number from 10 up to the highest for that argument, all equally likely (so deep books never teach spells 0–9).
3. That spell's "known" counter is increased by one, whether or not it was already known, and `\nSpell "<code>":  <name>\n<description>.` is printed.
4. If rnd(10) is exactly 4 (1 in 10): `\nYou feel clever!` and intelligence +1.

Spell numbers, for reference (Part 07 owns the spells): 0 pro, 1 mle, 2 dex, 3 sle, 4 chm, 5 ssp, 6 web, 7 str, 8 enl, 9 hel, 10 cbl, 11 cre, 12 pha, 13 inv, 14 bal, 15 cld, 16 ply, 17 can, 18 has, 19 ckl, 20 vpr, 21 dry, 22 lit, 23 drl, 24 glo, 25 flo, 26 fgr, 27 sca, 28 hld, 29 stp, 30 tel, 31 mfi, 32 mkw, 33 sph, 34 gen, 35 sum, 36 wtw, 37 alt, 38 per.

## 17. Random item generation used by objects

These helpers live with the level builder (Part 05 owns them) but chests, thrones and the create‑artifact scroll depend on them, so they are restated.

### 17.1 The random‑item routine ("something")

Given a position and an item level (valid 0..20, otherwise nothing happens): pick a random object and argument (17.2) and place it next to the position (17.4); then, while rnd(101) < 8 (7 in 101), do it again.

### 17.2 Random object and argument

Pick one entry from the list below, uniformly among the first 32 entries when the item level is 3 or less, the first 35 when it is 4 to 6, and all 37 when it is 7 or more.

| Entries | Object | Argument |
|---|---|---|
| 1–4 | scroll | a random scroll type (15.3) |
| 5–8 | potion | a random potion type (14.3) |
| 9–12 | gold pile | rnd((level + 1) × 10) + level × 10 + 10 pieces |
| 13–16 | book | the item level |
| 17–19 | dagger | uniformly one of 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 5 |
| 20–22 | leather armor | uniformly one of 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7 (only the first 12 of these when the difficulty is above 0) |
| 23 | ring of regeneration | rund(level ÷ 3 + 1) |
| 24 | ring of protection | rnd(level ÷ 4 + 1) |
| 25 | energy ring | rund(level ÷ 4 + 1) |
| 26 | ring of dexterity | rnd(level ÷ 4 + 1) |
| 27 | ring of strength | rnd(level ÷ 2 + 1) |
| 28 | spear | rund(level ÷ 3 + 1) |
| 29 | belt of striking | rund(level ÷ 2 + 1) |
| 30 | ring mail | rund(level ÷ 2 + 1) |
| 31 | studded leather armor | rund(level ÷ 2 + 1) |
| 32 | shield | rund(level ÷ 3 + 1) |
| 33 | flail | rund(level ÷ 2 + 1) |
| 34 | chain mail | uniformly one of 0, 0, 0, 1, 1, 1, 2, 2, 3, 4 |
| 35 | two‑handed sword | rund(level ÷ 3 + 1) |
| 36 | plate mail | uniformly one of 0, 0, 0, 0, 1, 1, 2, 2, 3, 4 (only the first 3, so always 0, when the difficulty is above 0) |
| 37 | longsword | uniformly one of 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 3 (only the first 6, so always 0, when the difficulty is above 0) |

All divisions round down; "level" here is the item level passed in.

### 17.3 Random gem

Roll rnd(4): 1 diamond with size rnd(50) + 5 (6..55); 2 ruby with size rnd(40) + 4 (5..44); 3 emerald with size rnd(30) + 3 (4..33); 4 sapphire with size rnd(20) + 2 (3..22). The gem is placed next to the player (17.4).

### 17.4 Placing a created object next to a position

Objects created during play never land on the given cell itself. The eight neighbours are tried in a fixed cyclic order starting from a random one (rnd(8)): the order is south, east, north, west, north‑east, north‑west, south‑east, south‑west (south meaning y + 1), wrapping round.

- **Anything except gold**: the first neighbour that is on the map, is not the level‑1 exit cell, is not a wall or closed door and holds no object receives the object (a monster standing there does not matter). If none qualifies the object is silently lost.
- **Gold**, with an amount A: for each neighbour in turn,
  - a ×1 pile whose total would stay below 32,767 simply grows by A;
  - failing that, or if the neighbour is a ×10 pile: if 10 × its argument + A is below 327,670 it becomes a ×10 pile holding that total divided by 10;
  - failing that, or if it is a ×100 pile: if 100 × its argument + A is below 3,276,700 it becomes a ×100 pile holding that total divided by 100;
  - failing that, or if it is a ×1000 pile: if 1000 × its argument + A is at most 32,767,000 it becomes a ×1000 pile holding that total divided by 1000, otherwise its argument is set to 32,767; either way placement ends;
  - a neighbour holding no gold is used if it qualifies as for other objects: a ×100 pile is stored as A ÷ 100 (rounded down), a ×1 pile as A.
  
  (The "failing that" chain treats an overflowing pile's argument with the next multiplier, an original oddity that only arises with enormous piles.)

**Dropped gold** from chests (and from monsters, Part 04) uses a ×100 pile when the amount is above 250 and a ×1 pile otherwise. Because a ×100 pile stores the amount divided by 100 rounded down, an amount of 251..299 yields 200 gold, 300..399 yields 300, and so on.

## 18. Wild Edition hooks in these routines

One line each; Part 12 describes them.

- Any object id of 101 or more found by the look step is passed to the Wild object handler; if no enabled feature claims it, the generic find prompt of section 5 is used.
- Gold pick‑up reports the amount as an event.
- Taking any item notifies the Wild layer.
- Trap doors and pits are skipped while a propeller beanie is worn (message `\nYour propeller whirs and you fly right over the <trap door or pit>.`).
- Fountains offer fishing when a rod is carried.
- The teleport routine has a small chance of a glitch event.
- The random scroll picker may return the segfault scroll (id 24), and reading it calls the glitch routine.
- Object ids 101–117 and their tiles 368–383 and 388 are appended to the table.

## 19. Quirks worth preserving

- A chest only yields its contents by exploding; "Nothing happens." is the other outcome, and the prompt returns every turn.
- Damage from a chest explosion or a pit is capped at the player's current hit points, which does not save them: damage equal to current hit points kills.
- Praying with money can drive gold negative, and pressing Escape at the amount prompt donates everything.
- Armour never passes +9 by enchantment; weapons can, one time in ten.
- The heroism and giant‑strength potions leave a permanent +1 when they wear off; cocaine leaves a permanent −1 on every ability.
- A backward time warp extends every running timed effect by 100 turns per mobul.
- Gold detection ignores ×10 and ×1000 piles.
- Enlightenment's block is 50 by 14 cells, not symmetric.
- The book‑reading intelligence bonus is 1 in 10 (the original comment claims 40%).
- Hidden traps are never shown by mapping magic; only being caught reveals them.
- Drinking the potion of cure dianthroritis destroys the only way to win.
- The brass lamp, drugs and chests cannot be used from the pack; they must lie on the floor under the player.
- The golden urn exists in the table but is never created.
- Both "u" and "d" are accepted on any staircase, giving the "stairs don't go up/down" messages.
