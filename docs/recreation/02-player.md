# Part 02 — The player: classes, attributes, levels, inventory, combat stats, timers

This part specifies everything the game keeps about the player character and every helper that changes it: the attribute array (in its exact saved order), character creation and the eight classes, experience levels and titles, hit point and spell point bookkeeping, the per-turn update that regenerates the player and runs every timed effect, the armour class and weapon class recalculation, the inventory (slots, taking, dropping, weight), enchanting, theft, gem creation, and the player's own movement and positioning helpers. It is a description of the player module of the game core (the upstream files `player.c` / `player.h`), plus the few rules elsewhere that are needed to make sense of it.

It leans on: Part 01 (turn loop, commands, options file, the `nomove` and repeat-count flags, wizard mode), Part 03 (inventory line formatting, death, scoring, the save file container), Part 04 (monster attacks that call the loss helpers), Part 06 (object ids, what item arguments mean per object, picking things up from the floor), Part 07 (spells that start timers), Part 09 (text/message output, status display refresh, beep, pause) and Part 12 (Wild Edition hooks and the Wild attributes).

Conventions used below:

- Message strings are quoted exactly. A leading `\n` in a quoted message means the message begins by starting a new line in the message log; nearly every game message is written that way. `%d`, `%s`, `%c` mark where a number, a string or a single character is substituted.
- "Beep" means the game's alert sound request (Part 09). "Pause N ms" means the game's delay call (Part 09; it can be disabled by an option).
- "Refresh status", "refresh effects" and "refresh both" are the three display refresh requests (Part 09): the status lines (numbers), the list of active effects, or both.
- The map is 67 columns by 17 rows; x runs 0..66 left to right, y runs 0..16 top to bottom.
- All divisions are integer divisions. "Halved" written as `x >> 1` in the source is floor(x / 2) for the non-negative values that occur.

## 1. Random helpers

Confirmed in the game's common header:

| Helper | Result |
|---|---|
| rnd(n) | uniform integer 1..n (C library random value modulo n, plus 1) |
| rund(n) | uniform integer 0..n-1 |
| rndl(n) | same as rnd(n), long integer result |
| rundl(n) | same as rund(n), long integer result |

## 2. Player state kept outside the attribute array

| State | Type / range | Initial value | Meaning |
|---|---|---|---|
| Class name | text, up to 19 characters | set at creation | One of the eight class names of section 4.3. Saved. |
| Class number | 0..7 | set at creation | Index of the class, in the order of section 4.3. Not saved; recomputed from the class name on load. Used by the front end to pick the player tile. |
| Rambo flag | 0/1 | 0 | Set to 1 when the character is created with a lance of death as a starting item (class Rambo). Saved. Only effect: the trading post refuses to buy a lance of death from this character ("You don't *really* want to sell that, now do you?", Part 08). |
| Gender | 1 = male, 0 = female | 1 | Set only by the options file (section 4.4). Not saved in the save file. |
| Wizard flag | 0/1 | 0 | Debug/wizard mode (Part 01). Saved. |
| Cheat flag | 0/1 | 0 | Set when a save file was tampered with; disqualifies scoring (Part 03). Saved. |
| Class picked in options | 0, or letter a..h | 0 | Pre-selected class from the options file; 0 means "ask". Not saved. |
| Player position | x, y | random at creation | Current square. Saved. |
| Previous position | x, y | 0, 0 | The square the player stood on before the last successful step. Used to bounce the player back off a closed door (Part 06). Not saved. |
| Start time | long | 0 | Wall-clock time play began (Part 03). Saved. |
| Game clock | long | 0 | Elapsed game time in turns ("mobuls" are 100 turns). Advanced only by the per-turn update (section 7). Saved. |
| Outstanding taxes | long | 0 | Tax bill carried over from the score file (Parts 03, 08). Saved. |
| Attribute array | 121 long integers | see section 3 | Saved raw, as one block. |
| Displayed-attribute copy | 121 long integers | all 0 | Last values shown on screen, used by the display to detect changes. Not saved. At creation the copy of "current spell points" is set to -50 so that the first status refresh is forced. |
| Inventory items | 26 object ids (signed 8-bit) | all "nothing" | Section 10. Saved. |
| Inventory arguments | 26 signed 16-bit integers | all 0 | Section 10. Saved. |
| Potions known | 35 flags | all 0 | Indexed by potion id. Saved. |
| Scrolls known | 28 flags | all 0 | Indexed by scroll id. Saved. |
| Spells known | 39 flags (may be decremented/incremented, treated as non-zero = known) | all 0 | Indexed by spell id. Saved. |
| Haste step | 0 or 1 | 0 | Half-turn counter used while hasted (section 7). Not saved, never reset. |
| Hit flags (three) | 0/1 | 0 | Set by monster attacks; used by the turn loop to stop running and to skip the monsters' move (Parts 01, 04). Not saved. |

Two constants belong here: the maximum character level is 100, and the "innate teleport" level is 10 (at character level 10 or more the teleport command works without an item; Part 01).

## 3. The attribute array

The array has 121 entries, each a signed long integer, addressed by the indexes below. Order matters: the whole array is written to and read from the save file as one raw block, and several pieces of code address abilities by position (indexes 0..5 are "the six abilities", and loops run from Strength to Charisma, or from Strength to Dexterity).

Kinds used in the table: **stat** (an ability score or core number), **derived** (recomputed by the recalculation of section 9), **counter** (a count or accumulator), **timer** (turns remaining; counts down in the per-turn update), **flag** (0 or non-zero), **slot** (an inventory slot index 0..25, or -1 for none), **unused** (never read or written by the game rules).

Every entry starts at 0 when a character is created unless the "Start" column says otherwise.

