# Part 03 — Text screens, item names, scoreboard, death and winning, save files

This part covers four things that live in the game core: (1) how an inventory item is named and
how the full-screen item lists and the "known spells / scrolls / potions" screen are laid out and
paged; (2) the scoreboard file, how a score is calculated, who gets on the board and what the
score screens look like; (3) the end-of-game sequence for every way a game can end (death, life
protection, quit, save-and-exit, winning), including every cause-of-death string; (4) the saved
game file, byte for byte, its checksum, the cheat checks made when it is loaded, and checkpoint
files.

It leans on: Part 01 (the commands that open these screens, the options file, start-up), Part 02
(the character array, inventory slots, experience-level titles, class names), Part 04 (monster
names), Part 06 (what each object's argument means), Part 07 (spell codes and names), Part 08
(the trading-post price table used for scoring, the tax office, the home building that triggers a
win), Part 09 (the text-screen primitives and the shared-memory contract), Part 10 (save slots and
what the app does with the files) and Part 12 (the Wild Edition data stored in the save trailer).

No random numbers are drawn anywhere in this part.

Conventions used below:

- "Slot" means one of the 26 inventory positions, lettered `a` to `z` (slot 0 is `a`).
- "Argument" means the small signed number stored with each inventory item (enchantment for
  weapons and armour, the kind for potions and scrolls, the value for gems, and so on).
- The text screen is 80 columns by 25 lines. Printing a newline moves to column 1 of the next
  line; printing past column 80 wraps; moving below line 25 scrolls the screen up one line. A tab
  advances to the next multiple-of-8 column plus one (columns 9, 17, 25 ...). Clearing the text
  screen also homes the cursor to line 1, column 1.
- "Standout" is the highlighted text style of the front end (Part 09).
- All integer divisions truncate toward zero.

---

## 1. Shared building blocks

### 1.1 The "press space" prompt and the line pager

Two helpers are shared by every list screen and by the scoreboard.

**More prompt.** Prints a newline, two spaces, `--- press `, the word `space` in standout,
` to continue --- `, then waits until the player presses the space bar (no other key is accepted).
On screen it reads:

`  --- press space to continue --- `

**Line pager.** A single counter, reset to zero by "clear pager". Each call to "pager" adds one;
when the counter reaches 22 it is reset to zero, the more prompt is shown and, after the key
press, the text screen is cleared. Callers call "pager" once after each line they print, so a page
holds 22 counted lines.

### 1.2 Entering and leaving a list screen

Every list screen switches the display to the full-screen text mode, clears it (cursor at line 1,
column 1), prints its lines, shows the more prompt and switches back to the map display. While a
list is on screen a global "no signals" flag is set and is cleared again on exit (it has no
visible effect on Android).

---

## 2. How an inventory item is named

One routine prints one inventory slot. Every list screen, the pick-up and drop messages, theft
and disenchantment messages, the trading post and the scoreboard's inventory listing all use it,
so they all share the same wording.

### 2.1 Line format

The routine prints, in this order:

1. A newline (so the line always starts on a fresh line and has no trailing newline).
2. The slot letter, a closing parenthesis and three spaces, for example `c)   `.
3. The object's name from the object name table (section 2.5).
4. A suffix that depends on the object group (section 2.2).
5. In wizard mode only, for the "plain" and potion/scroll groups: a space, `[`, a space, the raw
   argument in decimal, a space, `]` (for example ` [ 12 ]`).
6. ` (weapon in hand)` if this slot is the wielded slot.
7. ` (being worn)` if this slot is the worn-armour slot or the worn-shield slot.
8. Wild Edition hook: ` (on your head)` if this slot holds the hat being worn (see Part 12).

Steps 6 to 8 are independent tests, applied in that order, so more than one can appear.

Examples of complete lines:

- `a)   a dagger +1 (weapon in hand)`
- `b)   leather armor (being worn)`
- `c)   a magic potion`
- `d)   a magic scroll of enchant armor`
- `e)   a ring of protection -2`

### 2.2 The four object groups

| Group | Objects | What follows the name |
|---|---|---|
| Potion | magic potion (id 56) | If that potion kind is known to the player: ` of` followed by the potion's name, which itself begins with a space (so `a magic potion of healing`). If the kind is not known: nothing (just `a magic potion`). |
| Scroll | magic scroll (id 55) | Same rule with the scroll names: `a magic scroll of identify`, or just `a magic scroll` when unknown. |
| Plain (name only) | The Eye of Larn (23), book (57), chest (58), orb of enlightenment (59), scarab of negate spirit (62), cube of undead control (63), device of theft-prevention (64), brass lamp (65), Hand of Fear (66), Talisman of the Sphere (67), wand of wonder (68), diamond (71), ruby (72), emerald (73), sapphire (74), fortune cookie (94), golden urn (95) | Nothing. The argument (book level, chest level, gem value) is never shown to a normal player. |
| Enchantable (everything else) | All armour, all weapons, all rings, belt of striking, amulet of invisibility, orb of dragon slaying, staff of power, amulet of life preservation, the five drugs, and anything else that ends up in the pack | The enchantment suffix of section 2.3, but only if the object id is below 101 (the first Wild Edition object). Wild Edition objects (ids 101 and up) show the bare name. |

Details of the potion/scroll rule: a potion or scroll shows its kind only when the "known" flag
for that kind is non-zero. The unused potion kinds (24 to 34) and scroll kinds (24 to 27) have a
name consisting of two spaces; if such a kind were ever flagged known it would print as
`a magic potion of` followed by two spaces. A name that is an empty string would suppress the
` of` part, but no name in the tables is empty.

### 2.3 The enchantment suffix

Given the argument value n:

- n greater than 0: a space, a plus sign and n. Example: ` +3`.
- n less than 0: a space and n with its minus sign. Example: ` -2`.
- n equal to 0: nothing.
- In wizard mode the first form is used for every value, so zero prints ` +0` and a negative value
  prints ` +-2`.

The same suffix routine is used by the wield and wear confirmation messages (Part 01/02):
`You wield a dagger +1.`, `You put on your leather armor +2.`, `You put on your shield.`

### 2.4 Potion and scroll names

Each name is stored with one leading space. Index = the kind number stored as the item's argument.

Potions (35 entries, index 0 to 34):

| # | Name | # | Name | # | Name |
|---|---|---|---|---|---|
| 0 | sleep | 8 | learning | 16 | sturdiness |
| 1 | healing | 9 | gold detection | 17 | giant strength |
| 2 | raise level | 10 | monster detection | 18 | fire resistance |
| 3 | increase ability | 11 | forgetfulness | 19 | treasure finding |
| 4 | wisdom | 12 | water | 20 | instant healing |
| 5 | strength | 13 | blindness | 21 | cure dianthroritis |
| 6 | raise charisma | 14 | confusion | 22 | poison |
| 7 | dizziness | 15 | heroism | 23 | see invisible |

Entries 24 to 34 are two spaces each (unused).

Scrolls (28 entries, index 0 to 27):

| # | Name | # | Name | # | Name |
|---|---|---|---|---|---|
| 0 | enchant armor | 8 | teleportation | 16 | hold monsters |
| 1 | enchant weapon | 9 | expanded awareness | 17 | gem perfection |
| 2 | enlightenment | 10 | haste monsters | 18 | spell extension |
| 3 | blank paper | 11 | monster healing | 19 | identify |
| 4 | create monster | 12 | spirit protection | 20 | remove curse |
| 5 | create artifact | 13 | undead protection | 21 | annihilation |
| 6 | aggravate monsters | 14 | stealth | 22 | pulverization |
| 7 | time warp | 15 | magic mapping | 23 | life protection |

Entries 24 to 27 are two spaces each (unused).

### 2.5 The object name table

The full table, indexed by object id. Ids matter here because the filtered list screens walk the
ids in ascending order between fixed bounds (section 3). Names are exact, including the article
and capitalisation.

| Id | Name | Id | Name |
|---|---|---|---|
| 0 | (empty string: "unknown" marker) | 59 | an orb of enlightenment |
| 1 | (empty string: "nothing", an empty slot) | 60 | an amulet of invisibility |
| 2 | a holy altar | 61 | an orb of dragon slaying |
| 3 | a handsome, jewel-encrusted throne | 62 | a scarab of negate spirit |
| 4 | a handsome, jewel-encrusted throne | 63 | a cube of undead control |
| 5 | a massive throne | 64 | a device of theft-prevention |
| 6 | a pit | 65 | a brass lamp |
| 7 | a staircase leading upwards | 66 | The Hand of Fear |
| 8 | an elevator going up | 67 | The Talisman of the Sphere |
| 9 | a bubbling fountain | 68 | a wand of wonder |
| 10 | a great marble statue | 69 | a staff of power |
| 11 | a teleport trap | 70 | an amulet of life preservation |
| 12 | a mirror | 71 | a brilliant diamond |
| 13 | a staircase going down | 72 | a ruby |
| 14 | an elevator going down | 73 | an enchanting emerald |
| 15 | a dead fountain | 74 | a sparkling sapphire |
| 16 | an open door | 75 | the College of Larn |
| 17 | a closed door | 76 | the DND store |
| 18 | a wall | 77 | the 8th branch of the Bank of Larn |
| 19 | gold | 78 | the Bank of Larn |
| 20 | gold | 79 | the dungeon entrance |
| 21 | gold | 80 | a volcanic shaft leaning downward |
| 22 | gold | 81 | the base of a volcanic shaft |
| 23 | The Eye of Larn | 82 | your home |
| 24 | plate mail | 83 | the local trading post |
| 25 | chain mail | 84 | the Larn Revenue Service |
| 26 | leather armor | 85 | Dealer McDope's Pad |
| 27 | ring mail | 86 | an arrow trap |
| 28 | studded leather armor | 87 | an arrow trap |
| 29 | splint mail | 88 | a dart trap |
| 30 | plate armor | 89 | a dart trap |
| 31 | stainless plate armor | 90 | a trapdoor |
| 32 | a shield | 91 | a trapdoor |
| 33 | elven chain | 92 | a teleport trap |
| 34 | a sword of slashing | 93 | a sphere of annihilation |
| 35 | Bessman's flailing hammer | 94 | a fortune cookie |
| 36 | a sunsword | 95 | a golden urn |
| 37 | a two-handed sword | 96 | some speed |
| 38 | a spear | 97 | some LSD |
| 39 | a dagger | 98 | some hashish |
| 40 | a battle axe | 99 | some magic mushrooms |
| 41 | a longsword | 100 | some cocaine |
| 42 | a flail | 101 | a humming vending machine |
| 43 | a lance of death | 102 | a can of Fizz |
| 44 | the Vorpal Blade | 103 | a slice of pizza |
| 45 | Slayer | 104 | an energy drink |
| 46 | a ring of extra regeneration | 105 | a rubber duck |
| 47 | a ring of regeneration | 106 | a traffic cone |
| 48 | a ring of protection | 107 | a propeller beanie |
| 49 | an energy ring | 108 | a pointy wizard hat |
| 50 | a ring of dexterity | 109 | a party hat |
| 51 | a ring of strength | 110 | a tinfoil hat |
| 52 | a ring of cleverness | 111 | a mysterious egg |
| 53 | a ring of increase damage | 112 | a fishing rod |
| 54 | a belt of striking | 113 | a fish |
| 55 | a magic scroll | 114 | an old boot |
| 56 | a magic potion | 115 | a message in a bottle |
| 57 | a book | 116 | a cassette tape |
| 58 | a chest | 117 | a LarnCoin ATM |

Ids 101 to 117 are the Wild Edition objects (Part 12). The object count is 118.

---

## 3. The list screens

### 3.1 The inventory screen

Opened by the inventory command, by answering `*` at the "what do you want to drop" prompt, and
(without the mode switch, on the already-cleared store screen) by answering `*` at the trading
post.

Steps:

1. Switch to the text screen and clear it. Reset the pager.
2. If the player carries any gold: print, starting at line 1 column 1 with no leading newline,
   `.)   N gold piece` followed by `s` unless N is exactly 1, then a full stop. Example:
   `.)   137 gold pieces.` Count one pager line.
3. For each slot `a` to `z` in slot order, if it is not empty, print it with the item line format
   of section 2.1 and count one pager line.
4. Compute "mobuls used" = (game time divided by 100) plus 1, and "mobuls left" = 400 minus
   mobuls used (the time limit is 40,000 turns). Print a newline and
   `Elapsed time is U.  You have L mobuls left.` (two spaces after the first full stop; the word is
   `mobul` without the `s` only when L is exactly 1; L can be zero or negative and is printed
   as is).
5. Show the more prompt, then return to the map display.

Layout consequences: when the player has gold the gold line is on line 1 and the first item on
line 2; with no gold, line 1 is blank and the first item is on line 2 (because each item line
begins with a newline). There is no blank line before the elapsed-time line. With more than 22
counted lines (gold plus items) the more prompt appears after the 22nd, the screen is cleared, and
the remainder starts on line 2 of the fresh page.

The order is always slot order; items are never sorted or grouped on this screen.

### 3.2 The filtered lists (wear, wield, read, eat, quaff)

These appear when the player answers `*` to the corresponding "what do you want to ..." prompt
(section 3.4). All five work the same way:

1. Set the "no signals" flag, reset the pager, switch to the text screen and clear it.
2. For each object id from a lower bound to an upper bound in ascending order, and within one id
   for each slot `a` to `z` in slot order: if the slot holds that id and the id belongs to the
   list's set, print the slot with the item line format and count one pager line.
3. Show the more prompt and return to the map display.

So these lists are ordered by object id first and slot letter second, not by slot. There is no
heading, no gold line and no elapsed-time line; line 1 is blank and the first entry is on line 2.
An empty list shows only the more prompt.

| List | Id range walked | Objects shown | Resulting order |
|---|---|---|---|
| Wear | 22 to 92 | plate mail, chain mail, leather armor, ring mail, studded leather armor, splint mail, plate armor, stainless plate armor, shield, elven chain | ids 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 |
| Wield | 22 to 92 | everything in range except: diamond, ruby, emerald, sapphire, book, chest, Eye of Larn, device of theft-prevention, scarab, cube of undead control, potion, orb of enlightenment, Hand of Fear, brass lamp, golden urn, wand of wonder, Talisman of the Sphere, scroll | In practice: armour and shield (24 to 33), weapons (34 to 45), rings (46 to 53), belt (54), amulet of invisibility (60), orb of dragon slaying (61), staff of power (69), amulet of life preservation (70) |
| Read | 22 to 83 | scroll, book | all scrolls (55) first, then all books (57) |
| Eat | 22 to 83 | fortune cookie | see quirk below |
| Quaff | 22 to 83 | potion | potions in slot order |

Quirks that must be kept for fidelity:

- **The eat list is always empty.** The fortune cookie's id is 94, outside the walked range 22 to
  83, so cookies never appear when `*` is pressed at the eat prompt (the prompt itself still lists
  their letters, section 3.4).
- **The wield list omits anything with id 93 or higher**: cookies, the urn, the five drugs and all
  Wild Edition objects, although they can be wielded by typing their letter.
- **Wild Edition items never appear in the wear, read or eat lists** (hats, the rubber duck, the
  bottle, food), although the prompt offers their letters.
- The wield list includes armour, shields and rings; the wield prompt's letter list (section 3.4)
  does not.

### 3.3 The "known magic" screen (spells, scrolls, potions)

Opened by the list-spells command (full form) and by typing `D` as the first character when asked
for a spell code (spells-only form).

Entries are laid out in three columns per row. A row counter and a column counter drive paging:
after each entry the column counter is incremented; when it reaches 3 it is reset, a newline is
printed and the row counter is incremented; when the row counter exceeds 17 (that is, after 18
complete rows) it is reset, the more prompt is shown and the screen is cleared. Both counters
start at zero when the screen opens.

**Full form:**

1. Switch to the text screen and clear it.
2. Print `The magic spells you have discovered to date are:` followed by a blank line.
3. For each spell in spell-number order that the player knows: print its three-letter code, a
   space, its name left-justified and padded to 20 characters, and a space. Each entry is 25
   characters wide, so the three columns start at screen columns 1, 26 and 51. Apply the row/column
   paging step after each entry.
4. Add 3 to the row counter. If the last row is incomplete (column counter not zero), force the row
   to end (newline, row counter plus one, possible page break).
5. Print a blank line, `The magic scrolls you have found to date are:` and another blank line.
   Reset the column counter. For each scroll kind in index order that is known: print its name
   without the leading space, left-justified and padded to 26 characters (columns start at 1, 27
   and 53). Apply the paging step after each.
6. Repeat step 4, then print a blank line, `The magic potions you have found to date are:` and a
   blank line, reset the column counter and list the known potion kinds the same way (26-character
   fields).
7. Wild Edition hooks, each printed only when it has something to show (see Part 12 for the
   content): a section headed `Your perks are:` listing owned perk names in 26-character fields
   three per row; a section headed `Your pet:` followed by one sentence; a section headed
   `The town bounty board:` followed by one sentence. Each section first adds 3 to the row counter
   and ends any incomplete row, exactly as in step 4, and each heading is preceded and followed by
   a blank line.
8. If the row counter is not zero, show the more prompt. Return to the map display.

Because the "add 3" adjustments are only tested when a row completes, a page can run slightly
past 18 rows of content before the break; this is harmless on a 25-line screen.

**Spells-only form** (while casting): clear the screen, print the same spells heading and the
known spells as in steps 2 and 3, then perform one extra paging step (which can end the row),
show the more prompt and return to the map display. The casting prompt is then printed again.

**All-spells form:** the routine also has a third form that lists every spell in the game under
the heading `Availible spells are:` (spelled that way). Nothing in this build calls it; it can be
omitted.

A character with no known spells, scrolls or potions still gets the three headings.

### 3.4 Picking an item

Items are always picked by slot letter at a one-line prompt in the message area; the list screens
are only a reminder.

The prompt is: a newline, `What do you want to VERB [LETTERS * for all] ?` where VERB is one of
`wield`, `quaff`, `read`, `wear`, `eat`, `drop`, and LETTERS is a run of characters with no
separators followed by one space. For wield, the text ` - for none` is inserted after `all`.
Example: `What do you want to wield [acf * for all - for none] ?`

LETTERS is built as follows:

- For drop: a full stop (standing for gold) followed by the letter of every non-empty slot.
- For the other verbs: the letter of every slot whose "natural use" equals the verb.

| Natural use | Objects |
|---|---|
| wield | sword of slashing, Bessman's flailing hammer, sunsword, two-handed sword, spear, dagger, battle axe, longsword, flail, Slayer, lance of death, Vorpal Blade |
| quaff | potion |
| read | scroll, book; Wild Edition: rubber duck, message in a bottle |
| wear | the ten armour/shield objects (ids 24 to 33); Wild Edition: the five hats |
| eat | fortune cookie; Wild Edition: can of Fizz, slice of pizza, energy drink, fish |
| none (drop only) | everything else |

Accepted keys (the prompt ignores everything else): any lower-case letter `a` to `z` whether or
not it is in LETTERS, `*`, Escape, plus `-` for wield only and `.` for drop only. The cursor is
shown.

- Escape prints ` aborted.` and cancels the command.
- `*` shows the matching list screen (the inventory screen for drop) and then asks again.
- `-` (wield) unwields; `.` (drop) starts the drop-gold dialogue.
- A letter selects that slot; what happens next (including `You don't have item x!` for an empty
  slot) belongs to Parts 01 and 02.

---

## 4. The scoreboard file

### 4.1 Name and location

The file is called `Uscore` and lives in the game's library directory, which is the process
working directory (`./Uscore`). On Android the working directory is the app-private folder that
also holds the help, maze and fortune files; it is shared by all save slots, so there is one
scoreboard for the whole installation (Part 10).

At start-up, before a game begins, if the file does not exist a fresh board is created. If that
fails the game prints `I can't create the scoreboard.` and `Check permissions on DIR` (DIR being
the library directory) on separate lines and exits.

### 4.2 Record layout

The file is three raw blocks written one after another:

1. A header: the four characters `ulsb` followed by a native integer format number, currently 1.
2. An array of exactly 25 "visitor" records (players who did not win).
3. An array of exactly 25 "winner" records.

Unused records are present in the file with a score of zero. Records are written as native
structures, so the file is not portable between CPU word sizes; a re-implementation is free to
choose any fixed encoding as long as it holds these fields.

Visitor record fields, in order:

| Field | Type | Meaning |
|---|---|---|
| score | long integer | The score; 0 means the record is unused |
| user id | integer | Numeric user id of the player who owns the record |
| reason | integer (cause-of-death number, section 6) | Why the game ended |
| monster | 16-bit integer | Monster id, meaningful only when reason is 0 |
| level | 16-bit integer | Dungeon level index at death (0 = home level, 1 to 15 = dungeon, 16 to 20 = volcano 1 to 5) |
| difficulty | 16-bit integer | Difficulty level of that game |
| order | 16-bit integer | Rank of this record, 0 = best; recomputed on every update |
| name | 41 characters, NUL-terminated | The character's display name |
| class | 20 characters, NUL-terminated | The character class name |
| inventory | 26 pairs of 16-bit integers | For each slot: object id, argument |

Winner record fields, in order:

| Field | Type | Meaning |
|---|---|---|
| score | long integer | The score; 0 means unused |
| time used | 16-bit integer | Game time at the win divided by 100 (mobuls) |
| taxes | long integer | Tax still owed by this player (section 8.3) |
| user id | integer | Numeric user id |
| difficulty | 16-bit integer | Difficulty of the winning game |
| order | 16-bit integer | Rank, 0 = best |
| name | 41 characters | Display name |
| class | 20 characters | Class name |

For reference, with 8-byte longs and natural alignment the header is 8 bytes, a visitor record 192
bytes and a winner record 96 bytes, giving a 7,208-byte file.

A fresh board has, in every record of both arrays, score 0, difficulty 0, taxes 0 (winners) and
order equal to the record's index; everything else is zero. On Unix-like systems the file is made
readable and writable by everyone.

### 4.3 Reading and writing

The whole file is read into memory before any use and rewritten in full after any change. Failures
print one of these lines and abort the operation (a failed read means "no board": no scores shown,
no entry made, taxes cannot be paid, the player is treated as never having won):

| Situation | Message |
|---|---|
| Cannot open for reading | `Can't open scorefile 'PATH' for reading` |
| Header too short | `Can't read score board header` |
| First four characters are not `ulsb` | `Not a valid ularn score board` |
| Format number is not 1 | `Incorrect score board version` |
| Either record array too short | `Can't read scoreboard` |
| Cannot open for writing | `Can't open scorefile for writing` |
| Any block fails to write | `Can't write scorefile` |

There is no automatic upgrade or re-creation of a board with a wrong format number: it simply
cannot be read until the file is removed.

### 4.4 Whose record is whose

Records are matched by **numeric user id only**; the name is never compared. Each user id owns at
most one visitor record and one winner record.

On Android every game runs under the app's single user id, so in practice **the visitor board
holds one entry and the winner board holds one entry for the whole installation**, whatever the
character is called and whichever save slot is used. The 25-entry capacity only fills on a
multi-user system. (The login name is fixed to `player`; the display name comes from the options
file. See Parts 01 and 10.)

### 4.5 Sort order

After every update each board is ranked separately:

1. Mark every record unranked.
2. For rank 0, 1, ... 24: among the unranked records find the one with the largest score, where a
   later array index wins ties, and where only scores of minus one or more are considered. Give it
   that rank.

Higher scores therefore rank first; among equal scores (including all the unused zero-score
records) the record later in the array ranks first. A record with a score below minus one is never
ranked and keeps rank minus one.

### 4.6 Adding a score

This runs at the end of a game unless the character is flagged as a wizard or a cheater (section
10). Let "score" be the value from section 5, "difficulty" the character's difficulty level and
"gold" the sum of gold carried and gold in the bank.

If the board cannot be read, or the cheater flag is set, nothing happens.

**If the player won:**

1. Set the score of every visitor record owned by this user id to 0 (winners are removed from the
   visitor log; the records keep their owner id).
2. Compute taxes = outstanding taxes + gold × 0.05, computed in floating point and truncated to a
   whole number, and make that the new outstanding-tax figure.
3. Choose a record: the last winner record owned by this user id if there is one (used or not).
   Otherwise take the lowest-ranked winner record (rank 24), but only if the new score is strictly
   greater than that record's score.
4. If a record was chosen: store the new tax figure in it. Then, if the new score is strictly
   greater than the record's score **or** the difficulty is strictly greater than the record's
   difficulty, overwrite its name, class, score, difficulty, owner id and time used (game time
   divided by 100). Otherwise only the tax figure changes.

**If the player did not win:**

1. Choose a record: the last visitor record owned by this user id if there is one. Otherwise the
   lowest-ranked visitor record (rank 24).
2. If the new score is strictly greater than that record's score **or** the difficulty is strictly
   greater than the record's difficulty, overwrite: name, class, score, reason, monster id,
   difficulty, owner id, level index, and all 26 inventory slots (id and argument). Remember this
   record so the score screen can highlight it.
3. Otherwise nothing is stored and nothing is highlighted.

In both cases the boards are then re-ranked and the file rewritten.

Consequences worth knowing:

- A player's stored score is replaced by a **lower** score if the new game was played at a higher
  difficulty.
- A user's existing record is replaced only by that user's own better (or harder) game. A user
  with no record competes for the bottom-ranked record: on the visitor board it is overwritten if
  the new score is strictly higher than its score or the new difficulty is strictly higher than
  its difficulty; on the winner board the new score must be strictly higher than its score.
- Quitting never creates an entry (section 7.3).

### 4.7 Start-up maintenance modes

Three command-line switches act on the board and then exit without starting a game (Part 01 has
the full option list):

- Show scores (`-s`): runs the score display of section 5.3. The Android app's "Scoreboard" menu
  entry uses this.
- Show scores with inventories (`-i`): runs the detailed display of section 5.4. Not reachable
  from the Android app's menus.
- Create new board (`-c`): prints `Preparing to initialize the scoreboard.`, asks for the wizard
  password with the prompt `Enter Password: ` (typed characters echo as `*`, up to 8 characters,
  the password is `fizban`). A wrong password prints `Sorry.` and exits. A right one writes a
  fresh board (on failure `Scoreboard creation failed!!` and exit), runs the score display and
  prints `Scoreboard initialized.` Not reachable from the Android app's menus.

---

## 5. Score calculation and the score screens

### 5.1 The score formula

Computed once, at the end of the game, in this order:

1. Worth = gold carried + gold in the bank.
2. For each of the 26 inventory slots add the item's trading-post value (section 5.2). **An empty
   slot, and any object the price list does not know, contributes minus 1.**
3. Wild Edition hook: add the gold value of any LarnCoin held at the current price (zero when the
   feature is off; Part 12).
4. If the player did **not** win: subtract one tenth of Worth (Worth = Worth − Worth/10).
   If the player won: add 100,000 × difficulty.
5. Volcano special case: if volcano level 1 has been visited while none of dungeon levels 1 to 15
   has (the player went straight into the volcano), and the player won, double Worth.
6. Halve Worth.
7. Score = Worth + experience points.

Two upstream oddities shape this and must be reproduced:

- The code intends to add 50 points per deepest level reached and to halve Worth only as a
  "stupidity penalty" for dying after going straight into the volcano. But the penalty flag starts
  out set and is never cleared, so **Worth is always halved and the depth bonus is always zero**.
  Depth reached does not affect the score at all.
- The arithmetic is done in 32-bit signed integers.

In short: a dead character scores 45% of net worth plus experience; a winner scores 50% of net
worth plus 50,000 per difficulty level plus experience. A character with an empty pack loses 26
from Worth before the percentages are applied.

### 5.2 Item values used for scoring

The value of one inventory item is what the trading post would pay (Part 08 has the price table;
the Wild Edition "Haggler" perk is not applied here):

- Diamond, ruby, emerald, sapphire: 20 × argument.
- Wild Edition: an old boot is worth 1 when the fishing feature is on.
- Otherwise find the first entry for that object in the DND store price list. If there is none
  the value is minus 1 (this covers empty slots, the Eye of Larn, the golden urn, the drugs and
  most Wild Edition objects).
- Potion or scroll: twice the price of the list entry found by counting "argument" entries forward
  from the first potion (or first scroll) entry.
- Anything else: start from the list price; if the argument is zero or positive, double it; then
  while the argument is not zero and the value is below 500,000: for a positive argument replace
  the value with 14 × (value + 67) / 10 and reduce the argument by one; for a negative argument
  replace the value with value × 10 / 14 and increase the argument by one.
- A result of exactly 0 becomes 1.

### 5.3 The final score page

Shown at the end of every game that ends through the scoring path (section 7.4). The text screen
is cleared and the following is printed from line 1:

1. A rule of 63 hyphens.
2. Seventeen spaces then `U L A R N    S C O R E S` (single spaces between letters, four spaces
   between the words).
3. The same rule.
4. `Score: S  Diff: D  Level: TITLE  Char: CLASS` (two spaces between the parts). S is the score, D
   the difficulty, CLASS the class name (or `<unknown>` if the character has no class yet). TITLE
   is an experience-level title from the table in Part 02, taken **one position too far**: the
   title belonging to the next experience level (a level-1 character is shown as
   `apprentice explorer`, not `  novice explorer  `). Titles are 19 characters wide and padded
   with spaces on both sides.
5. A tab (so the text starts in column 9), the character's display name, then the cause-of-death
   phrase of section 6.2, which begins with a space and ends the line.
6. On the next line: ` (wizard)` if the wizard flag is set, then ` (cheater)` if the cheater flag
   is set; otherwise the line is empty.
7. The rule again, with no newline after it.

Then, on the next line, the prompt `Press return to continue` waits for the Return key (carriage
return, code 13; nothing else is accepted, no cursor).

Example for a level-3 ogre:

```text
---------------------------------------------------------------
                 U L A R N    S C O R E S
---------------------------------------------------------------
Score: 412  Diff: 0  Level:   expert explorer    Char: Ogre
        Thorin killed by a giant ant on  2

---------------------------------------------------------------
Press return to continue
```

Compatibility note: the Android front end recognises the end of a run by scanning the text screen
for this page (the title line, a line starting `Score: `, the lines up to the closing rule) at the
moment the "Press return" prompt is issued, and classifies it as won, quit or died from the
phrase after the name (Part 09). The wording and layout therefore matter.

Class-name quirk: when a game is restored from a save or checkpoint the stored class name is
converted to lower case in memory, so a restored character shows `ogre` here and on the board
while a character played in one sitting shows `Ogre`. On Android nearly every game has been
restored at least once.

### 5.4 The scoreboard display

Runs after the final score page (for everyone, including wizards and cheaters), and for the
show-scores start-up switch.

1. Read the board. On failure the error line(s) of section 4.3 appear followed by the
   `Press return to continue` prompt, and the display ends.
2. **Winners.** Count the winner records with a non-zero score. If there are any: clear the
   screen, reset the pager and print the heading
   `   Score       Diff   Time Needed  Ularn Winners List`
   (3 spaces, `Score`, 7 spaces, `Diff`, 3 spaces, `Time Needed`, 2 spaces, the title). Then for
   each rank from 0 upward print the record of that rank if its score is positive: the score
   left-justified in a 10-character field, the difficulty right-justified in 8, the time used
   right-justified in 8, then ` Mobuls  (CLASS) NAME`. One pager line is counted for the heading
   and for each record. Example:
   `184220           1     212 Mobuls  (wizard) Merlin`
3. If any winners were shown, the `Press return to continue` prompt follows (on a new line after a
   blank line).
4. **Visitors.** Count the visitor records with a non-zero score. If there are any: clear the
   screen, reset the pager and print the heading
   `  Score Diff  Ularn Visitor Log`
   (2 spaces, `Score`, 1 space, `Diff`, 2 spaces, the title). Then for each rank from 0 upward
   print the record of that rank if its score is positive: the score right-justified in 7, a
   space, the difficulty right-justified in 3, three spaces, `(CLASS)`, a space, the name, a
   space, then the cause-of-death phrase of section 6.2 (which begins with its own space, so two
   spaces separate the name from the phrase) ending the line. One pager line is counted per record.
   Example:
   `    412   0   (ogre) Thorin  killed by a giant ant on  2`
   The record written by the game that has just ended (non-winners only) is printed in standout.
   Winner lines are never highlighted.
5. If both counts are zero print a blank line and `The scoreboard is empty.`; otherwise print a
   blank line.
6. The `Press return to continue` prompt again, then the display ends.

If there are winners but no visitors, the winners list is still on screen for the second prompt.
With more than 21 records on a page the pager's more prompt and screen clear intervene.

Side effects of the visitor listing (visible only if play continued afterwards, which it never
does): every scroll and potion kind is temporarily treated as known and restored afterwards; the
wielded, worn-armour and shield slots are set to "none" and not restored.

The cause-of-death phrases on the board use the gender of the **current** player for every line.

### 5.5 The detailed listing (scores with inventories)

Same as section 5.4 with these differences: there is no prompt between the winners and the
visitors; and after each visitor line the 26 saved inventory slots of that record are printed, one
line per non-empty slot in slot order, using the item line format of section 2.1 with all potion
and scroll kinds shown as known and no "in hand / being worn" marks. One pager line is counted per
item and one more after the last item of each record.

Layout quirk: item lines begin with a newline and do not end with one. So a blank line separates
each visitor line from its first item, and the **next visitor's score line starts on the same
line as the previous record's last item**, immediately after it.

This mode overwrites the in-memory inventory with each record's inventory in turn and does not
restore it; that is harmless because it only runs from the start-up switch.

---

## 6. Causes of death

### 6.1 The table

The number is the value stored in a visitor record. "Life protection" says whether a charge of
life protection (section 7.2) can cancel that ending. "Raised by" is a pointer to where the event
originates; the rules for those events are in the parts named.

| # | Phrase | Life protection saves? | Raised by |
|---|---|---|---|
| 0 | `killed by a` / `killed by an` + monster name (section 6.2) | yes | Any monster attack that brings hit points to zero or below (Part 04) |
| 1 | `quit` | no | Never raised in this build |
| 2 | `suspended` | no | Save-and-exit (section 7.3); never displayed |
| 3 | `self - annihilated` | yes | Enveloped by one's own sphere of annihilation (Part 07) |
| 4 | `shot by an arrow` | yes | Arrow trap (Part 06) |
| 5 | `hit by a dart` | yes | Dart trap (Part 06) |
| 6 | `fell into a pit` | yes | Pit damage (Part 06) |
| 7 | `fell into a pit to HELL` | no | Pit on the bottom dungeon or bottom volcano level (Part 06) |
| 8 | `a winner` | no | Winning (section 8) |
| 9 | `trapped in solid rock` | yes | Teleport mishap (Part 06) |
| 10 | `killed by a missing save file` | no | Save file cannot be opened on restore (section 9.5) |
| 11 | `killed by an old save file` | no | Save file from another version (section 9.5) |
| 12 | `caught by the greedy cheater checker trap` | no | Impossible attributes in a restored game (section 9.6) |
| 13 | `killed by a protected save file` | no | Save file could not be deleted after restore (section 9.6) |
| 14 | `killed his family and committed suicide` | no | Out of time, or cure brought home too late (Part 08) |
| 15 | `erased by a wayward finger` | yes | Finger of death backfire (Part 07) |
| 16 | `fell through a trap door to HELL` | no | Trap door on a bottom level (Part 06) |
| 17 | `fell through a trap door` | yes | Trap door fall damage (Part 06) |
| 18 | `drank some poisonous water` | yes | Drinking from or washing at a fountain (Part 06) |
| 19 | `fried by an electric shock` | yes | A trapped closed door that fails to open (Part 06) |
| 20 | `slipped in a volcano shaft` | yes | Volcano shaft fall (Part 06) |
| 21 | `killed by a stupid act of frustration` | yes | Never raised in this build |
| 22 | `attacked by a revolting demon` | yes | Summon-demon spell turning on the caster (Part 07) |
| 23 | `hit by his own magic` | yes | Own spell damage (Part 07); also the Wild debug "damage" test |
| 24 | `demolished by an unseen attacker` | yes | A monster kill when no attacker is recorded, typically because the player is blind (section 6.2) |
| 25 | `fell into the dreadful sleep` | yes | Drinking from a fountain (Part 06) |
| 26 | `killed by an exploding chest` | yes | Chest trap (Part 06) |
| 27 | `died of internal complications` | no | Internal consistency failures: level checksum mismatch, sphere allocation failure |
| 28 | `annihilated by a sphere` | yes | Walking into, or standing too near, a sphere of annihilation (Part 07) |
| 29 | `died a post mortem death` | no | Read or write error on the save file (section 9.4) |
| 30 | `wasted by a malloc() failure` | no | Out of memory |
| 31 | `wasted by an annoyed genie` | yes | Brass lamp genie (Part 06) |
| 32 | `took an elevator straight to HELL` | no | Down elevator on a bottom level (Part 06) |
| 33 | `quit` | no | Quick quit (section 7.3); never displayed |
| 34 | `crushed by a vending machine` | yes | Wild Edition vending machine (Part 12) |

The total number of reasons is 35.

### 6.2 How the phrase is printed

The phrase always starts with a single space.

- **Reason 0 (monster):** ` killed by ` + article + space + the monster's name from the monster
  table (Part 04). The article is `an` if the name's first character is one of the lower-case
  letters a, e, i, o, u; otherwise `a`. Capitalised names always get `a`.
- **Female characters:** reason 14 prints ` killed her family and committed suicide` and reason 23
  prints ` hit by her own magic`. All other phrases are the same for both genders. Gender is the
  current player's setting from the options file (it is not stored in the save file or in the
  scoreboard).
