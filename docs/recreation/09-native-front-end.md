# Part 09 — Native front end: display/input contract, shared state, process model, context action

This part specifies the layer between the game core (the C rules engine of Parts 01–08 and 12) and the Android app (Parts 10, 11 and 13). It covers: how the game is run as a separate process and wired to the app; the shared display-state block that is the whole contract between game and renderer; every function of the display/input interface the core calls, with exactly what the Android implementation does; input modes and key handling; checkpointing; the context-action export; the minimap/inventory export; the event ring; and the JNI/Kotlin bridge that reads all of it. It leans on Part 01 (turn loop, commands, options), Part 03 (save files, scoreboard, death pages), Part 07 (which spell uses which animation), Part 10 (start screen, save slots, settings), Part 11 (how the renderer draws what is published here), Part 12/13 (meaning of the Wild fields and events) and Part 14 (what each tile id looks like).

Conventions used here: map coordinates are (x, y) with x = 0..66 (west to east) and y = 0..16 (north to south). Text cursor positions are 1-based (column 1..80, line 1..N). "The core" means the upstream game logic; "the front end" means the Android implementation of the display/input interface; "the app" means the Kotlin side. `rnd(n)` is the game's helper returning a uniform integer 1..n (remainder of the C library `rand()` by n, plus 1); `rund(n)` returns 0..n-1.

---

## 1. Architecture and process model

### 1.1 The pieces

| Piece | Role |
|---|---|
| Game core (static library) | The unmodified-in-spirit Ularn 1.6.3a sources plus the Android front end, the side-panel export, the context export and the Wild files, compiled together. |
| Native bridge (shared library named `ularn`) | A spawner (forks the game, owns the shared block and pipes) and five JNI entry points. |
| Kotlin `NativeBridge` | Declares the five native functions and loads the library. |
| Kotlin `GameSession` | Owns one running game: prepares files, starts it, sends keys, hears "something changed" and "it exited". |
| Kotlin `GameState` | A snapshot reader over the shared block. |
| Kotlin `GameEvents` | Drains the event ring and dispatches events to listeners. |

The upstream TTY, X11, Win32 and Amiga front ends are not built. The Android front end was ported from the X11 one: same text model, same tile tables, same default key bindings; but instead of drawing, every display call edits the shared block, and instead of window events, input is a stream of bytes.

### 1.2 Why the game is a forked child process

1. The core keeps all its state in global variables and assumes each program run starts with them pristine. A fresh process per game gives that for free.
2. The core calls the C library's "exit" from deep inside the game (after death, quit, save, fatal errors) and expects never to return. In the child that is harmless.
3. The core blocks waiting for input in the middle of deep call stacks (prompts inside spells, shops and so on). A separate process can simply block on a pipe read.
4. Android apps cannot conveniently ship and execute a standalone binary, so the game is linked into the app's native library and started by "fork without exec": the child is a copy of the app process that immediately calls the game's main function.

Two compile-time shims make this work, applied to every game source file by a forced include: the game's main function is renamed (so the spawner can call it), and every call to "exit" is redirected to a function that flushes all C stdio streams and then terminates the process immediately without running exit handlers (running the real exit in a forked copy of an app process would run the app runtime's own shutdown handlers). The same forced include replaces the upstream configuration header: it selects the Unix file-handling paths, selects the Android front end, and sets the game's library directory to "." (the working directory).