| Idx | Name | Kind | Start | Meaning |
|---|---|---|---|---|
| 0 | STRENGTH | stat | by class | Base strength. Displayed strength is this plus STREXTRA (index 75). |
| 1 | INTELLIGENCE | stat | by class | Intelligence. |
| 2 | WISDOM | stat | by class | Wisdom. |
| 3 | CONSTITUTION | stat | by class | Constitution. Drives hit points gained per level. |
| 4 | DEXTERITY | stat | by class | Dexterity. |
| 5 | CHARISMA | stat | by class | Charisma. |
| 6 | HPMAX | stat | by class | Maximum hit points. |
| 7 | HP | stat | by class | Current hit points. |
| 8 | GOLD | counter | 0 | Gold carried. |
| 9 | EXPERIENCE | counter | 0 | Experience points. |
| 10 | LEVEL | stat | 1 | Character experience level, 1..100. |
| 11 | REGEN | derived | 1 | Hit points restored per regeneration tick (section 9). |
| 12 | WCLASS | derived | by equipment | Weapon class (WC). |
| 13 | AC | derived | by equipment | Armour class (higher is better in this game). |
| 14 | BANKACCOUNT | counter | 0 | Gold in the bank (Part 08). |
| 15 | SPELLMAX | stat | by class | Maximum spell points. |
| 16 | SPELLS | stat | by class | Current spell points. |
| 17 | ENERGY | derived | 0 | Spell regeneration bonus from energy rings (section 9). |
| 18 | ECOUNTER | counter | 96 | Countdown to the next spell point (section 7). |
| 19 | MOREDEFENSES | counter | 0 | Temporary armour class bonus from protection effects; added into AC. |
| 20 | WEAR | slot | -1, then by class | Inventory slot of the body armour worn. |
| 21 | PROTECTIONTIME | timer | 0 | Protection spell. |
| 22 | WIELD | slot | -1, then by class | Inventory slot of the wielded weapon. |
| 23 | AMULET | unused | 0 | Never used. |
| 24 | REGENCOUNTER | counter | 16 | Countdown to the next hit point regeneration (section 7). |
| 25 | MOREDAM | counter | 0 | Extra weapon class. Read by the recalculation and by the player's damage formula, but nothing ever sets it, so it is always 0. |
| 26 | DEXCOUNT | timer | 0 | Dexterity spell. |
| 27 | STRCOUNT | timer | 0 | Strength spell. |
| 28 | BLINDCOUNT | timer | 0 | Blindness. |
| 29 | CAVELEVEL | counter | 0 | Copy of the current dungeon level number; written whenever the level changes (Part 05). |
| 30 | CONFUSE | timer | 0 | Confusion. |
| 31 | ALTPRO | timer | 0 | Altar protection ("You have been heard!"). |
| 32 | HERO | timer | 0 | Heroism potion. |
| 33 | CHARMCOUNT | timer | 0 | Charm monsters spell. |
| 34 | INVISIBILITY | timer | 0 | Player invisible. |
| 35 | CANCELLATION | timer | 0 | Cancellation spell. |
| 36 | HASTESELF | timer | 0 | Player hasted. |
| 37 | EYEOFLARN | flag | 0 | 1 while the Eye of Larn is in the pack (lets the player see demons). |
| 38 | AGGRAVATE | timer | 0 | Monsters aggravated. |
| 39 | GLOBE | timer | 0 | Globe of invulnerability spell. |
| 40 | TELEFLAG | flag | 0 | Set after an uncontrolled teleport: the level name is shown as "?" until the player learns where they are (Parts 05, 06, 09). |
| 41 | SLAYING | counter | 0 | Number of orbs of dragon slaying carried. |
| 42 | NEGATESPIRIT | counter | 0 | Number of scarabs of negate spirit carried. |
| 43 | SCAREMONST | timer | 0 | Scare monster. |
| 44 | AWARENESS | timer | 0 | Expanded awareness (also incremented by carrying the orb of enlightenment). |
| 45 | HOLDMONST | timer | 0 | Hold monsters. |
| 46 | TIMESTOP | timer (special) | 0 | Time stop; handled first in the per-turn update (section 7). |
| 47 | HASTEMONST | timer | 0 | Monsters hasted. |
| 48 | CUBEofUNDEAD | counter | 0 | Number of cubes of undead control carried. |
| 49 | GIANTSTR | timer | 0 | Giant strength potion. |
| 50 | FIRERESISTANCE | timer | 0 | Fire resistance. |
| 51 | BESSMANN | flag | 0 | "Bessman's flailing hammer has been created in this game" (unique-item flag, Part 05). |
| 52 | NOTHEFT | counter | 0 | Number of devices of theft prevention carried. |
| 53 | HARDGAME | stat | set after creation | Difficulty level 0..100 (Part 01 sets it right after the character is created). Used in the level-up and regeneration formulas here. |
| 54 | BYTESIN | unused | 0 | Never used. |
| 55 | UNUSED_55 | unused | 0 | |
| 56 | UNUSED_56 | unused | 0 | |
| 57 | UNUSED_57 | unused | 0 | |
| 58 | UNISED_58 (sic) | unused | 0 | |
| 59 | VORPAL | flag | 0 | "The Vorpal Blade has been created" (unique-item flag, Part 05). |
| 60 | LANCEDEATH | derived flag | by equipment | 1 while a lance of death is the wielded weapon. |
| 61 | SPIRITPRO | timer | 0 | Spirit protection. |
| 62 | UNDEADPRO | timer | 0 | Undead protection. |
| 63 | SHIELD | slot | -1 | Inventory slot of the shield worn. |
| 64 | STEALTH | timer | 0 | Stealth. |
| 65 | ITCHING | timer (curse) | 0 | Itching: may force armour off each turn. |
| 66 | LAUGHING | curse, never set | 0 | In the curse list only; nothing sets it. |
| 67 | DRAINSTRENGTH | curse, never set | 0 | In the curse list only; nothing sets it. |
| 68 | CLUMSINESS | timer (curse) | 0 | Clumsiness: may make the player drop the wielded weapon each turn. |
| 69 | INFEEBLEMENT | curse, never set | 0 | In the curse list only; nothing sets it. |
| 70 | HALFDAM | timer (curse) | 0 | The player's attacks do half damage. |
| 71 | SEEINVISIBLE | timer | 0 | See invisible. |
| 72 | FILLROOM | unused | 0 | Never used. |
| 73 | SPHCAST | counter | 0 | Number of spheres of annihilation in existence (Part 07). |
| 74 | WTW | timer | 0 | Walk through walls. |
| 75 | STREXTRA | stat | 0 | Bonus strength from items, spells and potions; added to STRENGTH wherever strength is used or shown. Not subject to the minimum of 3. |
| 76 | TMP | derived | 0 | Scratch: the status display stores STRENGTH + STREXTRA here so that a change in total strength is noticed. |
| 77 | LIFEPROT | counter | 0 | Number of life protection scrolls read and not yet used up (Part 03 applies it at death). |
| 78 | ORB | counter/flag | 0 | Unique-item flag for the orb of enlightenment, and also incremented/decremented when an orb is picked up/lost. Non-zero stops AWARENESS from running down (section 7). |
| 79 | ELVUP | flag | 0 | An "elevator up" has been created (Part 05). |
| 80 | ELVDOWN | flag | 0 | An "elevator down" has been created (Part 05). |
| 81 | HAND | flag | 0 | The Hand of Fear has been created. |
| 82 | CUBEUNDEAD | flag | 0 | A cube of undead control has been created. |
| 83 | DRAGSLAY | flag | 0 | An orb of dragon slaying has been created. |
| 84 | NEGATE | flag | 0 | A scarab of negate spirit has been created. |
| 85 | URN | unused | 0 | Golden urn flag; never used. |
| 86 | LAMP | flag | 0 | A brass lamp has been created (reset to 0 in one outcome of rubbing the lamp, Part 06). |
| 87 | TALISMAN | flag | 0 | The Talisman of the Sphere has been created. |
| 88 | WAND | flag | 0 | A wand of wonder has been created. |
| 89 | STAFF | flag | 0 | A staff of power has been created. |
| 90 | DEVICE | flag | 0 | A device of theft prevention has been created. |
| 91 | SLASH | flag | 0 | A sword of slashing has been created. |
| 92 | ELVEN | flag | 0 | Elven chain has been created. |
| 93 | VORP | unused | 0 | Intended Vorpal Blade flag; the game uses index 59 instead. |
| 94 | SLAY | flag | 0 | Slayer has been created. |
| 95 | PAD | flag | 0 | Dealer McDope's Pad has been created. |
| 96 | LIFE_PRESERVER | flag | 0 | An amulet of life preservation has been created. |
| 97 | COKED | timer | 0 | Cocaine high. |
| 98 | TMP2 | counter | 0 | Intelligence as it was when Bessman's flailing hammer was picked up (section 10.4). |
| 99 | UNUSED_98 (sic) | counter (port) | 0 | Unused by the game rules. The Android front end counts monsters killed here (Part 09). |
| 100 | UNUSED_99 (sic) | counter (port) | 0 | Unused by the game rules. The Android front end stores a random non-zero "run id" here (Part 09). |

The source names of indexes 99 and 100 are one lower than their true positions; the positions in this table are what matters.

### 3.1 Wild Edition attributes (indexes 101..120)

Appended so the array still saves as one block. Behaviour is in Part 12; all start at 0 here and are initialised by the Wild "new character" hook that runs immediately after character creation.

| Idx | Name | One-line meaning |
|---|---|---|
| 101 | WILD_FLAGS | Bit mask of Wild features this character was started with. |
| 102 | WILD_HAT | Object id of the hat being worn, 0 = none. |
| 103 | WILD_PERKS | Bit mask of perks owned. |
| 104 | WILD_PERK_PICKS | Number of level-ups for which a perk is still to be chosen. |
| 105 | WILD_EGG | Turns until the carried egg hatches, 0 = no egg. |
| 106 | WILD_PET_MON | Pet species (monster id), 0 = no pet. |
| 107 | WILD_PET_HP | Pet's last known hit points. |
| 108 | WILD_COINS | LarnCoin holdings. |
| 109 | WILD_COIN_PRICE | Current price of one LarnCoin in gold. |
| 110 | WILD_GLITCH | Turns of "glitch" effect left. |
| 111 | WILD_DUCK | Rubber duck uses consumed. |
| 112 | WILD_WEATHER | Hour of day (0..23) in the low 8 bits, weather state in the next 8 bits. |
| 113 | WILD_FISH_XP | Fishing experience (number of catches). |
| 114 | WILD_CARD_LAST | Monster id of the last trading card dropped. |
| 115..120 | WILD_SPARE_A..F | Six spare entries, unused. |

## 4. Character creation

### 4.1 Sequence

Creating a character happens once, at the start of a new game (never when a saved game is restored):

1. Set all 121 attributes and all 121 displayed-attribute copies to 0.
2. Empty all 26 inventory slots (item "nothing", argument 0).
3. Mark every potion, scroll and spell as unknown.
4. Set LEVEL = 1, REGENCOUNTER = 16, ECOUNTER = 96, and SHIELD = WEAR = WIELD = -1.
5. Choose the class: if the options file pre-selected a class (letter a..h), use it silently; otherwise show the class selection screen (4.2) and wait for a letter a..h.
6. Apply the class (4.3): class name and number, the six abilities, hit points and spell points (current = maximum), known spells, the starting items into inventory slots a, b, c in table order, and the worn/wielded slots.
7. Place the player at a random square: x = rnd(65) (1..65), y = rnd(15) (1..15). Set the previous position to (0, 0).
8. Set the game clock to 0, set the displayed copy of current spell points to -50, and run the armour/weapon recalculation (section 9).

After this the caller runs the Wild Edition new-character hook (Part 12), generates the home level (which calls the "find a free square" helper of section 14.2, so the player never starts on a building or other object), optionally shows the welcome text, and sets the difficulty attribute (Part 01).

While the class is being chosen the display is in text mode and interrupt signals are blocked.

### 4.2 The class selection screen

The Android build uses a narrow layout (longest line 48 characters) so that it reads on an upright phone. The text screen is cleared, the cursor is put at column 1, row 1, and the following is written. "Highlighted" means the standout (emphasised) text style; other lines are plain.