- **All other reasons:** a space and the phrase from the table.
- Then, unless the reason is 8 (winner): ` on ` + the two-character level name, and a newline.
  For a winner just the newline.

Level names are two characters each: index 0 is a space and `H`; indexes 1 to 9 are a space and
the digit; 10 to 15 are `10` to `15`; 16 to 20 are `V1` to `V5`. Because single-digit names carry
a leading space, the text reads `on  3` (two spaces) but `on 12` and `on V2`.

**Which monster is named.** When hit points run out the game passes on the id of the monster
most recently involved in combat. Part 04 maintains it: each time a monster attacks the player, is
attacked by the player or is the target of a spell, the id is set to that monster, or to "none"
if the player is blind at that moment. If the reason is "monster" but the id is "none", the
reason is changed to 24, the unseen attacker; so a blinded player killed in melee is
`demolished by an unseen attacker`. Non-monster deaths that go through the hit-point routine also
pass that id along, but it is only displayed (and only meaningful on the board) for reason 0.

---

## 7. The end-of-game sequence

### 7.1 Hit points reaching zero

Whenever the player loses hit points through the general damage routine (Part 02):

1. Wild Edition hook: the "player hit" hook runs first and may top hit points up (Second Wind,
   Part 12).
2. The damage is subtracted. If hit points are now zero or less:
3. Beep. Print a newline, `Alas, you have died.`, a newline, and one more newline.
4. Pause 3 seconds.
5. If the reason is "killed by a monster" and no last attacker is recorded, change it to "unseen
   attacker".
