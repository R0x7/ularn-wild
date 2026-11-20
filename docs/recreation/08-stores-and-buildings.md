# Part 08 — Stores and buildings: DND store, bank, trading post, college, LRS, home, dealer

This part specifies every "walk-in" building screen of the game: the DND store (the "Ularn Thrift Shoppe"), the College of Ularn, the First National Bank of Ularn and its 8th-level branch, the Ularn Trading Post, the Ularn Revenue Service (LRS) office, the player's home, and Dealer McDope's pad. It also covers the bank-interest routine, the item valuation routine that the trading post and the final score share, and the block of store data written to the save file.

It leans on other parts as follows. Part 01 owns the turn loop and the "show taxes" command. Part 02 owns the player's attributes, the carrying limit, the regeneration tick and the timed-effect counters. Part 03 owns the inventory line format, the scoreboard (where the tax bill is stored), death and winning, and the save-file container. Part 05 owns where the buildings are placed. Part 06 owns what the purchased items do (including the drugs). Part 09 owns the text display and input primitives. Part 12 owns everything behind the Wild Edition hooks that are only noted here in one line each.

No random numbers are used anywhere in this part: every outcome in every building is deterministic. (For reference, the game's helpers are rnd(n) = uniform integer 1..n and rund(n) = uniform integer 0..n-1; none of the building code calls them.)

Conventions used below:

- Time is counted in game time units; 100 time units are one **mobul**. The time limit of the game is 40000 time units (400 mobuls).
- "Gold" means gold carried; "bank balance" is the separate bank account figure.
- In quoted screen text the mark ⏎ stands for a line break. Words shown as **[bold in brackets]** are printed in the "standout" (highlighted) text style.
- All divisions are integer divisions that truncate toward zero unless stated otherwise.

## 1. Common screen and input behaviour

### 1.1 The text screen

Every building takes over the whole display with a full-screen text page (the "text display mode") and returns to the map display on exit. The text page is 80 columns by 25 lines. Cursor positions are given as (column, line), both starting at 1; a position outside the page is clamped to it. Printing a line break moves to column 1 of the next line; printing past the last line scrolls the whole page up one line. A tab advances to the next tab stop; tab stops are at columns 1, 9, 17, 25, 33, 41, 49, 57, 65, 73. Entering a building clears the page (the trading post and the home screens also clear it explicitly).

Three clearing operations are used: clear the whole page (cursor to 1,1); clear from the cursor to the end of the current line (cursor does not move); and clear from a given (column, line) to the end of that line plus all lines below it (cursor does not move).

Pauses are real-time delays given in milliseconds; the screen is refreshed before each pause.

### 1.2 Single-key prompts

A single-key prompt prints its prompt text and then waits until the player presses one of an explicit set of accepted keys; all other keys are ignored silently. The key is **not** echoed by the prompt itself; where a building wants the key or a word shown, it prints it afterwards (noted in each section).

Several buildings (DND store, college, dealer) test for key code 12 (a "redraw screen" control key) and would redraw their page if they saw it. In this port that key is never in the accepted set, so the redraw branch can never run. A rebuild may omit it.

### 1.3 Yes/no prompts

The yes/no helper accepts the keys y, Y, n, N, Escape and space, and reports space as Escape. Every building that asks a yes/no question tests the answer for lower-case **y** only. Upper-case Y therefore counts as "no", as do n, N, Escape and space.

### 1.4 Number entry

The bank and the LRS ask "How much?" with a number-entry helper that is given a default value (all the gold carried, or the whole bank balance). Its rules:

1. Digits 0-9 are appended to the number and echoed.
2. A minus sign is accepted only as the very first character; it is echoed and makes the result negative.
3. Backspace removes the last character typed (the minus sign or the last digit) and erases it on screen.
4. An asterisk (*) ends entry at once and returns the default value, whatever has been typed.
5. Escape ends entry at once and returns the default value. (So pressing Escape at "How much?" in the bank deposits everything or withdraws everything; it does not cancel.)
6. Enter ends entry. If nothing was typed, the default value is returned; otherwise the typed number (negated if a minus was typed).
7. There is no limit on the number of digits; the value is a 32-bit signed integer and is not protected against overflow.

### 1.5 Entering a building

Buildings are map objects. When the player's square holds a building object the object handler runs (Part 06 describes the handler in general). For every building in this part:

1. If time is stopped, nothing happens at all.
2. If any monster stands on one of the nine squares centred on the player (the player's square and its eight neighbours, clipped to the map), nothing happens: the building cannot be entered, and no message is printed.
3. Otherwise an announcement and a go-in prompt are printed in the map message area, and the building function is called only on the "go in" key.

| Building | Announcement | Prompt | Accepted keys | Enter on | Text printed when not entering |
|---|---|---|---|---|---|
| College | `You have found the College of Ularn.` (preceded by a blank line) | `Do you (g) go inside, or (i) stay here? ` | g, i, Escape | g | ` stay here.` |
| Bank (home level) | `You have found the bank of Ularn.` (preceded by a blank line) | same | g, i, Escape | g | ` stay here.` |
| Bank branch (level 8) | `You have found a branch office of the bank of Ularn.` (preceded by a blank line) | same | g, i, Escape | g | ` stay here.` |
| DND store | `There is a DND store here.` (preceded by a blank line) | same | g, i, Escape | g | ` stay here.` |
| LRS office | `There is an LRS office here.` (preceded by a blank line) | same | g, i, Escape | g | ` stay here.` |
| Trading post | `You have found the Ularn trading Post.` | same | g, i, Escape | g | `stay here.` (no leading space) |
| Home | `You have found your way home.` | same | g, i, Escape | g | `stay here.` (no leading space) |
| Dealer McDope's pad | `You have found Dealer McDope's Hideout!` | `Do you (c) check it out, or (i) ignore it? ` | c, i, Escape | c | `forget it.` |

Each announcement and each prompt starts on a new line. Escape behaves like "i".

The home-level buildings (DND store, college, bank, home, trading post, LRS) are placed on level 0; the bank branch is placed on dungeon level 8; the dealer's pad is placed at most once per game on a dungeon level of depth 4 or more (see Part 05 for the placement rules).

While a building screen is open no game time passes and no monsters move; the only building that advances the clock is the college (section 4).

### 1.6 What owing taxes blocks

If the player's outstanding tax bill is greater than zero:

- the DND store refuses to serve (section 2.2);
- both bank offices refuse to serve (section 3.2).

Nothing else in this part is blocked: the trading post, the college, the home, the dealer and the LRS itself work normally. (Wild Edition hook: while taxes are owed a tax auditor can appear on the home level and the LarnCoin cash machine still works, see Part 12.)

## 2. The DND store ("Ularn Thrift Shoppe")

### 2.1 Stock table

The store has one fixed table. Each entry has a table price, an item kind, an item argument and a stock quantity. The **shelf price** shown and charged is the table price times 10. The table price itself is also the base figure used by the trading post (section 5). The order of the table matters: it fixes the page and letter of each item, the order of the stock bytes in the save file, and the price lookup for potions and scrolls in the trading post.

Stock quantities are per game: they start at the values below, go down by one per purchase, never go up (selling an item at the trading post does not restock the store), and are stored in the save file. Entries whose starting stock is 0 can never be bought; they exist only so the trading post can price them.

There is no price variation by difficulty level, by character level or by anything else in the base game. The only effect of taxes is the total refusal in section 2.2.

The name shown is the object's standard name (Part 06 item table), except that potions are shown as "potion of X" and scrolls as "scroll of X" using the real potion or scroll name, whether or not the player has identified that kind yet.

| Index | Page | Letter | Name shown | Argument | Shelf price | Starting stock |
|---|---|---|---|---|---|---|
| 0 | 1 | a | leather armor | 0 | 20 | 3 |
| 1 | 1 | b | studded leather armor | 0 | 100 | 2 |
| 2 | 1 | c | ring mail | 0 | 400 | 2 |
| 3 | 1 | d | chain mail | 0 | 850 | 2 |
| 4 | 1 | e | splint mail | 0 | 2200 | 1 |
| 5 | 1 | f | plate mail | 0 | 4000 | 1 |
| 6 | 1 | g | plate armor | 0 | 9000 | 1 |
| 7 | 1 | h | stainless plate armor | 0 | 26000 | 1 |
| 8 | 1 | i | a shield | 0 | 1500 | 1 |
| 9 | 1 | j | elven chain | 0 | 50000 | 0 |
| 10 | 1 | k | an orb of enlightenment | 0 | 10000 | 0 |
| 11 | 1 | l | Slayer | 0 | 100000 | 0 |
| 12 | 1 | m | a dagger | 0 | 20 | 3 |
| 13 | 1 | n | a spear | 0 | 200 | 3 |
| 14 | 1 | o | a flail | 0 | 800 | 2 |
| 15 | 1 | p | a battle axe | 0 | 1500 | 2 |
| 16 | 1 | q | a longsword | 0 | 4500 | 2 |
| 17 | 1 | r | a two-handed sword | 0 | 10000 | 2 |
| 18 | 1 | s | a sunsword | 0 | 50000 | 1 |
| 19 | 1 | t | a lance of death | 0 | 200000 | 1 |
| 20 | 1 | u | a sword of slashing | 0 | 20000 | 0 |
| 21 | 1 | v | Bessman's flailing hammer | 0 | 75000 | 0 |
| 22 | 1 | w | a ring of protection | 1 | 1500 | 1 |
| 23 | 1 | x | a ring of strength | 1 | 850 | 1 |
| 24 | 1 | y | a ring of dexterity | 1 | 1200 | 1 |
| 25 | 1 | z | a ring of cleverness | 1 | 1200 | 1 |
| 26 | 2 | a | an energy ring | 0 | 1800 | 1 |
| 27 | 2 | b | a ring of increase damage | 0 | 1250 | 1 |
| 28 | 2 | c | a ring of regeneration | 0 | 2200 | 1 |
| 29 | 2 | d | a ring of extra regeneration | 0 | 10000 | 1 |
| 30 | 2 | e | a belt of striking | 0 | 2800 | 1 |
| 31 | 2 | f | an amulet of invisibility | 5 | 4000 | 1 |
| 32 | 2 | g | a cube of undead control | 0 | 5000 | 0 |
| 33 | 2 | h | a device of theft-prevention | 0 | 6000 | 0 |
| 34 | 2 | i | a chest | 3 | 5900 | 1 |
| 35 | 2 | j | a book | 2 | 2000 | 1 |
| 36 | 2 | k | a fortune cookie | 0 | 100 | 3 |
| 37 | 2 | l | The Hand of Fear | 0 | 6660 | 0 |
| 38 | 2 | m | potion of sleep | potion 0 | 200 | 6 |
| 39 | 2 | n | potion of healing | potion 1 | 900 | 5 |
| 40 | 2 | o | potion of raise level | potion 2 | 5200 | 1 |
| 41 | 2 | p | potion of increase ability | potion 3 | 1000 | 2 |
| 42 | 2 | q | potion of wisdom | potion 4 | 500 | 2 |
| 43 | 2 | r | potion of strength | potion 5 | 1500 | 2 |
| 44 | 2 | s | potion of raise charisma | potion 6 | 700 | 1 |
| 45 | 2 | t | potion of dizziness | potion 7 | 300 | 7 |
| 46 | 2 | u | potion of learning | potion 8 | 2000 | 1 |
| 47 | 2 | v | potion of gold detection | potion 9 | 500 | 1 |
| 48 | 2 | w | potion of monster detection | potion 10 | 800 | 1 |
| 49 | 2 | x | potion of forgetfulness | potion 11 | 300 | 3 |
| 50 | 2 | y | potion of water | potion 12 | 200 | 5 |
| 51 | 2 | z | potion of blindness | potion 13 | 400 | 3 |
| 52 | 3 | a | potion of confusion | potion 14 | 350 | 2 |
| 53 | 3 | b | potion of heroism | potion 15 | 5200 | 1 |
| 54 | 3 | c | potion of sturdiness | potion 16 | 900 | 2 |
| 55 | 3 | d | potion of giant strength | potion 17 | 2000 | 2 |
| 56 | 3 | e | potion of fire resistance | potion 18 | 2200 | 4 |
| 57 | 3 | f | potion of treasure finding | potion 19 | 800 | 6 |
| 58 | 3 | g | potion of instant healing | potion 20 | 3700 | 3 |
| 59 | 3 | h | potion of poison | potion 22 | 500 | 1 |
| 60 | 3 | i | potion of see invisible | potion 23 | 1500 | 3 |
| 61 | 3 | j | an orb of dragon slaying | 0 | 8500 | 0 |
| 62 | 3 | k | a scarab of negate spirit | 0 | 7500 | 0 |
| 63 | 3 | l | the Vorpal Blade | 0 | 80000 | 0 |
| 64 | 3 | m | scroll of enchant armor | scroll 0 | 1000 | 2 |
| 65 | 3 | n | scroll of enchant weapon | scroll 1 | 1250 | 2 |
| 66 | 3 | o | scroll of enlightenment | scroll 2 | 600 | 4 |
| 67 | 3 | p | scroll of blank paper | scroll 3 | 100 | 4 |
| 68 | 3 | q | scroll of create monster | scroll 4 | 1000 | 3 |
| 69 | 3 | r | scroll of create artifact | scroll 5 | 2000 | 2 |
| 70 | 3 | s | scroll of aggravate monsters | scroll 6 | 1100 | 1 |
| 71 | 3 | t | scroll of time warp | scroll 7 | 5000 | 2 |
| 72 | 3 | u | scroll of teleportation | scroll 8 | 2000 | 2 |
| 73 | 3 | v | scroll of expanded awareness | scroll 9 | 2500 | 4 |
| 74 | 3 | w | scroll of haste monsters | scroll 10 | 200 | 5 |
| 75 | 3 | x | scroll of monster healing | scroll 11 | 300 | 3 |
| 76 | 3 | y | scroll of spirit protection | scroll 12 | 3400 | 1 |
| 77 | 3 | z | scroll of undead protection | scroll 13 | 3400 | 1 |
| 78 | 4 | a | scroll of stealth | scroll 14 | 3000 | 2 |
| 79 | 4 | b | scroll of magic mapping | scroll 15 | 4000 | 2 |
| 80 | 4 | c | scroll of hold monsters | scroll 16 | 5000 | 2 |
| 81 | 4 | d | scroll of gem perfection | scroll 17 | 10000 | 1 |
| 82 | 4 | e | scroll of spell extension | scroll 18 | 5000 | 1 |
| 83 | 4 | f | scroll of identify | scroll 19 | 3400 | 2 |
| 84 | 4 | g | scroll of remove curse | scroll 20 | 2200 | 3 |
| 85 | 4 | h | scroll of annihilation | scroll 21 | 39000 | 0 |
| 86 | 4 | i | scroll of pulverization | scroll 22 | 6100 | 1 |
| 87 | 4 | j | scroll of life protection | scroll 23 | 30000 | 0 |
| 88 | 4 | k | The Talisman of the Sphere | 0 | 3000 | 0 |
| 89 | 4 | l | a wand of wonder | 0 | 1500 | 0 |
| 90 | 4 | m | a brass lamp | 0 | 500 | 0 |
| 91 | 4 | n | a staff of power | 0 | 95000 | 0 |
| 92 | 4 | o | an amulet of life preservation | 0 | 100000 | 0 |
| 93 | 4 | p | a traffic cone (Wild) | 0 | 600 | 1 |
| 94 | 4 | q | a propeller beanie (Wild) | 0 | 1200 | 1 |
| 95 | 4 | r | a pointy wizard hat (Wild) | 0 | 2500 | 1 |
| 96 | 4 | s | a party hat (Wild) | 0 | 400 | 1 |
| 97 | 4 | t | a tinfoil hat (Wild) | 0 | 800 | 1 |
| 98 | 4 | u | a fishing rod (Wild) | 0 | 250 | 2 |

Notes on the table:

- The potion and scroll argument is the potion or scroll kind number (Part 06 gives the numbering; it is repeated here in the "Argument" column). The potion rows run in kind order 0 to 20, then skip kind 21 (cure dianthroritis, which is never sold) and continue with kinds 22 and 23. The scroll rows run in kind order 0 to 23 with no gap. Section 5.2 depends on this.
- The arguments of the non-potion, non-scroll items are what the bought item carries: the four rings at w to z of page 1 are +1 rings, the amulet of invisibility has argument 5, the chest has argument 3 and the book has argument 2 (Part 06 defines what those arguments mean). All armour and weapons are +0.
- The base game's table ends at index 92 (93 entries). The Wild Edition appends six entries, so the Wild table has 99 entries. They do not add a page: they occupy letters p to u of the existing fourth page.
- Wild Edition hook: the five hats (indices 93 to 97) are treated as absent while the hats feature is off, see Part 12.
- Wild Edition hook: the fishing rod (index 98) is treated as absent while the fishing feature is off, see Part 12.
- Wild Edition hook: every shelf price shown and charged is passed through two discount filters, each taking off one tenth (integer division) when active: one when the player is wearing the party hat, one when the player has the Haggler perk; see Part 12.

### 2.2 Entry and the tax refusal

On entry the page index is reset to the first page, the text page is cleared and the three-line heading is printed at the top:

- Line 1: `Welcome to the Ularn Thrift Shoppe.  We stock many items explorers find useful`
- Line 2: `in their adventures.  Feel free to browse to your heart's content.`
- Line 3: `Also be advised that if you break 'em, you pay for 'em.`

If the outstanding tax bill is greater than zero, the store then prints, starting on line 5:

- `The Ularn Revenue Service has ordered us to not do business with tax evaders.` (a beep sounds here)
- `They have also told us that you owe N gp in back taxes and, as we must` (N is the tax bill)
- `comply with the law, we cannot serve you at this time.  So Sorry.`
- a blank line, then `Press **[escape]** to leave: `

It waits for Escape (the only accepted key) and returns to the map. No stock is shown.

### 2.3 Page layout

A page shows up to 26 consecutive table entries, lettered a to z, in two columns of 13 rows on lines 5 to 17. Entry number p within the page (0 to 25) is drawn on line 5 + p/2, in the left column if p is even and the right column if p is odd. So the left column reads a, c, e, ... y from top to bottom and the right column reads b, d, f, ... z.

- The left column starts at column 1 and the right column at column 41.
- An entry is drawn as the letter, a closing parenthesis, a space, then the name (so the name starts at column 4 or 44).
- The price is drawn right-aligned in a field of 6 characters starting 31 columns after the start of the entry (columns 32-37 or 72-77). It is the shelf price after the Wild discount filters.
- An entry whose stock is 0 (or which is hidden by a Wild feature being off) is drawn as 39 spaces, i.e. it is simply blank; the letters of the other items do not close up. Positions past the end of the table draw nothing.
- There are no column headings.

Line 18 shows the player's gold: the text `You have ` starts at column 50 and, from column 59, `N gold piece` followed by `s` unless N is exactly 1. The rest of line 18 is cleared after the amount each time it is redrawn.

Line 19 holds the prompt: `Enter your transaction [**[space]** for more, **[escape]** to leave]? `. Before the prompt is printed, everything from line 20 downwards is cleared, so messages from the previous transaction disappear when the prompt comes back. (The letter echoed for the previous transaction, which sits just after the prompt on line 19, is not cleared and stays visible until the next letter overwrites it or the page is changed.)

The four pages start at table indices 0, 26, 52 and 78. The last page has 21 entries in the Wild table (15 in the base table).

### 2.4 The transaction loop

The loop repeats the following until the player leaves:

1. Redraw the gold amount on line 18, clear below, print the prompt on line 19.
2. Wait for one of: a lower-case letter a to z, space, or Escape.
3. **Escape**: return to the map.
4. **Space**: clear the page from line 4 downwards, advance the page start by 26, wrap to 0 if that reaches or passes the table size, draw the new page and the `You have ` label, and go to step 1.
5. **A letter**: echo the letter after the prompt, then compute the table index as page start plus letter position (a = 0). The following checks are made in this exact order; the first that applies decides the outcome.
   1. The index is past the end of the table, or the entry's stock is 0 or less, or the entry is hidden by a Wild feature: print on a new line `Sorry, but we are out of that item.` and pause 2200 ms.
   2. The player's pack is full (no free slot within the carrying limit; Part 02: the limit is 15 plus half the character level, at most 26): print on a new line `You can't carry anything more!` and pause 2200 ms.
   3. The player's gold is less than the (filtered) shelf price: print on a new line `You don't have enough gold to pay for that!` and pause 2200 ms.
   4. Otherwise the sale goes through:
      1. If the item is a potion or a scroll, that potion or scroll kind becomes known (identified) to the player.
      2. The price is subtracted from the player's gold.
      3. The entry's stock goes down by one.
      4. The item, with its table argument, is added to the pack by the normal "take" routine (Part 02), which prints `You pick up:` on a new line followed by the inventory line of the new item and applies any carry effects of the item.
      5. If the stock has reached 0, the entry is blanked on screen at once.
      6. Pause 1001 ms.

Because the pack check comes before the gold check, a player with a full pack and too little gold is told about the pack, not the gold.

Buying is one item per key press; there is no quantity prompt, no confirmation, no haggling and no way to sell here.

## 3. The bank

There are two offices, the First National Bank of Ularn on the home level and the 8th-level branch in the dungeon. They share one account, one interest clock and one routine; they differ only in the title line. Additionally, entering either office while the player is on dungeon level 8 clears the "teleported, current level unknown" flag (so the status display shows the level number again).

### 3.1 Interest

The interest routine is run every time a bank office is entered (before anything is drawn, but only when the player is actually served, see 3.2) and every time the game is saved, including automatic checkpoint saves. It works as follows.

1. If the balance is negative, set it to 0.
2. Otherwise, if the balance is greater than 0 and less than 1,000,000: let n = (current game time − time of last interest) / 100, i.e. the number of whole mobuls since interest was last credited. Repeat n times, stopping early as soon as the balance is 1,000,000 or more: add balance / 877 (integer division) to the balance.
3. In every case, finally set the "time of last interest" to the current game time rounded down to a whole mobul (a multiple of 100).

Consequences that a rebuild must reproduce:

- Interest is compounded once per whole mobul at a rate of 1/877 of the balance (about 0.114 % per mobul; the original comment equates this to 10 % a year at one mobul per hour).
- A balance below 877 earns nothing, because 1/877 of it truncates to 0.
- A balance of 1,000,000 or more earns nothing. The last step may carry the balance slightly past 1,000,000. Deposits themselves are not limited: the player may bank any amount; it just stops earning.
- The time of last interest starts at 0 in a new game and is stored in the save file.
- If the game clock has gone backwards since the last credit (a time warp scroll, Part 06), n is negative, no interest is paid and the reference time is moved back to the new clock value. Time skipped forwards (time warp, college courses) earns interest for the skipped mobuls.

### 3.2 Entry, title and the tax freeze

On entry the text page is cleared and the title is printed on line 1:

- Home office: `    Welcome to the First National Bank of Ularn.` (four leading spaces)
- Branch: `Welcome to the 8th-level branch office of the First National Bank of Ularn.`

If the outstanding tax bill is greater than zero the bank prints, starting on line 3:

- `The Ularn Revenue Service has ordered that your account be frozen until all` (a beep sounds here)
- `levied taxes have been paid.  They have also told us that you owe N gp in`
- `taxes and we must comply with them. We cannot serve you at this time.  Sorry.`
- `We suggest you go to the LRS office and pay your taxes.`
- a blank line, then `Press **[escape]** to leave: `

It waits for Escape and returns to the map. In this case the interest routine is not run and the level-8 flag is not cleared.

### 3.3 The account screen

Otherwise a heading line is printed on line 3 with the words `Gemstone` at column 9, `Appraisal` at column 31, `Gemstone` at column 49 and `Appraisal` at column 71. Then:

1. Interest is credited (section 3.1) and, on level 8, the teleport flag is cleared.
2. All 26 pack slots are scanned in order. Each slot that holds the Eye of Larn, a diamond, an emerald, a ruby or a sapphire is given an appraisal value and listed; every other slot gets value 0.
   - Diamond, emerald, ruby, sapphire: value = the gem's argument × 100. (The trading post pays only 20 × argument for the same gem.)
   - The Eye of Larn: value = 250000 − ((game time × 7) / 100) × 100, with a minimum of 50000. In words: it loses 700 gold per mobul elapsed, in steps of 100 gold, from 250000 down to a floor of 50000 (reached after about 286 mobuls).
   - The k-th listed gem (k from 0) is drawn on line 4 + k/2, in the left half (starting column 1) if k is even or the right half (starting column 41) if k is odd: the gem's **pack slot letter**, a closing parenthesis, a space and the object name (`a brilliant diamond`, `a ruby`, `an enchanting emerald`, `a sparkling sapphire`, `The Eye of Larn`). Its value is drawn right-aligned in a 5-character field starting 32 columns after the start of the entry (columns 33-37 or 73-77; the Eye's six-digit value overflows the field by one character).
   - These values are computed once on entry and are not recomputed while the screen is open.
3. Line 17, from column 31: `You have NNNNNNNN gold pieces in the bank.` The number is right-aligned in 8 characters; the final `s` of "pieces" is omitted when the balance is exactly 1.
4. Line 18, from column 40: `You have NNNNNNNN gold pieces` (carried gold, same formatting).
5. If balance + carried gold is 500,000 or more, line 19 shows: `Note:  Only deposits under 1,000,000gp can earn interest.`
6. If the Eye of Larn was found in the pack, the appraisal dialogue of section 3.4 runs once. (Quirk: the test uses the Eye's slot number as a yes/no value, so an Eye in the very first slot, slot "a", is listed and can be sold but never triggers the dialogue.)
7. The command loop of section 3.5 runs.

After every command the two numbers are redrawn in place (8 characters at column 40 of line 17 and at column 49 of line 18); the surrounding words, including the plural "s", are not redrawn.

### 3.4 The Eye of Larn appraisal dialogue

1. At line 20: `I see you have The Eye of Larn  I must commend you.  I didn't think⏎you could get it.  Shall I appraise it for you? ` and a yes/no prompt.
2. If the answer is not "y": print `no.`, pause 500 ms, clear from line 20 down, done.
3. If "y": print `yes.⏎  Just one moment please...`, pause 1000 ms, compute the Eye's value with the same formula as above, clear from line 20 down, and print (starting on line 21): `This is an excellent stone.⏎It is worth N gold pieces to us⏎Would you like to sell it? ` and a yes/no prompt.
4. If "y": print `yes`; add N to carried gold; empty the Eye's pack slot; clear the player's "has the Eye" flag; blank the Eye's entry in the gem list (39 spaces); redraw the two numbers.
5. Otherwise: print `no`, then on a new line `It is, of course, your privilege to keep the stone.`, pause 500 ms.
6. Clear from line 20 down.

Quirks to reproduce:

- Selling the Eye through this dialogue does not zero its stored appraisal value. The emptied slot therefore still counts as a sellable gem: choosing "sell a stone" and that slot's letter (or `*`) afterwards pays the Eye's value a second time. After that second payment the value is zeroed.
- Selling the Eye at the bank (by either route) only clears the "has the Eye" flag; it does not print the "your sight fades" messages that losing the Eye by other means prints.

### 3.5 The command loop

Each pass:

1. Clear from line 20 down and put the cursor at line 20, column 1.
2. Wild Edition hook: a LarnCoin status line may be printed here on line 20 (nothing when the feature is off), see Part 12.
3. On the next line (line 21) print `Your wish? [(**[d]**) deposit, (**[w]**) withdraw, (**[s]**) sell a stone, or **[escape]**]  ` and wait for one of d, w, s, Escape. Wild Edition hook: the keys b (buy LarnCoin) and c (cash out) are added to the accepted set and handled by the Wild code, see Part 12.
4. Act on the key (below), then redraw the two numbers and repeat. Escape returns to the map.

**d, deposit.** Print `deposit⏎How much? ` and read a number with default = all carried gold (section 1.4).

- A negative amount is treated as 0, silently.
- An amount larger than the carried gold prints, on a new line, `  You don't have that much.` (two leading spaces), pauses 2000 ms, and is treated as 0.
- Otherwise the amount moves from carried gold to the bank balance.

**w, withdraw.** Print `withdraw⏎How much? ` and read a number with default = the whole balance.

- A negative amount is treated as 0, silently.
- An amount larger than the balance prints, on a new line, `  You don't have that much in the bank!`, pauses 2000 ms, and is treated as 0.
- Otherwise the amount moves from the bank balance to carried gold.

There are no fees, no minimums and no maximums on either operation.

**s, sell a stone.** Print `sell⏎Which stone would you like to sell? ` and wait for a key. The accepted keys are the letters a to y and the asterisk. The letter z is not accepted (the accepted-key list has a second "s" where "z" should be), and Escape is not accepted either: once "s" is pressed the player must name a slot or press `*`.

- `*` sells every slot whose stored appraisal value is non-zero: for each, in slot order, clear the "has the Eye" flag if the slot holds the Eye, add the value to carried gold, empty the slot, zero the value and blank its entry in the gem list.
- A letter whose slot has value 0 prints, on a new line, `Item X is not a gemstone!` (X is the letter) and pauses 2000 ms.
- A letter whose slot has a non-zero value sells that one stone in the same way as above.

There is no confirmation for selling stones through this command and no message on success; the entry simply disappears and the gold figure rises.

## 4. The College of Ularn

### 4.1 Course list

All courses cost 250 gold. Each can be taken once per game; the "taken" flags are stored in the save file.

| Key | Course name | Time | Prerequisite | Effect | Message |
|---|---|---|---|---|---|
| a | Fighter's Training I | 10 mobuls | none | Strength +2, Constitution +1 | `You feel stronger!` |
| b | Fighter's Training II | 15 mobuls | course a taken | Strength +2, Constitution +2 | `You feel much stronger!` |
| c | Introduction to Wizardry | 10 mobuls | none | Intelligence +2 | `The task before you now seems more attainable!` |
| d | Applied Wizardry | 20 mobuls | course c taken | Intelligence +2 | `The task before you now seems very attainable!` |
| e | Behavioral Psychology | 10 mobuls | none | Charisma +3 | `You now feel like a born leader!` |
| f | Faith for Today | 10 mobuls | none | Wisdom +2 | `You now feel more confident that you can find the potion in time!` |
| g | Contemporary Dance | 10 mobuls | none | Dexterity +3 | `You feel like dancing!` |
| h | History of Ularn | 5 mobuls | none | Intelligence +1 | `Wow! e = mc^2!` |

The attribute increases are permanent additions to the base attribute values (Part 02); there is no cap applied here.

### 4.2 Screen

The page is cleared and drawn as follows.

- Line 1: `The College of Ularn offers the exciting opportunity of higher education to`
- Line 2: `all inhabitants of the caves.  Here is the class schedule:`
- Line 5: `Course Name` starting at column 21 and `Time Needed` starting at column 48.
- Lines 7 to 14: one line per course a to h, **only for courses not yet taken** (a taken course leaves its line blank). Each line starts at column 17 with the key letter, a closing parenthesis and two spaces, the course name starting at column 21, and the time starting at column 49 written as `10 mobuls`, `15 mobuls`, `20 mobuls` or, for course h, ` 5 mobuls` (padded so the digits line up).
- Line 16, from column 17: `All courses cost 250 gold pieces.`
- Line 18: `You are presently carrying ` from column 30, then from column 57 the gold amount as `N gold piece` plus `s` unless N is 1, a full stop and three spaces.
- Line 19: the prompt `What is your choice [**[escape]** to leave] ? `.

### 4.3 Flow

Loop until the player leaves:

1. Redraw the gold amount on line 18 and the prompt on line 19; wait for a letter a to h or Escape.
2. Escape returns to the map.
3. Echo the letter. Then, in this order:
   1. If the player carries less than 250 gold: print on a new line `You don't have enough gold to pay for that!`, pause 2200 ms. (This is checked first, even for a course already taken.)
   2. Otherwise, if the course has already been taken: print on a new line `Sorry but that class is filled.`, pause 1000 ms.
   3. Otherwise 250 gold is deducted and the course is attempted:
      - For course b without course a: print on a new line `Sorry but this class has a prerequisite of Fighter's Training I`, refund the 250 gold; the course is not taken and no time passes.
      - For course d without course c: print on a new line `Sorry but this class has a prerequisite of Introduction to Wizardry`, refund the 250 gold; the course is not taken and no time passes.
      - Otherwise apply the attribute changes of the table, print the course's message on a new line (clearing the rest of that line), and blank the course's line in the schedule (from column 16 to the end of its line).
      - Pause 1000 ms in all three cases.

### 4.4 What studying does to time and timed effects

When a course is actually taken, with T = course time in mobuls × 100 time units:

1. The game clock advances by T.
2. The course is marked as taken.
3. Hit points are set to their maximum and spell points to their maximum.
4. If the blindness counter is non-zero it is set to 1; if the confusion counter is non-zero it is set to 1 (so both end on the next tick).
5. The general "time has jumped" adjustment (Part 02) is applied with T: every timed-effect counter that is currently non-zero is reduced by T, and any that would drop to 0 or below is set to 1 instead, so that it expires, with its normal expiry handling, on the next regeneration tick. The 28 counters covered are: heroism, altered protection, protection, dexterity boost, strength boost, giant strength, charm, invisibility, cancellation, haste self, aggravate monsters, scare monster, stealth, awareness, hold monster, haste monster, fire resistance, globe of invulnerability, spirit protection, undead protection, half damage, see invisible, itching, clumsiness, walk-through-walls, the cocaine timer, blindness and confusion.
6. One regeneration tick is then run immediately (Part 02), which among other things advances the clock by one more time unit (subject to the haste rule) and expires the counters that were set to 1.

Not done here: no monsters move, no check is made against the 400-mobul time limit (the player can study past the deadline and will only find out at home), and the status display is not refreshed until the player returns to the map. Bank interest for the elapsed mobuls is credited the next time interest is computed.

## 5. The trading post

### 5.1 Screen and flow

On entry the page is cleared and the five-line heading is printed, followed by a blank line:

- `Welcome to the Ularn Trading Post.  We buy items that explorers no longer find`
- `useful.  Since the condition of the items you bring in is not certain,`
- `and we incur great expense in reconditioning the items, we usually pay`
- `only 20% of their value were they to be new.  If the items are badly`
- `damaged, we will pay only 10% of their new value.`

Then the loop below runs. Text simply accumulates down the page and scrolls when it reaches the bottom; nothing is cleared between sales.

1. Print on a new line `What item do you want to sell to us [**[*]** for list, or **[escape]**] ? ` and wait for one of: a letter a to z, a full stop, an asterisk, Escape. The key is not echoed.
2. **Escape**: recompute the player's derived combat values (armour class and weapon class, Part 02) and return to the map.
3. **Asterisk**: clear the page, show the standard paged inventory listing (Part 03: an optional gold line, one line per carried item, the elapsed-time line, and a "press space to continue" pause), then redraw the heading and continue.
4. **Full stop**: accepted but does nothing; the prompt is printed again.
5. **A letter**: take the pack slot with that letter (all 26 letters are accepted regardless of the carrying limit) and apply these checks in order:
   1. If the slot holds a scroll whose kind the player has not identified, or a potion whose kind the player has not identified: print on a new line `Sorry, we can't accept unidentified objects.` and stop.
   2. If the slot holds the lance of death and the character is the class that starts the game with it (the "rambo" flag, Part 02): print on a new line `You don't *really* want to sell that, now do you?` and stop.
   3. If the slot is empty: print on a new line `You don't have item X!` (X is the letter) and stop.
   4. Compute the item's value (section 5.2). Wild Edition hook: a positive value is increased by one tenth (integer division) when the player has the Haggler perk, see Part 12.
   5. If the value is negative (the item is not in the price list): print on a new line `So sorry but we are not authorized to accept that item.` and stop.
   6. Otherwise print the item's inventory line (Part 03), then on a new line `Item (X) is worth N gold pieces to us.  Do you want to sell it? ` ("piece" without "s" when N is 1) and ask yes/no.
   7. On "y": print `yes.` and a line break; add N to carried gold; if the slot was the worn armour, the wielded weapon or the worn shield, clear that equipment slot; apply the "item lost" attribute adjustments for that item kind and argument (Part 02: removes ring bonuses and artifact effects, clamps attributes to a minimum of 3, and recomputes derived values where needed); empty the slot.
   8. On anything else: print `no thanks.` and a line break.

Selling never identifies anything: unidentified potions and scrolls are refused outright, and all other items are shown with whatever name the inventory line gives them. The store's stock is not affected by sales. There is no limit on how much the post will pay out.

### 5.2 Item valuation

The same valuation routine is used by the trading post and by the final score calculation (Part 03 adds the value of every pack slot to the player's worth; a slot the routine does not price, including an empty slot, contributes −1).

Given an item kind and its argument:

1. **Gems** (diamond, ruby, emerald, sapphire): value = 20 × argument. There is no minimum; a gem with argument 0 is worth 0.
2. Wild Edition hook: the old boot is worth 1 while the fishing feature is on, see Part 12.
3. **Everything else**: find the first entry of the DND store table (section 2.1, all 99 entries, regardless of stock or hidden state) whose item kind equals the item's kind. If there is none, the value is −1 ("not authorized"). Otherwise let P be a **table price**, which is one tenth of the shelf price in section 2.1:
   - **Potions and scrolls**: the first potion entry is table index 38 and the first scroll entry is table index 64. P is taken from the entry at (that first index + the item's kind number), and value = 2 × P. Because the rows are in kind order this is 20 % of the shelf price for potion kinds 0 to 20 and scroll kinds 0 to 23. The gap in the potion rows makes the remaining kinds read the wrong row, and this must be reproduced:

     | Item | Row actually read | Value paid |
     |---|---|---|
     | potion of cure dianthroritis (kind 21) | index 59, potion of poison | 100 |
     | potion of poison (kind 22) | index 60, potion of see invisible | 300 |
     | potion of see invisible (kind 23) | index 61, orb of dragon slaying | 1700 |
     | Wild scroll kind 24 (Part 12) | index 88, Talisman of the Sphere | 600 |

     (The see-invisible potion therefore sells for 1700 while the store charges 1500 for it.)
   - **All other listed items**: start with value = P (10 % of the shelf price). If the item's argument is 0 or more, double it (20 % of the shelf price). Then, while the argument is not 0 and the value is below 500,000:
     - if the argument is positive: value = (14 × (value + 67)) / 10, and reduce the argument by one;
     - if the argument is negative: value = (value × 10) / 14, and increase the argument by one.
   - In both sub-cases, a resulting value of exactly 0 is raised to 1.

Points to note:

- The appreciation step is applied once per point of positive argument and stops as soon as the value reaches 500,000 (the step that crosses the line is kept, later points are ignored). The depreciation step is applied once per point of negative argument. A negatively enchanted item is never doubled, which is the "badly damaged, 10 %" case of the heading.
- The argument is used as "plusses" for every listed kind, whatever it really means for that kind: a chest's or book's level and the amulet of invisibility's argument appreciate the price exactly as weapon enchantment does.
- Items that are in the table can be sold even though the store never stocks them (elven chain, Slayer, the Vorpal Blade, the orb of enlightenment and so on). The Wild hats and the fishing rod are in the table and so are sellable.
- Items not in the table are refused: the Eye of Larn, the golden urn, the five drugs, and all Wild objects other than the hats, the rod and the old boot.

Worked examples (base game, no perk): a longsword (table price 450) is worth 900 at +0, 1353 at +1, 1988 at +2, 321 at −1 and 229 at −2. A dagger (table price 2) is worth 4 at +0, 99 at +1 and 232 at +2. The store's level-2 book (table price 200) is worth 1008. A +1 ring of protection (table price 150) is worth 513.

## 6. The Ularn Revenue Service (LRS)

### 6.1 How tax arises

Tax exists only for a player who has won a previous game (Part 03 has the scoreboard details). In outline:

- The tax rate is 5 %.
- When a game is won, the new bill = the bill outstanding at that moment + (carried gold + bank balance at the moment of winning) × 0.05, rounded down. It is recorded in the player's entry on the winners' scoreboard.
- When a new game is started (not when a saved game is restored), the game looks the player up on the winners' scoreboard; if there is a winning entry, the outstanding bill is loaded from it (and the difficulty is raised, Part 01/03). The outstanding bill is also carried in the save file.
- The bill can be inspected with the "show taxes" command (Part 01), which prints `You presently owe N gp in taxes.` or `You do not owe any taxes.`
- Wild Edition hook: a tax auditor monster can collect part of the bill from the player's gold on the home level, see Part 12.

### 6.2 The office

On entry the page is cleared and the cursor is placed at line 4.

**Cheater check.** If the player owes any tax and the game is flagged as cheating (an altered save file was detected, Part 03), the office prints on line 4 `Sorry but it seems you are trying to pay off your taxes by cheating!`, then two lines lower, indented by one tab, `press [escape] to exit the office.` (plain text, no highlight), waits for Escape and returns to the map. Nothing can be paid.

**Normal screen.** Line 4: `Welcome to the Ularn Revenue Service district office.  How can we help you?` Then the loop:

1. Line 6: `You presently owe N gp in taxes.  ` if the bill is greater than zero, otherwise `You do not owe us any taxes.` followed by padding spaces.
2. Line 8: `You have NNNNNN gp.    ` (the gold right-aligned in 6 characters) if the player carries any gold, otherwise `You have no gold pieces.  `
3. Line 22: `Your wish? [(**[p]**) pay taxes, or **[escape]**]  `; the rest of line 22 and all lines below are cleared.
4. Wait for p or Escape. Escape returns to the map.
5. On p: print `pay taxes.⏎How much? ` and read a number with default = all carried gold (section 1.4).
   - A negative amount is treated as 0.
   - An amount larger than the carried gold prints `  You don't have that much.` directly after the typed number (no line break) and is treated as 0.
   - The amount is then offered to the tax-payment routine, which pays at most the bill recorded on the scoreboard: the amount actually paid is the smaller of the offered amount and the recorded bill. That amount is subtracted from the recorded bill, from the in-game outstanding bill and from the player's gold, and the scoreboard is written back. If the offered amount is 0 or less, the scoreboard cannot be read or written, or the player has no winning entry, nothing is paid and no message is shown.
   - Pause 500 ms and loop (the two status lines are redrawn with the new figures).

Overpaying is impossible, partial payment is allowed, and the bank balance cannot be used directly (the bank is frozen while tax is owed, so a taxed player must pay from gold found, earned at the trading post, or otherwise carried).

## 7. The player's home

Going home is how the game is won or lost at the end; the details of scoring and the end-of-game screens are in Part 03. The time limit is 40000 time units; "too late" means the game clock is strictly greater than 40000.

### 7.1 With the potion of cure dianthroritis

On entry all 26 pack slots are scanned in order. At the first slot holding a potion of cure dianthroritis:

1. The potion is removed from the pack and the page is cleared.
2. `Congratulations.  You found the potion of cure dianthroritis!` is printed, then a blank line, then `Frankly, No one thought you could do it.  Boy!  Did you surprise them!`; pause 1000 ms.
3. **Too late:** a blank line, then `However... the doctor has the sad duty to inform you that your daughter has⏎died! You didn't make it in time.  In your agony, you kill the doctor,⏎your wife and yourself!  Too bad...` (the word is `wife` for a male character and `husband` for a female one); pause 5000 ms; the game ends with the death reason "killed family" (Part 03). Life protection does not prevent this ending.
4. **In time:** a blank line, then `The doctor is now administering the potion and, in a few moments,⏎your daughter should be well on her way to recovery.`; pause 6000 ms; a blank line, then `The potion is.`; pause 1000 ms; `.`; pause 1000 ms; `.`; pause 1000 ms; ` working!  The doctor thinks that⏎your daughter will recover in a few days.  Congratulations!`; a beep; pause 5000 ms; the game ends as a win (Part 03).

Both endings terminate the game; control never returns to the map.

### 7.2 Without the potion

The following screen is shown in a loop.

1. Clear the page and print `Welcome home NAME.  Latest word from the doctor is not good.` (NAME is the character's name).
2. **Too late:** a blank line, then `The doctor has the sad duty to inform you that your daughter has died!⏎You didn't make it in time.In your agony, you kill the doctor,⏎your wife and yourself!  Too bad...` (again `husband` for a female character; note there is no space between `time.` and `In`); pause 5000 ms; the game ends with the death reason "killed family".
3. **Otherwise:** a blank line, then
   `The diagnosis is confirmed as dianthroritis.  He guesses that⏎your daughter has only M mobuls left in this world.  It's up to you,⏎NAME, to find the only hope for your daughter, the very rare⏎potion of cure dianthroritis.  It is rumored that only deep in the⏎depths of the caves can this potion be found.`
   where M = (40000 − game clock + 99) / 100, i.e. the remaining time rounded up to whole mobuls (the word is always `mobuls`, even for 1).
4. Four lines lower, indented five spaces: `----- press **[return]** to continue, **[escape]** to leave ----- `. Wait for Enter or Escape.
5. Escape returns to the map. Enter simply redraws the same screen (it does not leave).

Visiting home costs no game time.

## 8. Dealer McDope's pad

### 8.1 Stock

| Key | Name on the price list | Price | Screen line | Item received (inventory name) |
|---|---|---|---|---|
| a | Killer Speed | 100 | 8 | some speed |
| b | Groovy Acid | 250 | 9 | some LSD |
| c | Moster Hash (spelled exactly so) | 500 | 10 | some hashish |
| d | Trippy Shrooms | 1000 | 11 | some magic mushrooms |
| e | Cool Coke | 5000 | 12 | some cocaine |

The dealer has exactly one of each per game. Each has a "sold" flag, stored in the save file. The effects of using the drugs are in Part 06. Taxes do not matter here.

### 8.2 Screen

- Line 1: `Hey man, welcome to Dealer McDope's Pad!  I gots the some of the finest dope` (wording exactly so)
- Line 2: `you'll find anywhere in Ularn -- check it out...`
- Line 5: `The Stash` starting at column 21 and `The Cash` starting at column 49.
- Lines 8 to 12: for each drug **not yet sold**, at column 17 the key letter, a closing parenthesis, two spaces and the name; at column 49 the price followed by ` bucks` (for example `100 bucks`). Sold drugs leave their line blank.
- Line 18: `Looks like you got about ` from column 30, then from column 57 `N buck` plus `s` unless N is 1, then ` on you.` and three spaces.
- Line 19: the prompt `So, whaddya want [**[escape]** to split] ? `.

### 8.3 Flow

Loop until the player leaves:

1. Redraw the gold amount on line 18 and the prompt on line 19; wait for a letter a to e or Escape.
2. Escape returns to the map.
3. Echo the letter. Then, in this order:
   1. If that drug is already sold: print on a new line `Sorry man, I ain't got no more of that stuff.`, pause 2200 ms.
   2. Otherwise, if the player's gold is less than the price: print on a new line `Whattaya trying to pull on me? You aint got the cash!`, pause 1200 ms.
   3. Otherwise clear the page from line 20 down, then:
      - if the pack is full (same carrying limit as the store): print on a new line `Hey, you can't carry any more.`; nothing is charged and there is no pause;
      - otherwise print on a new line `Ok, here ya go.`, add the drug to the pack with argument 0 through the normal "take" routine (which prints `You pick up:` and the item's inventory line), subtract the price from the player's gold, blank that drug's line on the price list (from column 16), mark it sold, and pause 1000 ms.

Note the order differs from the DND store: here gold is checked before the pack.

## 9. Save data owned by this part

The store module writes one block to the save file (Part 03 gives its position in the file and the container format) and reads it back in the same order:

1. The DND store stock: one signed byte per table entry, in table order (99 bytes in the Wild Edition table; a base-game table would have 93).
2. The college "course taken" flags: 8 bytes, courses a to h.
3. The bank's "time of last interest": one native long integer (8 bytes on 64-bit builds, 4 bytes on 32-bit builds).
4. The dealer's "sold" flags: 5 bytes, drugs a to e.

The bank balance, the carried gold, the outstanding tax bill, the potion/scroll "known" flags and the "has the Eye" flag belong to the player data (Part 02/03), not to this block. Interest is brought up to date immediately before every save.

All four pieces start a new game at their initial values (table stock, no courses taken, last-interest time 0, nothing sold). The module itself contains no routine that resets them; they rely on a fresh process per game (Part 09 describes the process model), so a rebuild that reuses one process for several games must reset them explicitly when a new character is created.

## 10. Summary of quirks worth reproducing

- Upper-case Y is "no" at every yes/no question in these buildings (sections 1.3, 3.4, 5.1).
- Escape at a "How much?" prompt means "the default", i.e. everything (section 1.4).
- The DND store checks stock, then pack space, then gold; the dealer checks stock, then gold, then pack space.
- Store items with starting stock 0 are in the table only for trading-post pricing and can never be bought.
- Potion trading-post prices for kinds 21 to 23 read the wrong table row; the potion of see invisible sells for more than it costs (section 5.2).
- An Eye of Larn in pack slot "a" is never offered the appraisal dialogue; an Eye sold through the dialogue can be "sold" a second time through the sell command (sections 3.3, 3.4).
- The bank's sell command accepts neither "z" nor Escape (section 3.5).
- Bank balances below 877 earn no interest; balances of 1,000,000 or more earn none; deposits are unlimited (section 3.1).
- The college checks gold before checking whether the course is already taken, and a failed prerequisite refunds the fee and uses no time (section 4.3).
- Pressing Enter on the home screen redraws it instead of leaving (section 7.2).
- The "too late" text on the plain home screen lacks a space after "time." (section 7.2).