| Row | Style | Text |
|---|---|---|
| 1 | highlighted | `The Addiction of Ularn` |
| 2 | | (blank) |
| 3 | plain | `Pick a character class...` |
| 4 | | (blank) |
| 5 | highlighted | `a) Ogre` |
| 6 | plain | `   Exceptional strength, but thick as a brick` |
| 7 | highlighted | `b) Wizard` |
| 8 | plain | `   Smart, good at magic, but very weak` |
| 9 | highlighted | `c) Klingon` |
| 10 | plain | `   Strong and average IQ, but unwise & very ugly` |
| 11 | highlighted | `d) Elf` |
| 12 | plain | `   OK at magic, but a mediocre fighter` |
| 13 | highlighted | `e) Rogue` |
| 14 | plain | `   Nimble and smart, but only average strength` |
| 15 | highlighted | `f) Adventurer` |
| 16 | plain | `   Jack of all trades, master of none` |
| 17 | highlighted | `g) Dwarf` |
| 18 | plain | `   Strong and healthy, but not good at magic` |
| 19 | highlighted | `h) Rambo` |
| 20 | plain | `   A special challenge. Bad at everything,` |
| 21 | plain | `   but has a lance of death` |
| 24 | prompt | `So, what are ya? ` |

Description lines are indented by exactly three spaces. The prompt is placed at column 1, row 24 and accepts only the single keys a, b, c, d, e, f, g, h (lower case); any other key is ignored and the prompt keeps waiting; the accepted key is echoed. There is no way to cancel.

For reference, the upstream wide layout (not compiled into the Android build) put the highlighted title at column 29 of row 1, the same "Pick a character class..." line, then one tab-indented line per class of the form `a)  Ogre       - Exceptional strength, but thick as a brick` with a blank line between classes (class names padded to 10 characters), with Rambo's entry split as `h)  Rambo      - A special challenge.` followed by an aligned continuation line `Bad at everything, but has a lance of death`, and the same prompt on row 24. The descriptions are word-for-word the ones above.

### 4.3 The eight classes

Class numbers 0..7 correspond to letters a..h. Hit points and spell points are both the current and the maximum value.

| # | Key | Class name | STR | INT | WIS | CON | DEX | CHA | HP | Spell points |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | a | Ogre | 18 | 4 | 6 | 16 | 6 | 4 | 16 | 1 |
| 1 | b | Wizard | 8 | 16 | 16 | 6 | 6 | 8 | 8 | 2 |
| 2 | c | Klingon | 14 | 12 | 4 | 12 | 8 | 3 | 14 | 1 |
| 3 | d | Elf | 8 | 14 | 12 | 8 | 8 | 14 | 8 | 2 |
| 4 | e | Rogue | 8 | 12 | 8 | 10 | 14 | 6 | 12 | 1 |
| 5 | f | Adventurer | 12 | 12 | 12 | 12 | 12 | 12 | 10 | 1 |
| 6 | g | Dwarf | 16 | 6 | 8 | 16 | 4 | 4 | 12 | 1 |
| 7 | h | Rambo | 3 | 3 | 3 | 3 | 3 | 3 | 1 | 0 |

Starting items occupy slots a, b, c in the order listed. The "argument" is the item's inventory argument: an enchantment for weapons and armour, and the potion or scroll type for potions and scrolls. "random 0..5" means the argument is rund(6), rolled separately for each item. Slots not listed stay empty.

| Class | Slot a | Slot b | Slot c | Worn | Wielded | Spells known at start |
|---|---|---|---|---|---|---|
| Ogre | magic potion, type random 0..5 | magic potion, type random 0..5 | — | nothing | nothing | magic missile |
| Wizard | magic potion of type 19 (treasure finding) | magic scroll, type random 0..5 | magic scroll, type random 0..5 | nothing | nothing | magic missile, charm monsters |
| Klingon | studded leather armor +0 | magic potion, type random 0..5 | — | slot a | nothing | sonic spear |
| Elf | leather armor +0 | magic scroll, type random 0..5 | — | slot a | nothing | magic missile |
| Rogue | leather armor +0 | dagger +0 | magic scroll of type 14 (stealth) | slot a | slot b | magic missile |
| Adventurer | leather armor +0 | dagger +0 | — | slot a | slot b | protection, magic missile |
| Dwarf | spear +0 | — | — | nothing | slot a | protection |
| Rambo | lance of death +0 | — | — | nothing | slot a | magic missile |

Random potion types 0..5 are, in id order: sleep, healing, raise level, increase ability, gain wisdom, gain strength. Random scroll types 0..5 are: enchant armor, enchant weapon, enlightenment, blank paper, create monster, create artifact. (Names and effects: Part 06.) None of the starting potions or scrolls is marked as known: the Wizard's treasure-finding potion and the Rogue's stealth scroll appear unidentified like any other.

Resulting derived values after the creation-time recalculation (section 9), before any Wild Edition adjustment:

| Class | AC | WC | Lance-of-death flag |
|---|---|---|---|
| Ogre | 0 | 0 | 0 |
| Wizard | 0 | 0 | 0 |
| Klingon | 3 | 0 | 0 |
| Elf | 2 | 0 | 0 |
| Rogue | 2 | 3 | 0 |
| Adventurer | 2 | 3 | 0 |
| Dwarf | 0 | 10 | 0 |
| Rambo | 0 | 20 | 1 |

For every class REGEN = 1 and ENERGY = 0 at start.

### 4.4 Gender

Gender is not chosen on a game screen. It defaults to male and is changed only by a `gender:male` or `gender:female` entry in the options file read at start-up (Part 01); any other value prints `\nUnknown gender '%s'` and leaves it unchanged. The Android app writes that entry from a per-save-slot "Female character" checkbox (Part 10). Gender is not stored in the save file, so it is whatever the options file says each time the game process starts.

Effects of gender, all outside this module: the player tile is chosen by class number and gender (Part 14); when the daughter dies the text says the player killed "your wife" (male) or "your husband" (female) (Part 08); and two death descriptions on the scoreboard are reworded for female characters (Part 03).

### 4.5 Class-specific rules

There are very few. Everything that differs between classes is in the tables of 4.3, plus:

- A class can be pre-selected by a `class:<name>` entry in the options file. The name is compared without regard to letter case against the eight class names; a match skips the selection screen, no match (or no entry) shows it. The Android app never writes this entry, so the screen always appears for a new game.
- Rambo: the Rambo flag is set to 1 because a lance of death is among the starting items. Its only effect is the trading post's refusal to buy the lance (section 2).
- Level titles (section 5.2) are the same for every class.
- Hit points and spell points gained per level do not depend on class directly, only on constitution and difficulty (section 5.3).
- When a saved game is loaded, the class number is found again from the saved class name. If the name matches no class, the name is reset to "Ogre" (class 0).

## 5. Experience and levels

### 5.1 Experience table

The table gives the total experience needed to *be* a given level. The character is level L when experience is at least the threshold for L and below the threshold for L+1 (subject to the cap of level 100).

| Level | Experience | Level | Experience | Level | Experience |
|---|---|---|---|---|---|
| 1 | 0 | 10 | 2,560 | 19 | 1,000,000 |
| 2 | 10 | 11 | 5,120 | 20 | 2,000,000 |
| 3 | 20 | 12 | 10,240 | 21 | 3,000,000 |
| 4 | 40 | 13 | 20,480 | 22 | 4,000,000 |
| 5 | 80 | 14 | 40,960 | 23 | 5,000,000 |
| 6 | 160 | 15 | 100,000 | 24 | 6,000,000 |
| 7 | 320 | 16 | 200,000 | 25 | 8,000,000 |
| 8 | 640 | 17 | 400,000 | 26 | 10,000,000 |
| 9 | 1,280 | 18 | 700,000 | | |

Above level 26 the table is regular:

| Levels | Threshold |
|---|---|
| 27..71 | 12,000,000 at level 27, then +2,000,000 per level (level L needs 12,000,000 + 2,000,000 × (L − 27)); level 71 needs 100,000,000 |
| 72..91 | 105,000,000 at level 72, then +5,000,000 per level; level 91 needs 200,000,000 |
| 92..95 | 210,000,000; 220,000,000; 230,000,000; 240,000,000 |
| 96..100 | 260,000,000; 280,000,000; 300,000,000; 320,000,000; 340,000,000 |

The table has one further entry, 370,000,000, which would be "level 101"; it is never used for levelling because of the cap, but wizard mode awards exactly 370,000,000 experience to reach level 100 (Part 01).

### 5.2 Level titles

The title shown for the character depends only on the level, not on the class. Each title is stored as a string of exactly 19 characters, padded with spaces on both sides so that it is roughly centred; the front end shows the padded string. The table gives the visible text and the number of leading and trailing spaces.