6. Run the end-of-game routine of section 7.4 with the reason and the last attacker's id.

Some endings call the end-of-game routine directly, after their own message and pause, without the
"Alas" text (bottomless pits, elevators and trap doors to HELL, the dreadful sleep, solid rock,
sphere annihilation, the wayward finger, the family ending, winning; see the parts named in the
table).

### 7.2 Life protection

The character has a counter of life protections (gained from the scroll of life protection, Part
06). At the very start of the end-of-game routine, if the counter is above zero and the reason is
one marked "yes" in the table of section 6.1:

1. Reduce the counter by one.
2. Set hit points to maximum hit points.
3. Reduce constitution by one.
4. Print a newline and `You feel wiiieeeeerrrrrd all over! ` (trailing space), beep, pause 4
   seconds.
5. Return to the caller: the game continues. This is the only case in which the end-of-game
   routine returns.

Nothing else is undone: for example a player "revived" after being trapped in solid rock or
annihilated simply carries on from where the calling code left off.

No other life-saving item is handled here. (The amulet of life preservation protects against
level drain, Part 04; the Wild Edition Second Wind perk acts before hit points are subtracted,
Part 12.)

Wild Edition hook: the very first thing the end-of-game routine does, before the life-protection
test, is post a "won" or "died" event to the app (Part 12/13). The event is therefore also posted
when life protection then saves the player, and when the game is merely saved or quit.

