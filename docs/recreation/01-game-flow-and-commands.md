# Part 01 — Game flow, turn loop, commands, options, help and fortunes

This part covers the frame the whole game hangs on: what the game is about, how the program starts (command-line options, the options file, environment variables, restoring a save, creating a character, the welcome page), the random number generator, the main loop and the exact order of events in one player turn, game time and the time limit, the difficulty level, every command the player (and the wizard/debug user) can give, the help system and its data file, the fortune file, the diagnostic dump, and the constants and small tables defined in these modules.

It leans on other parts as follows. Part 02 owns the player's attributes, classes, inventory mechanics and the per-turn timer table. Part 03 owns the text screens (inventory lists, "known items" list), the scoreboard, death/winning and the save-file code. Part 04 owns monster movement and creation. Part 05 owns level generation. Part 06 owns what objects and dungeon features do when stood on or used. Part 07 owns spells. Part 08 owns the buildings. Part 09 owns the display/input contract and the Android front end; Parts 10 and 11 own the app around it; Part 12 owns everything called a "Wild Edition hook" here. Where a command simply hands off to one of those, this part says so and stops.

Notation used below:

- "⏎" inside a quoted message means a line break written by the game. Most messages go to a small scrolling message area and begin with a line break; that leading break is shown because it is part of how the output is spaced.
- The map is 67 columns (x = 0..66) by 17 rows (y = 0..16). Levels are numbered 0 (the surface/home level), 1..15 (the dungeon) and 16..20 (the volcano).
- "Text display" means the full-page text screen (80 columns by at least 24 lines); "map display" means the map with status lines and the message area.

---

## 1. What the game is

The program calls itself "The Addiction of Ularn", version 1.6, patch level 3, dated "26 Mar 2004". Its credit text, shown by the version command and by the command-line help, is three lines: "Ularn created by Phil Cordier -- based on Larn by Noah Morgan", "  Updated by Josh Brandt and David Richerby", "  Rewrite and Windows32/X11/Amiga graphics conversion by Julian Olds".

### 1.1 Story, as the game tells it

The welcome page (the first page of the help file, section 11) opens with "Welcome to the game of Ularn.  At this moment, you face a great problem." and explains, in three short paragraphs:

1. Your daughter has contracted a strange disease; none of your home remedies works; you sense she is in mortal danger and must try to save her. Some time ago you heard of a land of great danger and opportunity, and perhaps the solution is there.
2. A great magician who called himself Polinneaus, after many miraculous successes, retired to the caverns of Ularn and spent his time creating magic. Rumour has it that he set out one day to dispel an attacking army in a forest some distance to the north and is believed to have met his end there.
3. The caverns are thought to be magnificent and to hold much magic and treasure; one option you have is to journey into them.

It ends "Good Luck!  (You're going to need it.)".

The rest of the story is told at the player's home (Part 08): the disease is "dianthroritis", the only hope is "the very rare potion of cure dianthroritis", rumoured to be found only deep in the caves, and the doctor estimates how many "mobuls" the daughter has left.

### 1.2 Goal, winning and losing

- The game is won by carrying a potion of cure dianthroritis into the home building on the surface while the game clock is at or under the time limit (section 7). The home building then ends the game as a win.
- If the potion is brought home after the limit, or the player merely visits home after the limit, the daughter has died and the game ends as a loss ("killed family" ending, Part 08).
- The game is otherwise lost by dying (hit points reaching zero and the many special deaths in Parts 03, 04 and 06), or abandoned by quitting.
- Saving ends the session without a result; the saved game is picked up automatically at the next start.
- A winner starts every later new game at a higher difficulty and owes taxes on the winnings (section 6, Part 03, Part 08).

---

## 2. Random numbers

All randomness comes from the C library's standard pseudo-random generator (the plain seed/next pair of the platform's C runtime; the game has no generator of its own and none of the alternative generators the source can be configured for is selected in this build). The exact sequence is therefore platform-defined (unverified beyond that: the platform library source is not part of this repository).

Seeding happens once, at the very start of set-up, before the command line is read:

1. The current wall-clock time in seconds is stored as the game's "initial time" and used as the seed.
2. If the environment variable ULARN_SEED is set and not empty, it is parsed as an unsigned decimal number and the generator is re-seeded with that value. This is a test aid that makes a run reproducible.

The generator state is not stored in save files; a restored game is seeded from the clock (or ULARN_SEED) like a new one.

Helper functions, defined as simple reductions of the raw generator value:

| Helper | Result | Range |
|---|---|---|
| rnd(n) | raw value modulo n, plus 1 | uniform integer 1..n |
| rund(n) | raw value modulo n | uniform integer 0..n-1 |
| rndl(n), rundl(n) | same two, with a long-integer result | 1..n and 0..n-1 |

n must be positive (a zero argument is a division by zero). The modulo reduction is used as is, with its slight bias toward small results.

Other small helpers defined alongside: integer minimum, maximum and absolute value, and a plural helper that yields an empty string when the count is exactly 1 and "s" otherwise (so 0 and negative counts are plural: "0 pounds", "-3 mobuls").

---

## 3. Files, paths and environment

### 3.1 Directories