| Level(s) | Title | Leading spaces | Trailing spaces |
|---|---|---|---|
| 1 | novice explorer | 2 | 2 |
| 2 | apprentice explorer | 0 | 0 |
| 3 | practiced explorer | 1 | 0 |
| 4 | expert explorer | 2 | 2 |
| 5 | novice adventurer | 1 | 1 |
| 6 | adventurer | 5 | 4 |
| 7 | apprentice conjurer | 0 | 0 |
| 8 | conjurer | 5 | 6 |
| 9 | master conjurer | 2 | 2 |
| 10 | apprentice mage | 2 | 2 |
| 11 | mage | 7 | 8 |
| 12 | experienced mage | 1 | 2 |
| 13 | master mage | 4 | 4 |
| 14 | apprentice warlord | 1 | 0 |
| 15 | novice warlord | 2 | 3 |
| 16 | expert warlord | 2 | 3 |
| 17 | master warlord | 3 | 2 |
| 18 | apprentice gorgon | 1 | 1 |
| 19 | gorgon | 6 | 7 |
| 20 | practiced gorgon | 2 | 1 |
| 21 | master gorgon | 3 | 3 |
| 22 | demi-gorgon | 4 | 4 |
| 23 | evil master | 4 | 4 |
| 24 | great evil master | 1 | 1 |
| 25..39 | mighty evil master | 1 | 0 |
| 40..48 | apprentice demi-god | 0 | 0 |
| 49..57 | minor demi-god | 2 | 3 |
| 58..66 | major demi-god | 2 | 3 |
| 67..75 | minor deity | 4 | 4 |
| 76..84 | major deity | 4 | 4 |
| 85..87 | novice guardian | 2 | 2 |
| 88..93 | apprentice guardian | 0 | 0 |
| 94 | earth guardian | 2 | 3 |
| 95 | air guardian | 3 | 4 |
| 96 | fire guardian | 3 | 3 |
| 97 | water guardian | 2 | 3 |
| 98 | time guardian | 3 | 3 |
| 99 | ethereal guardian | 1 | 1 |
| 100 | The Creator | 4 | 4 |

(The title list has two more "The Creator" entries for levels 101 and 102, which cannot be reached.)

### 5.3 Gaining experience and levels

"Award x experience" does the following:

1. Remember the current level. Add x to EXPERIENCE.
2. While EXPERIENCE is at least the threshold for the next level and LEVEL is below 100, gain one level:
   1. Let t = (CONSTITUTION − HARDGAME) halved (floor), computed before the level number changes.
   2. LEVEL increases by 1. (Wild Edition hook: the Wild level-up function is told the new level here, see Part 12.)
   3. Maximum hit points and current hit points both rise by rnd(3) + rnd(t if t > 0, otherwise 1). So with t ≤ 1 the gain is 2..4; with constitution 16 at difficulty 0 it is rnd(3) + rnd(8) = 2..11.
   4. Maximum spell points and current spell points both rise by rund(3), that is 0, 1 or 2.
   5. If the new LEVEL is less than 7 − HARDGAME, maximum and current hit points rise by a further CONSTITUTION ÷ 4 (floor). At difficulty 0 this bonus applies on reaching levels 2 to 6; at difficulty 5 or more it never applies.