### 7.3 Quitting and saving

**Quit command.** Prompts, after two newlines, `Do you really want to quit? (y)es, (n)o, (s)ave`
and accepts `y`, `n`, `s` or Escape (cursor shown).

- `n` or Escape: prints ` no.` and play continues (no turn is used).
- `y`: ends the game with reason 33 (quick quit).
- `s`: prints ` save.`, a newline and `Saving...`, writes the save file (section 9), then ends the
  game with reason 2 (suspended).

**Save command.** Clears the current text area, prints `Saving . . .`, writes the save file; if
that fails prints a newline and `Save game failed.`; then ends the game with reason 2 regardless.

In both save paths the wizard flag is set **after** the file has been written, purely as an
internal "do not score this" marker; it is not stored in the file.

**What reasons 2 and 33 do** (after the life-protection test, which never applies to them):

1. Delete the checkpoint file, if checkpointing is enabled (it always is).
2. Clear the text screen and print `Tidying up...`.
3. Free memory, shut the display down and exit the process with status 0.

No final score page, no scoreboard update, no scoreboard display. A quick quit writes no save and
deletes the checkpoint, and the save file the game was restored from was already deleted at load,
so the character is gone, and it never appears on the scoreboard. A failed save also deletes the
checkpoint and exits, so that character is lost too.