Build flags that affect behaviour: the core is compiled with `char` forced to be signed (the game stores −1 in `char` direction tables; with ARM's default unsigned `char` every move with a negative component became "off the map"), as GNU C99, with warnings off, and with hidden symbol visibility plus symbolic linking so the game's globals (some share names with C library functions) can never bind to the C library's symbols. Only the JNI entry points are exported.

### 1.3 Spawning a game, step by step

The spawner is given a working directory, an argument list, an environment list, and returns the child's process id and two file descriptors.

1. Obtain the shared block (section 2). It is created once per app process, on first use, as an anonymous shared memory mapping the size of the block, readable and writable; every later game reuses the same mapping. If mapping fails, spawning fails with "out of memory".
2. Zero the whole block, then set the magic number, the version number, and the character-class field to −1. (So every counter in it, including the change sequence and the event sequence, restarts from 0 for every game.)
3. Create two pipes: the key pipe (app writes, game reads) and the notify pipe (game writes, app reads). Make the game's writing end of the notify pipe non-blocking, so the game can never stall on a notification.
4. Fork. On failure close everything and return the negated error number.
5. In the child:
   1. Unblock all signals, reset every signal to its default action, then ignore the broken-pipe signal, and start a new session (detaching from the app's process group).
   2. Change to the working directory; if that fails, terminate with status 126.
   3. Redirect standard input from the null device; redirect standard output and standard error to a file named `ularn.log` in the working directory (created or truncated, owner read/write only).
   4. Close the unused pipe ends, then close every other inherited file descriptor above 2 except the key-pipe read end and the notify-pipe write end (found by listing the process's own descriptor directory).
   5. Put each environment string into the environment.
   6. Point the front end at the shared block and the two descriptors.
   7. Make standard output line-buffered, call the game's main function with the arguments, and terminate with its return value (in practice main never returns; the game ends through the redirected exit).
6. In the parent: close the child's pipe ends, mark the two remaining descriptors close-on-exec, and return the process id, the key-pipe write end and the notify-pipe read end.

A wait function blocks until the child ends and returns its exit status, or 128 + signal number if it was killed by a signal, or −1 on failure.

### 1.4 Directories and files

| Path | What |
|---|---|
| Working directory: `<app private files>/ularn163` | The game's current directory and its "library directory". Shared by all save slots. |
| `Uhelp`, `Umaps`, `Ufortune` (in the working directory) | The help text, pre-made level maps and fortunes. Copied from the app's bundled assets (asset folder `ularn/`) over the top of any existing copies at every game start. |
| `Uscore` (in the working directory) | The scoreboard. Created by the game at start-up if missing. Because it lives in the working directory, one scoreboard is shared by every save slot. |
| `ularn.log` (in the working directory) | The child's standard output and error; truncated at each start. |
| HOME directory | A save slot's own folder (Part 10), or the working directory itself when no slot is selected (used for the scoreboard-only run). Created if missing. |
| `ularn.opt` (in HOME) | The options file, rewritten by the app at every start. |
| `ularn_player.sav` (in HOME) | The saved game ("player" is the fixed login name, section 4.12). |
| `ularn_player.ckp` (in HOME) | The checkpoint (section 7). |

The options file the app writes has two lines: a comment line "# Written by the Ularn app; edit in Settings." and one line starting with the word OPTION followed by comma-separated options: `name:<character name>`, `gender:male` or `gender:female`, then optionally `nowelcome`, `nonap`, `nobeep`, and finally either `enhanced_interface` or `noenhanced_interface`. The character name has commas, colons and line breaks replaced by spaces, is trimmed, cut to 30 characters, and becomes "Adventurer" if blank. (Which settings produce which option is Part 10; how the core parses the file is Part 01.) The app never writes a class option, nor the library/save directory options.

### 1.5 Environment variables

The app passes exactly four:

| Variable | Value | Read by |
|---|---|---|
| `HOME` | Absolute path of the HOME directory above | The core's set-up: the options file, the save file and the checkpoint file all live there. |
| `USER` | `player` | The front end's user-name function (section 4.12): becomes the login name used in the save and checkpoint file names. |
| `LOGNAME` | `player` | Nothing in the game reads it; it is passed for tidiness. |
| `ULARN_WILD` | Decimal integer: the Wild Edition feature mask for a *new* character | The Wild hub at start-up (Part 12). Missing or empty means "all features" (4095); the value is masked to 12 bits. A restored game keeps the mask stored in its save. |

One more variable is *read* but never set by the app: `ULARN_SEED`. If present and non-empty, the core seeds its random generator with that decimal number instead of the wall-clock time, making a run reproducible. Only the desktop test harness (Part 15) sets it; in the shipped app the seed is always the start time in seconds.

### 1.6 Arguments

Argument 0 is always the word `ularn`. The app then appends one of:

| Extra arguments | When | Effect in the core (Part 01/03) |
|---|---|---|
| none | New game in an empty slot at difficulty 0, or continuing a slot that has a save file | If the save file exists it is restored (and deleted); otherwise a new character is made. |
| `-d N` | New game at difficulty N (1..10 from the app's slider; the core accepts up to 100) | Sets the difficulty. |
| `-r` | Resuming a slot that has a checkpoint but no save file | Restores the checkpoint file. |
| `-s` | "Scoreboard" on the start screen (HOME = working directory) | Shows the scoreboard, waits for return, exits. |

The app's rule "a save wins over a checkpoint" mirrors the core: with `-r` the checkpoint is restored and an existing save file would be left behind, stale, to win next time.

### 1.7 The two pipes

* **Key pipe.** The app writes raw bytes; each byte is one key. The game reads one byte at a time, blocking. Bytes sent while the game is busy (animating, sleeping) simply queue in the pipe and are consumed later, in order: there is no type-ahead flushing anywhere.
* **Notify pipe.** After every batch of display changes the game writes a single byte (value 1) without blocking; if the pipe is full the write is dropped (the app is already due to redraw). The byte carries no information: it only means "the shared block changed; look at it".

### 1.8 How the app learns the game has ended, and how

The app runs a waiter thread blocked in the wait function. When the child ends, the thread posts to the main thread, which (if that process is still the current one) marks the session not running, closes the key pipe, signals one last "changed", and reports the exit code to the app shell.

| Exit status | Meaning |
|---|---|
| 0 | Every orderly ending: death or win (after the final pages), quit, save-and-exit, scoreboard-only run, and the core's fatal-error endings. |
| 1 | The key pipe reached end-of-input (the app closed it or died): the game left "like a hang-up", without saving and without deleting anything. |
| 126 | The child could not enter the working directory. |
| 128 + n | Killed by signal n (137 for the kill signal). |

The exit code alone does not say what happened to the character. The shared block is not cleared when the game ends, so the app reads the final state from it and combines that with the files left behind:

* The block's end-state fields (section 6.3) say died / won / (quit), with score and cause, if the final score page was shown.
* If a save file or checkpoint file exists in HOME afterwards, the run is resumable (saved, or left with a checkpoint).
* If neither exists and no end state was recorded, the player quit (the quick-quit path shows no score page).

### 1.9 Lifecycle

| Moment | What happens |
|---|---|
| Start | Refused silently (treated as success) if a game is already running; fails if the shared block is unavailable. Otherwise: copy the data files, write the options file, spawn, start the notify-reader and waiter threads, and signal "changed" once. |
| App goes to the background (activity stopped) | If a game is running the app sends the checkpoint key (section 7). The game process stays alive, blocked on its key read. Music pauses when the activity is merely paused (Part 13). |
| App process killed by the system while in the background | The key pipe's write end disappears, the game reads end-of-input and exits with status 1 (if it is not killed along with the app). The checkpoint written when the app was stopped stands, and the slot offers "resume" next time. |
| "Leave" (menu) or the activity finishing | The app sends the checkpoint key and then closes the key pipe, both through the same single-threaded writer so the order is kept. At a command prompt the game writes the checkpoint, then sees end-of-input and exits with 1. Inside a sub-prompt the checkpoint key is ignored, so the last automatic checkpoint (at most 24 commands old) stands. |
| "Save game & quit" (menu) | The app sends the letter S (the core's save command): the save file is written, the checkpoint is deleted, the game exits with 0. |
| Kill | The session can send the kill signal (9) to the child. Nothing in the app currently calls it. |
| A second game | Reuses the same shared block; the spawner zeroes it first. |

### 1.10 What a re-implementation must preserve, and what it may change

Free to change (not observable by the player):

* The process model. A single-process design is fine: for example the game on its own thread with all state reset at each start, a blocking key queue instead of a pipe, and a lock-protected or atomically swapped snapshot instead of shared memory. Exit can be an unwinding of the game thread instead of a process exit.
* The byte layout of the state block, the offset-export mechanism, the notify mechanism (any "changed" signal with a monotonically increasing sequence number will do), the log file.
* The exact exit status numbers, provided the same distinctions can be made (ended normally / input closed / failed to start / killed).

Must be preserved (observable):

* The game *blocks* for input at well-defined points, publishes which kind of input it is waiting for (with the valid answers for single-key prompts), and publishes "busy" in between.
* Keys are a strictly ordered byte stream; keys sent while busy are consumed later; nothing is discarded except by the rules of the current input mode.
* Every item in the shared state (section 2) with the same meaning and the same update moments, including the fact that some values are only refreshed at particular moments (noted per field).
* A change sequence number that changes on every publish: the app's "send a key, then answer the prompt that follows" logic relies on "sequence changed since I sent my key" meaning "the game has reacted".
* Animation timing: each animation frame is published, then the game sleeps for the stated time before continuing.
* Checkpoint timing and semantics (section 7), the file names and directory roles (section 1.4), the save/checkpoint precedence, and the three ways a run can end with their different leftovers (section 1.8).
* State starts pristine for every game, and the final display state remains readable after the game has ended.
* One scoreboard shared by all slots; per-slot options, save and checkpoint.

---

## 2. The shared display state

One fixed-size block, written only by the game, read only by the app. All integers are 32-bit signed in native byte order unless stated; "text" fields are byte strings in a single-byte encoding (the app decodes them as ISO-8859-1), NUL-terminated unless stated. Two-dimensional arrays are stored row by row (all of row 0, then row 1, ...), and map arrays are indexed [y][x].

### 2.1 Constants

| Name | Value | Meaning |
|---|---|---|
| Magic | 0x554C524E ("ULRN") | Block is initialised. |
| Version | 2 | 2 = layout with Wild Edition fields. (The app does not check it.) |
| Map width × height | 67 × 17 | |
| Line length | 80 | Columns in both text areas. |
| Message lines | 5 | |
| Text-page lines | 25 | |
| Effect indicators | 17 | |
| Inventory slots × text length | 26 × 80 | |
| Spell slots | 40 | At least the game's 39 spells. |
| Context-name length | 40 | |
| Kill-count slots | 96 | At least the number of monster ids (70). |
| Event ring size | 64 | A power of two. |

Display mode: 0 = map, 1 = text page. Input mode: 0 busy, 1 command, 2 single-key prompt, 3 direction, 4 number, 5 password. Text format codes: 0 normal, 1 standout, 2 standout 2, 3 standout 3, 4 bold, 5 inverse (the core only ever uses 0–3; how each is drawn is Part 11). End state: 0 none, 1 died, 2 won, 3 quit.

### 2.2 Every field, in order

Offsets and sizes are those of the natural C layout (4-byte alignment; total size 14,892 bytes). They are informative: the app learns offsets at run time (section 11.2), so a rebuild may lay the block out differently.

| # | Field | Type | Offset | Bytes | Meaning | Written when |
|---|---|---|---|---|---|---|
| 1 | magic | int | 0 | 4 | The magic constant. | Spawn. |
| 2 | version | int | 4 | 4 | The version constant. | Spawn. |
| 3 | seq | int (volatile) | 8 | 4 | Change sequence: incremented once per publish (section 3). 0 means "nothing published yet". | Every publish. |
| 4 | display_mode | int | 12 | 4 | 0 map (map + status + 5 message lines), 1 text page (25 lines). | Front-end start; every display-mode switch. |
| 5 | input_mode | int | 16 | 4 | What the game is waiting for (0 = not waiting). | On entering and leaving each input function. |
| 6 | answers | text[64] | 20 | 64 | Acceptable keys for the current prompt (at most 63 kept). Empty when not applicable. | With input_mode. |
| 7 | caret | int | 84 | 4 | 1 = show a text caret at the cursor position. | With input_mode. |
| 8 | cursor_x | int | 88 | 4 | Text cursor column, 1-based, in the *active* text area (message area in map mode, text page in text mode). | Every publish. |
| 9 | cursor_y | int | 92 | 4 | Text cursor line, 1-based. | Every publish. |
| 10 | beep_count | int | 96 | 4 | Number of beeps so far; the app reacts to a change. | Each beep (unless beeps are off). |
| 11 | player_x | int | 100 | 4 | Player's map column as last drawn. | Full map repaint; "show player". |
| 12 | player_y | int | 104 | 4 | Player's map row as last drawn. | Same. |
| 13 | player_cursor | int | 108 | 4 | 1 = draw the cursor tile (id 174) over the player's square; 0 when the player is blind. | Same. |
| 14 | tiles | int16[17][67] | 112 | 2278 | Tile id displayed on each map square (ids per Part 14). | Per-square updates (section 4.7–4.9). |
| 15 | overlay | int16[17][67] | 2390 | 2278 | A second tile drawn on top of the square, −1 = none. Used by the sparkle animation and by Wild features (egg cracks). | Set to −1 whenever a square's tile is recomputed; set by overlay animations. |
| 16 | status1 | text[96] | 4668 | 96 | First classic status line, preformatted (section 4.5). | Status refresh. |
| 17 | status2 | text[96] | 4764 | 96 | Second classic status line, preformatted. | Status refresh. |
| 18 | hp | int | 4860 | 4 | Current hit points. | Status refresh. |
| 19 | hpmax | int | 4864 | 4 | Maximum hit points. | Status refresh. |
| 20 | spells | int | 4868 | 4 | Spell points left. | Status refresh; also each command prompt. |
| 21 | spellmax | int | 4872 | 4 | Maximum spell points. | Same. |
| 22 | gold | int | 4876 | 4 | Gold carried. | Status refresh. |
| 23 | plevel | int | 4880 | 4 | Character experience level. | Status refresh. |
| 24 | ac | int | 4884 | 4 | Armour class. | Status refresh. |
| 25 | wc | int | 4888 | 4 | Weapon class. | Status refresh. |
| 26 | exp | int | 4892 | 4 | Experience points. | Status refresh. |
| 27 | dlevel | int | 4896 | 4 | Dungeon level index: 0 town, 1–15 dungeon, 16–20 volcano. Always the true level, even when the level name is hidden. | Status refresh. |
| 28 | level_name | text[8] | 4900 | 8 | Two-character level name: " H", " 1" … " 9", "10" … "15", "V1" … "V5" (note the leading space on single-character names; the app trims it), or "?" when the player does not know where they are (after a teleport trap). | Status refresh. |
| 29 | effects | int | 4908 | 4 | Bit i set = timed effect i is active (section 4.6). | Effects refresh. |
| 30 | class_num | int | 4912 | 4 | Character class 0..7, or −1 before a character exists. | Spawn (−1); status refresh. |
| 31 | sex | int | 4916 | 4 | 0 female, 1 male. (Exported, but the app has no reader for it.) | Status refresh. |
| 32 | stats | int[6] | 4920 | 24 | Strength (including the temporary strength bonus), intelligence, wisdom, constitution, dexterity, charisma. | Status refresh. |
| 33 | level_title | text[40] | 4944 | 40 | Title for the experience level, exactly as the core's table has it: centred with spaces to 19 characters, e.g. "  novice explorer  ". | Status refresh. |
| 34 | game_time | int | 4984 | 4 | The game's turn counter. Only as fresh as the last status refresh. | Status refresh. |
| 35 | hardness | int | 4988 | 4 | Difficulty level. | Status refresh. |
| 36 | msg_chr | char[5][81] | 4992 | 405 | Message area characters: 5 lines of 80, each followed by a NUL. Space-filled. | Text output in map mode. |
| 37 | msg_fmt | byte[5][80] | 5397 | 400 | Format code of each message-area cell. | Same. |
| 38 | text_chr | char[25][81] | 5797 | 2025 | Text page characters: 25 lines of 80, each followed by a NUL. | Text output in text mode. |
| 39 | text_fmt | byte[25][80] | 7822 | 2000 | Format code of each text-page cell. | Same. |
| 40 | mm_cat | byte[17][67] | 9822 | 1139 | Minimap class of each square (section 9.1): low 5 bits = class 0..15, bit 0x80 = a monster is displayed there. | Whenever that square's tile is recomputed. |
| 41 | inv_text | text[26][80] | 10961 | 2080 | Inventory slot a..z as one line of text, "" for an empty slot (section 9.2). | Every publish (only rewritten when changed). |
| 42 | ctx_object | int | 13044 | 4 | Object id under the player; reads as "nothing" (1) whenever the context kind is "none" so hidden traps never leak. | Each command prompt. |
| 43 | ctx_kind | int | 13048 | 4 | Context kind 0..16 (section 8). | Each command prompt. |
| 44 | ctx_flags | int | 13052 | 4 | Bit 1 enhanced interface on; bit 2 blocked; bit 4 time is stopped. | Each command prompt. |
| 45 | ctx_doors | int | 13056 | 4 | Bit i: a known closed door lies in the direction of the i-th key of "hjklyubn". | Each command prompt. |
| 46 | ctx_name | text[40] | 13060 | 40 | Name of the object underfoot, e.g. "a staircase going down" (at most 39 characters). | Each command prompt. |
| 47 | spell_count | int | 13100 | 4 | Number of spells in the game (39). | Each command prompt. |
| 48 | spell_known | byte[40] | 13104 | 40 | 1 if spell i is known to this character. | Each command prompt. |
| 49 | spell_codes | text[40][4] | 13144 | 160 | Three-letter casting code of spell i (Part 07). | Each command prompt. |
| 50 | run_id | int | 13304 | 4 | Random identity of the current character, 0 = no character yet (section 6.1). | Each command prompt. |
| 51 | kills | int | 13308 | 4 | Monsters this character has killed in melee/spell combat (section 6.2). | Each kill; each command prompt; game end. |
| 52 | end_state | int | 13312 | 4 | 0 none, 1 died, 2 won, 3 quit: set when the final score page is shown (section 6.3). | Game end. |
| 53 | end_score | int | 13316 | 4 | Final score. | Game end. |
| 54 | end_reason | text[81] | 13320 | 81 | Cause, e.g. "killed by a giant ant on 3". | Game end. |
| 55 | wild_flags | int | 13404 | 4 | Low 16 bits: the character's Wild feature mask; high 16 bits: its perk bits. | Full map repaint. |
| 56 | hat_tile | int | 13408 | 4 | Tile to draw over the player (a worn hat), 0 = none. | Full map repaint; hat changes (Part 12). |
| 57 | glitch | int | 13412 | 4 | Turns of "glitch" left, 0 = normal. | Full map repaint. |
| 58 | weather | int | 13416 | 4 | Hour of day (0–23) in the low byte, weather state in the next byte. | Full map repaint; weather changes (Part 12). |
| 59 | coin_price | int | 13420 | 4 | LarnCoin price in gold; negative means "hide the ticker" (the magnitude is still the price). | Full map repaint. |
| 60 | pet_x | int | 13424 | 4 | Pet's column on this level, −1 = none here. | Full map repaint; pet updates (Part 12). |
| 61 | pet_y | int | 13428 | 4 | Pet's row, −1 = none here. | Same. |
| 62 | pet_info | int[6] | 13432 | 24 | [0] pet species (0 = no pet, rest 0); [1] hit points (as last seen if on another level); [2] maximum hit points; [3] growth stage 0 (baby) .. 3; [4] its own kills; [5] kills at which it next grows, 0 = fully grown. | Pet updates (Part 12). |
| 63 | pet_level | text[8] | 13456 | 8 | Name of the level the pet was left on ("3", "V1"), "" when it is with the player or on its way. | Pet updates. |
| 64 | bounty_info | int[4] | 13464 | 16 | [0] wanted species (0 = no poster, rest 0); [1] reward in gold; [2] 1 = killed, reward waiting in town; [3] turn after which an unanswered poster is taken down at the next town visit (0 = claimed, or to be taken down regardless). | Zeroed at each full map repaint, then refilled by the bounty feature (Part 12). |
| 65 | kill_counts | int[96] | 13480 | 384 | Kills per monster id by this character (saved with the game). | Full map repaint only. |
| 66 | ev_seq | int (volatile) | 13864 | 4 | Number of events ever appended this game. | Each event. |
| 67 | ev | int[64][4] | 13868 | 1024 | Event ring: entry (n mod 64) holds event number n as {type, a, b, c} (section 10). | Each event. |

Notes that apply to several fields:

* "Status refresh", "effects refresh" and "full map repaint" are defined in sections 4.5–4.7. None of them happens while the display is in text mode, so those fields freeze while a text page is up.
* "Full map repaint only" fields (the Wild summary values and per-species kill counts) can lag behind the game by many turns: they are refreshed when the display returns to map mode from a text page, on a "redraw screen" request, and whenever the core asks for an area redraw. Wild features that need promptness publish their own fields directly (Part 12).
* Before the first switch to map mode the tile array is all zeros (tile 0), the minimap classes are all "unknown", and the overlay array is all −1. A renderer must not draw the map until the display mode is "map"; the first switch to map mode repaints everything.
* The preformatted status lines are written without an explicit terminator, relying on the block having been zeroed at spawn and the text never exceeding 95 characters.

---

## 3. Publishing changes and concurrency

"Publish" is the front end's single notification routine. It does, in order:

1. Copy the current text cursor (column, line) into the block.
2. Rebuild the inventory text (section 9.2).
3. Increment the change sequence, with memory barriers before and after.
4. Write one byte to the notify pipe, ignoring any error.

Rules:

* The game is the only writer. There is no lock. The app copies the whole block into a private snapshot before reading (section 11.3); a copy taken mid-update may show a half-updated frame, which is accepted because another notification (and redraw) follows.
* Many front-end functions change the block *without* publishing (single-square map updates, cursor moves, clears to end of line/page, format changes, the context export, the event ring). Their effect becomes visible at the next publish, which is never far away: every printed string, every sleep, every beep, every input-mode change and every full repaint publishes.
* The game publishes only while it runs. While it is blocked waiting for a key the block does not change. Hence "the sequence number differs from when I sent my key" means "the game has processed something since".

---

## 4. The display interface, function by function

### 4.1 Exported variables

| Variable | Initial | Behaviour on Android |
|---|---|---|
| "No nap" flag | 0 | Set to 1 by the `nonap` option, 0 by `nap`. **Nothing reads it**: sleeps always happen (section 4.11). The setting is accepted and has no effect. |
| "No signal" flag | 0 | The core raises it during screens that must not be interrupted. No signal handling exists here; the only use is that the checkpoint routine preserves its value (section 7). |
| "Enable scroll" flag | 0 | Unused. |
| Repeat count | 0 | The front end never sets it: there is no numeric-prefix repeat on Android, so it is always 0. |

### 4.2 Start-up and shut-down

**Initialise the application.** Builds the key table (section 5.3); fills both text areas with spaces (each line terminated after column 80) and normal format; sets every overlay cell to −1; sets the class field to −1; makes the text page the active text area; sets display mode "text" and input mode "busy"; publishes; reports success.

**Close the application.** Publishes once (so the last text is visible). The core calls it immediately before exiting.

### 4.3 The text model

There are two independent text areas: the **message area** (5 lines × 80 columns, shown under the map) and the **text page** (25 × 80, shown instead of the map). Exactly one is *active*: the message area in map mode, the text page in text mode. Each has its own remembered cursor position and current format; these are swapped on a display-mode switch. All text functions operate on the active area. Each cell holds one character and one format code.

Internal cursor: column 1..80, line 1..N (N = 5 or 25). Both start at (1, 1), format normal.

**Advancing a line.** Moving down past the last line scrolls the area: every line moves up one (characters and formats), the last line becomes blank/normal, and the cursor stays on the last line. This applies to both areas (the text page also scrolls if written past line 25).

**Advancing columns.** After the cursor moves past column 80 it goes to column 1 of the next line (scrolling if needed). The wrap is immediate: a character written in column 80 leaves the cursor at the start of the next line. There is no word wrap; long messages break mid-word at column 80 (the app may reflow text for narrow screens, Part 11).

**Print one character.**

| Character | Effect |
|---|---|
| Tab | Advance to the next tab stop (columns 1, 9, 17, …, i.e. move forward 1–8 columns). Skipped cells are *not* erased or reformatted. If this passes column 80 the cursor goes to column 1 of the next line. |
| Line feed | Column 1 of the next line (scrolling if needed). |
| Carriage return | Ignored. |
| Anything else | If the cursor is outside the area, nothing happens (the cursor does not move). Otherwise the character and the current format are stored at the cursor and the cursor advances one column. No filtering: control characters would be stored as-is. |

**Print a string.** A missing or empty string does nothing at all (no publish). Otherwise: the kill-counting check (section 6.2) is applied to the whole string; each character is printed as above; then one publish.

**Formatted print.** Formats into a 2,048-byte buffer (longer output is truncated) and prints the result as one string. This matters for the kill-counting check, which sees the formatted text.

**Standout print.** Sets the format to "standout", prints the string, then sets the format to "normal" (not to the previous format).

**Set format.** Sets the current format for subsequent characters. No publish.

**Move cursor (x, y).** Clamps x to 1..80 and y to 1..N and moves there. No publish.

**Clear text.** Blanks every line of the active area (spaces, normal format), puts the cursor at (1, 1), publishes. The current format is left unchanged.

**Clear to end of line.** From the cursor column to column 80 on the cursor line: spaces, normal format. Cursor unchanged. No publish.

**Clear to end of page (x, y).** If y is below 1 it is treated as 1; if y is past the last line nothing happens. Otherwise clears line y from column x to 80 and every later line completely. Cursor unchanged. No publish.

**Message-window behaviour as the player sees it.** The core starts almost every message with a line feed, so each message begins on a fresh line and the area shows the last five lines; older lines scroll off the top and are gone from the block. There is **no "--more--" pause in the front end**: however many messages arrive in one turn, they just scroll. (The core's own pager for long text pages, with its "--- press space to continue ---" prompt, is an ordinary single-key prompt and belongs to Parts 01/03. Any message history the app keeps is Part 11.)

### 4.4 Display mode

**Set display (map or text).**

1. Save the cursor and current format into the slot of the mode being left.
2. Switch mode; restore the cursor and format of the mode being entered; make that mode's area active (5 or 25 lines); write the display-mode field.
3. Do a "paint window": if the new mode is map, refresh effects, refresh status and repaint the whole map (in that order); in either mode, publish.

Neither area is cleared by a mode switch. The message area keeps its lines across a visit to a text page, and the text page keeps its old content until the core clears it (it always does before showing a new page). Setting the mode that is already current is harmless: it still repaints and publishes.

At start-up the mode is "text"; the core switches to map mode once the character exists and the first level is ready.

### 4.5 The status area

Three entry points — **update status**, **update effects**, **update status and effects** — do nothing at all in text mode. In map mode they refresh the corresponding fields and publish. The core calls them whenever it has changed something they show (dozens of call sites: after regeneration, gold changes, level changes, spell timers and so on; see the owning parts). A full "paint window" also refreshes both.

A **status refresh** writes:

* The numeric fields 18–35 of section 2.2 (hit points, spell points, gold, level, armour class, weapon class, experience, dungeon level index and name, class, sex, the six abilities, level title, turn counter, difficulty).
* Two preformatted lines reproducing the classic bottom lines:
  * Line 1, in order: the text "Spells:"; current spell points right-aligned in 3; "("; maximum spell points right-aligned in 3 followed by ")" when the maximum exceeds 99, otherwise right-aligned in 2 followed by ")" and a space; " AC: "; armour class left-aligned in 3; "  WC: "; weapon class left-aligned in 3; "  Level"; the experience level (right-aligned in 3 when above 99, otherwise a space then the number left-aligned in 2); " Exp: "; experience left-aligned in 9; a space; the 19-character level title. Example: `Spells:  5(12)  AC: 3    WC: 4    Level 1  Exp: 0           novice explorer  `.
  * Line 2, in order: "HP: "; the text "current (maximum)" right-aligned in 11; then " STR=", " INT=", " WIS=", " CON=", " DEX=", " CHA=" each followed by the value left-aligned in 2 (strength includes the temporary bonus); " LV:"; the two-character level name, or " ?" when the level is unknown to the player; "  Gold: "; gold left-aligned in 8. Example: `HP:     12 (12) STR=18 INT=10 WIS=10 CON=12 DEX=14 CHA=8  LV: H  Gold: 0       `.
  * (The app's own HUD uses the numeric fields; the two lines exist for a classic-style display.)

Side effects on game state, inherited from upstream and therefore behaviour:

1. Before deciding whether the level name is hidden, a status refresh **clears the "doesn't know where they are" flag whenever the player is in town (level 0) or is a wizard**. So the "?" disappears at the first status refresh after reaching town.
2. It stores strength + bonus in the game's scratch "total strength" attribute, and copies the first 100 attributes into the core's "last displayed" backup array.

The level title is looked up by experience level minus one in the core's title table (Part 02).

### 4.6 The effects list

An **effects refresh** tests 17 timed-effect counters of the character and sets bit i of the effects field when counter i is non-zero (and copies those counters to the "last displayed" backup). Order, with the label the app shows for each (Part 11 draws them; Part 02/07 define the counters):

| Bit | Counter | App label |
|---|---|---|
| 0 | Stealth | Stealth |
| 1 | Undead protection | Undead Pro |
| 2 | Spirit protection | Spirit Pro |
| 3 | Charm monsters | Charm |
| 4 | Time stop | Time Stop |
| 5 | Hold monsters | Hold Monst |
| 6 | Giant strength | Giant Str |
| 7 | Fire resistance | Fire Resist |
| 8 | Dexterity boost | Dexterity |
| 9 | Strength boost | Strength |
| 10 | Scare monsters | Scare |
| 11 | Haste self | Haste Self |
| 12 | Cancellation | Cancel |
| 13 | Invisibility | Invisible |
| 14 | Altar protection (+3) | Protect 3 |
| 15 | Protection spell (+2) | Protect 2 |
| 16 | Walk through walls | Wall-Walk |

An indicator is shown exactly while its bit is set; there is no countdown in the block.

### 4.7 Choosing the tile for a map square

The decision procedure for square (x, y). "Known object" is what the player's memory of the level records there (which may be out of date); "unknown" is the memory value for unexplored squares. Tile tables indexed by object id and by monster id are in Part 14; the ids that matter to this procedure are quoted.

1. **Player.** If (x, y) is the player's position and the player is not blind: the tile is the player tile for the character's class and sex — 165 + class for a female character, 181 + class for a male one (class 0..7) — and the procedure ends (no later step applies).
2. **Unexplored.** Otherwise, if the known object is "unknown": tile 175. Monsters standing on unexplored squares are not drawn.
3. **Monster present** (any monster occupies the square):
   1. The monster is *visible* only if the player is not blind **and** the square's "monster has been seen" flag or its "monster is awake" flag is set. If not visible, use the known object's tile.
   2. Visible **mimic**: it is drawn as some other monster. One disguise is shared by every mimic and persists between draws; it starts as the mimic's own tile. Each time a visible mimic's tile is computed while the turn counter is a multiple of 10, a new disguise is rolled: pick rnd(M) repeatedly, where M is the id of the first demon lord (61 in the Wild Edition), until the pick is neither the invisible stalker nor one of the four Wild monsters. The pick can therefore be any ordinary monster, the mimic itself, or the first demon lord. (Wild Edition hook: the exclusion of Wild monsters; see Part 12.) Because the roll happens at draw time it may happen several times in one turn, and it consumes numbers from the game's own random stream.
   3. Visible **invisible stalker** while the player cannot see invisible things: use the known object's tile.
   4. Visible **demon lord, demon prince or Lucifer** while the player does not hold the Eye of Larn: use the known object's tile.
   5. Any other visible monster: the monster's tile.
4. **No monster:** the known object's tile.
5. **Glitch.** If the tile chosen in steps 3–4 is the known object's tile, a Wild Edition hook may replace it with another item's tile while reality is glitched (Part 12). With that feature inactive the tile is unchanged.
6. **Walls.** If the resulting tile is the generic wall tile (84), replace it with 352 + the wall's connection number (0..15). The connection number is kept per wall square by the dungeon code and is the sum of: 1 if the square to the west is a wall or a door (open or closed), 2 for the south, 4 for the east, 8 for the north; squares off the map never count. It is computed from the *true* map, not from what the player knows, so a known wall's shape reveals whether its four neighbours are walls or doors even when they are unexplored. The dungeon code recomputes it when a level is created and around any square whose wall status changes (Parts 05/07).

Consequences worth stating: hidden traps have the plain-floor tile (191) and so look like floor; a blind player sees neither themselves nor any monster, only remembered objects; the square under a non-blind player always shows the player, never the object beneath.

**Setting a square** (the internal step every map update goes through): compute the tile as above, store it, set the square's overlay to −1, and record the square's minimap class (section 9.1), with the "monster displayed" bit set exactly when a monster occupies the square, the square is not the player's, and the chosen tile is that monster's tile (or, for a mimic, the current disguise's tile).

**Full map repaint:** set every one of the 67 × 17 squares; write the player position; set the player-cursor flag to "not blind"; then let the Wild hub copy its summary values into the block (Wild Edition hook: fields 55–65; see Part 12).

### 4.8 Map functions the core calls

**Show one cell (x, y).** If the player is blind: nothing. Otherwise: the player's memory of that square becomes the true object there; if a monster is there, mark it seen; set the square. Does **not** publish.

**Show the area around the player — "show cell" (x, y).** Called by the core every turn with the player's position.

1. The area is the 3 × 3 block around (x, y), or 7 × 7 when the character has expanded awareness; when blind it is the single square (x, y). Clip to the map.
2. For each square in the area: if it is the player's square, memory := true object (this happens even when blind). Otherwise, if memory differs from the true object, or the square is the player's previous position, or an unseen monster stands there: memory := true object and, if a monster is there, mark it seen.
3. Set the square at the player's previous position (if on the map), then set every square of the area (all of them, changed or not), then do "show player".

**Show player.** Set the square at the previously *published* player position (so it loses the player tile), write the new position and the player-cursor flag (1 unless blind), set the player's square, publish. The map is never scrolled by the front end; centring is the renderer's job (Part 11).

**Redraw screen.** A "paint window": in map mode refresh effects, status and the whole map; then publish (in text mode it only publishes).

**Redraw an area (four bounds).** The bounds are ignored; identical to "redraw screen".

### 4.9 Animations

**Directional effect (x, y, effect type, direction)** — the travelling bolt of the five missile-style spells. Nothing happens when the player is blind or (x, y) is off the map. Otherwise the square's tile becomes the effect tile, its overlay is cleared, and the change is published. The front end does not restore the square; the core does that itself by "show one cell" after sleeping.

Effect tiles: each effect type has eight consecutive tiles, one per direction, clockwise from west. With base B, the tile is B + 0 west, +1 north-west, +2 north, +3 north-east, +4 east, +5 south-east, +6 south, +7 south-west. Direction 0 ("no direction") maps to tile 191 (plain floor) for every type.

| Effect type (interface order) | Base tile B | Tiles | Used for (Part 07) | Sleep per square |
|---|---|---|---|---|
| 0 magic missile | 192 | 192–199 | Magic missile | 100 ms |
| 1 sonic spear | 200 | 200–207 | Sonic spear | 70 ms |
| 2 fireball | 208 | 208–215 | Fireball | 40 ms |
| 3 lightning | 216 | 216–223 | Lightning; also the ray from one scroll | 10 ms |
| 4 cold | 224 | 224–231 | Cone of cold | 60 ms |

The core's direction numbers are 1 south, 2 east, 3 north, 4 west, 5 north-east, 6 north-west, 7 south-east, 8 south-west. The per-square sequence the core drives is: effect tile (published) → sleep for the listed time → square restored → next square. The whole flight is skipped when the player is blind.

**Magic effect frame count (effect).** Returns 8 for the sparkle and 6 for every other effect.

**Magic effect frame (x, y, effect, frame).** Nothing happens if the frame number is beyond the effect's frame count, is negative or is 8 or more; if the player is blind; or if (x, y) is off the map. Otherwise:

* For an **overlay** effect (only the sparkle): the square's tile is recomputed by the normal decision procedure and the frame tile is put in the square's *overlay*.
* For every other effect: the frame tile *replaces* the square's tile and the overlay is cleared.
* Then publish.

Frame tiles:

| # | Effect (interface order) | Overlay? | Frames | Tile ids, frame 0 first |
|---|---|---|---|---|
| 0 | Sparkle | yes | 8 | 240, 241, 242, 243, 244, 245, 246, 247 |
| 1 | Sleep | no | 6 | 256, 272, 288, 304, 320, 336 |
| 2 | Web | no | 6 | 257, 273, 289, 305, 321, 337 |
| 3 | Phantasmal forces | no | 6 | 258, 274, 290, 306, 322, 338 |
| 4 | Cloud kill | no | 6 | 259, 275, 291, 307, 323, 339 |
| 5 | Vaporize rock | no | 6 | 260, 276, 292, 308, 324, 340 |
| 6 | Dehydrate | no | 6 | 261, 277, 293, 309, 325, 341 |
| 7 | Drain life | no | 6 | 262, 278, 294, 310, 326, 342 |
| 8 | Flood | no | 6 | 263, 279, 295, 311, 327, 343 |
| 9 | Finger of death | no | 6 | 264, 280, 296, 312, 328, 344 |
| 10 | Teleport away | no | 6 | 265, 281, 297, 313, 329, 345 |
| 11 | Magic fire | no | 6 | 266, 282, 298, 314, 330, 346 |
| 12 | Make wall | no | 6 | 267, 283, 299, 315, 331, 347 |
| 13 | Summon demon | no | 6 | 268, 284, 300, 316, 332, 348 |
| 14 | Annihilate | no | 6 | 269, 285, 301, 317, 333, 349 |

(For effects 1–14, frame f of effect k is tile 256 + 16·f + (k − 1).)

Timing, driven by the core: every frame is shown and then the game sleeps 75 ms. A single-square effect is 8 × 75 = 600 ms (sparkle) or 6 × 75 = 450 ms. Area effects draw the same frame on every affected square, then sleep 75 ms once per frame. After a tile-replacing effect the core restores the squares with "show one cell". After a sparkle nothing restores the square explicitly: the last sparkle frame (247) stays in the overlay until that square is next set — normally the per-turn "show cell" just before the next command prompt, so the player does not see it linger. Which spell uses which effect, on which squares, is Part 07.

### 4.10 Beep

If beeps are not disabled by the `nobeep` option: increment the beep counter and publish. The app turns a change of the counter into feedback (Part 11); the game itself makes no sound.

### 4.11 Sleep ("nap")

Publish, then, if the delay is positive, sleep that many milliseconds. Always: the "no nap" option is not consulted. During the sleep the game does nothing and keys accumulate in the pipe. The core uses this for the animation timings above, for a 100 ms pause at start-up, and for multi-second pauses after certain dramatic messages (owned by the parts describing those events).

### 4.12 User name

The core asks the front end for the login name and a numeric user id. The Android version returns the value of `USER` (or "player" if unset or empty) — always "player" in the app — and the process's real user id. Consequences:

* The save and checkpoint files are always named after "player", so renaming a character never orphans its save; slots are separated by HOME directory, not by name.
* The *displayed* name (scoreboard, final score line, "Welcome home" text) comes from the `name:` option. If there is no such option the display name falls back to the login name, "player".
* The numeric id is the app's own Unix user id, the same for every slot and character. The scoreboard keys entries on this id (Part 03), so all characters on a device compete for the same single scoreboard entries.

---

## 5. Input

### 5.1 Reading a key

All input functions read through one routine: block until one byte arrives on the key pipe (retrying if interrupted). Two translations are applied to what touch and soft keyboards send: line feed (10) becomes carriage return (13), and delete (127) becomes backspace (8). The app is responsible for everything else: it sends plain single-byte codes, mapping arrows, on-screen buttons and gestures to the letters below (Part 11).

**End of input.** If the read reports end-of-file or an error, the game exits at once with status 1. Nothing is saved or deleted at that moment.

### 5.2 Input modes

| Mode | Entered by | Published answers | Caret | Accepts |
|---|---|---|---|---|
| 0 busy | Leaving any input function | empty | 0 | (not waiting) |
| 1 command | Normal command input | empty | 0 | Any key bound in the command table (5.3). |
| 2 prompt | Single-key prompt | The prompt's answer set, exactly as the core passes it | as requested by the caller | Only bytes in the answer set (case-sensitive). |
| 3 direction | Direction input | empty | as requested (the core always asks for 1) | The eight lower-case movement keys. |
| 4 number | Number input | minus, asterisk, the ten digits, backspace, carriage return | 1 | Those, plus escape. |
| 5 password | Password input | empty | 1 | Printable characters, backspace, carriage return, escape. |

Entering a mode writes the mode, caret flag and answers and publishes. Leaving sets "busy", caret 0, empty answers; all input functions except command input then publish immediately (after command input the "busy" state becomes visible with the next publish, which follows as soon as the command does anything).

### 5.3 Command input

Called by the main loop each time it wants a command. Steps:

1. Count the call; on every 25th call write a checkpoint (section 7).
2. Record the run identity and kill count (section 6.1).
3. Export the context action and spell list (section 8).
4. Enter command mode (publishes).
5. Read keys until one produces an action:
   * The checkpoint key (byte 0x1D, Ctrl+]) writes a checkpoint and continues waiting.
   * Wild Edition hook: in wizard mode, "=" opens a debug menu which may itself return an action (Part 12).
   * A byte of 128 or more is ignored.
   * Otherwise look the byte up in the table below; unbound bytes are ignored without any feedback or publish.
   * When the enhanced interface option is off, "open door" and "open chest" are discarded (so `o` and `O` do nothing in the classic interface).
6. Leave command mode and return the action.

Command table (everything not listed is unbound, including digits, escape, return and the comma):

| Key | Action | Key | Action |
|---|---|---|---|
| `h` / `H` | Move / run west | `w` | Wield |
| `l` / `L` | Move / run east | `W` | Wear |
| `j` / `J` | Move / run south | `r` | Read |
| `k` / `K` | Move / run north | `q` | Quaff |
| `u` / `U` | Move / run north-east | `d` | Drop |
| `y` / `Y` | Move / run north-west | `c` | Cast a spell |
| `n` / `N` | Move / run south-east | `o` | Open door (enhanced interface only) |
| `b` / `B` | Move / run south-west | `C` | Close door |
| `.` | Wait a turn | `O` | Open chest (enhanced interface only) |
| space | "None": no turn passes | `i` | Inventory |
| `e` | Eat | `\` and `D` | List known spells |
| `?` | Help | `S` | Save and exit |
| `Z` | Teleport (wizard) | `^` | Identify traps |
| `_` | Become creator (wizard password) | `+` | Create item (wizard) |
| `-` | Toggle wizard mode | `` ` `` | Debug mode (wizard) |
| `T` | Take off armour | `g` | Pack weight |
| `v` | Version | `Q` | Quit |
| Ctrl+R (byte 0x12) | Redraw screen | `P` | Show tax status |
| `~` | Diagnostics dump (wizard) | | |

`D` is an Android addition (a convenience alias for the spell list). What each action does is Part 01.

### 5.4 Single-key prompt (prompt text, answer set, caret flag)

1. Run the game-end detector on the prompt and answers (section 6.3).
2. Print the prompt text into the active text area (an empty prompt prints nothing).
3. Enter prompt mode with the answer set and caret flag (publishes).
4. Read keys until one is in the answer set; all others are silently ignored.
5. Leave the mode, publish, return the key.

Because of the key translations, an answer set containing carriage return is satisfied by either return or line feed, and one containing backspace by delete. There is no default answer and no timeout. The answer set is how the app knows which buttons to offer (Part 11); typical sets are quoted with their prompts in the parts that own them (for example yes/no questions use "y", "Y", "n", "N", escape and space).

### 5.5 Direction input (prompt text, caret flag)

Prints the prompt, enters direction mode, and reads keys until one is a lower-case movement key (`h j k l y u b n`); returns the corresponding "move" action. Upper-case run keys are not accepted. **Escape is not accepted either: a direction prompt cannot be cancelled.** The core's one direction prompt is "In What Direction? " (preceded by a line feed), after which it prints the name of the chosen direction.

### 5.6 Number input (default value)

Enters number mode with a caret, then processes keys, publishing after every key:

| Key | Effect |
|---|---|
| Escape | Return the default immediately. |
| `*` | Return the default immediately (at any point, even after digits). |
| `-` as the very first character | The number will be negative; echo "-". Elsewhere it is ignored. |
| Digit | Value := value × 10 + digit; echo the digit. No length or overflow limit. |
| Backspace, with at least one character entered | If the only character is the minus sign, drop the sign; otherwise value := value ÷ 10 (integer). Erase the last echoed character on screen (step back one column, write a space, step back again). |
| Return | Finish: the default if nothing is entered; otherwise the value, negated if the sign is set. (A lone "-" therefore yields 0, not the default.) |
| Anything else | Ignored. |

The on-screen erase does not handle a line wrap: if the echoed text had wrapped to a new line, backspacing leaves the cursor outside the line and further echo is suppressed until the core next positions the cursor or starts a new line.

### 5.7 Password input (buffer, maximum length)

Enters password mode with a caret. Each printable character is stored (if fewer than the maximum have been entered; extra ones are ignored) and echoed as "*". Backspace removes the last character and its star. Input ends on return **or escape** — escape does not cancel, it submits what has been typed. The core uses this only for the 8-character wizard password prompt "Enter Password: ".

---

## 6. Run identity, kill counting and end-of-game detection

These exist only for the app's statistics (Part 10) and danger cues; they piggy-back on front-end calls.

### 6.1 Run identity

Two otherwise unused slots at the end of the base game's character-attribute array (the two immediately before the first Wild attribute) hold a run id and a kill count. They are part of the attribute array and therefore saved in save files and checkpoints (Part 03), so a restored game keeps both.

At every command prompt: if the run-id slot is zero or negative it is set to a fresh random value — 30 random bits from the system's secure random source (not the game's generator, so seeded runs are unaffected) with the lowest bit forced to 1, hence a positive odd number below 2^30. The run id and the kill count are then copied into the block. Until the first command prompt the published run id is 0 ("no character yet").

### 6.2 Kill counting

Every string passed to "print a string" is tested: if it is longer than 11 characters, begins with a line feed followed by "The " and ends with " died!", the kill slot is incremented and published. This matches exactly the core's message for a monster killed by the player's attacks or damaging spells ("The <name> died!", printed on a new line). Kills that produce no such message — notably monsters removed by a scroll of annihilation — do not increase this counter (the Wild per-species counts do include them; Part 12).

### 6.3 End-of-game detection

The detector runs at the start of every single-key prompt and records the outcome when it recognises the final score page:

1. Do nothing if an end state is already recorded, if no run id has been published (0), or if the display is not in text mode.
2. The prompt must contain "Press return" and its answer set must be exactly carriage return.
3. Search the text page from the top for the line containing the banner "U L A R N    S C O R E S"; from there search for the first line that reads as "Score: <number>" at its start. If none is found, do nothing.
4. Take every following line up to (not including) the first line that begins with five hyphens, concatenating them as 80-character pieces. This is the "<name> <cause>" sentence, which may have wrapped.
5. Collapse runs of spaces and tabs to single spaces, drop leading whitespace, and keep at most 80 characters.
6. If the result begins with the character's display name, remove it, then remove following spaces. (If the name itself contains doubled spaces the collapsed text no longer matches and the name stays in the cause.)
7. Publish: the cause text, the score, the current kill count, and the end state: **won** if the cause begins with "a winner", **quit** if it begins with "quit", otherwise **died**.

Typical causes: "killed by a giant ant on 3" (the level name's leading space is swallowed by the collapse), "a winner". The core's two normal ways of leaving without dying — quitting and saving — never show this page, so in practice the "quit" state is not produced by play; the app recognises those from the files left behind (section 1.8).

---

## 7. Checkpointing

Upstream 1.6.3a knows how to restore a checkpoint (`-r`) and deletes the checkpoint when a game ends, but never writes one. The Android front end adds the writing, because the system may kill a backgrounded app at any time.

* **When.** Only inside command input, i.e. between commands, where the game state is always consistent: (a) automatically on every 25th command prompt (the counter starts at 0 for each process and counts every prompt, including ones answered by keys that take no turn); (b) immediately whenever the checkpoint key (byte 0x1D) is read at a command prompt. The app sends that key when its activity is stopped and before a "leave". At any other prompt the key is not in the accepted set and is silently discarded.
* **How.** The ordinary save routine (Part 03) is run with the checkpoint file as its target, so a checkpoint is byte-for-byte a save file. The save routine's "no signal" side effect is undone afterwards. The save routine first stores the current level into the level store and brings bank interest up to date, exactly as a real save does. On failure it prints its usual "Can't open file <path> to save game" message into the message area and play continues.
* **File.** `ularn_player.ckp` in HOME.
* **Resume.** With `-r`: if the file is missing the game prints "Cannot find checkpoint file <path>" and exits; otherwise it prints "Restoring..." and restores it. Unlike a save file, a checkpoint is *not* deleted when restored (the core only verifies it can be opened for appending, treating failure as cheating), so it remains valid until overwritten by the next checkpoint.
* **Deletion.** The core deletes the checkpoint whenever a game ends through its death routine: death, win, quit and save-and-exit. Hence after "save & quit" there is a save file and no checkpoint; after "leave" or an app kill there is a checkpoint and no save file.
* Checkpointing cannot be turned off (the core's flag for it is always on).

---

## 8. The context action

Ularn has no "pick up", "climb" or "enter" commands: at the start of every turn the core looks at what is under the player and asks about it. The app's context button re-triggers that question by sending the "wait" key and then answers it. To let the button show the right label and know the right answer, the front end exports what is underfoot at every command prompt (and only then: the fields are stale at any other time).

### 8.1 Export procedure

1. Read the true object on the player's square.
2. Classify it into a context kind (table below). If the kind is "none", the exported object id is forced to "nothing" so that hidden traps cannot be detected through this channel.
3. Name: for any other kind, the object's standard name (Part 06 table). An identified potion or scroll is named "a potion of <name>" / "a scroll of <name>" instead (the stored names begin with a space, so the result reads "a potion of healing"). Truncated to 39 characters.
4. Flags: bit 1 if the enhanced interface option is on. If time is stopped: bits 2 and 4 (the core does not look at objects while time is stopped). Otherwise bit 2 if the object is one the core will not prompt about while a monster is adjacent (list below) and any monster — including a pet — occupies one of the nine squares around and under the player.
5. Doors: for each of the eight neighbours in the order west, south, north, east, north-west, north-east, south-west, south-east (the directions of the keys `h j k l y u b n`), set bit i if the square is on the map, the player remembers a closed door there, and there really is one.
6. Spells: the spell count (39), a known/unknown byte per spell, and each spell's three-letter code. Also refresh the spell-point fields.

Objects blocked by an adjacent monster: altar, both live thrones, orb of enlightenment, wand of wonder, Hand of Fear, live fountain, statue, mirror, dead fountain, college, DND store, both banks, trading post, home, Dealer McDope's pad, revenue service.

### 8.2 Kinds, and what each implies

The app decides what the button does in this priority order: (1) a kind from "take" to "ATM" underfoot; (2) otherwise adjacent known closed doors; (3) otherwise an "inspect" kind (a disabled label); (4) otherwise nothing (neutral, disabled button). The button is live only in map mode while the game waits for a command. "`.` then X" means: send the wait key; when the game then shows a single-key prompt whose answer set matches the expected one, send X; if the game returns to a command prompt, shows a different prompt, or 3 seconds pass, send nothing more.

| Kind | Id | Objects underfoot | Button label | Key sequence | Expected answer set |
|---|---|---|---|---|---|
| None | 0 | Nothing; unknown; wall; visible pit, teleporter, arrow trap, dart trap, trap door, sphere of annihilation; both elevators; all four hidden traps; a closed door | — | — | — |
| Inspect | 1 | Statue, mirror, dead fountain, open door | Last word of the name, capitalised ("Statue", "Mirror", "Fountain", "Door"); always disabled | none | — |
| Take | 2 | Any other item: potions, scrolls, books, cookies, drugs, weapons, armour, rings, gems, chests' siblings, Wild items, and a vending machine or coin ATM when its Wild feature is off | "Pick up" | `.` then `t` | Any set containing both `t` and `i` |
| Gold | 3 | The four gold-pile objects | "Gold" | `.` (walking on gold already collects it; this is a harmless wait) | — |
| Stairs up | 4 | Staircase up | "Stairs ▲" | `.` then `u` | `s u d i` + escape |
| Stairs down | 5 | Staircase down | "Stairs ▼" | `.` then `d` | `s u d i` + escape |
| Shaft down | 6 | Volcanic shaft (surface) | "Climb ▼" | `.` then `c` | `c i` + escape |
| Shaft up | 7 | Base of the volcanic shaft | "Climb ▲" | `.` then `c` | `c i` + escape |
| Enter | 8 | College, DND store, both banks, dungeon entrance, home, trading post, revenue service | "Enter" | `.` then `g` | `g i` + escape |
| Pad | 9 | Dealer McDope's pad | "Visit" | `.` then `c` | `c i` + escape |
| Chest | 10 | Chest | "Open chest" | Enhanced interface: `O` alone. Classic: `.` then `o` | `o t i` + escape |
| Fountain | 11 | Live fountain | "Fountain" | `.` (the game's own drink/wash menu appears) | — |
| Altar | 12 | Altar | "Altar" | `.` (pray/desecrate menu) | — |
| Throne | 13 | All three thrones | "Throne" | `.` (pry/sit menu) | — |
| Lamp | 14 | Brass lamp | "Lamp" | `.` (rub/take menu) | — |
| Vending | 15 | Vending machine, Wild feature on | "Vending" | `.` (buy/kick menu; Part 12) | — |
| Coin ATM | 16 | LarnCoin ATM, Wild feature on | "ATM" | `.` (buy/sell menu; Part 12) | — |

For every `.`-based action the button is disabled while the "blocked" flag is set. Note that each use of `.` costs a game turn.

Doors (only when nothing actionable is underfoot):

| Situation | Label | Key sequence |
|---|---|---|
| Time stopped | "Open door", disabled | — |
| Enhanced interface, exactly one door | "Open " + an arrow for its direction (← ↓ ↑ → ↖ ↗ ↙ ↘ in bit order) | `o`, then, when the game is in direction mode, that direction's key |
| Enhanced interface, several doors | "Open door" | `o` only; the player then picks the direction |
| Classic interface, exactly one door | "Open " + arrow | The direction key (walk into the door), then, on a prompt whose answer set is exactly `o i` + escape, `o` |
| Classic interface, several doors | "Doors", disabled | — |

While the game is briefly busy the button keeps its previous appearance to avoid flicker. Presentation and placement of the button are Part 11.

---

## 9. Minimap and side-panel export

### 9.1 Minimap class per square

Recorded every time a square is set (section 4.7), from the player's *memory* of the square — never from the true map — so the minimap can only show what the main map shows. Directional and magic-effect animations write tiles directly and do not touch the classes.

| Class | Id | Remembered object |
|---|---|---|
| Unknown | 0 | Unexplored |
| Floor | 1 | Nothing; the four hidden traps; anything unrecognised |
| Wall | 2 | Wall |
| Door | 3 | Open or closed door |
| Item | 4 | Every other object: armour, weapons, rings, potions, scrolls, books, chests, gems, special items, cookies, drugs, Wild items including a vending machine |
| Gold | 5 | The four gold-pile objects |
| Up | 6 | Staircase up, elevator up |
| Down | 7 | Staircase down, elevator down |
| Volcano | 8 | Volcanic shaft down, shaft base (up) |
| Entrance | 9 | Dungeon entrance |
| Building | 10 | College, DND store, both banks, home, trading post, revenue service, Dealer McDope's pad, and the Wild coin ATM |
| Fountain | 11 | Live or dead fountain |
| Altar | 12 | Altar |
| Throne | 13 | All three thrones |
| Trap | 14 | Visible pit, teleporter, arrow trap, dart trap, trap door, sphere of annihilation |
| Feature | 15 | Statue, mirror |

The value 0x80 is added when a monster is displayed on the square, except on unexplored squares. The player's own square never carries the monster bit and holds the class of the object underfoot. The level overview dialog and minimap (Part 11) are computed by the app from this array alone.

### 9.2 Inventory text

Rebuilt on every publish for slots 0..25 (letters a..z); a slot's text in the block is rewritten only if it changed. An empty slot is the empty string. Otherwise the line is built like the game's own inventory listing:

1. The object's standard name. For an identified potion or scroll with a non-empty name: the standard name, then " of", then the potion/scroll name (which starts with a space) — e.g. "a magic potion of healing".
2. Enchantment: omitted for potions, scrolls, the Eye of Larn, books, the scarab, all four gems, the cube of undead control, chests, cookies, the orb of enlightenment, the Hand of Fear, the brass lamp, the urn, the wand of wonder, the talisman of the sphere, the theft-prevention device, and all Wild objects. For everything else: " +N" when the item's value N is positive (or always, in wizard mode), " N" (which shows the minus sign) when negative, nothing when zero.
3. " (weapon in hand)" if it is the wielded item.
4. " (being worn)" if it is the worn armour or the shield.
5. " (on your head)" if it is the hat being worn (Wild Edition hook; Part 12).

Each line is limited to 79 characters. Example: "a dagger +1 (weapon in hand)".

---

## 10. The event ring

A lossy, one-way channel for things the app reacts to with sound, toasts, cards and effects. An event is four integers {type, a, b, c}. To append: write the four values into ring entry (event sequence mod 64), then increment the event sequence. Appending does not publish or notify by itself; the app looks at the ring whenever it is notified of any change (section 11.5). Events more than 64 behind the reader are lost; so are all events emitted before the reader's first look at a new game. This is acceptable by design.

Base events (emitted from one-line hooks in the core, in every game regardless of Wild features):

| Type | Id | a, b, c | Emitted when |
|---|---|---|---|
| None | 0 | — | Never emitted. |
| Kill | 1 | monster id, x, y | The player kills a monster in combat (right after the "died!" message, before experience is awarded), and for each ordinary monster destroyed by a scroll of annihilation. Also increments that species' kill count. |
| Level up | 2 | new character level, 0, 0 | Once for each experience level gained. |
| New level | 3 | dungeon level index, 0, 0 | At the end of every arrival on a level, including the very first (town) of a new game. |
| Take | 4 | object id, its value, 0 | An item is placed into an inventory slot. |
| Player hit | 5 | damage, 0, 0 | At the start of every loss of hit points through the core's damage routine, before the points are deducted (traps and self-inflicted damage included, not only monster blows). |
| Gold | 6 | amount, 0, 0 | Gold is picked up from the floor. |
| Spell | 7 | spell index, 0, 0 | A known spell is cast, just before it takes effect. |
| Died | 8 | 0, 0, 0 | On entry to the core's death routine with any reason other than winning. Because quitting and saving also go through that routine, **this event also fires on quit and on save-and-exit**, and it fires even when an amulet of life preservation then revives the player. |
| Won | 9 | 0, 0, 0 | On entry to the death routine with the "winner" reason. |

Wild Edition events use ids 100 + 10 × feature index. Their meaning and consumers are Parts 12 and 13; the ids and argument conventions are:

| Type | Id | Arguments |
|---|---|---|
| Radio | 100 | a = track unlocked |
| Card | 110 | a = monster id, b = 1 for a holographic card |
| Vending | 120 | a = 0 buy, 1 kick, 2 topple, 3 "a roomba ate it" (b = object) |
| Hat | 130 | a = hat object id, or 0 when taken off |
| Coin | 140 | a = 0 price tick (b = price), 1 news (b = price, c = news item), 2 rug pull, 3 auditor (b = sub-case, c = amount) |
| Glitch | 150 | a = 1 start (b = turns), 0 end |
| Fishing | 160 | a = 0 cast, 1 catch (b = object, c = its value), 2 kraken (b, c = x, y), 3 treasure map (b, c = x, y) |
| Pet | 170 | a = 0 hatch, 1 attack, 2 died, 3 follow, 4 grew; b = species (or the target for an attack); c = cause of death or new stage |
| Perk | 180 | a = perk index chosen; perk triggers reuse the id with other a/b values |
| Weather | 190 | a = new weather state, b = hour, c = 1 for a lightning strike |

---

## 11. The JNI layer and the Kotlin bridge

### 11.1 Native functions

| Function | Arguments | Returns |
|---|---|---|
| Spawn | working directory, argument strings, environment strings | Four integers: status (0, or the negated error number), process id, key-pipe descriptor, notify-pipe descriptor (the last three are −1 on failure). Strings are copied before the fork and freed in the parent afterwards. |
| Shared state | — | A direct byte buffer over the whole shared block (creating the block on first use), or null if it could not be mapped. |
| Layout | — | The integer array of section 11.2. |
| Wait for | process id | Blocks; the exit status as in section 1.8. |
| Send signal | process id, signal number | Sends the signal if the id is positive. The bridge defines the kill signal as 9. |

### 11.2 The layout array

So that nothing has to be mirrored by hand, the native side reports where each field lives. The array has 78 entries; the Kotlin reader uses the same indices.

| Index | Content | Index | Content |
|---|---|---|---|
| 0 | Total block size | 39 | Map width (67) |
| 1 | magic | 40 | Map height (17) |
| 2 | seq | 41 | Line length (80) |
| 3 | display_mode | 42 | Message lines (5) |
| 4 | input_mode | 43 | Text lines (25) |
| 5 | answers | 44 | mm_cat |
| 6 | caret | 45 | inv_text |
| 7 | cursor_x | 46 | Inventory slots (26) |
| 8 | cursor_y | 47 | Inventory text length (80) |
| 9 | beep_count | 48 | ctx_object |
| 10 | player_x | 49 | ctx_kind |
| 11 | player_y | 50 | ctx_flags |
| 12 | player_cursor | 51 | ctx_doors |
| 13 | tiles | 52 | ctx_name |
| 14 | overlay | 53 | spell_count |
| 15 | status1 | 54 | spell_known |
| 16 | status2 | 55 | spell_codes |
| 17 | hp | 56 | Context-name length (40) |
| 18 | hpmax | 57 | Spell slots (40) |
| 19 | spells | 58 | run_id |
| 20 | spellmax | 59 | kills |
| 21 | gold | 60 | end_state |
| 22 | plevel | 61 | end_score |
| 23 | ac | 62 | end_reason |
| 24 | wc | 63 | wild_flags |
| 25 | exp | 64 | hat_tile |
| 26 | dlevel | 65 | glitch |
| 27 | level_name | 66 | weather |
| 28 | effects | 67 | coin_price |
| 29 | class_num | 68 | pet_x |
| 30 | sex | 69 | pet_y |
| 31 | stats | 70 | kill_counts |
| 32 | level_title | 71 | Kill-count slots (96) |
| 33 | game_time | 72 | ev_seq |
| 34 | hardness | 73 | ev |
| 35 | msg_chr | 74 | Event ring size (64) |
| 36 | msg_fmt | 75 | pet_info |
| 37 | text_chr | 76 | pet_level |
| 38 | text_fmt | 77 | bounty_info |

All entries are byte offsets except the sizes and counts noted. The version field's offset is not exported.

### 11.3 The state reader (`GameState`)

* Holds the live buffer (native byte order) and a private byte array of the block's size. **Refresh** copies the entire block into the private array and returns whether the magic number matches. Every accessor reads the private copy, so one frame is drawn from one consistent-enough snapshot; callers refresh once per frame or per decision.
* Three accessors read the live buffer instead: the live change sequence (a cheap "anything changed?"), the live event sequence and a live ring entry (indexed modulo the ring size).
* Integers: read at the field's offset. Arrays of integers: offset + 4 × index. Tiles and overlays: 16-bit at offset + 2 × (y × width + x). Minimap class: one unsigned byte at offset + y × width + x.
* Text cells: message character at offset + row × 81 + column and its format at offset + row × 80 + column (rows and columns 0-based); likewise for the text page. Characters are unsigned bytes.
* Strings: bytes up to the first NUL or the field size, decoded as ISO-8859-1. The level name, end reason and pet level are additionally trimmed of surrounding spaces; the level title is not.
* Derived values: caret and player-cursor as booleans; perk bits = high 16 bits of the Wild flags; weather hour = low byte, weather state = next byte; spell count clamped to 0..40; a spell's code/known flag only for indices below the count; kill count 0 for out-of-range species; the pet and the bounty are "absent" (null) when their first value is 0 or less.
* It also carries the constants a renderer needs: display modes, input modes, format codes, end states, the cursor tile id (174), minimap classes with the class mask 0x1F and monster bit 0x80, the ability names "STR INT WIS CON DEX CHA", and the 17 effect labels of section 4.6.

### 11.4 The session (`GameSession`)

State: the current process id (−1 when not running), the key-pipe output stream, a single-thread executor for writes, and two callbacks supplied by the app shell: "changed" and "exited(code)".

* **Start** — as in section 1.9. The descriptors returned by the spawner are adopted so that closing the Kotlin objects closes them.
* **Notify thread** (one per game): reads the notify pipe in chunks of up to 256 bytes and calls "changed" once per successful read, so bursts of notifications coalesce. It ends when the read returns end-of-file or fails (the game has gone), then closes its descriptor. "Changed" is therefore invoked on this background thread; the shell's handler only schedules work (invalidate the game view for the next animation frame, nudge the side panel, the context action, the accessibility announcer, and poll events).
* **Waiter thread** (one per game): blocks in "wait for"; then, on the main thread, if the session still refers to that process: mark not running, close the key pipe, call "changed", call "exited(code)".
* **Send keys**: a byte array is queued to the writer executor, which writes and flushes it; write errors are logged and ignored. Empty arrays and sends without a running game are dropped. All keys, the checkpoint request and the final close go through this one executor, which preserves their order.
* **Request checkpoint**: sends the single byte 0x1D.
* **Leave**: request a checkpoint, then close the key pipe (on the writer executor).
* **Kill**: send signal 9 to the child.
* Helpers report the save file and checkpoint file paths for the current HOME and whether each exists.

On exit the app shell (Part 10) feeds the final state and "is there a save or checkpoint?" to the statistics tracker, records slot progress, hides the keyboard, and returns to the start screen (or finishes the activity if the exit was requested by "save & quit" or "leave").

### 11.5 Event dispatch (`GameEvents`)

Keeps the last event sequence it consumed (initially "none"). **Poll**, which must run on the main thread (a call from any other thread re-posts itself there):

1. Read the live event sequence.
2. If nothing has been consumed yet, or the live sequence is *lower* than the last consumed one (a new game zeroed the block): adopt the live sequence and deliver nothing.
3. If equal: nothing to do.
4. Otherwise start from the last consumed sequence, or from (live − 64) if further behind than the ring holds, and for each event up to the live sequence read its four values from the live buffer and hand an event object to every registered listener (iterating over a copy of the listener list).
5. Remember the live sequence.

Listeners registered by the shell include the radio/sound effects, card drops, photo mode and a weather-lightning flash (Part 13). Because the comparison in step 2 is the only new-game detection, a new game whose event count has already passed the previous game's final count before the first poll delivers the surplus events rather than skipping them; this only affects cosmetic feedback.

---

## 12. Android-specific changes to game behaviour made in this layer

Collected for convenience; each is described in place above.

| Change | Where |
|---|---|
| Game data files, scoreboard and log live in one working directory; options, save and checkpoint live in a per-slot HOME. | 1.4 |
| Login name fixed to "player"; display name from the options file; user id is the app's. | 4.12 |
| Checkpoints are actually written (every 25 command prompts and on request). | 7 |
| End of input exits the game immediately with status 1, without saving. | 5.1 |
| "Exit" never runs exit handlers; it flushes stdio and terminates. | 1.2 |
| The random seed can be forced with an environment variable (test harness only). | 1.5 |
| No numeric repeat counts; `D` added as an alias for "list spells"; line feed and delete are translated; bytes ≥ 128 ignored. | 5 |
| `o` and `O` are dead keys unless the enhanced interface is on (inherited from the X11 front end). | 5.3 |
| The "no nap" option has no effect; delays always run. | 4.1, 4.11 |
| No "--more--" paging of messages; the 5-line message area just scrolls. | 4.3 |
| Area redraws repaint the whole map; "show cell" re-sets every square in view each turn. | 4.8 |
| A status refresh clears the "lost" flag in town or for wizards, and so removes the "?" level name. | 4.5 |
| Two spare character attributes are used for a run id and a kill counter, and are saved with the game. | 6.1 |
| Kill counter driven by recognising the "The … died!" message; end state scraped from the final score page. | 6.2, 6.3 |
| Signedness of `char` forced to signed. | 1.2 |
| Wild Edition hooks in this layer: feature mask from the environment; mimic never disguised as a Wild monster; glitched item tiles; Wild summary fields copied at each full map repaint; wizard "=" debug menu; hat suffix in the inventory text; no enchantment shown for Wild objects; coin ATM classed as a building; vending-machine and ATM context kinds. | 1.5, 4.7, 5.3, 8, 9 (details in Part 12) |

## 13. Quirks checklist

Small behaviours that are easy to lose in a rewrite:

1. "Show one cell", cursor moves, clears to end of line/page and format changes do not publish; they ride on the next publish.
2. A tab moves the cursor without erasing the cells it skips.
3. "Standout print" always returns to the normal format, whatever was set before.
4. Writing in column 80 wraps at once; the text page scrolls like the message area when written past its last line.
5. The last sparkle frame stays in the overlay until the square is next recomputed.
6. The mimic's disguise is global, re-rolled at draw time on turns divisible by 10, uses the game's random stream, and may be the first demon lord or the mimic itself.
7. Wall shapes come from the true map and can hint at unexplored neighbours.
8. Monsters on unexplored squares are invisible; unseen, sleeping monsters are invisible even on explored squares; a blind player sees no monsters and no player tile, and the cursor tile is switched off.
9. Even when blind, the per-turn update still records the true object under the player in the player's memory.
10. The game-time, Wild summary and per-species kill fields are only as fresh as the last status refresh / full map repaint, and all map-mode fields freeze while a text page is shown.
11. A direction prompt cannot be cancelled; escape submits a password; `*` or escape returns the default from a number prompt; a lone minus sign yields 0.
12. The checkpoint key is honoured only at a command prompt and silently eaten everywhere else.
13. The "died" event fires for quit, save-and-exit and life-saving revivals as well as real deaths.
14. The context fields and the spell list are refreshed only when the game waits for a command, and hidden traps always read as "nothing".
15. The context "blocked" test counts any adjacent monster, including the player's own pet.
16. The shared block is zeroed at every start and left intact after the game ends.