3. If the level changed: beep and print `\nWelcome to level %d!` with the final level (once, even if several levels were gained), then apply the special case for the *final* level only:

   | Final level | Effect |
   |---|---|
   | 94 | Walk-through-walls timer set to 99,999 |
   | 95 | Invisibility timer set to 99,999 |
   | 96 | Fire resistance timer set to 99,999 |
   | 97 | Cancellation timer set to 99,999 |
   | 98 | Haste-self timer set to 99,999 |
   | 99 | Stealth timer and spirit protection timer both set to 99,999 |
   | 100 | Print `\nYou are now The Creator!`; the whole current map becomes known (every square's remembered contents become its real contents); every spell, every scroll and every potion becomes known |

   The timers are set (not added to), and they are ordinary timers that count down from 99,999. Because only the final level is examined, a single award that jumps over one of these levels skips its gift.
4. Refresh both status and effects.

The constitution used is the current value, including temporary boosts such as heroism. There is no cap on maximum hit points or spell points here; however, the save-file loader treats a character with more than 999 maximum hit points or more than 125 maximum spell points as a cheater (Part 03).

"Raise one level" (the raise level potion and several dungeon-feature outcomes in Part 06, plus one Wild Edition caller in Part 12): if LEVEL is below 100, award exactly (threshold of the next level − EXPERIENCE), which lands the character precisely on the next threshold. At level 100 it does nothing.

Experience keeps accumulating at level 100.

### 5.4 Losing experience and levels

"Lose x experience" does the following:

1. Remember the current level. Subtract x from EXPERIENCE; if the result is negative it becomes 0.
2. While EXPERIENCE is below the threshold of the current level, lose one level:
   1. LEVEL decreases by 1 (never below 1).
   2. Let t = (CONSTITUTION − HARDGAME) halved. Lose rnd(t if t > 0, otherwise 1) maximum hit points, using the "lower maximum hit points" helper of section 6 (current hit points drop by the same amount; neither goes below 1).
   3. If the new LEVEL is less than 7 − HARDGAME, lose a further CONSTITUTION ÷ 4 maximum hit points the same way.
   4. Lose rund(3) (0..2) maximum spell points with the "lower maximum spell points" helper (current spell points drop by the same amount; neither goes below 0).
3. If the level changed, print `\nYou went down to level %d!` with the new level.
4. Run the armour/weapon recalculation and refresh status.

Note the asymmetries with gaining: the fixed rnd(3) part of the gain is not taken back, and the constitution bonus is lost on arriving at levels 6 down to 1 but gained only on arriving at levels 2 to 6.

"Lose one level" (level-drain attacks, Part 04): if LEVEL is above 1, lose (EXPERIENCE − threshold of the current level + 1) experience, which leaves experience exactly one point short of the level just lost. At level 1 it does nothing. The amulet of life preservation check that prevents drain attacks is made by the monster attack code (Part 04), not here.

## 6. Hit point, spell point and ability helpers

| Helper | Exact behaviour |
|---|---|
| Lose x hit points, with a cause | Wild Edition hook first (the Wild "player hit" function is told x and may adjust hit points before the loss, Part 12). Then HP decreases by x. If HP is now 0 or less: beep, print `\nAlas, you have died.\n` then an empty line `\n`, pause 3000 ms; if the cause is "killed by a monster" but no last-attacking monster is recorded, the cause becomes "unseen attacker"; then the death routine runs with the cause and the last attacking monster (Part 03). The death routine may return if a life protection is available, in which case play continues. |
| Lower maximum hit points by x | HP decreases by x, minimum 1. HPMAX decreases by x, minimum 1. Never kills. |
| Gain x hit points | HP increases by x but not above HPMAX. |
| Raise maximum hit points by x | HPMAX and HP both increase by x. No cap. |
| Gain x spell points | SPELLS increases by x but not above SPELLMAX. |
| Raise maximum spell points by x | SPELLMAX and SPELLS both increase by x. No cap. |
| Lose x spell points | SPELLS decreases by x, minimum 0. |
| Lower maximum spell points by x | SPELLMAX decreases by x, minimum 0; SPELLS decreases by x, minimum 0. |
| Adjust an ability by an amount | Only for the six abilities (indexes 0..5); any other attribute is ignored. Add the amount (which may be negative); if the result is below 3 it becomes 3. There is no upper bound. |

The life protection rule itself (a life protection scroll saves the character from most deaths, restoring full hit points at the cost of one constitution point, with the message "You feel wiiieeeeerrrrrd all over! ") lives in the death routine and is specified in Part 03. Nothing in this module checks it.

Many places change abilities directly rather than through the helper (for example item pick-up bonuses); the minimum of 3 is then re-imposed only when the "item lost" adjustment of section 10.5 runs.

## 7. The per-turn update: regeneration and timers

The turn loop (Part 01) calls this update once after every player command that consumed time, and the run command calls it after every step. It is also called once at the end of the "adjust time" helper (section 8). It processes, in this order:

### 7.1 Time stop

If the time-stop timer is non-zero: decrease it by 1; if it reached 0 (or below), run the armour/weapon recalculation and refresh both status and effects. Then **stop**: nothing else in this update happens while time is stopped or on the turn it ends. The game clock does not advance, nothing regenerates and no other timer moves.

### 7.2 Game clock

If the haste-self timer is 0, the game clock advances by 1. Otherwise the haste step counter increases by 1, and when it reaches 2 the game clock advances by 1 and the counter returns to 0; so while hasted the clock advances on every second update. The haste step counter is never reset when haste ends and is not saved.

Everything below still runs on every update, hasted or not: regeneration and timers are per player action, not per clock tick.

### 7.3 Hit point regeneration

Only if HP is not equal to HPMAX: decrease REGENCOUNTER by 1. If it is now below 0: set REGENCOUNTER to 22 + 2 × HARDGAME − LEVEL, add REGEN to HP (not above HPMAX), and request a status refresh.

Consequences: the counter starts at 16, so a new character's first tick comes on the 17th wounded turn. After that, with reset value R, a tick comes every R + 1 turns if R ≥ 0 and every turn if R < 0 (which happens once LEVEL exceeds 22 + 2 × HARDGAME). The counter is frozen while at full hit points. REGEN is 1 plus ring bonuses (section 9).

### 7.4 Spell point regeneration

Only if SPELLS is below SPELLMAX: decrease ECOUNTER by 1. If it is now below 0: set ECOUNTER to 100 + 4 × (HARDGAME − LEVEL − ENERGY), add 1 to SPELLS, and request a status refresh. (Wild Edition hook: the wizard hat may add a second point here, see Part 12.)

The counter starts at 96. With reset value R a point comes every R + 1 turns if R ≥ 0, every turn otherwise. ENERGY comes from energy rings (section 9).

### 7.5 Timed effects

For each of the 28 timers below, **in this order**: if the timer is non-zero, decrease it by 1; apply the two "keep-alive" rules where noted; and if it is now exactly 0, perform the expiry action. Messages are printed at that moment, so their order on a turn where several expire follows the table.

The "what ends" column describes the reversal performed here. What the effect does while active is governed by the code that reads the timer (Parts 04, 06, 07); a one-line summary and the start-of-effect change this module must undo are given so the net result is clear.

| # | Timer | While active | On reaching 0 | Refresh |
|---|---|---|---|---|
| 1 | Heroism (HERO) | All six abilities were raised by 11 when it started. | Each of the six abilities is adjusted by −10 through the ability helper (minimum 3). Net: each heroism leaves a permanent +1 on every ability. No message. | status |
| 2 | Altar protection (ALTPRO) | MOREDEFENSES was raised by 3 when it started. | MOREDEFENSES decreases by 3. No message. | both |
| 3 | Protection spell (PROTECTIONTIME) | MOREDEFENSES was raised by 2. | MOREDEFENSES decreases by 2. No message. | both |
| 4 | Dexterity spell (DEXCOUNT) | Dexterity was raised by 3. | Dexterity is adjusted by **+3 again** (the source adds instead of subtracting). Net: every dexterity spell that runs out leaves the character 6 points more dexterous than before it was cast. No message. | both |
| 5 | Strength spell (STRCOUNT) | STREXTRA was raised by 3. | STREXTRA decreases by 3. No message. | both |
| 6 | Giant strength (GIANTSTR) | STREXTRA was raised by 21. | STREXTRA decreases by 20. Net: a permanent +1 bonus strength each time. No message. | both |
| 7 | Charm monsters (CHARMCOUNT) | Monsters may be charmed instead of attacking (Part 04). | Nothing but the refresh. | both |
| 8 | Invisibility (INVISIBILITY) | Monsters often miss (Part 04). | Nothing but the refresh. | effects |
| 9 | Cancellation (CANCELLATION) | Blocks many special attacks (Part 04). | Nothing but the refresh. | effects |
| 10 | Haste self (HASTESELF) | Player acts twice as often relative to monsters and the clock. | Nothing but the refresh. | effects |
| 11 | Aggravate monsters (AGGRAVATE) | Monsters are roused and hunt the player. | Nothing. | none |
| 12 | Scare monster (SCAREMONST) | Monsters flee (Part 04). | Nothing but the refresh. | effects |
| 13 | Stealth (STEALTH) | Sleeping monsters stay asleep. | Nothing but the refresh. | effects |
| 14 | Awareness (AWARENESS) | Expanded awareness of surroundings. Keep-alive: immediately after the decrement, if the orb attribute (index 78) is non-zero the timer is increased by 1 again, so it never runs down. | Nothing. | none |
| 15 | Hold monsters (HOLDMONST) | Monsters cannot move. | Nothing but the refresh. | effects |
| 16 | Haste monsters (HASTEMONST) | Monsters move twice per turn. | Nothing. | none |
| 17 | Fire resistance (FIRERESISTANCE) | Fire attacks are reduced (Part 04). | Nothing but the refresh. | effects |
| 18 | Globe of invulnerability (GLOBE) | MOREDEFENSES was raised by 10. | MOREDEFENSES decreases by 10. No message. | status |
| 19 | Spirit protection (SPIRITPRO) | Spirits cannot hurt the player. | Nothing but the refresh. | effects |
| 20 | Undead protection (UNDEADPRO) | Undead cannot hurt the player. | Nothing but the refresh. | effects |
| 21 | Half damage (HALFDAM) | The player's attacks do half damage. | Print `\nYou now feel better.` and beep. | none |
| 22 | See invisible (SEEINVISIBLE) | Invisible stalkers are drawn. | Keep-alive: if an amulet of invisibility is anywhere in the pack, the timer is set back to 1 (so it lasts as long as the amulet is carried). Otherwise nothing. No message. | none |
| 23 | Itching (ITCHING) | See 7.6. | Print `\nThe irritation subsides.` and beep. | none |
| 24 | Clumsiness (CLUMSINESS) | See 7.7. | Print `\nYou now feel less awkward.` and beep. | none |
| 25 | Walk through walls (WTW) | The player may step into wall squares (section 14.1). | Nothing but the refresh. | effects |
| 26 | Cocaine (COKED) | All six abilities were raised by 33 when it started. | Each of the six abilities is adjusted by −34 (minimum 3). Net: one point lower than before on every ability. No message. | status |
| 27 | Blindness (BLINDCOUNT) | Cannot read; the display is restricted. | Print `\nThe blindness lifts.` and beep. | none |
| 28 | Confusion (CONFUSE) | Movement direction may be randomised (section 14.1); spells may misfire. | Print `\nYou regain your senses.` and beep. | none |

Notes that affect behaviour:

- The reversals for the three protection effects (rows 2, 3, 18) change MOREDEFENSES only. The armour class itself is **not** recalculated here, so the displayed and effective AC keeps the bonus until the next recalculation, which happens on the next of: a monster's attack that produced a message, killing a monster, casting a spell, wielding, wearing, taking armour off, picking up or losing an item that affects stats, losing a level, or time stop ending. (The bonus was likewise only reflected in AC at the first recalculation after the effect started; casting a spell recalculates immediately, as does the altar.)
- Because the orb attribute is also the "an orb of enlightenment exists in this game" flag (Part 05 sets it to 1 when the orb is generated on some level), awareness from any source stops running down as soon as the orb has been generated anywhere, whether or not the player carries it.
- A timer changed from elsewhere to a negative value would simply keep decreasing; the helpers in section 8 never leave one negative.

### 7.6 Itching: forced armour removal

After the timer loop, if the itching timer is still non-zero and the player is wearing body armour or a shield (either slot is not −1): with probability 49% (rnd(100) < 50) both the worn-armour slot and the shield slot are set to −1 (the items stay in the pack), `\nThe hysteria of itching forces you to remove your armor!` is printed, a beep sounds, the recalculation runs, and a status refresh is requested. On the turn the timer reaches 0 no check is made.

### 7.7 Clumsiness: dropping the weapon

Then, if the clumsiness timer is still non-zero, a weapon is wielded, and the square the player stands on holds no object at all: with probability 32% (rnd(100) < 33) the wielded item is dropped using the drop helper of section 10.3 (with its normal "You drop:" message and its "just dropped" flag).

### 7.8 Display refresh

Finally one refresh request is issued according to what was requested above: status only, effects only, or both. If nothing was requested, nothing is refreshed.

## 8. Curses and time adjustment

### 8.1 Remove curse

The curse list is, in order: blindness, confusion, aggravate monsters, haste monsters, itching, laughing, drain strength, clumsiness, infeeblement, half damage. For each one whose attribute is non-zero, the attribute is set to 1. Nothing else happens immediately; the next per-turn update counts the real timers among them down to 0 and prints their normal "wears off" messages (the three that are never set are irrelevant). Used by the remove-curse scroll and by the instant healing potion (Part 06).

### 8.2 Adjust time

Given a signed number of turns T (time warp scroll: 100 × the number of mobuls travelled, negative when going back; college course: the length of the course; the Permanence spell: −99,999):

1. For each of the 28 timers of 7.5 that is non-zero: subtract T. If the result is 0 or less, set it to 1 (so that it expires, with its message and reversal, in the update that follows).
2. Run the per-turn update (section 7) once.

Consequences: going forward in time ends effects early; going backward in time, and Permanence, *lengthen* every active timer, good and bad alike (Permanence adds 99,999 turns to blindness or itching just as it does to haste). The time-stop timer is not in the list and is unaffected, and if time is stopped the update in step 2 does only its time-stop step. The game clock itself is changed by the caller, not here.

## 9. Armour class and weapon class recalculation

The recalculation rebuilds five attributes from scratch: AC, WC, the lance-of-death flag, REGEN and ENERGY. In this game a higher AC is better for the player. It is run at character creation, on wield, wear and take off, after every spell cast, when a monster attack produces a message, when a monster is killed, when an item that affects stats is picked up, dropped, stolen, destroyed or re-enchanted, on level loss, and by a few other events owned by other parts.

Steps:

1. AC = MOREDEFENSES.
2. Body armour: if something is in the worn-armour slot, add its base value plus its enchantment (the item's inventory argument, which may be negative):

   | Item in the worn-armour slot | Base AC |
   |---|---|
   | shield (only possible by unusual means; the wear command puts shields in the shield slot) | 2 |
   | leather armor | 2 |
   | studded leather armor | 3 |
   | ring mail | 5 |
   | chain mail | 6 |
   | splint mail | 7 |
   | plate mail | 9 |
   | plate armor | 10 |
   | stainless plate armor | 12 |
   | elven chain | 15 |
   | anything else | adds nothing (enchantment ignored) |

3. Shield: if the shield slot is occupied and the item there is a shield, add 2 plus its enchantment.
4. Wild Edition hook: add the perk armour bonus (Part 12).
5. Clear the lance-of-death flag.
6. Weapon: if nothing is wielded, WC = 0. Otherwise WC = base value plus the wielded item's enchantment:

   | Wielded item | Base WC |
   |---|---|
   | dagger | 3 |
   | belt of striking | 7 |
   | shield | 8 |
   | staff of power | 10 |
   | spear | 10 |
   | flail | 14 |
   | battle axe | 17 |
   | lance of death | 20, and the lance-of-death flag is set to 1 |
   | longsword | 22 |
   | the Vorpal Blade | 22 |
   | two-handed sword | 26 |
   | sword of slashing | 30 |
   | Slayer | 30 |
   | sunsword | 32 |
   | Bessman's flailing hammer | 35 |
   | anything else | 0 (enchantment ignored) |

7. Add MOREDAM (always 0 in practice) to WC. Wild Edition hook: add the perk weapon bonus (Part 12).
8. REGEN = 1 and ENERGY = 0.
9. For every inventory slot, whether or not the item is "worn" (rings and the belt work simply by being carried, and several of the same kind stack):

   | Item in the pack | Effect (arg = its inventory argument) |
   |---|---|
   | ring of protection | AC += arg + 1 |
   | ring of increase damage | WC += arg + 1 |
   | belt of striking | WC += 2 × arg + 2 (in addition to the wielded value if it is also the wielded item) |
   | ring of regeneration | REGEN += arg + 1 |
   | ring of extra regeneration | REGEN += 5 × (arg + 1) |
   | energy ring | ENERGY += arg + 1 |

Upstream quirk: the pack loop examines one position beyond the 26th slot (an out-of-bounds read of whatever byte follows the inventory array). A recreation should examine exactly the 26 slots.

The rings of dexterity, strength and cleverness, and the artifacts that change abilities, do **not** act through this recalculation; their bonuses are applied once when picked up and removed once when lost (sections 10.4 and 10.5).

AC and WC are not clamped; negative enchantments can make them negative.

## 10. Inventory

### 10.1 Slots and arguments

- There are 26 slots, shown to the player as letters a..z (slot index 0 = a). Each slot holds one object id and one signed 16-bit argument. An empty slot holds the "nothing" object.
- Items never stack: every potion, scroll or gem takes its own slot.
- Gold is not an inventory item; it is the GOLD attribute.
- The meaning of the argument depends on the object (Part 06 is authoritative): for weapons, armour, shields, rings and the belt of striking it is the enchantment ("plusses", may be negative); for a potion or scroll it is the potion or scroll type id; for a book or chest it is the dungeon level it came from; for gems it is their value figure; for most other things it is 0 or unused.
- Three attributes point into the inventory: the wielded slot, the worn-armour slot and the shield slot (−1 = none). The Wild Edition hat is tracked separately (Part 12).

### 10.2 Carrying limit and the "take" helper

The number of usable slots grows with level: limit = 15 + LEVEL ÷ 2 (floor), capped at 26. So 15 slots at level 1, 16 at levels 2–3, 17 at 4–5, and all 26 from level 22. Only slots with index below the limit are searched for free space; items already sitting in higher slots (after a level loss) stay where they are and remain usable.

"Pockets full?" is true when no slot below the limit is empty.

"Take item (object, argument)" — used by floor pick-up, stores, and anything that hands the player an item:

1. Find the lowest-indexed empty slot below the limit. If there is none, print `\nYou can't carry anything else.` and report failure (the caller leaves the item where it was).
2. Put the object and argument in the slot. (Wild Edition hook: the Wild "item taken" function is told the object and argument, Part 12.)
3. Apply the pick-up side effect for that object (section 10.4).
4. Print `\nYou pick up:` followed by the standard one-line inventory description of that slot (Part 03: a new line, the slot letter, a closing parenthesis, three spaces, the item's name with its enchantment or identified type).
5. If the object was one whose side effect needs it, run the recalculation and refresh status.
6. If the object was the Eye of Larn and the player is not blind: print `\nYour sight fades for a moment...`, pause 2000 ms, redraw the whole screen, and print `\nYour sight returns, and everything looks crystal-clear!`.
7. Report success.

"Has item X?" is true if any of the 26 slots holds object X.

"Empty handed?" is true if every non-empty slot is the wielded, worn-armour or shield slot (that is, there is nothing a thief could take).

### 10.3 The "drop" helper

"Drop the item in slot k" — used by the drop command (Part 01) and by clumsiness:

1. If k is outside 0..25, do nothing (reports "nothing wrong").
2. If the slot is empty, print `\nYou don't have item %c! ` (the slot letter; note the trailing space) and fail.
3. Look at the player's square. If it holds a pit, remember that. Otherwise, if it holds any object or feature at all, beep, print `\nThere's something here already.` and fail.
4. If the player stands at x = 33, y = 16 (the dungeon exit square), fail silently.
5. Unless there is a pit, place the object and its argument on the square.
6. Print `\n  You drop:` followed by the standard inventory description of the slot. Because the slot has not been cleared yet, the description still carries its " (weapon in hand)" or " (being worn)" suffix if it applied.
7. Empty the slot. If it was the wielded, worn-armour or shield slot, set that pointer to −1.
8. Apply the "item lost" adjustment (section 10.5) for the object and its argument, run the recalculation, refresh status.
9. If there was a pit, print `\nIt disappears down the pit.` (the item is gone for good).
10. Set the "just dropped" flag so that the turn loop does not immediately offer to pick the item up again, and succeed.

### 10.4 Side effects when an item enters the pack

Applied by the take helper, once per item. "Recalc" marks those that trigger the recalculation and status refresh.

| Object | Effect | Recalc |
|---|---|---|
| ring of protection, ring of increase damage, belt of striking | none here (they act through the recalculation) | yes |
| ring of dexterity | Dexterity += arg + 1 | yes |
| ring of strength | STREXTRA += arg + 1 | yes |
| ring of cleverness | Intelligence += arg + 1 | yes |
| Bessman's flailing hammer | Dexterity += 10; STREXTRA += 10; the current Intelligence is saved in TMP2; Intelligence becomes (saved value − 10), or 3 if that would be 2 or less | yes |
| orb of enlightenment | orb attribute += 1; awareness timer += 1 | no |
| orb of dragon slaying | SLAYING += 1 | no |
| scarab of negate spirit | NEGATESPIRIT += 1 | no |
| cube of undead control | CUBEofUNDEAD += 1 | no |
| device of theft prevention | NOTHEFT += 1 | no |
| sword of slashing | Dexterity += 5 | yes |
| Slayer | Intelligence += 10 | yes |
| staff of power | Wisdom += 10 | yes |
| the Eye of Larn | EYEOFLARN = 1, and the sight message of 10.2 step 6 | no |
| anything else (including rings of regeneration, extra regeneration and energy, which therefore only take effect at the next recalculation) | none | no |

The ability artifacts work merely by being carried; they need not be wielded.

### 10.5 The "item lost" adjustment

Called whenever an item leaves the pack other than by being used up: dropped, stolen, sold at the trading post, crumbled by over-enchanting, or destroyed in the player's hand. Given the object and its argument:

| Object | Effect |
|---|---|
| ring of dexterity | Dexterity −= arg + 1 |
| ring of strength | STREXTRA −= arg + 1 |
| ring of cleverness | Intelligence −= arg + 1 |
| Bessman's flailing hammer | Dexterity −= 10; STREXTRA −= 10; Intelligence is set to the value saved in TMP2 (whatever happened to intelligence while the hammer was held is discarded) |
| orb of enlightenment | orb attribute −= 1; awareness timer −= 1 |
| sword of slashing | Dexterity −= 5 |
| Slayer | Intelligence −= 10 |
| staff of power | Wisdom −= 10 |
| orb of dragon slaying | SLAYING −= 1 |
| scarab of negate spirit | NEGATESPIRIT −= 1 |
| cube of undead control | CUBEofUNDEAD −= 1 |
| device of theft prevention | NOTHEFT −= 1 |
| lance of death | lance-of-death flag = 0 |
| the Eye of Larn | EYEOFLARN = 0; if the player is not blind: print `\nYour sight fades for a moment...`, pause 2000 ms, redraw the screen, print `\nYour sight returns but everything looks dull and faded.` |
| magic potion, magic scroll | nothing |
| anything else | nothing specific |

Afterwards, in every case, each of the six abilities that is below 3 is raised to 3. Then, for every object except the orb of enlightenment, orb of dragon slaying, scarab, cube, theft-prevention device, lance of death, Eye of Larn, potions and scrolls, the recalculation runs and both status and effects are refreshed.

### 10.6 Changing an item's enchantment in place

"Adjust the argument of slot i by an amount" (used by enchanting, rust, disenchantment and weapon dulling) adds the amount to the slot's argument and then:

| Item in the slot | Follow-up |
|---|---|
| any body armour (leather, studded leather, ring mail, chain mail, splint mail, plate mail, plate armor, stainless plate armor, elven chain) or a shield | recalculation, refresh status |
| sword of slashing, Bessman's flailing hammer, sunsword, two-handed sword, spear, dagger, battle axe, longsword, flail, lance of death, the Vorpal Blade, Slayer, belt of striking | recalculation, refresh status |
| ring of protection, ring of increase damage | recalculation, refresh status |
| ring of regeneration, ring of extra regeneration, energy ring | recalculation only |
| ring of dexterity | Dexterity += amount; recalculation, refresh status |
| ring of strength | STREXTRA += amount; recalculation, refresh status |
| ring of cleverness | Intelligence += amount; recalculation, refresh status |
| anything else (including the staff of power) | nothing further |

### 10.7 Pack weight

Pack weight in pounds = GOLD ÷ 1000 (floor) plus the weight of every carried item:

| Item | Pounds |
|---|---|
| stainless plate armor, plate armor | 40 each |
| plate mail | 35 |
| Bessman's flailing hammer | 30 |
| chest | 30 + its argument |
| splint mail | 26 |
| chain mail, battle axe, two-handed sword | 23 each |
| longsword, staff of power, sunsword, ring mail, flail | 20 each |
| elven chain, sword of slashing, lance of death, Slayer, studded leather armor | 15 each |
| leather armor, spear | 8 each |
| shield | 7 |
| orb of dragon slaying, orb of enlightenment, belt of striking | 4 each |
| everything else (dagger, the Vorpal Blade, every ring, potion, scroll, book, gem, amulet, the Eye of Larn, cookies, drugs, other artifacts, all Wild Edition items) | 1 each |

There is no general carrying capacity and weight never stops the player picking something up; only the slot limit of 10.2 does. Weight matters in exactly three places, all outside this module (Part 06):

- Climbing down the volcanic shaft: the player slips (and takes damage) if weight > 45 + 3 × (STRENGTH + STREXTRA).
- Climbing up the volcanic shaft: the player slips if weight > 40 + 5 × (DEXTERITY + STRENGTH + STREXTRA).
- Pits: the chance of falling in uses 9 × DEXTERITY − weight.

The pack-weight command (Part 01) prints `\nThe stuff you are carrying presently weighs %d pound%s.` with "s" added unless the weight is exactly 1.

## 11. Wield, wear and take off

None of these commands lives in the player module; they are part of the command code (Part 01). Their rules are summarised here because they are the only writers of the three slot pointers besides the helpers above, and this module's recalculation depends on them.

**Wield.** The prompt is `\nWhat do you want to wield [<letters> * for all - for none] ?`, where the letters listed are the slots holding recognised weapons (sword of slashing, hammer, sunsword, two-handed sword, spear, dagger, battle axe, longsword, flail, Slayer, lance of death, the Vorpal Blade); any slot letter may be typed regardless.
- `-` unwields: the wielded slot becomes −1, `\nYou unwield your weapon.`, recalculation, status refresh.
- An empty slot: `\nYou don't have item %c!`.
- A potion or a scroll: `\nYou can't wield item %c!`.
- A two-handed sword while a shield is worn: `\nBut one arm is busy with your shield!`.
- The slot that is currently the worn armour or the worn shield: `\nYou can't wield your %s while you're wearing it!` with "armor" or "shield".
- Anything else (including non-weapons such as a ring, a book or the staff of power) is wielded: the wielded slot is set, `\nYou wield %s` followed by the enchantment (" +n" when positive, " -n" when negative, nothing at zero) and a full stop; recalculation, status refresh. A non-weapon gives WC 0 as per section 9.

**Wear.** Only body armour and shields (and Wild Edition hats, Part 12) can be worn.
- Body armour while some is already worn: `\nYou are already wearing some armor.`. Otherwise the worn-armour slot is set; if that slot was the wielded one it is unwielded; `\nYou put on your %s` plus enchantment and a full stop; recalculation, status refresh.
- A shield while one is already worn: `\nYou are already wearing a shield.`. While a two-handed sword is wielded: `\nYour hands are busy with the two handed sword!`. Otherwise the shield slot is set, the slot is unwielded if it was wielded, `\nYou put on your shield` plus enchantment and a full stop; recalculation, status refresh.
- An empty slot: `\nYou don't have item %c!`. Anything else: `\nYou can't wear that!` and the prompt repeats.

So the two-handed rule is symmetric: no shield with a two-handed sword, and no two-handed sword with a shield. Nothing else restricts combinations, and there is no class restriction on any item.

**Take off.** (Wild Edition hats are handled first, Part 12.) If a shield is worn it comes off: `\nYour shield is off.`. Otherwise, if body armour is worn it comes off: `\nYour armor is off.`. Otherwise `\nYou aren't wearing anything.`. So it takes two uses to remove both, shield first. Each removal runs the recalculation and refreshes status.

Items are also unwielded/unworn automatically by: dropping (10.3), itching (7.6), theft never (worn and wielded items cannot be stolen, section 13.1), destruction by over-enchanting (section 12), and several monster attacks and store actions in other parts.

## 12. Enchanting

Two helpers are shared by the enchant scrolls and by altars (the caller says which); "from an altar" is the gentler variant.

### 12.1 Enchant armour

1. If neither body armour nor a shield is worn: beep, print `\nYou feel a sense of loss.`, stop.
2. Choose the target: 50% the shield slot, 50% the body-armour slot; if the chosen slot is empty use the other one.
3. If the target's enchantment is 9 or more:
   - from an altar: print `\nYour %s glows briefly.`; nothing changes;
   - from a scroll: with probability 90% (rnd(10) ≤ 9) print `\nYour %s vibrates violently and crumbles into dust!`, apply the item-lost adjustment, empty the slot (object and argument), clear the slot pointer, and run the recalculation; in the remaining 10% nothing happens and nothing is printed.
4. Otherwise print `\nYour %s glows for a moment.` and raise the enchantment by 1 through the helper of 10.6.
5. Refresh status.

`%s` is the object's full name as it appears in the object table, including any article, so a shield produces "Your a shield glows for a moment." while armour produces, for example, "Your leather armor glows for a moment.".

Armour therefore can never pass +9 by enchanting.

### 12.2 Enchant weapon

1. If nothing is wielded: beep, print `\nYou feel depressed.`, stop.
2. If the wielded item is a scroll or a potion, do nothing except refresh status.
3. If its enchantment is 9 or more **and** rnd(10) ≤ 9 (90%):
   - from an altar: print `\nYour weapon glows a little.`; nothing changes;
   - from a scroll: print `\nYour weapon vibrates violently and crumbles into dust!`, apply the item-lost adjustment, empty the slot, set the wielded slot to −1, run the recalculation.
4. Otherwise (enchantment below 9, or the 10% case at 9 or more) print `\nYour weapon glows for a moment.` and raise the enchantment by 1 through the helper of 10.6.
5. Refresh status.

Unlike armour, a weapon at +9 or more still has a 10% chance per attempt of gaining another plus, from either source. "Weapon" means whatever is wielded, so a wielded ring is enchanted too (which is how ring bonuses can be raised).

## 13. Theft, gems and nearby monsters

### 13.1 Stealing from the player

Called by a thieving monster's attack (Part 04) with the monster's position; the caller has already printed "The <monster> picks your pocket and takes:" and handles the theft-prevention device and the "couldn't find anything to steal" case.

1. Each monster can carry at most 6 stolen items. If the monster on that square already holds 6, nothing is stolen (the caller prints "  nothing").
2. Up to 100 attempts: pick a random slot 0..25. If it is non-empty and is not the wielded, worn-armour or shield slot, steal it:
   1. print its standard inventory description line;
   2. apply the item-lost adjustment (10.5);
   3. append the object and its argument to the monster's list of carried items (it is dropped when the monster dies, Part 04);
   4. empty the slot (object and argument);
   5. increase the "level has been visited" marker of the current dungeon level by 1 (the marker is otherwise only tested for zero or non-zero, Part 05, so this normally has no visible effect);
   6. report success.
3. If 100 attempts found nothing, report failure.

### 13.2 Creating a gem

Used when gems are pried off a throne, when a leprechaun is hit, and when a chest is opened (Parts 04, 06; those parts give the chances and counts). Roll rnd(4):

| Roll | Gem | Size j | Argument (value figure) |
|---|---|---|---|
| 1 | diamond | 50 | rnd(50) + 5, i.e. 6..55 |
| 2 | ruby | 40 | rnd(40) + 4, i.e. 5..44 |
| 3 | emerald | 30 | rnd(30) + 3, i.e. 4..33 |
| 4 | sapphire | 20 | rnd(20) + 2, i.e. 3..22 |

The gem is then created on a free square adjacent to the player using the general "create item near a position" routine (Part 05: it tries the eight neighbouring squares starting from a random direction and silently creates nothing if none is suitable).

### 13.3 Monster nearby?

True if any monster occupies any square of the 3 × 3 block centred on the player, clipped to the map. Used by Part 06 to refuse actions such as praying or sitting while a monster is adjacent.

## 14. Movement and positioning helpers

### 14.1 Moving the player one step

Directions are numbered 0..8 with these offsets (x, y): 0 = (0, 0) no movement; 1 = (0, +1) south; 2 = (+1, 0) east; 3 = (0, −1) north; 4 = (−1, 0) west; 5 = (+1, −1) north-east; 6 = (−1, −1) north-west; 7 = (+1, +1) south-east; 8 = (−1, +1) south-west.

"Move the player in direction d" returns 1 if a run in that direction may continue and 0 otherwise. `nomove` and the repeat count are turn-loop flags (Part 01): setting `nomove` means the command used no time, and clearing the repeat count stops a repeated command.

1. Wild Edition hook: a worn tinfoil hat clears confusion first (Part 12).
2. If confused, and LEVEL < rnd(30), the direction is replaced by rund(9), a random direction 0..8 including "no movement". (At level 30 or more confusion never deflects movement.)
3. Compute the target square. If it is off the map: set `nomove`, clear the repeat count, return 0.
4. If the target is a wall and the walk-through-walls timer is 0: set `nomove`, clear the repeat count, return 0. Bumping a wall costs no time.
5. If the "enhanced interface" option is on and the target is a closed door: set `nomove`, clear the repeat count, return 0. (With that option doors are opened by a separate command; without it the player steps onto the door square and is asked what to do, Part 06.)
6. If the target is x = 33, y = 16 and the current level is dungeon level 1, the player leaves the dungeon: the home level is loaded, the map is searched column by column (x from 0, and within each column y from 0) for the dungeon entrance, the player is put on it and then moved to the nearest free square by the helper of 14.2 (the entrance square itself is occupied by the entrance), the screen is redrawn, and the function returns 0. This is checked before the monster check, so a monster standing in the exit does not block it.
7. Wild Edition hook: if the target holds the player's pet, the two swap places (or the move is refused), see Part 12.
8. If a monster is on the target square: the player attacks it (Part 04), the repeat count is cleared, return 0. The player does not move; the attack costs a turn.
9. Otherwise the previous-position pair is set to the current square and the player moves to the target.
10. If the new square holds anything other than nothing, an invisible arrow trap, an invisible teleport trap, an invisible dart trap or an invisible trap door: clear the repeat count and return 0 (the player has moved; the turn loop will then deal with what is there). Otherwise return 1.

With walk through walls active the player can step onto wall squares, including the border; such a step returns 0 because a wall is "something".

The run command (Part 01) simply repeats this step, running the monsters' move, random monster generation and the per-turn update after each successful step, until the step returns 0 or the player is hit.

### 14.2 Finding a free square for the player

Called after every level change (by the level loader for both new and revisited levels), after arriving by stairs when a monster occupies them, after teleports and volcanic shaft trips, after returning home, and after Alter Reality.

1. If the player's current square holds no object and no monster, do nothing.
2. Otherwise scan the whole map, x from 0 to 66 and within each x, y from 0 to 16, for squares with no object and no monster. Keep the one with the smallest squared distance (dx² + dy²) to the current position; ties go to the first one found in scan order.
3. Move the player there. If no free square exists (the search starts with a sentinel distance of 10,000 and nothing beat it), print `Failure in positionplayer` followed by a line break and leave the player where they are.

The previous-position pair is not updated by this helper.

Initial placement on a new level is the caller's business: random x 1..65 and y 1..15 at creation and for teleports and the shaft down; the matching staircase for stairs; then this helper nudges the player off anything occupied.

## 15. The player block of the save file

The save file container, checksums and the rest of the layout are in Part 03. The player block written by this module is, in order, as raw native-endian binary of the platform's own type sizes:

1. Class name, 20 bytes.
2. Rambo flag, 1 byte.
3. Wizard flag, 1 byte.
4. Cheat flag, 1 byte.
5. Player x, one native int.
6. Player y, one native int.
7. Start time, one native long.
8. Game clock, one native long.
9. Outstanding taxes, one native long.
10. The attribute array, 121 native longs, indexes 0..120.
11. Inventory items, 26 bytes.
12. Inventory arguments, 26 16-bit integers.
13. Potions known, 35 native ints.
14. Scrolls known, 28 native ints.
15. Spells known, 39 native ints.

Not saved: class number (recomputed from the name; an unrecognised name is replaced by "Ogre"), gender, the options-file class pick, the previous position, the haste step counter, the hit flags and the displayed-attribute copy.

## 16. Wild Edition hooks in this module

Each is a single call into the Wild code; behaviour is in Part 12.

| Where | Hook |
|---|---|
| Start of the move helper | Tinfoil hat clears confusion. |
| Move helper, before the monster check | Walking into the player's pet swaps places. |
| Each level gained | The Wild level-up function is told the new level (perk picks). |
| Start of "lose hit points" | The Wild player-hit function is told the damage and may alter hit points first. |
| Recalculation | Perk bonus to AC; perk bonus to WC. |
| Take helper | The Wild item-taken function is told the object and argument. |
| Spell point regeneration | Wizard hat bonus point. |
| Right after character creation (in the caller) | The Wild new-character function initialises attributes 101..120. |

## 17. Message catalogue for this module

Every string printed by the functions specified in this part (class screen text is in 4.2):

| Message | When |
|---|---|
| `So, what are ya? ` | Class prompt. |
| `\nWelcome to level %d!` | Level(s) gained (with a beep). |
| `\nYou are now The Creator!` | Reaching level 100. |
| `\nYou went down to level %d!` | Level(s) lost. |
| `\nAlas, you have died.\n` then `\n` | Hit points reached 0 or less (with a beep, then a 3000 ms pause). |
| `Failure in positionplayer` + line break | No free square on the level. |
| `\nYou pick up:` + inventory line | Item taken. |
| `\nYour sight fades for a moment...` | Eye of Larn taken or lost while not blind (followed by a 2000 ms pause and a screen redraw). |
| `\nYour sight returns, and everything looks crystal-clear!` | Eye of Larn taken. |
| `\nYour sight returns but everything looks dull and faded.` | Eye of Larn lost. |
| `\nYou can't carry anything else.` | No free slot. |
| `\nYou don't have item %c! ` | Dropping from an empty slot. |
| `\nThere's something here already.` | Dropping onto an occupied square (with a beep). |
| `\n  You drop:` + inventory line | Item dropped. |
| `\nIt disappears down the pit.` | Dropped while standing on a pit. |
| `\nYou feel a sense of loss.` | Enchant armour with nothing worn (with a beep). |
| `\nYour %s glows briefly.` | Altar enchant of armour already at +9 or more. |
| `\nYour %s vibrates violently and crumbles into dust!` | Scroll enchant destroys armour. |
| `\nYour %s glows for a moment.` | Armour enchanted. |
| `\nYou feel depressed.` | Enchant weapon with nothing wielded (with a beep). |
| `\nYour weapon glows a little.` | Altar enchant of a weapon at +9 or more (90% case). |
| `\nYour weapon vibrates violently and crumbles into dust!` | Scroll enchant destroys the weapon. |
| `\nYour weapon glows for a moment.` | Weapon enchanted. |
| `\nThe blindness lifts.` | Blindness ends (with a beep). |
| `\nYou regain your senses.` | Confusion ends (with a beep). |
| `\nYou now feel better.` | Half damage ends (with a beep). |
| `\nThe irritation subsides.` | Itching ends (with a beep). |
| `\nYou now feel less awkward.` | Clumsiness ends (with a beep). |
| `\nThe hysteria of itching forces you to remove your armor!` | Itching removes armour (with a beep). |

No other effect prints anything when it wears off: heroism, the protections, dexterity, strength, giant strength, charm, invisibility, cancellation, haste, aggravation, scare, stealth, awareness, hold, haste monsters, fire resistance, globe, spirit and undead protection, see invisible, walk through walls and cocaine all end silently.

## 18. Quirks checklist

Behaviours a recreation must reproduce because the player experiences them:

1. The dexterity spell adds 3 when cast and adds 3 again when it expires (section 7.5 row 4).
2. Heroism adds 11 and removes 10; giant strength adds 21 bonus strength and removes 20; cocaine adds 33 and removes 34. Each use therefore leaves a permanent +1, +1 and −1 respectively (subject to the minimum of 3).
3. Protection-type timers running out do not recalculate AC; the bonus lingers until the next recalculation.
4. Level 94–99 gifts are 99,999-turn timers, granted only if that level is the final level of a single experience award.
5. Losing levels is not the mirror image of gaining them (5.4).
6. Awareness never wears off once an orb of enlightenment exists in the game; see invisible never wears off while an amulet of invisibility is carried.
7. Rings and the belt of striking work from anywhere in the pack and stack; ability artifacts work by being carried.
8. Dropping Bessman's flailing hammer restores intelligence to its value at pick-up time.
9. Any non-potion, non-scroll item can be wielded; only listed weapons give weapon class; the staff of power and a shield do give weapon class when wielded.
10. Armour cannot be enchanted past +9; a weapon can, 10% of the time.
11. The armour enchant message includes the item's article ("Your a shield glows for a moment.").
12. Bumping a wall, the map edge or (with the enhanced interface) a closed door costs no time; attacking by moving does.
13. A confused character below level 30 may "move" in direction 0 and stand still, still spending the turn.
14. Time stop freezes the entire per-turn update, including the game clock and all other timers.
15. Going back in time and the Permanence spell lengthen bad timers as well as good ones.
16. The carrying limit is a slot count that grows with level (15 + level ÷ 2, at most 26); weight never limits pick-up.
17. Gender is not stored in the save file.