### 7.4 Every other ending (death and winning)

After the life-protection test has failed to save the player:

1. Delete the checkpoint file.
2. Compute the score (section 5.1); "winner" is true only for reason 8.
3. Switch to the text screen and clear it.
4. Show the final score page and its `Press return to continue` prompt (section 5.3).
5. Unless the wizard or cheater flag is set, add the score to the board (section 4.6) and remember
   which visitor record, if any, was written.
6. Show the scoreboard display (section 5.4). This happens for wizards and cheaters too.
7. Clear the text screen and print one newline.
8. Free memory, shut the display down and exit the process with status 0.

Files at this point: the save file was already deleted when the game was restored from it (section
9.5) and the checkpoint has just been deleted, so nothing of the character remains on disk. The
game core does not offer "play again"; starting a new game is the app's business (Part 10).

Upstream also contained a call, now commented out, that would have mailed letters to a winner at
this point (section 8.4).

---

## 8. Winning

### 8.1 Trigger

The only place that ends the game as a win is the player's home (Part 08): entering it while
carrying the potion of cure dianthroritis, within the time limit. After the doctor's
"the potion is... working" sequence, a beep and a 5-second pause, the end-of-game routine is called
with reason 8. If the time limit has passed the same building ends the game with reason 14
instead. The home's text is specified in Part 08.