| Name | Value in this build | Used for |
|---|---|---|
| Library directory | "." (the process's working directory) unless changed by the options file | score file, help file, maps file, fortune file |
| Home directory | value of the HOME environment variable | options file, checkpoint file, and the default save directory |
| Save directory | the home directory unless changed by the options file | the save file |

HOME must be set (the game copies it without checking; the app always sets it, Part 10).

### 3.2 File names

| File | Path | Notes |
|---|---|---|
| Options file | home directory + "/ularn.opt", or exactly the path given with the -o option | optional; silently skipped if it cannot be opened |
| Score file | library directory + "/Uscore" | created at start-up if missing (Part 03) |
| Help file | library directory + "/Uhelp" | section 11 |
| Maps file | library directory + "/Umaps" | pre-made levels (Part 05) |
| Fortune file | library directory + "/Ufortune" | section 12 |
| Save file | save directory + "/ularn_" + login name + ".sav" | Part 03 |
| Checkpoint file | home directory + "/ularn_" + login name + ".ckp" | note: home directory, not the save directory |
| Diagnostic dump | "diagfile.txt" in the working directory | section 13 |

The library, save, checkpoint and options paths are held in buffers of 1024 characters.

### 3.3 Environment variables read by the game

| Variable | Effect |
|---|---|
| HOME | home directory (above) |
| USER | login name used in the save and checkpoint file names; "player" if unset or empty. The numeric user id is the process's real user id. The login name is deliberately independent of the character's display name so that renaming the character does not orphan a save |
| ULARN_SEED | fixed random seed (section 2) |
| ULARN_WILD | Wild Edition feature mask for a new character (Wild Edition hook, see Part 12) |
| _POSIX_OPTION_ORDER | if set, the option parser stops at the first non-option argument instead of permuting arguments |

The app also sets LOGNAME, which the game does not read.

---

## 4. Start-up sequence

The game runs as its own process (Part 09) and its entry point does the following, in this order.

1. Initialise the front end (key map, blank text buffers), starting in text display.
2. Clear the login name and display name, select the text display, clear it, and pause 100 ms.
3. Read HOME; set the save directory to it.
4. Initialise the storage for the 21 levels (Part 05).
5. Record the start time and seed the random generator (section 2).
6. Parse the command line (4.1). Several options only set flags that are acted on later.
7. If no -o option was given, the options file path becomes home directory + "/ularn.opt".
8. Read the options file (4.2).
9. Wild Edition hook: feature initialisation (see Part 12).
10. Build the score file path from the library directory. Then:
    - if -c was given: print "Preparing to initialize the scoreboard.⏎", ask for the wizard password (9.14); if it is correct, create an empty scoreboard (on failure print "Scoreboard creation failed!!⏎" and exit), show the scores, and print "⏎Scoreboard initialized.⏎". Exit in every case, including a wrong password.
    - else if -s was given: show the scoreboard and exit; if -i was given: show the scoreboard with inventories and exit (Part 03). If both were given, whichever came last on the command line wins.
11. If the score file does not exist, create it. If that fails print "I can't create the scoreboard.⏎" and "Check permissions on " + library directory + "⏎" and exit.
12. Identity: the login name starts as the display name from the options file, is then replaced by the platform's user name (USER or "player") and user id; if the options file gave no display name, the display name becomes the login name. The display name is what appears in messages and on the scoreboard; the user id is what the scoreboard uses to recognise a returning player.
13. Build the help, maps, fortune, save and checkpoint paths (3.2).
14. If -r was given: if the checkpoint file does not exist print "Cannot find checkpoint file " + path + "⏎" and exit. Otherwise print "Restoring...", mark the game as restored, set the "player was hit" flag (so monsters do not get a free move, section 8) and load the checkpoint file (Part 03).
15. Otherwise, if the save file exists: mark as restored, set the hit flag, print "Restoring..." and load the save file. A save is therefore always resumed automatically; there is no prompt. (The app starts the game with -r only when a slot has a checkpoint but no save, Part 10.)
16. If nothing was restored, create a new game:
    1. Create the character (4.3).
    2. Wild Edition hook: new-character set-up (see Part 12).
    3. Generate and enter level 0, the surface (Part 05). The whole surface map is known from the start.
    4. Unless the welcome page is suppressed, show it (4.4).
17. Set the difficulty (section 6). This also rescales the monster table.
18. Switch to the map display, draw the player, and clear the repeat count and the second hit flag.
19. Enter the main loop (section 8).

Every "exit" above frees level storage and spheres, closes the front end and ends the process with status 0.

### 4.1 Command-line options

The option letters are s, i, c, n, h, r, and o and d which take an argument. Parsing is the usual getopt style (options may be combined, as in -sn; an argument may be attached or separate), with the parser's own error messages turned off.

| Option | Effect |
|---|---|
| -s | show the scoreboard, then exit |
| -i | show the scoreboard with inventories, then exit |
| -c | create a new, empty scoreboard; needs the wizard password; then exit |
| -n | suppress the welcome page for a new game (the options file is read later and a "welcome" keyword there turns it back on) |
| -h | print the credits, "⏎⏎  Version 1.6.3 (26 Mar 2004)", a blank line and the usage text, then "⏎⏎Press space to exit", wait for a space and exit |
| -o file | use this options file path exactly as given |
| -d number | requested difficulty. The argument is read as a decimal integer (non-numeric text reads as 0). Above 100 it is clamped to 100. If negative: print "Difficulty level must be > 0⏎", the usage text, "⏎⏎Press space to exit", wait for space and exit (the message says "> 0" although 0 is accepted) |
| -r | restore from the checkpoint file instead of the save file |
| any unknown option, or an option missing its argument | treated exactly like -h |

The usage text is headed "Cmd line format: Ularn [-sicnh] [-o <optsfile>] [-d #] [-r]" followed by one line per option. Two details in it are wrong and harmless: it describes -o as replacing "~/.Ularnopts", while the real default is "ularn.opt" in the home directory; and it presents -c as "wizard only". Arguments that are not options are ignored.

How the app uses these: a new game at slider difficulty N > 0 is started with "-d N" (the app's slider stops at 10; the game accepts up to 100); a slot that only has a checkpoint is resumed with "-r"; otherwise no options are passed (Part 10).

### 4.2 The options file

A plain text file read line by line (lines of up to 255 characters). The app rewrites it before every start from its own settings (Part 10), in the form: a comment line, then one line consisting of the word OPTION, one space, and a comma-separated keyword list.

Parsing rules, exactly:

1. If the file cannot be opened, nothing happens (all defaults apply).
2. A line whose first character is "#" is a comment.
3. Otherwise the first word of the line is the text up to the first space, tab, "=" or end of line. A line with no such word is blank and ignored.
4. If the first word is exactly OPTION, the rest of the line is a list of items separated by commas, colons or the end of line. Each item is compared, case-sensitively and without trimming spaces, to the keyword list below. A keyword that takes a value consumes the next item as its value. Processing continues until the line is exhausted.
5. If an item matches no keyword, print "⏎Unrecognised option '" + item + "'" and abandon the rest of that line.
6. If the first word is exactly LIBDIR, the following text up to a colon or end of line becomes the library directory. If it is exactly SAVEDIR, likewise for the save directory.
7. Any other first word: print "⏎Unrecognised option '" + word + "'".

Consequences worth reproducing or knowing:

- Exactly one separator character (a space, a tab or "=") may stand between OPTION and the first keyword. A second space becomes part of the first item, which then fails to match and discards the whole line. Likewise a space after a comma breaks the next keyword.
- Values may contain spaces (a name like "Fred the Bold" works) but not commas or colons. The app replaces commas, colons and line breaks in the player's name with spaces, trims it, cuts it to 30 characters and substitutes "Adventurer" if nothing is left.
- LIBDIR and SAVEDIR only work in the form "LIBDIR=path" or "LIBDIR path". The form the help file documents, "LIBDIR:path", is not split at the colon and is rejected as unrecognised. The path itself cannot contain a colon.
- A value keyword at the very end of a line with no value following is not handled by the original code (it reads a missing item); a rebuild should simply ignore it.
- Multiple OPTION lines are allowed; later settings override earlier ones.

Keywords:

| Keyword | Effect | Default when absent |
|---|---|---|
| name:text | the character's display name, at most 40 characters | the login name ("player" under the app if it wrote no name) |
| class:classname | pre-selects the character class and skips the class menu. Compared without regard to case against the eight class names Ogre, Wizard, Klingon, Elf, Rogue, Adventurer, Dwarf, Rambo. An unknown name is ignored and the menu is shown | menu shown |
| gender:male or gender:female | sets the character's sex. Any other value prints "⏎Unknown gender '" + value + "'" and changes nothing | male |
| nap / nonap | clear / set the "no delays" flag | delays on |
| welcome / nowelcome | show / suppress the welcome page for a new game | shown |
| enhanced_interface / noenhanced_interface | turn the enhanced command interface on / off (9.9) | off |
| beep / nobeep | allow / silence the game's beeps | beeps on |

Note on "nonap": the flag is stored, but the Android front end's delay routine does not consult it, so the timed pauses in the game core happen either way in this build (Part 09 has the delay routine).

Three further option flags exist with fixed values and no keyword: "bold objects" (on), "mail bills after winning" (on) and "checkpointing" (on). Only the last has any effect in this build (section 8.5); the mail feature is not compiled in.

### 4.3 Creating the character (name, class, gender)

There are no in-game prompts for the name or the gender: both come from the options file (4.2), which the app fills in from its new-game dialog (Part 10). Only the class is asked by the game itself.

Character creation does this:

1. Zero all character attributes and their display copies, empty all 26 inventory slots, and forget all potions, scrolls and spells.
2. Set experience level 1, the hit-point regeneration counter to 16 and the spell regeneration counter to 96; nothing wielded or worn.
3. If the options file pre-selected a class, apply it. Otherwise show the class menu on the text display and wait for a letter a to h (no other key is accepted; there is no cancel).
4. Apply the class's starting attributes, spells and equipment (Part 02).
5. Choose a random starting position, x = rnd(65) and y = rnd(15); the level entry code then moves the player to the nearest empty cell if that one is occupied.
6. Set the game clock to 0.

The class menu, in this build (a narrow layout for phone screens), is: the title "The Addiction of Ularn" highlighted on the first line, a blank line, "Pick a character class..." and a blank line, then for each class a highlighted line with its letter and name followed by an indented description line:

| Key | Class | Description line(s) |
|---|---|---|
| a | Ogre | Exceptional strength, but thick as a brick |
| b | Wizard | Smart, good at magic, but very weak |
| c | Klingon | Strong and average IQ, but unwise & very ugly |
| d | Elf | OK at magic, but a mediocre fighter |
| e | Rogue | Nimble and smart, but only average strength |
| f | Adventurer | Jack of all trades, master of none |
| g | Dwarf | Strong and healthy, but not good at magic |
| h | Rambo | A special challenge. Bad at everything, / but has a lance of death |

The prompt "So, what are ya? " is placed on line 24. (The original desktop layout is the same text as one line per class, tab-indented, in the form "a)  Ogre       - Exceptional strength, but thick as a brick" with blank lines between, and the title starting at column 29.)

### 4.4 The welcome page

Shown once for a new game unless suppressed: switch to the text display, open the help file, display its first 23-line page (the story, section 1.1), then show "Press return to continue: " on line 24 (the word "return" highlighted) and wait for Return or Escape; then return to the map display. If the help file cannot be opened, the message "Can't open help file " followed by the quoted path is shown for 4 seconds instead.

---

## 5. Input primitives the commands use

The game core asks the front end for input in five ways (full contract in Part 09). They matter here because they define what a command accepts:

| Kind | Behaviour relevant to commands |
|---|---|
| Command key | Waits for one key and maps it to a command (section 9.1). Keys with no binding are ignored silently, so the core's "unknown command" message never appears |
| Choice prompt | Prints a prompt, then waits until a key from a given set of characters is pressed (case-sensitive); other keys are ignored. Nothing is echoed; the command prints its own echo word |
| Number entry | Digits typed are echoed; a minus sign is allowed as the first character; Backspace edits; Return ends. Each call has a default value which is returned when the entry is empty, when an asterisk is typed, or when Escape is pressed. So Escape does not cancel a number prompt: it answers with the default |
| Direction | Prints a prompt and waits for one of the eight plain movement keys. There is no cancel |
| Password | Up to a given number of printable characters, echoed as asterisks, Backspace edits, ended by Return or Escape |

---

## 6. Difficulty

The difficulty is a non-negative integer (0 is the standard game; the command line allows up to 100). It is stored among the character's attributes and so travels with the save file.

### 6.1 How it is chosen and raised

At the end of set-up, with "requested" being the -d value or "none":

1. If the game was restored, the difficulty stored in the save is kept and the request is ignored.
2. Otherwise the winners' scoreboard is searched for an entry with this user id and a positive score (Part 03).
   - No such entry: difficulty = the requested value, or 0 if none.
   - Entry found: difficulty starts at that entry's difficulty + 1, and the player's outstanding tax bill is loaded from the entry. A requested value replaces it only if it is strictly greater. (Wizard mode would also force the requested value, but wizard mode cannot be on this early.)

So difficulty rises by one automatically after each win, and can be pushed higher, never lower, by request. Under the app all save slots share one scoreboard and one user id, so a win in any slot raises the difficulty of every later new game.

### 6.2 What it changes at set-up: the monster table

If the difficulty k is not zero, every entry of the monster table (all monster ids from 0 through the last, including the demons and the Wild Edition monsters) is rescaled once, with integer arithmetic (division truncates):

| Field | New value | Clamp |
|---|---|---|
| hit points | ((6 + k) × hit points + 1) / 6 | at most 32767 |
| damage | ((6 + k) × damage + 1) / 5 | at most 127 |
| gold carried | (10 × gold) / (10 + k) | at most 32767 |
| armour class | armour class − k | at least −127 |
| experience value | (7 × experience) / (7 + k) + 1 | at least 1 |

Note the damage divisor is 5, not 6: at difficulty 1 monsters have about 117% hit points but 140% damage. The table is rebuilt from its base values in each new process, so the scaling is applied exactly once per session, also for restored games (using the saved difficulty).

### 6.3 Where else it changes a number

In the turn loop (this part):

- Hit-point regeneration interval: each time a hit point batch is regenerated the counter is reset to 22 + 2k − experience level (section 8.3).
- Spell regeneration interval: reset to 100 + 4 × (k − experience level − energy bonus).
- The version command prints it: "Difficulty level k".

Elsewhere (owned by the parts named; listed so nothing is missed):

| Where | Effect | Part |
|---|---|---|
| Monster's blow against the player | attack bias = k + 1 added to the monster's roll | 04 |
| Monster movement | a monster moves "intelligently" when its intelligence exceeds 10 − k | 04 |
| Player's chance to hit | the to-hit total is reduced by k | 04 |
| Player's weapon damage | k is subtracted inside the damage multiplier | 04 |
| Level gain and loss | hit points gained/lost use (constitution − k) / 2, and the extra constitution/4 bonus applies only while the level is below 7 − k | 02 |
| New leather armour, plate armour and swords | when k > 0 their random bonus is drawn from a shorter (worse) prefix of the bonus table | 05 |
| Treasure rooms | below difficulty 3 each filled spot gets rnd(6) + 1 item-and-monster pairs with monsters two levels deeper than the room's level; from difficulty 3 it gets rnd(4) + 1 pairs with monsters four levels deeper | 05 |
| A group of five bonus items placed on deeper levels (sunsword, two-handed sword, belt of striking, energy ring, plate mail) | always attempted below difficulty 3; from difficulty 3 only when rnd(4) = 3 | 05 |
| Directed spell rays | a wall is only broken by damage ≥ 50 + k; statue outcomes switch at k > 3; the ray loses 3 + k/2 damage for every cell it travels | 07 |
| Score | winning bonus of 100000 × k; scoreboard entries rank by difficulty before score | 03 |

The time limit does not depend on difficulty (section 7).

---

## 7. Game time and the time limit

- The game clock is one counter of "time units", 0 at character creation, stored in the save file.
- It advances by 1 each time the end-of-turn bookkeeping runs (section 8.3), except: not at all while time is stopped; and only on every second such call while the player is hasted.
- A "mobul" is 100 time units.
- The time limit is 40000 time units, that is 400 mobuls. It is a fixed constant: it does not scale with difficulty or anything else.
- Other things move the clock directly: college courses add their length (Part 08), and time-warp effects add or subtract whole mobuls (Part 06).

Where the player sees it:

- The inventory screen ends with "⏎Elapsed time is U.  You have L mobul(s) left.", where U = clock / 100 + 1 (integer division) and L = 400 − U, with "mobul" singular only when L is exactly 1. L goes negative after the limit.
- At home, the doctor quotes (40000 − clock + 99) / 100 mobuls left.

What happens as time runs out: nothing at all in the dungeon. There are no warnings and no effects tied to the limit. The limit is tested only inside the home building, and the test is "clock strictly greater than 40000": entering home after that point, with or without the potion, ends the game with the daughter dead (Part 08). Arriving with the potion at exactly 40000 still wins. A player who never goes home can play on indefinitely.

Things that read the clock for other purposes: slow monsters move only when the clock is even and trolls regenerate on even clock values (Part 04); bank interest and gem prices depend on elapsed time (Part 08); the winners' board records clock / 100 (Part 03).

---

## 8. The main loop

### 8.1 State flags

| Flag | Meaning | Set by | Cleared by |
|---|---|---|---|
| No-move flag | the last command did not use a turn; ask for another command without running the rest of the loop | commands marked "no" in the turn column of 9.1; a blocked move | start of each command read |
| Drop flag | skip the "what is under me" check once | dropping an item or gold, closing or (enhanced) opening a door, opening a chest, wizard item creation | the loop, when it skips the check |
| Hit flag | the player was attacked by a monster since the flag was last cleared | every monster attack on the player (it is set at the start of the attack routine, before hit or miss is decided); restoring a game; one Wild Edition feature | the loop, just before reading a command |
| Second and third hit flags | set together with the hit flag (the third also when the player attacks); nothing in this build reads them | — | third: by the loop |
| View flag | skip one redraw of the player's surroundings | resting with a repeat count pending (never happens, 8.6) | the loop |
| Repeat count | upstream's count for repeated commands; always 0 in this build (8.6) | — | almost every command zeroes it |
| Random-monster timer | turns until the next wandering monster; starts at 80; stored in the save file | — | reset to 120 − 4 × level number when it fires |

### 8.2 One iteration

Each pass of the loop is one player turn. In order:

1. Object underfoot. If the drop flag is clear, examine the cell the player stands on (Part 06): this is where every "You find...", "There is ... here" prompt, pick-up question, trap, staircase, shop entrance and so on happens. If the drop flag is set, clear it and skip this once. The check does nothing while time is stopped or when the cell is empty.
2. Monsters. If the hit flag is clear: if the "haste monsters" effect is active, run the monster-movement pass once; then run it once more (so twice under haste monsters). If the hit flag is set, monsters do not move in this iteration. The movement pass itself (Part 04) does nothing while time is stopped; does nothing on turns where the player's haste counter is non-zero and even (8.4); otherwise moves the spheres of annihilation and then, unless monsters are held, the monsters.
3. Redraw. Unless the view flag is set, reveal and draw the cells around the player (the 3 by 3 block; 7 by 7 with awareness; only the player's own cell when blind). If the view flag was set, just clear it.
4. Clear the hit flag and the third hit flag.
5. Command. Repeat: clear the no-move flag, wait for a command key, execute the command; until a command leaves the no-move flag clear. Commands that cost no turn can therefore be chained freely without anything else happening.
   - In this build, each wait for a command key first does the front end's housekeeping: count the prompt and write a checkpoint on every 25th (8.5), and refresh the app's context information (Part 09).
6. End-of-turn bookkeeping (8.3): clock, regeneration, effect timers.
7. Wild Edition hook: per-turn feature logic (see Part 12). It is skipped entirely when the character's Wild feature mask is zero.
8. Wandering monsters. Unless time is stopped: decrease the random-monster timer by 1; when it reaches 0 or less, reset it to 120 − 4 × current level number and create one monster appropriate to the level at a random place (Part 04).
9. Go to step 1. (The loop's formal exit condition, "the command was Quit", is never what ends the game: quitting, saving, dying and winning all end the process from inside.)

Notes on the order:

- Monsters react at the start of the next iteration, after the object check for the player's new cell. A prompt in step 1 (for example a pick-up question) is therefore answered before the monsters make the move that follows the player's step.
- Because step 1 runs every turn, a player who stays on an object (by resting, or by any turn-using command that does not move) is asked about it again each turn. Resting one turn is the way to get a second chance at something that was ignored.
- On the first iteration of a new game the hit flag is clear, so monsters get a move before the first command; after a restore the hit flag is set, so they do not. In both cases step 1 runs first, so a game saved while standing on an object shows that object's prompt immediately on resuming.

### 8.3 End-of-turn bookkeeping

This routine runs once per turn from the main loop, once per step of a run (9.4), once per forced extra turn (below), and once after a time jump. Its steps:

1. If time is stopped: decrease the time-stop counter; if it reaches 0, recompute the player's derived statistics and refresh status and effects. Do nothing else and return. (No clock tick, no regeneration, no other timers.)
2. Clock: if the player is not hasted, add 1 to the game clock. If hasted, add 1 to a private half-step counter; when it reaches 2, add 1 to the clock and reset it. (The half-step counter is not saved and is not reset when haste ends.)
3. Hit points: if current hit points differ from the maximum, decrease the regeneration counter; when it drops below 0, reset it to 22 + 2 × difficulty − experience level, and add the player's regeneration amount to hit points, capped at the maximum.
4. Spells: if current spell points are below the maximum, decrease the spell counter; when it drops below 0, reset it to 100 + 4 × (difficulty − experience level − energy bonus) and add 1 spell point (Wild Edition hook: a hat can add more, see Part 12).
5. Timed effects: every active timed effect is decreased by 1 and its expiry action applied when it reaches 0. The list of effects, the two that are held up by carried items, and the expiry actions and messages belong to Part 02.
6. Itching: while the itching curse is active and armour or a shield is worn, with probability rnd(100) < 50 both are removed ("⏎The hysteria of itching forces you to remove your armor!", beep).
7. Clumsiness: while the clumsiness curse is active, a weapon is wielded and the player's cell is empty, with probability rnd(100) < 33 the weapon is dropped (the normal drop routine, which also sets the drop flag).
8. Refresh the status lines and/or the effects panel as needed.

The "forced extra turn" used when the player is made to lose turns (for example by sleep, Parts 06 and 07) is: monster pass (twice under haste monsters), the wandering-monster countdown of step 8 of the loop, then this bookkeeping. It does not include the Wild per-turn hook.

### 8.4 Haste, slowness and stopped time

- Haste self. The haste counter is one of the timed effects and drops by 1 per turn. The monster-movement pass returns immediately whenever the counter is non-zero and even, so monsters (and spheres of annihilation) act on only every second player turn. The clock also runs at half speed (8.3 step 2). Everything else is per player turn and is not slowed: regeneration counters, all other effect timers, the wandering-monster countdown, the Wild hook.
- Haste monsters. The monster pass runs twice per player turn (in the main loop, in runs and in forced turns). Combined with haste self, both passes are skipped or both run.
- Slow monsters. A monster flagged slow moves only when the game clock is even (Part 04).
- Hold monsters. The pass moves spheres but no monsters.
- Stopped time. No object check (so nothing can be picked up or triggered), no monster or sphere movement, no wandering-monster countdown, and the bookkeeping only counts the stop down. The commands read, quaff, drop and eat do nothing and print nothing, but still use up the turn. Door commands print a refusal (9.9). Movement and fighting work normally.

### 8.5 Checkpoints

Upstream defines a checkpoint interval of 400 moves but contains no code that writes checkpoints. In this build the front end writes one: every 25th time the game waits for a command key (counting prompts, including those following no-turn commands), and whenever the app sends the special key code 29 (Ctrl+]) at a command prompt, which it does when it goes to the background. A checkpoint is the full save routine written to the checkpoint path, and happens only while the checkpointing flag is on (it always is). The checkpoint file is deleted whenever a game ends in any way, including saving and quitting (Part 03). Checkpoints are loaded only with -r.

### 8.6 Repeat counts and numeric prefixes

There are none. The repeat-count variable from older versions still exists, and most commands dutifully reset it to zero, but nothing in this build ever sets it to a non-zero value: digits typed at the command prompt are unbound keys and are ignored. Likewise the help file's remarks about the keypad, arrow keys and "Keypad 5" describe desktop front ends; the native layer here accepts only the ASCII keys in 9.1, and the app translates its own controls into those keys (Part 11).

---

## 9. Commands

### 9.1 Key table

"Turn" says whether the command lets the rest of the loop run (monsters move, time passes). "Yes" means always, including when the command fails, is aborted with Escape, or is not permitted.

| Key | Command | Help-file wording | Turn |
|---|---|---|---|
| h, j, k, l | move west, south, north, east | move to the left / down / up / to the right | yes, unless blocked (9.3) |
| y, u, b, n | move north-west, north-east, south-west, south-east | move northwest / northeast / southwest / southeast | yes, unless blocked |
| H, J, K, L, Y, U, B, N | run in the same eight directions | run left / down / up / right / northwest ... | see 9.4 |
| . | rest | stay here | yes |
| space | nothing | — | no |
| w | wield | wield a weapon | yes |
| W | wear | wear armor | yes |
| T | take off | take off armor | yes |
| r | read | read a scroll | yes |
| q | quaff | quaff a potion | yes |
| e | eat | eat something | yes |
| d | drop | drop an item | yes |
| c | cast | cast a spell | yes |
| C | close door | close an open door | yes |
| o | open door (enhanced interface only) | open a closed door | yes |
| O | open chest (enhanced interface only) | open a chest here | yes |
| Z | teleport | teleport yourself | yes |
| ^ | identify traps | identify a trap | yes |
| i | inventory | inventory | no |
| \ (backslash), D | list known spells, scrolls and potions | list all items found | no |
| g | pack weight | present pack weight | no |
| P | tax status | give tax status | yes |
| v | version | program version | no |
| ? | help | this help screen | no |
| Ctrl+R (code 18) | redraw | — | no |
| S | save | save the game | ends the session |
| Q | quit | quit the game | no (if declined) |
| - | toggle wizard mode | — | no |
| _ (underscore) | "become creator" | — | no |
| backquote (grave accent) | debug mode | — | no |
| + | create item or monster (wizard) | — | yes |
| ~ | diagnostic dump (wizard) | — | yes |
| = | Wild Edition debug menu, only in wizard mode (see Part 12) | — | per choice |
| code 29 (Ctrl+]) | write a checkpoint now; not a command, the prompt keeps waiting | — | no |

When the enhanced interface is off, the o and O keys are discarded at the prompt like unbound keys. A source comment says the comma key also lists spells; it does not (it is unbound).

Odd turn costs that are real behaviour: asking for tax status, identifying traps and pressing the wizard keys + or ~ (even without being a wizard, when they do nothing visible) each cost a turn; so does every aborted item command.

### 9.2 Directions

Directions are numbered, and this numbering is used wherever a direction is stored or passed around:

| Number | Name printed | x change | y change | Reverse |
|---|---|---|---|---|
| 0 | None | 0 | 0 | 0 |
| 1 | South | 0 | +1 | 3 |
| 2 | East | +1 | 0 | 4 |
| 3 | North | 0 | −1 | 1 |
| 4 | West | −1 | 0 | 2 |
| 5 | Northeast | +1 | −1 | 8 |
| 6 | Northwest | −1 | −1 | 7 |
| 7 | Southeast | +1 | +1 | 6 |
| 8 | Southwest | −1 | +1 | 5 |

y grows downward: "north" is toward row 0.

### 9.3 Moving one step

A step in direction d works as follows and reports whether the player "moved freely" (used by running):

1. Wild Edition hook: a hat may cancel confusion (see Part 12).
2. If the player is confused and experience level < rnd(30), the direction is replaced by rund(9), that is any of the nine direction numbers including 0 ("stay").
3. Target cell = player position plus the direction's offsets.
4. If the target is outside the map: nothing happens, no turn is used.
5. If the target is a wall and the player cannot walk through walls: nothing happens, no turn is used.
6. If the enhanced interface is on and the target is a closed door: nothing happens, no turn is used. (With the enhanced interface off, the player steps onto the closed door and is asked about opening it by the object check, Part 06.)
7. If the player is on level 1 and the target is column 33 of the bottom row (the dungeon exit): go to level 0, take the position of the dungeon entrance there and then move to the nearest empty cell (the entrance cell is occupied by the entrance itself, so the player always arrives beside it; "nearest" is the level-entry placement rule of Part 02), and redraw everything. The turn is used; this does not count as moving freely.
8. Wild Edition hook: stepping into one's own pet swaps places with it, or is refused (see Part 12).
9. If a monster is on the target cell: attack it (Part 04). The turn is used; the player does not move.
10. Otherwise move: remember the old position as the "previous position" and set the new one.
11. The step counts as "moved freely" only if the new cell holds nothing, or one of the four hidden (not yet revealed) traps: hidden arrow trap, hidden teleport trap, hidden dart trap, hidden trap door. Stepping onto anything else is a normal turn but not a free move.

There are no messages for bumping into walls or the map edge. Steps 4 to 6 also clear the repeat count.

### 9.4 Running

A run in direction d repeats single steps until something interesting happens. Precisely:

1. Take one step as in 9.3.
2. If the step "moved freely": do the monster pass (twice under haste monsters), the wandering-monster countdown and the end-of-turn bookkeeping, exactly as a forced extra turn (8.3).
3. If the hit flag is now set (a monster attacked during step 2), stop.
4. If the step did not move freely, stop. Otherwise redraw the surroundings and go to 1.

So a run stops when:

- the next cell is a wall, the map edge, or (enhanced interface) a closed door. The blocked attempt costs nothing, and since it sets the no-move flag the game returns straight to the command prompt: the main loop's own monster move and bookkeeping are not run after such a run. A run whose very first step is blocked costs no time at all;
- the player steps onto any object or feature other than a hidden trap. The step is taken, and the main loop then finishes that turn normally and the object check asks about the object;
- a monster is in the way: the player attacks it once, as a normal turn;
- the player leaves level 1 by the exit;
- a monster attacks the player. The run ends where the player stands. The main loop then runs its end-of-turn bookkeeping, the Wild hook and the wandering-monster countdown once more for that same step (so one extra time unit passes), and the following iteration skips the monster move because the hit flag is set.

Things a run does not stop for: monsters coming into view, corridor branches, doorways (an open door is an object, so the run does stop on it), or hidden traps. Because the object check is not performed between the steps of a run, a hidden trap that is run across does not fire and is not revealed; it can only fire if the run happens to end on that cell. Also, the Wild per-turn hook runs once per run, not once per step. Confusion applies to every step of a run separately.

### 9.5 Rest and "nothing"

- Rest (the period key) does nothing itself; the turn passes. It is the standard way to wait, to let monsters come, and to be asked again about the object underfoot.
- Space does nothing and uses no turn.

### 9.6 The item-selection prompt

Wield, wear, read, quaff, eat and drop share one prompt:

"⏎What do you want to VERB [LETTERS * for all] ?"

where VERB is wield, quaff, read, wear, eat or drop, and for wield the text " - for none" follows "for all" inside the brackets. LETTERS lists, in slot order, the inventory letters that suit the command:

| Command | Items listed |
|---|---|
| wield | the twelve weapon types (sword of slashing, Bessman's hammer, sunsword, two-handed sword, spear, dagger, battle axe, longsword, flail, Slayer, lance of death, Vorpal Blade) |
| wear | the nine body armours and the shield (plus Wild Edition hats) |
| read | scrolls and books (plus Wild Edition readables) |
| quaff | potions |
| eat | fortune cookies (plus Wild Edition food) |
| drop | a period (meaning gold) first, then every occupied slot |

With nothing suitable the brackets read "[ * for all]". The list is only a hint: the prompt accepts any letter a to z, the asterisk, and Escape; drop additionally accepts the period, wield additionally the minus sign.

- Escape prints " aborted." and ends the command. The turn is still used.
- The asterisk shows the matching list screen for that command (for drop, the full inventory; Part 03) and then asks again.
- "⏎You don't have item X!" is the answer to an empty slot letter, in every command.

### 9.7 Wield, wear, take off

Wield (w). After the prompt:

| Answer | Result |
|---|---|
| minus sign | nothing is wielded any more; "⏎You unwield your weapon." (printed even if nothing was wielded) |
| empty slot | "⏎You don't have item X!" |
| a potion or a scroll | "⏎You can't wield item X!" |
| a two-handed sword while a shield is worn | "⏎But one arm is busy with your shield!" |
| the armour or shield currently worn | "⏎You can't wield your armor while you're wearing it!" (or "shield") |
| anything else | it becomes the wielded item: "⏎You wield NAME" + bonus + "." |

Anything that is not a potion or scroll can be wielded: books, rings, gems, cookies. NAME is the item's standard name, which carries its own article ("a dagger", "the Vorpal Blade", "Slayer"). The bonus text is " +N" when the item's bonus is positive (or always, in wizard mode, including " +0"), " -N" when negative, and nothing for zero. After any change the derived statistics are recomputed and the status refreshed.

Wear (W). After the prompt:

| Answer | Result |
|---|---|
| empty slot | "⏎You don't have item X!" |
| a body armour while armour is already worn | "⏎You are already wearing some armor." |
| a body armour otherwise | worn; if it was the wielded item it is unwielded; "⏎You put on your NAME" + bonus + "." (armour names have no article: "leather armor") |
| a shield while one is already worn | "⏎You are already wearing a shield." |
| a shield while a two-handed sword is wielded | "⏎Your hands are busy with the two handed sword!" (the original performs this test without first checking that anything is wielded; with nothing wielded it must simply be false) |
| a shield otherwise | worn; unwielded if it was wielded; "⏎You put on your shield" + bonus + "." |
| anything else | Wild Edition hook (hats, see Part 12); otherwise "⏎You can't wear that!" and the prompt is asked again (this is the one wrong answer that does not end the command) |

Take off (T). Wild Edition hook first (a worn hat is removed instead, see Part 12). Otherwise: if a shield is worn it is taken off, "⏎Your shield is off."; else if armour is worn it is taken off, "⏎Your armor is off."; else "⏎You aren't wearing anything.". There is no prompt; with both worn it takes two commands, shield first. The items stay in the pack.

### 9.8 Read, quaff, eat, drop, cast

Read (r). If the player is blind: "⏎You can't read anything when you're blind!" and nothing else. If time is stopped: nothing. Otherwise the prompt, then:

- a scroll: its effect is carried out (Part 06) and only afterwards the scroll is removed from the pack;
- a book: its effect is carried out (Part 06), then the book is removed;
- empty slot: "⏎You don't have item X!";
- anything else: Wild Edition hook (see Part 12), otherwise "⏎There's nothing on it to read.".

Quaff (q). Nothing while time is stopped. Otherwise the prompt, then: a potion has its effect (Part 06) and is then removed; an empty slot gives the usual message; anything else gives "⏎You wouldn't want to quaff that, would you? ". Blindness does not prevent quaffing.

Eat (e). Nothing while time is stopped. Otherwise the prompt, then:

- a fortune cookie: "⏎The cookie was delicious."; the cookie is removed; unless the player is blind, a fortune is drawn (section 12) and, if one is available, "  Inside you find a scrap of paper that says:⏎" and the fortune are printed;
- empty slot: the usual message;
- anything else: Wild Edition hook (see Part 12), otherwise "⏎You can't eat that!".

Drop (d). Nothing while time is stopped. Otherwise the prompt, then:

- a letter: the item is dropped by the inventory drop routine (Part 02). In outline: an empty slot gives "⏎You don't have item X! "; if the player's cell already holds anything other than a pit, a beep and "⏎There's something here already."; nothing can be dropped while standing at column 33 of the bottom row, the dungeon exit position (the command ends silently; this check comes after the two above and does not apply to gold); otherwise the item is placed on the floor (or lost, if the cell is a pit: "⏎It disappears down the pit."), "⏎  You drop:" and the item's inventory line are printed, the item leaves the pack and is unwielded/unworn, its carried effects are withdrawn, and the drop flag is set so the game does not immediately offer to pick it up again.
- the period: drop gold.
  1. If the cell holds something other than a pit: "⏎There's something here already!" and stop.
  2. Print two line breaks and "How much gold do you drop? " and read a number whose default is all the gold carried. So an asterisk, an empty entry, and also Escape, drop everything.
  3. An amount of 0 or less ends the command silently. An amount above the gold carried gives "⏎You don't have that much!".
  4. The pile's type and stored count depend on the amount (the stored count is at most 32767):

     | Amount | Pile type | Stored count | Amount actually taken |
     |---|---|---|---|
     | up to 32767 | ordinary gold pile | the amount | the amount |
     | up to 327670 | tens pile | amount / 10 | 10 × that |
     | up to 3276700 | hundreds pile | amount / 100 | 100 × that |
     | up to 32767000 | thousands pile | amount / 1000 | 1000 × that |
     | more | thousands pile | 32767 | 32767000 |

     Division truncates, so large amounts are rounded down to the pile's unit and only the rounded amount leaves the purse.
  5. Print " You drop N gold piece(s)." (singular only for 1). If the cell was a pit it stays a pit and "⏎The gold disappears down the pit." is printed: the gold is gone. Otherwise the pile lies on the cell. Refresh the status and set the drop flag.

Cast (c). Hands off to the spell system (Part 07), which handles "⏎You don't have any spells!", the three-letter code entry and everything after. From the loop's point of view casting always uses the turn, even when refused or aborted.

### 9.9 Doors, chests and the enhanced interface

The enhanced interface is an option (4.2) that changes door and chest handling. The complete list of differences:

| Situation | Enhanced interface off (classic) | Enhanced interface on |
|---|---|---|
| Walking into a closed door | the player steps onto the door's cell; the object check then asks whether to open it, and puts the player back if not or if it resists (Part 06) | the move is refused, no turn used |
| Open door key (o) | key is ignored | asks a direction and tries to open the door there |
| Open chest key (O) | key is ignored | opens the chest the player stands on |
| Close door key (C) | closes the open door the player stands in | asks a direction and closes the door there |

Open door (o, enhanced only):

1. While time is stopped: "⏎Nothing can be moved while time is stopped!" and stop.
2. Ask "⏎In What Direction? " (direction input, no cancel) and echo the direction's name (9.2).
3. Set the drop flag (so the player's own cell is not re-examined this turn).
4. If the target is off the map or holds no door: "⏎You see no door there.", or when blind "⏎You cannot feel any door there.".
5. If it is an open door: "⏎That door is already open.".
6. If it is a closed door: attempt to open it (Part 06 owns the attempt, including door traps and the chance of it sticking), then report "⏎The door opens." or "⏎The door resists." according to whether the cell is now an open door.

Close door (C):

1. While time is stopped: the same refusal message.
2. Redraw the player's surroundings.
3. Enhanced: ask the direction and echo it, set the drop flag, then: off-map or not a door gives the "no door there" messages as above; a closed door gives "⏎That door is already closed."; an open door becomes a closed door with its trap state cleared, "⏎The door closes.".
4. Classic: if the player is not standing on an open door, "⏎There is no open door here.". Otherwise "⏎The door closes.", the cell becomes a closed door with its trap state cleared (its remembered appearance is reset to "nothing" until the next redraw of the player's surroundings), and the drop flag is set so the player (now standing inside a closed door) is not at once asked to open it.

Monsters or items in the doorway do not prevent closing in the enhanced form.

Open chest (O, enhanced only): if the player's cell is not a chest, "⏎There isn't a chest to open here."; otherwise run the chest-opening routine (Part 06) and set the drop flag.

### 9.10 Praying and the other "stand on it" actions

There are no command keys for praying, sitting on thrones, drinking from or washing at fountains, taking stairs, entering buildings, picking things up, opening chests in the classic interface, and so on. All of these are offered by the object check at the start of each turn (8.2 step 1) when the player stands on the feature or item: the game describes what is there and asks a one-key question with its own answer letters (for example go inside or stay, eat or take or ignore). Part 06 lists every such prompt and outcome; Part 08 covers the buildings. For several features nothing is offered while a monster is adjacent. To be asked again, rest for a turn.

### 9.11 Teleport and identify traps

Teleport (Z):

- In wizard mode: ask "⏎Which level do you wish to teleport to? " (number entry, default 20). A number below 0 or above 20 gives " sorry!". Otherwise pick a random position x = rnd(65), y = rnd(15), enter that level (generating it if new; in wizard mode a new level arrives fully mapped), settle on the nearest free cell, redraw everything.
- Otherwise, if the character's experience level is at least 10: the ordinary random teleport, the same routine a teleport trap uses but called in its "risky" form, which first rolls a small chance of ending up in solid rock (Part 06 has the routine and the odds).
- Otherwise: "⏎You don't know how to teleport yet.".

All three branches use the turn.

Identify traps (^): prints a blank line, then looks at the nine cells of the 3 by 3 block centred on the player (clipped to the map), row by row from the top, left to right, and for each cell holding a revealed trap door, dart trap, arrow trap, teleport trap, up elevator or down elevator prints "⏎It's " followed by the object's name ("a trapdoor", "a dart trap", "an arrow trap", "a teleport trap", "an elevator going up", "an elevator going down"), with no full stop and no indication of where. If none: "⏎No traps are visible.". It reads the real map, not the player's knowledge, so it names a revealed trap next to a blind player; it never detects hidden traps. It uses the turn.

### 9.12 Information commands

| Command | Behaviour |
|---|---|
| Inventory (i) | shows the inventory screen: gold, every carried item, and the elapsed-time line (section 7); Part 03. No turn |
| List known (backslash or D) | text screen listing the spells known and the scrolls and potions identified so far; Parts 03 and 07. No turn |
| Pack weight (g) | "⏎The stuff you are carrying presently weighs N pound(s)." with N from the pack-weight formula of Part 02. No turn |
| Tax status (P) | "⏎You presently owe N gp in taxes." if the outstanding tax bill is positive, else "⏎You do not owe any taxes.". Uses a turn |
| Version (v) | "⏎The Addiction of Ularn -- Version 1.6.3 (26 Mar 2004)⏎Difficulty level K", then " (WIZARD)" in wizard mode, then " (Cheater)" if the save file was found tampered with (Part 03), then a line break and the three credit lines of section 1. No turn |
| Redraw (Ctrl+R) | redraws the whole map display. No turn |
| Help (?) | the help pages, section 11. No turn |

### 9.13 Save and quit

Save (S):

1. Clear the message area and print "Saving . . .".
2. Write the save file (Part 03). If that fails, print "⏎Save game failed.⏎" (after the save routine's own "Can't open file" line).
3. End the session as "suspended": the checkpoint file is deleted, "Tidying up...⏎" is shown and the process exits without showing scores.

The session ends even when the save failed; in that case the game in progress is lost (the checkpoint has just been deleted too).

Quit (Q): asks "⏎⏎Do you really want to quit? (y)es, (n)o, (s)ave" and accepts y, n, s or Escape.

- y: the game ends at once as a "quick quit": checkpoint deleted, "Tidying up...⏎", exit. No score page is shown and nothing is recorded (Part 03).
- n or Escape: " no."; play continues, no turn used.
- s: " save." then "⏎Saving..." and the same as the save command (without the failure message).

Life protection never intervenes in a quit or a save.

### 9.14 Wizard mode and the debug commands

The wizard password is the fixed word "fizban". The password prompt prints "⏎Enter Password: ", reads at most 8 characters echoed as asterisks, and compares exactly; a wrong entry prints "⏎Sorry.⏎".

Wizard mode is a flag saved with the game. While it is on: the game is barred from the scoreboard (Part 03); newly generated levels arrive fully mapped (Part 05); item bonuses are always displayed, even +0; the level indicator is never hidden after a teleport (Part 09); and the commands below are enabled.

| Key | Command | Behaviour |
|---|---|---|
| - | toggle wizard | If on: "⏎You are no longer a wizard." and off. If off: ask the password; correct gives "⏎You are now a wizard."; wrong gives the password prompt's "Sorry." followed by a second "Sorry.⏎". No turn |
| _ | become creator | If not a wizard, ask the password (a wrong one ends the command). Then grant 370,000,000 experience points, which raises the character to the top of the level table (Part 02), recompute and redraw. It does not turn wizard mode on, so a character raised this way with the password but outside wizard mode is not barred from the scoreboard by this command. No turn |
| backquote | debug mode | See below. No turn |
| Z | teleport to a chosen level | 9.11 |
| + | create | Wizard only (otherwise nothing, but a turn passes). See below |
| ~ | diagnostic dump | Wizard only (otherwise nothing, but a turn passes). Section 13 |
| = | Wild debug menu | Wild Edition hook, see Part 12 |

Debug mode (backquote). If not already a wizard, ask the password (wrong: stop) and turn wizard mode on. Then, in this order:

1. Set the six ability scores (strength, intelligence, wisdom, constitution, dexterity, charisma) to 70.
2. Empty inventory slots a and b (whatever was there is simply deleted), then add a ring of protection +50 and a lance of death +25 through the normal pick-up routine, and wield that lance. Mark the character as owning a lance of death.
3. Remove worn armour and shield (they stay in the pack).
4. Grant 370,000,000 experience points.
5. Add 25000 turns of awareness.
6. Learn every spell and mark every potion type as known. Scroll types are meant to be marked known as well, but the original marks the first 28 potion entries a second time instead, so scrolls stay unidentified.
7. Decorate the current level's border (overwriting whatever is there, including the outer walls):
   - top row: a scroll of type i at column i for each of the 28 scroll types that has a real name; a potion of type i at column 66 − i for each of the 35 potion types that has a real name (Part 06 has the type lists; unnamed filler types are skipped);
   - then 97 consecutive object ids, starting with the holy altar (id 2) and counting upward, laid out along the left column from row 1 to row 16 (ids 2 to 17), the bottom row from column 1 to column 66 (ids 18 to 83) and the right column from row 1 to row 15 (ids 84 to 98), all with a zero argument. The object id order is given in Part 06; ids 99 and above (the last two base objects and all Wild Edition objects) are not placed.
8. Make the whole level known.
9. Add 250,000 gold.
10. Recompute statistics and redraw.

Create (+, wizard). Asks "⏎Type of item (Scroll/Potion/Monster/Other) : " and accepts S, P, M, O in either case, or Escape (which does nothing).

- Scroll: " Scroll Arg: ", a number (default 28), then echoes "⏎createitem(OSCROLL, N)" and creates a scroll of type N at the player's position (the creation routine places it on the nearest suitable cell, Part 05).
- Potion: " Potion Arg: ", a number (default 35), echo "⏎createitem(OPOTION, N)", likewise.
- Other: two line breaks, "Item : " and a number (default 0) for the object id, "Arg : " and a number (default 0), echo "⏎createitem(I, A)", create it.
- Monster: two line breaks, clear the message area, "Monst : " and a number (default 0), echo "⏎createmonster(N)" and create that monster next to the player (Part 04).

No range checks are made on the numbers. Each successful branch sets the drop flag.

---

## 10. Shared question helpers

| Helper | Behaviour |
|---|---|
| "More" pause | prints "⏎  --- press space to continue --- " with the word "space" highlighted, and waits for a space |
| Pager | list screens call it once per line printed; on every 22nd line it performs the "more" pause and clears the text display. A reset call restarts the count |
| Yes/no | waits for y, Y, n, N, Escape or space, and returns the key; a space is returned as Escape. Callers decide what Escape means |
| Password | section 9.14 |
| Direction | prints "⏎In What Direction? ", waits for one of the eight plain movement keys, prints the direction's name from the table in 9.2 and yields the direction number and the cell one step that way from the player (which may lie outside the map; callers check) |
| "Return to continue" | puts "Press return to continue: " at the start of line 24 with "return" highlighted and waits for Return or Escape |

---

## 11. The help system and the help file

### 11.1 File format

The help file "Uhelp" is plain ASCII text, 277 lines in the shipped file, no line longer than 88 characters (lines are read into a 127-character buffer; longer lines would be split).

- The very first character of the file encodes the number of help pages as the character whose code is the code of "0" plus the count. The shipped file has 11 pages, so the character is ";" (one past "9" is ":", two past is ";"). It is consumed and never displayed; the first line's text follows it directly.
- The first 23 lines are the welcome page (section 1.1): the story on lines 1 to 19 (three paragraphs of 5, 6 and 3 lines, each with its first line indented by four spaces and the rest flush left, separated by single blank lines; then two blank lines and the closing wish, also indented four spaces), then blank lines up to 23. The welcome page is not counted in the page number.
- Then follow the help pages, each exactly 23 lines, padded with blank lines. 23 + 11 × 23 = 276 lines; the shipped file has one extra trailing blank line.
- Text attributes are switched by five-character or four-character markers made of ordinary printable characters: a caret, two opening square brackets, then "7m" (highlight style 1, red in the original), "8m" (style 2, green), "9m" (style 3, blue), or just "m" (back to normal). These are literal characters, not real escape bytes. The marker characters are not displayed.

Marker parsing, exactly: when a caret is followed by two opening brackets and at least one more character exists on the line, those three characters are skipped; then if the next character is 7, 8 or 9 the corresponding style is selected and that character and the one after it (expected to be "m") are skipped; if instead it is "m", normal style is selected and it is skipped; any other character is skipped with no change of style. Everything else, including each line's line break, is printed as is.

A consequence that is visible in the shipped file: the note about the "list known items" key writes the backslash between a "7" marker and a normal marker without the "m", so the parser swallows the backslash as if it were the "m". On screen the sentence reads as: You may also type '' at any time. The key is never shown.

### 11.2 Viewing help

1. Switch to the text display and open the file; if it cannot be opened show the "Can't open help file" message for 4 seconds and return to the map.
2. Skip the 23 welcome lines.
3. For each page, counting down from the page total: clear the text display and print 23 lines. If it is not the last page, print (directly after the page, on line 24) "    ---- Press return to exit, space for more help ---- " with "return" and "space" highlighted, and wait for space, Return or Escape. Return or Escape closes help and returns to the map.
4. After the last page show the "Press return to continue: " prompt and return to the map.

Help uses no turn.

### 11.3 Page contents

Headings are in highlight style 1 unless stated.

Page 1, commands. A title line "Help File for The Caverns of Ularn" (indented about 14 columns), a blank line, the heading "Standard Commands", then twelve rows of three columns (key, two spaces, description; columns begin at character 1, 24 and 49) holding the 36 entries whose wording is the "help-file wording" column of table 9.1, laid out so that the first column runs h, j, k, l, y, u, b, n, ^, d, v, ?; the second H, J, K, L, Y, U, B, N, g, i, S, C; the third ., Z, c, r, q, W, T, w, P, Q, \, e. Two lines follow saying the keypad and arrow keys may also be used to move and that keypad 5 runs in the next direction entered. Then a blank line, the heading "Enhanced Interface Commands", two lines saying these are only available if the enhanced_interface option (highlighted) is set in the ularn.opt file, a blank line, and one row: o open a closed door, O open a chest here.

Page 2, options and notes. Heading "Command Line Options" and eight lines in the form "ularn -s" plus a description: -s list the scoreboard; -i list scores with inventories; -n suppress welcome message when beginning a game; -r restore checkpointed game; -d number specify difficulty level; -o optsfile specify the ularn.opt file to be used; -c create new scoreboard (requires wizard password); -h print out all the command line options. A blank line, heading "Special Notes", and three notes: when dropping gold an asterisk drops all of it, and in general an asterisk means "all" (also at the bank and when contributing at altars); in the store, trading post, school or home, Escape (highlighted) gets you out; when casting, typing D (highlighted) as the first letter of the spell lists the spells you can cast, after which the code can be entered, and the "list known" key (invisible, see above) shows all known spells and the potions and scrolls identified so far.

Pages 3 to 8, spells. Page 3 begins with the heading "Ularn Spell Descriptions" and three lines: the spells follow, some well understood and others less so, listed in order of their circle, each as Name (style 1), then "(Code) [Target] {Duration}" (style 3), and a description. Each of the six pages then has a circle heading in style 2 and its spells, each spell taking one header line and one or two description lines indented two spaces. The circle headings are "Spells of the Ekh-awd Mish-paw-khaw (First Circle)", "... Shay-nee ... (Second Circle)", "... Shel-ee-shee ... (Third Circle)", "... Reb-ee-ee ... (Fourth Circle)", "... Kham-ee-shee ... (Fifth Circle)", "... Shish-shee ... (Sixth Circle)".

| Page / circle | Spell | Code | Target | Duration | Gist of the description |
|---|---|---|---|---|---|
| 3 / first | Protection | pro | Self | Long | +2 protection field; non-cumulative, repetition lengthens it |
| | Magic Missile | mle | Range | Instant | a missile like a +1 magic arrow; improves with experience |
| | Dexterity | dex | Self | Long | +2 dexterity; non-cumulative, repetition lengthens it |
| | Sleep | sle | Touch | Instant | puts some monsters to sleep for easy hits |
| | Charm Monster | chm | Self | Short | raises your charm so some monsters are awed; repetition lengthens it |
| | Sonic Spear | ssp | Range | Instant | a screeching sound from the caster's hands; improves with experience |
| 4 / second | Web | web | Touch | Instant | entangles the enemy for easy hits |
| | Strength | str | Self | Long | +2 strength for a time; repetition lengthens it |
| | Enlightenment | enl | Level | "Permament" (spelled so) | the caster becomes aware of the surrounding area |
| | Healing | hel | Self | Instant | restores some health; improves with experience |
| | Cure Blindness | cbl | Self | Instant | restores sight |
| | Create Monster | cre | Next to | Instant | creates a monster near the caster |
| | Phantasmal Forces | pha | Touch | Instant | illusions that kill if believed; wisdom helps |
| | Invisibility | inv | Self | Long | the caster becomes invisible; repetition lengthens it |
| 5 / third | Fireball | bal | Range | Instant | a ball of fire that burns what it hits |
| | Cold | cld | Range | Instant | a cone of cold |
| | Polymorph | ply | Touch | Permanent | changes the attacker's nature, for better or worse |
| | Cancellation | can | Self | Long | stops monsters using special abilities; repetition and experience help |
| | Haste Self | has | Self | Long | speeds the caster; repetition and experience help |
| | Cloud Kill | ckl | Surrounds | Instant | poisonous fog; a strong spell |
| | Vaporize Rock | vpr | Surrounds | Instant | changes rock to air |
| 6 / fourth | Dehydration | dry | Touch | Instant | dries up water nearby; very powerful, improves with experience |
| | Lightning | lit | Range | Instant | lightning bolts from the finger |
| | Drain Life | drl | Touch | Instant | takes hit points from both you and a monster; powerful and dangerous |
| | Invulnerability | glo | Self | Long | a globe against physical attack; boosts defence at a cost |
| | Flood | flo | Surrounds | Instant | a deluge flooding the chamber; improves with experience |
| | Finger of Death | fgr | Touch | Instant | a holy spell; extremely powerful, potentially deadly |
| 7 / fifth | Scare Monster | sca | Self | Short | an aura that terrifies monsters; improves with experience |
| | Hold Monster | hld | Level | Short | freezes monsters; improves with experience |
| | Time Stop | stp | Level | Short | all movement in the caverns ceases; improves with experience |
| | Teleport Away | tel | Touch | Instant | teleports a monster |
| | Magic Fire | mfi | Surrounds | Instant | a curtain of fire around the caster |
| 8 / sixth | Make a Wall | mkw | Touch | Permanent | makes a wall in the chosen place |
| | Sphere of Annihilation | sph | Next to | Short | anything caught in it is instantly killed |
| | Genocide | gen | Game | Permanent | eliminates a species of monster |
| | Summon Demon | sum | Touch | Instant | summons a demon who may help; may backfire |
| | Walk Through Walls | wtw | Self | Short | walk through walls for a short time |
| | Alter Reality | alt | Level | Permanent | restructures reality as you know it |
| | Permanence | per | Self | Permanent | makes a character spell permanent (protection, strength, ...) |

The real behaviour of the spells is Part 07; this table records only what the help pages claim.

Page 9, the options file. Heading "How to use the ularn.opt option file". It says the file should be in your home directory or named with -o; that lines starting with "#" are comments; that game options are given with the word "OPTION=" followed by a comma-separated list, several such lines being allowed; that enabling options can be prefixed with "no" to disable; and that value options have the form word, colon, value. A two-column table with highlighted column heads "Option word" and "Meaning" lists: (no)welcome, (no)nap, (no)enhanced_interface, (no)beep, name:name, class:class, gender:male/female. Then: path names can also be given, "LIBDIR:location" and "SAVEDIR:location" (a form that does not actually work, 4.2), and otherwise games are saved in the current directory and the library is looked for in "./lib" (also not true of this build).

Page 10, an example. Heading "Example ularn.opt file", a blank line, then an eleven-line sample: comment lines, "OPTION=name:Fred,gender:male", "OPTION=nap", "OPTION=welcome", "OPTION=noenhanced_interface", "OPTION=beep", with a comment before some of them.

Page 11, the scoreboard. Heading "Explanation of the Ularn scoreboard facility" and one paragraph: there are two scoreboards, winners and deceased; each player gets one slot on each if in the top ten; this keeps frequent players from hogging the board; difficulty is noted and takes precedence over score when deciding which entry is kept (with an example of a low score at difficulty 1 replacing a high score at difficulty 0); and a dead player's inventory is stored for all to see. After blank lines, four centred credit lines: written in 1987 by Phil Cordier at UC Santa Cruz; updated in 2001 by Josh Brandt; a contact line with his address; Windows 32 conversion and tiles support in 2003 by Julian Olds.

---

## 12. The fortune file

Format. "Ufortune" is plain ASCII text, one fortune per line, 39 lines in the shipped file, the longest 64 characters. There is no header and no count. Lines are read with a limit of 79 characters (a longer line would become two fortunes); trailing white space is stripped; an empty line would become an (almost) empty fortune.

Loading and selection. The file is read on the first request and kept in memory for the rest of the session. A request returns the fortune at a uniformly random position, rund(number of fortunes). If the file cannot be opened the request yields nothing, and the next request tries again; an empty file also yields nothing.

When fortunes are shown.

- Eating a cookie from the pack (9.8), unless blind: "  Inside you find a scrap of paper that says:⏎" and the fortune.
- Eating a cookie directly off the floor (Part 06): after "The cookie tasted good.", unless blind: "  A message inside the cookie reads:⏎" and the fortune.
- Wild Edition: a message in a bottle can contain one (see Part 12).

When no fortune is available the lead-in line is omitted too.

Style and content. Each fortune is one short sentence, a hint or a tease about the game, mostly without a final full stop, occasionally a question or an exclamation. They fall into these groups: advice about items (gems, the hammer, the wand of wonder, the best weapon not having the highest weapon class, the most powerful scroll not being for sale, identify before use); about monsters (greedy monsters, nymphs' light fingers, kissing a disenchantress, dragons that fly, who is tougher than a demon prince); about rules (constitution and hit points on level-up, spell regeneration needing energy, difficulty affecting regeneration, dexterity letting you carry more, a level-25 character casting "like crazy"); about places and hazards (pay your taxes, ask the genie, the express elevator, trap doors, bottomless pits, opening doors, more than one way through a wall, sitting down having unexpected results, the king never being far from his throne, the dealer's shop); and about the quest itself. Examples, verbatim: "Nymphs have light fingers", "Be sure to pay your taxes", "Patience is a virtue unless your daughter dies", "Wonderful wands prevent you from falling", "What does a potion of cure dianthroritis taste like?".

---

## 13. The diagnostic dump

The wizard-only dump command prints "⏎Diagnosing . . .⏎", writes a text report to "diagfile.txt" in the working directory (overwriting it), and prints "⏎Done Diagnosing.⏎". If the file cannot be created nothing is printed at all. The report is for developers; a rebuild needs only something equivalent. Its sections, in order:

1. A banner line "-------- Beginning of DIAG diagnostics ---------".
2. Hit points as current(maximum); one line with gold, experience, character level and cave level.
3. "For the c[] array:" and the first 100 character attributes, one per line as index, a label and the value. The labels are, in index order: STRENGTH, INTELLIGENCE, WISDOM, CONSTITUTION, DEXTERITY, CHARISMA, HPMAX, HP, GOLD, EXPERIENCE, LEVEL, REGEN, WCLASS, AC, BANKACCOUNT, SPELLMAX, SPELLS, ENERGY, ECOUNTER, MOREDEFENSES, WEAR, PROTECTIONTIME, WIELD, AMULET, REGENCOUNTER, MOREDAM, DEXCOUNT, STRCOUNT, BLINDCOUNT, CAVELEVEL, CONFUSE, ALTPRO, HERO, CHARMCOUNT, INVISIBILITY, CANCELLATION, HASTESELF, EYEOFLARN, AGGRAVATE, GLOBE, TELEFLAG, SLAYING, NEGATESPIRIT, SCAREMONST, AWARENESS, HOLDMONST, TIMESTOP, HASTEMONST, CUBEofUNDEAD, GIANTSTR, FIRERESISTANCE, BESSMANN, NOTHEFT, HARDGAME, four blanks, VORPAL, LANCEDEATH, SPIRITPRO, UNDEADPRO, SHIELD, STEALTH, ITCHING, LAUGHING, DRAINSTRENGTH, CLUMSINESS, INFEEBLEMENT, HALFDAM, SEEINVISIBLE, FILLROOM, one blank, SPHCAST, WTW, STREXTRA, TMP, LIFEPROT, ORB, ELVUP, ELVDOWN, HAND, CUBEUNDEAD, DRAGSLAY, NEGATE, URN, LAMP, TALISMAN, WAND, STAFF, DEVICE, SLASH, ELVEN, VORP, SLAY, PAD, COKED, two blanks. (This is a label list for the dump; the authoritative attribute table is in Part 02.)
4. "Inventory": the 26 slots with a symbolic label, the object id, the object's name and its argument. The label table is a leftover from an older object numbering (98 names) and does not line up with the real object ids, so most labels are wrong, and ids of 100 or more are beyond the table.
5. "Here are the maps:" and, for each of the 21 levels in order, a ruled header "Map NAME    level N" and the level as 17 lines of 67 characters, each cell showing the monster's map letter if a monster is there and otherwise the object's map character (the ASCII character tables of Parts 04 and 06).
6. "Now for the monster data:", the total count, a column header (name, level, armour class, damage, attack, gold, hit points, experience) and one line per monster table entry, reflecting any difficulty scaling.
7. "Available potions:" and "Available scrolls:", each type name right-aligned in 20 columns.
8. "Spell list:" with each spell's code, name and description.
9. "Object list" with each object id, its map character and its name.
10. A banner line "-------- End of DIAG diagnostics ---------".

Side effects (wizard-only, but real): producing section 5 enters every level in turn, which generates all levels not yet visited (they become fully mapped, as always in wizard mode), re-rolls monsters on visited ones and may shift the player to the nearest free cell on each; afterwards the original level is reloaded. The command uses a turn.

---

## 14. Constants, limits and small tables in these modules

| Item | Value |
|---|---|
| Version / patch level / date | "1.6" / "3" / "26 Mar 2004", displayed as "1.6.3 (26 Mar 2004)" |
| Time limit | 40000 time units (400 mobuls) |
| Mobul | 100 time units |
| Upstream checkpoint interval (unused) | 400 moves |
| Checkpoint interval actually used | every 25th command prompt |
| Display name and login name length | 40 characters each |
| Path buffers | 1024 characters |
| Default options file name | ularn.opt |
| Data file names | Uscore, Uhelp, Umaps, Ufortune |
| Diagnostic file name | diagfile.txt |
| Wizard password | fizban (entry limited to 8 characters) |
| Default option flags | bold objects on, mail on, checkpointing on, beep on, welcome on, delays on, enhanced interface off |
| Default sex | male |
| Options file line limit | 255 characters |
| Help page size | 23 lines; line buffer 127 characters; page count in first character |
| Pager page size | 22 lines |
| Fortune line limit | 79 characters |
| Command-line difficulty range | 0 to 100 |
| Experience granted by the wizard "creator"/debug commands | 370,000,000 |
| Debug-mode extras | abilities 70; ring of protection +50; lance of death +25; awareness +25000; gold +250000 |
| Wizard teleport default level | 20 |
| Level needed for the teleport command | 10 |
| Random-monster timer | starts at 80; reset to 120 − 4 × level |
| Hit-point regeneration reset | 22 + 2 × difficulty − level (initially 16) |
| Spell regeneration reset | 100 + 4 × (difficulty − level − energy) (initially 96) |
| Gold pile thresholds when dropping | 32767; 327670; 3276700; 32767000 |
| Item-prompt answer sets | letters a to z, asterisk, Escape; plus period for drop; plus minus for wield |
| Direction tables | section 9.2 |
| Escape key code | 27 |

The front-end interface also fixes, by order, four small enumerations that other parts rely on: text formats (normal, highlight 1, highlight 2, highlight 3, bold, inverse), display modes (map, text), the five directional spell-effect kinds and the fifteen magic-effect animations (Parts 07 and 09), and the list of commands (the rows of table 9.1).

---

## 15. Quirks worth preserving (summary)

1. Every item command (wield, wear, read, quaff, eat, drop), casting, taking off, teleporting, identifying traps and asking for tax status use a turn regardless of outcome; even Escape at the item prompt costs a turn. Inventory, known-items, pack weight, version, help, redraw, space and a declined quit do not.
2. A blocked move costs nothing; a run that ends against a wall adds no extra turn; a run that ends because a monster attacked costs one extra time unit.
3. Running across hidden traps does not trigger or reveal them.
4. The object underfoot is re-offered every turn the player remains on it.
5. Escape at any number prompt answers with the default, so Escape at "How much gold do you drop?" drops all gold.
6. Large gold drops are rounded down to the pile's unit; the remainder stays in the purse.
7. Saving ends the session even if writing the save failed.
8. The "become creator" command, given the password, does not turn wizard mode on.
9. Debug mode fails to identify scrolls (it marks potions twice).
10. Hasted players get two actions per monster action and per clock tick, but regeneration and effect timers still tick per action.
11. The time limit is checked only at home, with a strict "greater than" test, and never scales.
12. The difficulty rescaling uses a divisor of 5 for damage and 6 for hit points.
13. The options parser is whitespace-sensitive, abandons a line at the first unknown keyword, and rejects the LIBDIR/SAVEDIR syntax its own help page documents.
14. The help page count is a single character beyond "9" (";" for 11), and one help sentence loses its backslash to the marker parser.
15. The usage text names the wrong default options file; the help file mentions keypad controls and a "./lib" directory that do not apply.
16. There are no repeat counts or numeric prefixes.