### 8.2 What the win looks like here

There is no separate congratulation screen in this module. The sequence is exactly section 7.4:
the final score page, on which line 5 reads the name followed by ` a winner` (with no level), then
the scoreboard display, starting with the winners list.

Scoring differences for a winner are in section 5.1 (no 10% deduction, 100,000 per difficulty
level added before the halving, doubling for the straight-to-the-volcano case). Board differences
are in section 4.6 (the player's visitor records are blanked; the winner board is used; time used
is recorded).

### 8.3 Taxes

Winning creates a tax debt that follows the **user id** into later games.

- **Assessment.** When a win is recorded: taxes = outstanding taxes + 5% of (gold carried + gold
  in the bank), truncated. Items and LarnCoin are not taxed. The figure is stored in the player's
  winner record (even when the record's score is not replaced). It is only stored if a winner
  record was chosen in step 3 of section 4.6.
- **Loading.** When a **new** game starts (not on restore), the board is read: if any winner
  record with a positive score belongs to this user id, the new game's difficulty is set to that
  record's difficulty plus one and the outstanding-tax figure is loaded from the record. Otherwise
  difficulty is 0 and the board's taxes are not consulted. (A difficulty requested on the command
  line then applies only if the player has never won, or if it is higher than the automatic one;
  Part 01.) If the board cannot be read the player counts as never having won.
- **Carrying.** The outstanding-tax figure is part of the save file (section 9.2) and is restored
  from it; the board is not re-read on restore.
- **Paying.** Paying an amount x (from the tax office, Part 08, or the Wild Edition auditor, Part
  12): if x is zero or negative, or the board cannot be read, nothing is paid. Otherwise find the
  first winner record with a positive score owned by this user id; the amount paid is the smaller
  of x and that record's taxes; subtract it from the record and from the in-memory figure, rewrite
  the board and report the amount paid (the caller deducts that much gold). If there is no such
  record nothing is paid, so a debt that exists only in a save file (for example after the board
  was deleted) can never be cleared.
- **Checking.** Other parts read the in-memory figure: the DND store and the bank refuse service
  while it is above zero, the tax office shows it and refuses to deal with cheaters, and a command
  prints either `You presently owe N gp in taxes.` or `You do not owe any taxes.` (Parts 01
  and 08).

### 8.4 The mail letters (not built)

Upstream Ularn has a file that, after a win and if the "mail" option was on, sent the player six
e-mails through the system mailer. That file is **not compiled into the Android build**, the call
to it in the end-of-game routine is commented out, and it depends on output helpers that no longer
exist. Nothing of it is visible in the game. For the record, it would have forked a background
process that wrote each letter to a temporary file, waited 20 seconds between letters, mailed it
to the player's login and deleted the file. The six letters were:

1. From the Ularn Revenue Service, subject "undeclared income": congratulations, a statement of
   how much gold the player brought back, and the tax bill, due within 5 days.
2. From King Wilfred, subject "a noble deed": declares a national holiday and invites the player
   to be made Knight (male) or Lady (female) of the Realm.
3. From Count Endelford: an insulting, jealous threat.
4. From the Duke of Ularnty (Duke Mainair), subject "High Praise": praise, and an offer to match
   any charitable gift.
5. From St. Mary's Children's Home, subject "these poor children": an appeal for a donation
   ("your gift is tax-deductible").
6. From the National Dianthroritis Society of Ularn, subject "hope": an appeal for research
   donations.

A recreation can leave these out entirely.

---

## 9. The save file

### 9.1 Files and names

All three files are named after the login name, which the Android front end fixes to `player`,
and live in the game's home directory (on Android: the folder of the active save slot; Part 10).

| File | Name | Written | Read | Deleted |
|---|---|---|---|---|
| Save file | `ularn_player.sav` | By the save command and by quit-then-save | Automatically at start-up if it exists and no checkpoint was restored | Immediately after it has been loaded; also when it is refused as an old version |
| Checkpoint | `ularn_player.ckp` | Every 25th command prompt, and whenever the app sends the checkpoint key (code 29, Ctrl-]) while the game is at the command prompt | Only when the game is started with the restore-checkpoint switch (`-r`) | At the end of every game: death, win, quick quit and save-and-exit alike; also when refused as an old version |

The checkpoint has exactly the same format as the save file and is written by the same routine.
Upstream 1.6.3a could restore and delete checkpoints but never wrote one; the periodic and
on-request writes are an Android front-end addition (Part 09). Writing a checkpoint has the same
side effects as saving (section 9.3, steps 1 and 2).

Start-up order (Part 01 has the surrounding detail):

1. With the restore-checkpoint switch: if the checkpoint does not exist print
   `Cannot find checkpoint file PATH` and exit. Otherwise print `Restoring...` and load it. The
   checkpoint file is **not** deleted by loading it; an ordinary save file, if any, is then
   ignored and left alone.
2. Otherwise, if the save file exists: print `Restoring...` and load it; it is deleted as part of
   loading.
3. Otherwise a new character is created.

### 9.2 Layout

Everything is raw native binary (little-endian on all supported devices), with no field separators.
"Long" is the platform long integer: 8 bytes on 64-bit builds, 4 bytes on 32-bit builds, so save
files are not interchangeable between those builds. "Int" is 4 bytes, "short" 2 bytes. The
**Chunks** column matters: the checksum (section 9.4) depends on how the bytes are divided into
separate write operations, and the reader must divide them identically.

| # | Content | Type and count | Bytes (64-bit) | Chunks |
|---|---|---|---|---|
| **Wild Edition header** | | | | |
| 1 | Magic text `ULRNWILD` (no terminator) | 8 characters | 8 | 1 |
| 2 | Save format version, currently 2 | 32-bit int | 4 | 1 |
| **Player block** | | | | |
| 3 | Character class name, NUL-padded | 20 characters | 20 | 1 |
| 4 | "Rambo" flag (character holds the non-sellable lance) | 1 byte | 1 | 1 |
| 5 | Wizard flag | 1 byte | 1 | 1 |
| 6 | Cheater flag | 1 byte | 1 | 1 |
| 7 | Player x position | int | 4 | 1 |
| 8 | Player y position | int | 4 | 1 |
| 9 | Wall-clock time the game was started | long | 8 | 1 |
| 10 | Game time in turns | long | 8 | 1 |
| 11 | Outstanding taxes | long | 8 | 1 |
| 12 | The whole character attribute array (Part 02): 121 entries, including the Wild Edition entries at indexes 101 to 120 and the app's run id at index 100 and its kill count at index 99 (two otherwise unused entries) | 121 longs | 968 | 1 |
| 13 | Inventory object ids, slots a to z | 26 bytes | 26 | 1 |
| 14 | Inventory arguments, slots a to z | 26 shorts | 52 | 1 |
| 15 | Potion-kind known flags | 35 ints | 140 | 1 |
| 16 | Scroll-kind known flags | 28 ints | 112 | 1 |
| 17 | Spell known flags | 39 ints | 156 | 1 |
| **Levels block** | | | | |
| 18 | Current level index (0 to 20) | int | 4 | 1 |
| 19 | Per-level "been here" bytes, levels 0 to 20 (non-zero = visited; the value also counts items stolen on that level, Part 05) | 21 bytes | 21 | 1 |
| 20 | For each level 0 to 20 whose "been here" byte is non-zero, in level order: one stored level (see below) | 36,450 bytes each | 36,450 × visited levels | 1 per level |
| **Stores block** | | | | |
| 21 | DND store: remaining quantity of each of the 99 stock entries (Part 08 order) | 99 bytes | 99 | 99 (one per byte) |
| 22 | College: course-taken flags for the 8 courses | 8 bytes | 8 | 1 |
| 23 | Bank: game time at which interest was last credited | long | 8 | 1 |
| 24 | Dealer McDope: sold-out flags for the 5 drugs | 5 bytes | 5 | 1 |
| **Monster block** | | | | |
| 25 | Random-monster generation timer | 1 byte | 1 | 1 |
| 26 | Genocide flag (0 or 1) for monster ids 0 to 60 | 61 bytes | 61 | 61 (one per byte) |
| **Spheres block** | | | | |
| 27 | Number of active spheres of annihilation | int | 4 | 1 |
| 28 | For each sphere, in list order: a link pointer (meaningless on disk; ignored on load), then x, y, level, direction, lifetime as shorts, then padding | 24 bytes each (16 on 32-bit) | 24 × spheres | 1 per sphere |
| **Wild Edition trailer** | | | | |
| 29 | Trailer magic: the 32-bit value 0x444C4957, which is the bytes `W`, `I`, `L`, `D` in file order | 32-bit int | 4 | 1 |
| 30 | Number of kill counters that follow: 70 (one per monster id, 0 to 69) | 32-bit int | 4 | 1 |
| 31 | Kills of each monster species by this character | 70 × 32-bit int | 280 | 70 (one per value) |
| 32 | Number of extra values that follow: 64 | 32-bit int | 4 | 1 |
| 33 | The 64 Wild Edition "extra" save values (Part 12 assigns their meanings) | 64 × 32-bit int | 256 | 64 (one per value) |
| **Checksum** | | | | |
| 34 | The running checksum of everything above | int | 4 | 1 |

A 64-bit save is therefore 2,284 bytes plus 36,450 per visited level plus 24 per sphere (a fresh
character standing on the home level: 38,734 bytes). A 32-bit save is 1,784 bytes plus 36,450 per
visited level plus 16 per sphere.

**One stored level** (item 20) is five arrays of 67 × 17 = 1,139 cells each, every array ordered
with x as the outer index (0 to 66) and y as the inner index (0 to 16):

| Offset | Array | Cell |
|---|---|---|
| 0 | Monster hit points | short |
| 2,278 | Monster record | 26 bytes: monster id (1 byte), number of stolen items held (1 byte), then six entries of object id (1 byte), one padding byte, argument (short) |
| 31,892 | Object id on the floor | 1 byte |
| 33,031 | (one padding byte) | |
| 33,032 | Object argument | short |
| 35,310 | What the player knows to be there | 1 byte |
| 36,449 | (one padding byte) | |

Things that are **not** in the save file and are rebuilt or lost on load: the player's display
name and gender (read from the options file at every start), the per-cell monster "awake/seen" and
"moved" flags, the difficulty scaling of the monster table (re-applied at start-up from the saved
difficulty), genocide flags for monster ids 61 and above, the last-attacker record, and any Wild
Edition state not held in the attribute array or the trailer.

On load the spheres are pushed onto the front of the list as they are read, so the list order
reverses on every save/load cycle.

A re-implementation that does not need to exchange files with the original may use any encoding;
what it must preserve is the *set* of saved state, the header check, a tamper check with the same
consequences, and the load-time rules of sections 9.5 and 9.6.

### 9.3 Writing

1. Copy the current level into its storage slot.
2. Bring bank interest up to date (Part 08). This means every save **and every checkpoint**
   credits interest for the whole mobuls elapsed since the last credit.
3. Open the file for writing, replacing any existing file. On failure print
   `Can't open file <PATH> to save game` and return failure (the save command then reports
   `Save game failed.`; a checkpoint failure is silent apart from this line).
4. Reset the running checksum to zero and write items 1 to 33 in order.
5. Write the running checksum as item 34 and close the file.

Any short write prints four lines, `Error writing to save file`, `wrote N, wanted M`,
`    Wrote T bytes so far` and `        Call: K`, pauses 4 seconds and ends the game with reason 29
(post mortem death).

### 9.4 The checksum

- For one chunk of bytes: start with 0; for each byte in order, rotate the 16-bit value right by
  one bit (the low bit moves to the top bit), add the byte's value (0 to 255), and keep the low 16
  bits. This is the classic BSD "sum" algorithm.
- The running checksum is the ordinary integer **sum of the per-chunk checksums** of every chunk
  written (or read) so far. Each chunk's checksum starts again from 0, which is why the chunk
  boundaries in the table matter: the same bytes split differently give a different total.
- The value stored as item 34 is the running total just before item 34 itself is written. On load
  the running total after item 33 is compared with the stored value.

The same per-chunk function also guards levels in memory: each time a level is put into storage
its checksum is remembered, and each time it is taken out the checksum is recomputed and compared.
A mismatch prints `OH NO!!!! INTERNAL MEMORY CORRUPTION!!!!` and
`(sum A of level L does not match saved sum B)`, beeps, pauses 5 seconds and ends the game with
reason 27. These remembered sums are not saved; after a restore the first retrieval of each level
just records its sum.

### 9.5 Loading

1. Open the file. On failure print `Can't open file <PATH> to restore game`, pause 4 seconds, set
   gold and bank balance to zero and end the game with reason 10.
2. Print ` Reading data...` (leading space; it follows the `Restoring...` already on screen).
   Allocate level storage and reset the running checksum.
3. **Header check.** Read 8 bytes and a 32-bit integer. If the bytes are not exactly `ULRNWILD` or
   the integer is not 2: close the file, **delete it**, print on two lines
   `That saved game is from a different version of Ularn and can't be loaded.` and
   `It has been removed. Please start a new game.`, pause 4 seconds, set gold and bank balance to
   zero and end the game with reason 11. This is how saves from the base game, from upstream Ularn
   and from earlier Wild Edition formats are refused: there is no conversion.
4. Read the player, levels, stores, monster and spheres blocks (items 3 to 28). After the player
   block the class name is matched case-insensitively against the class table (lower-casing the
   stored name in place, see the quirk in section 5.3); if it matches no class it is reset to the
   first class's name.
5. **Trailer.** Read the trailer magic; if it is wrong, stop reading the trailer. Otherwise read
   the kill-counter count; if it is negative or above 4,096, stop. Otherwise clear the kill
   counters, read that many values, keeping those with an index below 70 and discarding the rest;
   then read the extra-value count with the same 0 to 4,096 rule, clear the extra values and read
   them, keeping indexes below 64. Any "stop" marks the save as altered (step 7's consequence).
6. Compare the running checksum with the stored one (item 34).
7. **Altered save.** If the trailer was bad or the checksums differ, and the cheater flag is not
   already set: print, on three lines,
   `Sorry but your savefile has been altered.`,
   `However, since I am a good sport, I will let you play.` and
   `Be advised, though, that you won't be placed on the scoreboard.`, set the cheater flag and
   pause 4 seconds. Play continues with whatever was read.
8. Close the file and reset the "previous position" memory.
9. **Removal.** If the file is the checkpoint file, try to open it for appending; if that fails,
   apply the protected-file rule below; the checkpoint is kept. For any other file, delete it; if
   deletion fails, apply the protected-file rule.
10. Run the greedy checks (section 9.6).
11. Experience fix: if the character is exactly experience level 25 and has more experience than
    level 25 normally allows (8,000,000), reduce experience to that figure and re-award the excess
    through the normal experience routine so the level catches up (Part 02).
12. Load the current level from storage into the live map.

A short read at any point prints `Error reading from save file`, `  Got N, wanted M bytes`,
`    Read T bytes so far` (T is actually the write counter, so it normally shows 0) and
`        Call: K`, pauses 4 seconds and ends the game with reason 29. Because the end-of-game
routine then scores whatever was loaded so far, a truncated file produces a final score page and
possibly a scoreboard entry reading `died a post mortem death`.

When a load fails before a character exists (reasons 10 and 11) the final score page still
appears, with class `<unknown>` and a slightly negative score (minus 12 by the formula of section
5.1, since the 26 empty slots each count minus 1; derived from the formula, not observed).

### 9.6 Cheat checks

These are the only anti-cheat measures in this build. There are **no** checks of the file's
modification time, hard-link count, owner or inode, and the save file is not tied to the user id.

| Check | When | Consequence |
|---|---|---|
| Checksum / trailer | Load, steps 5 to 7 | Cheater flag set; play continues; no scoreboard entry |
| Save deleted on load | Load, step 9 | A save can be used once. Copying it elsewhere beforehand is not detected by the game core |
| Protected file | Load, step 9, if the save cannot be deleted (or the checkpoint cannot be opened for append) | Skipped for wizards and for already-flagged cheaters. Otherwise print, on four lines, `Sorry but your savefile can't be deleted.  This can only mean`, `that you tried to CHEAT by protecting the directory the savefile`, `is in.  Since this is unfair to the rest of the Ularn community, I` and `cannot let you play this game.`; pause 5 seconds; zero gold and bank; end the game with reason 13 |
| Greedy: single ability | Load, step 10: any of strength, intelligence, wisdom, constitution, dexterity above 300 (charisma is not examined) | Greedy rule below |
| Greedy: ability total | The sum of those five above 600 | Greedy rule below |
| Greedy: hit points / spells | Maximum hit points above 999, or maximum spell points above 125 | Greedy rule below |

**Greedy rule.** Skipped for wizards. Otherwise print two newlines and, on four lines,
`I am so sorry but your character is a little TOO good!  Since this`,
`cannot normally happen from an honest game, I must assume that you cheated.`,
`Since you are GREEDY as well as a CHEATER, I cannot allow this game` and `to continue.`;
pause 5 seconds; set the cheater flag; zero gold and bank; end the game with reason 12.

Note that these limits apply only at load time; a character that legitimately exceeds them (for
example more than 999 maximum hit points) is killed by the greedy check the next time the game is
restored, including from a checkpoint.

---

## 10. Wizard and cheater flags

Both are single bytes stored in the save file (items 5 and 6).

**Wizard flag.** Turned on by the toggle-wizard and debug-mode commands after the password
`fizban` (Part 01), turned off by the toggle command or the Wild debug menu. Effects in this
part:

- Item names show the raw argument and always show an enchantment suffix (sections 2.1, 2.3).
- The final score page adds ` (wizard)`.
- No scoreboard entry is made.
- The greedy and protected-file checks are skipped.
- The save paths set it just before exiting so that nothing is scored (section 7.3).

**Cheater flag.** Turned on by an altered save (section 9.5 step 7) or the greedy rule. It is
permanent for that character because it is saved. Effects in this part:

- The final score page adds ` (cheater)`.
- No scoreboard entry is made (tested both before calling the board update and inside it).
- An already-flagged cheater is not warned again about altered saves and is exempt from the
  protected-file rule.

Elsewhere the tax office refuses to take payment from a cheater with outstanding taxes and the
version command shows the flags (Parts 01 and 08).

---

## 11. Message strings printed by this part

Exact text; "NL" marks where a newline is printed. Placeholders are in capitals.

**Lists and item names**

| Text | Where |
|---|---|
| NL `  --- press ` `space` (standout) ` to continue --- ` | More prompt |
| `.)   N gold piece.` / `.)   N gold pieces.` | Inventory gold line |
| NL `X)   NAME` + suffixes | Item line; X is the slot letter |
| ` +N` / ` -N` | Enchantment suffix |
| ` of` + kind name | Known potion or scroll |
| ` [ N ]` | Wizard-mode raw argument |
| ` (weapon in hand)` | Wielded slot |
| ` (being worn)` | Worn armour or shield slot |
| ` (on your head)` | Wild Edition: worn hat slot |
| NL `Elapsed time is U.  You have L mobuls left.` (or `mobul` when L is 1) | End of inventory |
| `The magic spells you have discovered to date are:` NL NL | Known-magic screen |
| NL `The magic scrolls you have found to date are:` NL NL | Known-magic screen |
| NL `The magic potions you have found to date are:` NL NL | Known-magic screen |
| `Availible spells are:` NL NL | All-spells form (never shown) |
| NL `Your perks are:` NL NL | Wild Edition section |
| NL `Your pet:` NL NL | Wild Edition section |
| NL `The town bounty board:` NL NL | Wild Edition section |
| NL `What do you want to VERB [LETTERS * for all] ?` (with ` - for none` before `]` for wield) | Item prompt |
| ` aborted.` | Escape at the item prompt |

**Scoreboard**

| Text | Where |
|---|---|
| `Can't open scorefile 'PATH' for reading` NL | Read failure |
| `Can't read score board header` NL | Read failure |
| `Not a valid ularn score board` NL | Read failure |
| `Incorrect score board version` NL | Read failure |
| `Can't read scoreboard` NL (no newline after the second, winner-array, variant) | Read failure |
| `Can't open scorefile for writing` NL | Write failure |
| `Can't write scorefile` NL | Write failure |
| `   Score       Diff   Time Needed  Ularn Winners List` NL | Winners heading |
| `  Score Diff  Ularn Visitor Log` NL | Visitors heading |
| NL `The scoreboard is empty.` NL | No entries at all |
| NL `Press return to continue` | After the final score page and in the board display |
| `Preparing to initialize the scoreboard.` NL | Create-board switch |
| NL `Enter Password: ` / NL `Sorry.` NL | Password prompt and refusal |
| `Scoreboard creation failed!!` NL | Create-board switch |
| NL `Scoreboard initialized.` NL | Create-board switch |
| `I can't create the scoreboard.` NL `Check permissions on DIR` NL | Start-up |

**Final score page**

| Text | Where |
|---|---|
| 63 hyphens NL | Rule (three times; the last without NL) |
| 17 spaces + `U L A R N    S C O R E S` NL | Title |
| `Score: S  Diff: D  ` + `Level: TITLE  Char: CLASS` NL | Summary line |
| tab + NAME + phrase | Name line |
| ` killed by a MONSTER` / ` killed by an MONSTER` | Phrase, reason 0 |
| ` killed her family and committed suicide` / ` hit by her own magic` | Female variants |
| ` on LV` NL | Level suffix (not for winners) |
| ` (wizard)` / ` (cheater)` | Flag tags |
| `<unknown>` | Class when none is set |
| The 35 phrases of section 6.1 | |

**End of game**

| Text | Where |
|---|---|
| NL `Alas, you have died.` NL NL | Hit points exhausted |
| NL `You feel wiiieeeeerrrrrd all over! ` | Life protection used |
| `Tidying up...` NL | Quick quit and save-and-exit |
| NL NL `Do you really want to quit? (y)es, (n)o, (s)ave` | Quit prompt |
| ` no.` / ` save.` NL `Saving...` | Quit prompt answers |
| `Saving . . .` / NL `Save game failed.` NL | Save command |

**Save and restore**

| Text | Where |
|---|---|
| `Can't open file <PATH> to save game` NL | Save |
| `Error writing to save file` NL `wrote N, wanted M` NL `    Wrote T bytes so far` NL `        Call: K` NL | Write error |
| `Restoring...` | Start-up, before loading |
| `Cannot find checkpoint file PATH` NL | Restore-checkpoint switch |
| `Can't open file <PATH> to restore game` NL | Load |
| ` Reading data...` | Load |
| NL `That saved game is from a different version of Ularn and can't be loaded.` NL `It has been removed. Please start a new game.` NL | Header check |
| `Error reading from save file` NL `  Got N, wanted M bytes` NL `    Read T bytes so far` NL `        Call: K` NL | Read error |
| NL `Sorry but your savefile has been altered.` NL `However, since I am a good sport, I will let you play.` NL `Be advised, though, that you won't be placed on the scoreboard.` | Altered save |
| NL `Sorry but your savefile can't be deleted.  This can only mean` NL `that you tried to CHEAT by protecting the directory the savefile` NL `is in.  Since this is unfair to the rest of the Ularn community, I` NL `cannot let you play this game.` NL | Protected file |
| NL NL `I am so sorry but your character is a little TOO good!  Since this` NL `cannot normally happen from an honest game, I must assume that you cheated.` NL `Since you are GREEDY as well as a CHEATER, I cannot allow this game` NL `to continue.` NL | Greedy rule |
| NL `OH NO!!!! INTERNAL MEMORY CORRUPTION!!!!` NL `(sum A of level L does not match saved sum B)` NL | In-memory level check |

---

## 12. Summary of quirks to reproduce

1. The score never includes a depth bonus and net worth is always halved (section 5.1).
2. Empty inventory slots and unpriced objects each subtract 1 from net worth (section 5.1).
3. The final score page shows the experience title of the *next* level (section 5.3).
4. Class names are lower-cased by a restore (section 5.3).
5. One visitor record and one winner record per numeric user id; on Android that means one of
   each for the whole installation (section 4.4).
6. A harder game replaces a better score (section 4.6).
7. Quitting leaves no scoreboard entry and no files; a failed save also loses the character
   (section 7.3).
8. The eat list never shows cookies; the wield list never shows objects with id 93 or above
   (section 3.2).
9. In the detailed score listing each following score line is appended to the previous record's
   last item line (section 5.5).
10. Winners are never highlighted on the board; a non-winner's freshly written record is (section
    5.4).
11. The "died" event reaches the app before the life-protection test (section 7.2).
12. Greedy limits are enforced only at load time, and charisma is exempt (section 9.6).
13. The checksum depends on chunk boundaries; the DND quantities, genocide flags, kill counters
    and extra values are each written one value per chunk (sections 9.2, 9.4).
14. Saving and checkpointing credit bank interest (section 9.3).
15. A debt recorded only in a save file cannot be paid if the user has no winner record (section
    8.3).
