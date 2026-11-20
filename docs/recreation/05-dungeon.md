# Part 05 — Dungeon: level layout, maze generation, pre-made maps, object and monster placement

This part specifies the game world as a set of 21 fixed-size levels: what a level is, what
is stored for every square, how a level is built the first time the player arrives (the
open home level, the carved maze, the pre-made maps read from a data file, treasure rooms),
everything that is scattered on a fresh level (stairs, buildings, features, traps, items,
unique artifacts, monsters), how levels are kept and revisited, the level-change procedure,
the random-item generator that lives in the same module, and every helper of the module.

It leans on: Part 02 (player attributes such as the difficulty value and the per-game
"already created" counters), Part 03 (the save file as a whole), Part 04 (the monster table,
the rule that picks a monster for a depth, monster movement, the periodic wandering-monster
spawn), Part 06 (what each object does when stepped on: stairs, elevators, pits, trap
doors, the shaft, doors, chests, books, potions, scrolls), Part 07 (spells that rebuild or
reveal a level), Parts 09 and 11 (how the map is drawn), Part 12 (the Wild Edition hooks)
and Part 14 (tile ids).

## 1. Conventions

### 1.1 Random helpers

Two helpers are used everywhere in this part. Both were confirmed in the source:

* **rnd(n)** is a uniform integer from 1 to n inclusive (the platform random number modulo
  n, plus 1).
* **rund(n)** is a uniform integer from 0 to n-1 inclusive (the platform random number
  modulo n).

All divisions in formulas are integer divisions (truncating). "level/2" for level 7 is 3.

### 1.2 Coordinates

A level is a grid 67 squares wide and 17 squares high. A square is addressed (x, y) with
x = 0..66 counted from the left and y = 0..16 counted from the top (y grows downward on
screen). The width and height must both be odd, because the maze carver works on
odd-numbered squares.

"Interior" below means x = 1..65 and y = 1..15. The outermost ring (x = 0, x = 66, y = 0,
y = 16) is the border.

### 1.3 Object ids used here

Every square holds exactly one object id. Two ids matter structurally: id 0 is "unknown"
(only ever used in the player's memory of a square, never as a real object) and id 1 is
"nothing" (empty floor). "Empty" in this part always means the object is "nothing". The full
object table is in Part 06; this part uses display names.

### 1.4 The eight directions

Several helpers try the eight neighbours of a square in a fixed cyclic order, starting from
a random one. The order is a table indexed 1 to 8:

| Index | dx | dy | Compass |
|---|---|---|---|
| 1 | 0 | +1 | south |
| 2 | +1 | 0 | east |
| 3 | 0 | -1 | north |
| 4 | -1 | 0 | west |
| 5 | +1 | -1 | north-east |
| 6 | -1 | -1 | north-west |
| 7 | +1 | +1 | south-east |
| 8 | -1 | +1 | south-west |

"Try the eight neighbours" means: start index = rnd(8); use it, then the next index, wrapping
from 8 back to 1, until all eight have been used.

## 2. World structure

### 2.1 The levels

There are 21 levels, numbered 0 to 20. Level 0 is the home (town) level, levels 1 to 15 are
the dungeon, levels 16 to 20 are the volcano. "Dungeon bottom" is level 15 and "volcano
bottom" is level 20.

Each level has a two-character display name, used on the status line after "LV:", in the
scoreboard line (" on " followed by the name) and by the front end's level indicator:

| Level | Name | Kind |
|---|---|---|
| 0 | " H" (space, H) | home level |
| 1 to 9 | " 1" to " 9" (space, digit) | dungeon |
| 10 to 15 | "10" to "15" | dungeon |
| 16 to 20 | "V1" to "V5" | volcano |

When the player has been teleported and does not know where they are, the status line shows
" ?" instead of the name (Part 06 and Part 09); the flag for that is cleared at the start of
every level change that begins on the home level (see section 4).

Summary of what each level is and has. "Generator" and "pre-made" are explained in sections
7 and 8; stairs and other objects in section 9; monsters in section 12.

| Level | Layout | Down stairs | Up stairs | Special content |
|---|---|---|---|---|
| 0 (H) | Completely open field, no walls at all | no | no | The eight buildings and entrances (section 6). Fully known from the start. No monsters are placed. |
| 1 | Generator 49%, pre-made 51% | yes | no | The dungeon exit square at (33, 16). Exactly one chest. |
| 2, 3 | Generator 49%, pre-made 51% | yes | yes | |
| 4 | same | yes | yes | First level where the dealer's pad, the up elevator and the "deep" armour and weapons can appear. No treasure rooms yet. |
| 5 to 10 | same | yes | yes | Treasure rooms possible on generated layouts. Level 8 has the bank branch. |
| 11 to 14 | same | yes | yes | Treasure rooms. 1 to 4 demon lords. Monster squares of pre-made maps are live. |
| 15 | Always pre-made | no | no | The Eye of Larn guarded by a demon prince. 5 demon lords. No up elevator either. |
| 16 (V1) | Generator 49%, pre-made 51% | yes | yes, but it is a dead end | The base of the volcanic shaft. 1 demon prince. Treasure rooms possible. |
| 17 (V2) | same | yes | yes | 2 demon princes. |
| 18 (V3) | same | no | yes | A guaranteed pit and a guaranteed trap door. 3 demon princes. |
| 19 (V4) | same | no | yes | Guaranteed pit and trap door. 4 demon princes. |
| 20 (V5) | Always pre-made | no | yes | Guaranteed pit and trap door. The potion of cure dianthroritis guarded by Lucifer. 5 demon princes. |

### 2.2 What is stored for every square

Five grids are stored per level and belong to it (they are kept when the player leaves and
are written to the save file). The monster grid carries the stolen items with it, so the
table has six rows; "the five stored fields" elsewhere in this part means these:

| Field | Size | Meaning |
|---|---|---|
| Object | 1 byte | The object id on the square (floor, wall, door, stairs, an item, a trap, a building...). Exactly one object per square; there are no stacks. |
| Object argument | signed 16-bit | Meaning depends on the object: enchantment of a weapon, armour or ring; the potion or scroll kind; gold amount (in units, tens, hundreds or thousands depending on the gold object type); the level number for books and chests; gem value; the trap type of a closed door; and, for walls, the wall-shape bitmask of section 7.7. |
| Monster | 1 byte | Monster id standing on the square, 0 for none. At most one monster per square, and it shares the square with the object. |
| Monster hit points | signed 16-bit | Current hit points of that monster. |
| Stolen items | 1 count byte plus 6 slots of (object id byte, argument 16-bit) | Items the monster on this square has stolen from the player (Part 04). |
| Known | 1 byte | The object id the player last saw on this square, or 0 (unknown) if never seen. This is the player's map memory. |

Three more grids exist only for the level currently in play. They are not stored with the
level and not written to the save file:

| Field | Size | Meaning |
|---|---|---|
| Stealth flags | 1 byte | Bit value 1 = "seen" (the player has seen or otherwise knows of the monster on this square). Bit value 2 = "awake" (the monster moves). Both clear = asleep and unseen. |
| Moved flag | 1 byte | Scratch flag used during the monster movement pass (Part 04). |
| Path scratch | 16-bit | Scratch distances used by monster path-finding (Part 04). |

A monster is drawn on the map only if the square is known (not "unknown") and the monster is
either seen or awake (awake monsters are assumed to make noise). Part 09 has the complete
tile-selection rule, including invisibility and the demons that need the Eye of Larn.

### 2.3 Levels are generated once and kept

* A level is built the first time the player arrives on it and never rebuilt (the only
  exception is the alter reality spell, Part 07, which rebuilds the current level in place).
* Storage for all 21 levels is allocated when the game starts (running out of memory there
  ends the game with the "malloc failure" death, Part 03) and released when the game ends.
* A per-level "visited" marker (one signed byte per level, all zero in a new game) says
  whether the level exists. It is set to 1 when the level is created. The same byte is also
  used as a counter of items stolen from the player on that level: it is raised by one per
  theft and lowered by the number of items a dying thief drops, never below 1 (Part 04). So
  "visited" means "non-zero".
* When the player leaves a visited level, its five stored fields are copied into that
  level's storage record exactly as they are. Nothing happens to a level while the player
  is elsewhere: no time passes there, monsters do not move, heal or wander off.
* After each copy into storage a 16-bit checksum of the record is remembered. When a record
  is copied back out and a non-zero checksum was remembered, the checksum is recomputed and
  compared. A mismatch prints the two lines of section 15, beeps, waits 5 seconds and ends
  the game with the "internal complications" death. If the remembered checksum is zero (as
  after loading a saved game, where checksums are not stored), the checksum is simply
  computed and remembered. The checksum algorithm: start with 0; for each byte, rotate the
  16-bit sum right by one bit (shift right, and if the bit shifted out was 1 add 32768),
  add the byte, keep the low 16 bits.

## 3. How levels connect

The physical connections are fixed. The object behaviour (prompts, damage, weight checks)
is specified in Part 06; this table gives the geography and where the player arrives.

| Connection | From | To | Arrival position |
|---|---|---|---|
| Dungeon entrance (a building on the home level; the player chooses "go inside") | 0 | 1 | Always (33, 15), the square just inside the exit. The exit square (33, 16) is forced to empty floor and any monster on it is removed. No check is made on (33, 15) itself: if an object or a monster happens to be there the player simply shares the square. |
| Dungeon exit: the square (33, 16) in the bottom border of level 1. Walking onto it leaves the dungeon at once, no prompt. | 1 | 0 | On the dungeon entrance square, then adjusted by the "nearest empty square" rule of section 4.2. Because the entrance square is not empty, the player ends up on the square to its west if that is empty (then north, south, east, then diagonals). |
| Staircase going down | n | n+1, allowed on every level except 0, 15 and 20 | On the up staircase of the new level if one exists (the first found scanning x ascending, then y ascending); if a monster stands on it, adjusted to the nearest empty square. If the new level has no up staircase (arriving on 15 from 14), the player keeps the coordinates they had on the old level, adjusted to the nearest empty square. |
| Staircase leading upwards | n | n-1, allowed when n is at least 2 and n is not 16 | Same rule, looking for the down staircase. Levels 18 and 19 have no down staircase, so arriving there from below keeps the old coordinates. On level 16 the up staircase exists but using it only prints "The stairs lead to a dead end!". |
| Volcanic shaft (home level object "a volcanic shaft leaning downward") | 0 | 16 | A random interior square, x = rnd(65), y = rnd(15), then nearest empty square. |
| Base of the volcanic shaft (object on level 16) | 16 | 0 | On the shaft object of the home level, then nearest empty square (so normally the square to its west). |
| Pit, trap door | n | n+1 (deadly on 15 and 20) | Same coordinates as on the old level, adjusted to the nearest empty square. |
| Elevator going up (one in the whole game) | n | In the dungeon: rund(n), that is any level from 0 to n-1. In the volcano: 15 + rund(n-15), with a result of 15 replaced by 0. | Random interior square, then nearest empty. |
| Elevator going down (one in the whole game) | n | In the dungeon (n below 15): n + rnd(15-n). In the volcano (16 to 19): n + rnd(20-n). On 15 and 20 it kills. | Random interior square, then nearest empty. |
| Teleport (trap, scroll, spell, innate ability) | n | On level 0: stays on 0. Dungeon: rnd(5) + n - 3, clamped to 0..15. Volcano: rnd(4) + n - 2, clamped to at most 20, and anything below 16 becomes 0. | Random interior square, then nearest empty. If the destination equals the current level, no level change is done, only the repositioning. |

Consequences worth knowing when rebuilding:

* Level 15 has no stairs and no up elevator. The ways off it are teleporting and the
  one-per-game down elevator (which is fatal there). Pits and trap doors on 15 and 20 are
  fatal (Part 06).
* Levels 18, 19 and 20 have no down stairs; the generator guarantees a pit and a trap door
  on each so that there is always a way down from 18 and 19.
* The volcano is entered only through the shaft (or by elevator and teleport arithmetic
  from inside it); the dungeon and the volcano are not connected below ground.

## 4. The level-change procedure

One routine is the only way to change level. It takes the destination level number and is
also called with the current level number by the genocide spell (to sweep the level) and
once at the start of a new game with destination 0.

### 4.1 Steps

1. If the level being left is marked visited (always true except at the very start of a new
   game): Wild Edition hook "leaving level" is called with the old level number (pets
   follow the player and similar, see Part 12); then the old level's five stored fields are
   copied into its storage record and its checksum is refreshed.
2. If the level being left is level 0, the "teleported, level unknown" flag is cleared.
3. The remembered position of the monster last hit by the player is reset to "none" (-1, -1).
4. The current level number becomes the destination, and the copy of it kept among the
   player attributes is updated.
5. **If the destination is already visited:**
   1. Its record is copied into the working grids (with the checksum test of section 2.3).
   2. Return-visit monsters are added (section 12.2).
   3. Every monster of a genocided kind is removed from the level.
   4. The player is positioned by the nearest-empty-square rule (section 4.2).
6. **Otherwise (first visit):**
   1. Every square's "known" value is set to unknown and every square's monster to none.
      (Only the monster id is cleared. Hit points, stolen-item slots, object arguments and
      the stealth grid are left as they were on the previous level at this point.)
   2. The layout is built (section 5).
   3. If the level is 1, the exit square (33, 16) is set to empty floor.
   4. Wall shapes are computed for the whole map (section 7.7).
   5. Objects are placed (section 9, or section 6 for the home level).
   6. The level is marked visited.
   7. First-visit monsters are placed (section 12.1), which begins by clearing the whole
      stealth grid, so every monster on a new level starts asleep and unseen.
   8. If the game is in wizard mode, or the level is 0: every square's "known" value is set
      to its real object and every square gets the "seen" stealth bit.
   9. Every monster of a genocided kind is removed.
   10. The player is positioned by the nearest-empty-square rule.
   11. Wild Edition hook "make level" is called with the level number (Wild places its own
       objects and monsters, see Part 12).
7. In both cases, Wild Edition hook "new level" is called with the level number, and a
   "new level" event carrying the level number is posted to the front end (Parts 09, 12).

The caller then usually overrides the position (stairs, entrance, shaft: section 3) and
redraws the whole map.

### 4.2 The nearest-empty-square rule

This helper lives with the player code but decides where the player arrives, so it is
given here.

1. If the square at the player's current coordinates is empty floor and has no monster,
   nothing changes.
2. Otherwise every square of the map (all 67 by 17, border included) is scanned with x
   ascending in the outer loop and y ascending in the inner loop. Among squares that are
   empty floor with no monster, the one with the smallest squared distance
   (dx squared + dy squared) to the current coordinates is chosen; on ties the first one
   found in scan order wins. In practice the preference among the immediate neighbours is
   west, north, south, east, then north-west, south-west, north-east, south-east.
3. If no square qualifies, the message "Failure in positionplayer" (followed by a newline)
   is printed and the player is not moved.

The coordinates that go into this rule are whatever the player's coordinates were before the
call: on a first game start they are random (x = rnd(65), y = rnd(15), set when the
character is created); for stairs, pits and trap doors they are the coordinates on the old
level; for teleports and elevators they were randomised by the caller just before.

### 4.3 What a return visit looks like

* Objects, monsters, their hit points, stolen items and the player's map memory are exactly
  as they were left.
* A few new sleeping monsters have been added (section 12.2). This happens on every arrival
  on a visited level, including the genocide spell's "change to the same level" call, which
  therefore also adds monsters and may nudge the player off an object they were standing on.
* Monsters of kinds genocided in the meantime are gone.
* The stealth flags are not those the level had: see section 12.4.

## 5. Building a new level: order of operations

For a level other than 0, the layout step works as follows.

1. Try to load a pre-made map (section 8). Levels 15 and 20 always try; other levels try
   only when rnd(100) is 50 or more (51 chances in 100). If a map was loaded, the layout
   step is finished: no maze, no open rooms, no through-corridor and no treasure rooms are
   made on a pre-made map.
2. Otherwise (and also if the map file could not be opened or read): fill every square of
   the map, border included, with wall.
3. Carve the maze starting at (1, 1) (section 7.2).
4. If the level is not 15 and not 20: cut the open rooms (section 7.3), then the
   through-corridor (section 7.4).
5. If the level number is 5 or more: build treasure rooms (section 7.5).

For level 0 the layout step just fills every square with empty floor.

After the layout come, in this order: the level-1 exit, wall shapes, object placement,
monster placement, reveal (home level or wizard), genocide sweep, player positioning and the
Wild hooks, as listed in section 4.1.

## 6. The home level

* All 1,139 squares are empty floor. There are no walls, not even a border; the edge of the
  map is the edge of the world.
* Eight objects are then placed, one after another, each with the random free-square walk of
  section 9.1 (so each lands on a random empty interior square, x = 1..65, y = 1..15, never
  on the border and never on a square already taken). Nothing else constrains their
  positions: they may be adjacent to each other or far apart, and the layout differs every
  game. The order of placement is:

| Order | Building or entrance | Display name |
|---|---|---|
| 1 | Dungeon entrance | "the dungeon entrance" |
| 2 | DND store | "the DND store" |
| 3 | College | "the College of Larn" |
| 4 | Bank | "the Bank of Larn" |
| 5 | Volcanic shaft down | "a volcanic shaft leaning downward" |
| 6 | The player's home | "your home" |
| 7 | Trading post | "the local trading post" |
| 8 | Revenue service | "the Larn Revenue Service" |

* All object arguments are 0.
* Nothing else is placed by the base game: no stairs, no items, no gold, no traps, no
  monsters on creation and none added on return visits.
* The whole level is known to the player from the first moment.
* The player starts a new game at a random interior square (x = rnd(65), y = rnd(15)),
  moved to the nearest empty square if it coincides with a building.
* Wild Edition hook: the "make level" hook also runs for level 0, see Part 12.

## 7. The generated maze

### 7.1 Starting state

Every square is wall. All later steps only turn wall into floor, except the treasure rooms,
which also put walls and doors back.

### 7.2 The carver ("eat")

The carver is a recursive routine called once with the square (1, 1). Called for a square
(x, y) it does the following.

1. Choose a starting direction d = rnd(4) and set a pass counter to 2.
2. Attempt direction d (rules below).
3. Add 1 to d. If d is now greater than 4, set d to 1 and subtract 1 from the pass counter.
4. If the pass counter is still above 0, go back to step 2. Otherwise return.

So from a starting direction d0 the attempts are d0, ..., 4 and then 1, 2, 3, 4 once more:
between five and eight attempts, every direction at least once, always in ascending cyclic
order. The recursion goes as deep as it can before the next attempt is made.

An attempt in a direction succeeds only if the range test passes and both the square next
to (x, y) in that direction and the square two steps away are currently wall. On success
both squares become empty floor and the carver is called for the square two steps away.

| d | Direction | Range test | Squares opened |
|---|---|---|---|
| 1 | west (x decreases) | x greater than 2 | (x-1, y) and (x-2, y) |
| 2 | east (x increases) | x less than 64 | (x+1, y) and (x+2, y) |
| 3 | up (y decreases) | y greater than 2 | (x, y-1) and (x, y-2) |
| 4 | down (y increases) | y less than 14 | (x, y+1) and (x, y+2) |

Properties of the result, which give the levels their character:

* The carver visits every square with odd x (1..65) and odd y (1..15): 33 columns by 8 rows
  of "cells". Corridors are one square wide, separated by walls one square thick. The border
  ring stays solid wall.
* The result is a "perfect" maze (one route between any two cells) with long winding
  corridors and many dead ends, as produced by a depth-first carver.
* Quirk to reproduce: the starting square (1, 1) is not opened by its own call; only the
  squares it carves towards are. It stays wall unless a later branch arriving at (3, 1) or
  (1, 3) from elsewhere carves into it (which the test allows, because (1, 1) still reads
  as wall); in that case (1, 1) becomes floor and the maze gains one loop in its top-left
  corner. Implement the test literally and this falls out by itself.

### 7.3 Open rooms

Not done on levels 15 and 20. The number of rooms is rnd(3) + 3, that is 4, 5 or 6. For each
room, in this order of random draws:

1. Row centre my = rnd(11) + 2 (3..13). Top row = my - rnd(2). Bottom limit = my + rnd(2).
   The room covers rows from the top row up to but not including the bottom limit, so it is
   2, 3 or 4 rows high and lies within rows 1..14.
2. In the dungeon (levels 1 to 14): column centre mx = rnd(44) + 5 (6..49). Left column =
   mx - rnd(4). Right limit = mx + rnd(12) + 3. The room covers columns from the left column
   up to but not including the right limit: 5 to 19 columns wide, within columns 2..63.
   No monster is attached.
3. In the volcano (levels 16 to 19): column centre mx = rnd(60) + 3 (4..63). Left column =
   mx - rnd(2). Right limit = mx + rnd(2). Width 2, 3 or 4 columns, within columns 2..64.
   Then one monster kind is chosen for the room with the depth rule of Part 04 applied to
   the level number (summarised in section 12.3).
4. Every square of the rectangle becomes empty floor. In the volcano, every square of the
   rectangle additionally receives a monster of the room's kind at that kind's full
   starting hit points.

Notes:

* These "rooms" have no walls of their own and no doors. They are simply rectangular
  clearings punched through the maze, so they appear as open halls with corridor mouths all
  around their edge. Rooms may overlap one another.
* In the volcano a room is a solid block of 4 to 16 identical monsters (2 to 4 wide, 2 to 4
  high). An overlapping later room overwrites the monsters of the overlap with its own
  kind. With 4 to 6 such blocks, a generated volcano level starts with dozens of extra
  monsters. They are asleep like everything else on a new level.
* Dungeon rooms are wide and low (the map is wide and low); volcano rooms are small.

### 7.4 The through-corridor

Not done on levels 15 and 20. One row y = rnd(15) (1..15) is chosen and every square of it
from x = 1 to x = 65 becomes empty floor. Every generated level therefore has one straight
corridor running the full width of the map, which ties the maze together and makes it much
less of a maze. Monsters already placed in volcano rooms are not disturbed.

### 7.5 Treasure rooms

Only on generated layouts, only on levels 5 and deeper (5 to 14 and 16 to 19 in practice).

**Where and how many.** A column position tx starts at 1 + rnd(10) (2..11) and advances by
10 while it is less than 57. That gives 6 candidate slots when the start is 2..6 and 5
slots when it is 7..11. At each slot a treasure room is built with probability 2 in 10
(the test is rnd(10) at most 2). So a level has between 0 and 6 treasure rooms, on average
a little over one, and never two in the same 10-column slot.

For a room that is built, the draws are, in order: width xsize = rnd(6) + 3 (4..9), height
ysize = rnd(3) + 3 (4..6), top row ty = rnd(8) + 1 (2..9), then the door trap value
rnd(9) (1..9). The left column is the slot's tx. Width and height include the walls, so the
inside is 2 to 7 squares wide and 2 to 4 squares high.

**Construction of one room** (top-left corner (tx, ty)):

1. Every square of the rectangle one larger on each side, from (tx-1, ty-1) to
   (tx+xsize, ty+ysize) inclusive, becomes empty floor. This leaves a one-square walkway
   all round the room, so the room is always reachable. Monsters are not touched by this
   step.
2. Every square of the room rectangle proper, columns tx..tx+xsize-1 and rows
   ty..ty+ysize-1, becomes wall, and any monster on those squares is removed.
3. The inside, columns tx+1..tx+xsize-2 and rows ty+1..ty+ysize-2, becomes empty floor.
4. One closed door is placed in the wall ring. With rnd(2) = 1 it goes on a horizontal
   wall: x = tx + rund(xsize), and y is the top wall or the bottom wall with equal chance
   (ty + (ysize-1) times rund(2)). With rnd(2) = 2 it goes on a vertical wall: x is the
   left or the right wall with equal chance, y = ty + rund(ysize). The door can therefore
   fall on a corner of the ring, in which case it touches the inside only diagonally
   (diagonal movement makes that passable). The door's argument is the trap value drawn
   above.
5. Contents. A "seat" row is fixed at y = ty + ysize/2 (the second inside row for heights
   4 and 5, the third for height 6). Seats are the squares on that row at columns tx+1,
   tx+3, tx+5, ... up to and including tx+xsize-2 (1 seat for width 4, 2 for widths 5 and
   6, 3 for 7 and 8, 4 for 9). For each seat, from left to right:
   * Draw j = rnd(6) if the difficulty value is below 3, or j = rnd(4) if it is 3 or more.
   * Repeat j + 1 times (2 to 7 times, or 2 to 5 at high difficulty): first create random
     treasure around the seat with the "something" routine of section 11.2 at item depth
     (level + 2); then create one monster next to the seat, of a kind picked by the depth
     rule for depth (level + 2), or (level + 4) at difficulty 3 or more.
   * Both the treasure and the monster go onto one of the eight neighbours of the seat (in
     the cyclic order of section 1.4): the item on the first neighbour that is not wall,
     not a closed door and holds no object; the monster on the first neighbour that is not
     wall, not a closed door and holds no monster. When no neighbour qualifies the item or
     monster is silently not created. The seat squares themselves always stay free.
     Monsters may stand on items.

**Door traps.** A closed door's argument selects what happens when an attempt to open it
fails (Part 06): 6 aggravates monsters, 7 and 8 give an electric shock, 9 weakens the
player, anything else is harmless. Treasure room doors draw from 1..9, so 4 in 9 are
trapped (compare 4 in 30 for doors on pre-made maps).

**Quirks.**

* The treasure routine refuses item depths above 20. On level 19 (V4) the depth would be
  21, so treasure rooms there contain monsters but no treasure.
* Objects placed later by the general scatter (section 9) can land inside treasure rooms,
  and the general monster placement can put monsters there too.
* The routine has a second mode for levels 15 and 20 (a room at every slot, shifted right
  by a further rnd(43) columns, door trap value rnd(3) + 6 so always trapped). It can only
  run if the pre-made map file is missing, because those two levels otherwise never use the
  generator; in that fallback the levels have no Eye and no potion at all, and the shifted
  column can run off the map. A recreation that always ships its map file never executes
  this and may leave it out.

### 7.6 The level-1 exit

After the layout of level 1 exists (generated or pre-made), the square (33, 16), in the
middle of the bottom border, is set to empty floor. On a generated layout (33, 15) is always
a maze cell, and all pre-made maps keep (33, 15) open, so the exit is always reachable.

Rules attached to this square: nothing may be created on it by the item and monster
creation helpers while the current level is 1 (section 14, "is this square usable");
the player cannot drop things there; stepping on it leaves the dungeon (section 3).

### 7.7 Wall shapes

Each wall square stores, as its object argument, a 4-bit mask describing which of its four
orthogonal neighbours are "wall-like" (a wall, an open door or a closed door):

| Bit value | Neighbour |
|---|---|
| 1 | west (x-1, y) |
| 2 | south (x, y+1) |
| 4 | east (x+1, y) |
| 8 | north (x, y-1) |

Neighbours off the map count as not wall-like. The drawing code shows wall tile number
"base wall tile + mask" (16 wall tiles, Part 14). The mask is computed for the whole map
when a level is created (after the layout and the level-1 exit, before objects are placed)
and recomputed for a clamped rectangle around any place where spells or spheres remove
walls (Part 07). Because doors are fixed when the layout is built, and objects never
replace walls, nothing else needs to refresh it.

## 8. Pre-made maps

### 8.1 When a pre-made map is used

* Levels 15 and 20 always use one.
* Every other level from 1 to 19 first draws rnd(100); if the result is below 50 (49 cases
  in 100) the generator is used, otherwise a pre-made map.
* If the file cannot be opened, or ends early while being read, the generator is used
  instead (for an early end, squares already filled from the file keep their monsters,
  which is harmless garbage; a recreation should just treat the file as always present).
* Then one of the first 20 maps of the file is picked uniformly (index rund(20)), with no
  memory: the same map can serve several levels of the same game.
* There is no flipping, mirroring or rotation. The map is used exactly as drawn.

### 8.2 File layout

The file is a plain text file (named "Umaps" in the game's data directory).

* It holds 21 maps. Each map is 17 lines of exactly 67 characters, each ended by a line
  feed, and every map (including the last) is followed by one empty line. That makes 18
  lines per map, 378 lines and 24,296 bytes in total.
* To reach map number i (counting from 0) the reader skips i times 18 lines, then reads 17
  lines. Lines are read with a 127-character limit and only the first 67 characters are
  used, so trailing blanks would be tolerated; anything shorter than 67 characters would be
  read as floor and garbage.
* Line number r of a map is row y = r, character number k is column x = k (both from 0).
* Because only indices 0 to 19 can be drawn, the 21st map is never used. (The file's own
  format notes say "21 mazes"; the selection range is simply one short.)

### 8.3 Meaning of the characters

| Character | Result on the square |
|---|---|
| `#` | Wall. |
| space | Empty floor. |
| `D` | Closed door with trap value rnd(30) (so 1 in 30 aggravate, 2 in 30 shock, 1 in 30 weaken, otherwise harmless). |
| `.` | On levels 11 to 20: empty floor with a monster on it, of a kind picked by the depth rule for depth (level + 1), at full hit points. On levels 1 to 10: plain empty floor. |
| `-` | A random object from the random-object table of section 11.1 at item depth (level + 1), with its rolled argument. |
| `~` | On level 15 only: the Eye of Larn (argument 0) with a demon prince standing on it at full hit points. On every other level: plain empty floor. |
| `!` | On level 20 only: a potion of cure dianthroritis (a potion object whose argument is the potion number 21) with Lucifer standing on it at full hit points. On every other level: plain empty floor. |
| anything else | Empty floor. |

Every square of the level is written from the file (object, argument, monster, hit points),
and its "known" value is set to unknown (or to the real object in wizard mode).

After the map is loaded, the level goes through the same later steps as a generated one:
level-1 exit, wall shapes, the complete object scatter of section 9 (stairs included, placed
at random like everywhere else), and the monster placement of section 12. The map monsters
start asleep.

### 8.4 Requirements for newly drawn maps

A replacement map set must satisfy these, because the code relies on them:

* 67 by 17, with a solid wall on all four edges.
* The square (33, 15) is open (floor or a `.`), because the player arrives there on level 1
  and the exit is cut directly below it.
* Exactly one `~` and exactly one `!` in every map, since any of the 20 maps may be drawn
  for level 15 or level 20 and those squares are the only source of the Eye and of the
  potion.
* Both special squares, and all the rest of the open area, reachable from (33, 15) through
  floor and doors when diagonal steps are allowed. Several original maps depend on diagonal
  steps (their diagonal walls leave only diagonal gaps); none depends on tunnelling. (One
  original map has a sealed four-square pocket of no importance.)

### 8.5 Style of the original maps

So that equivalent maps can be drawn, here is what the originals look like.

**Proportions per map (the 20 usable ones).**

| Square type | Minimum | Maximum | Average |
|---|---|---|---|
| Wall `#` (out of 1,139 squares) | 431 | 606 | 528 |
| Monster squares `.` | 34 | 79 | 58 |
| Doors `D` | 2 | 20 | 9 |
| Random objects `-` | 3 | 18 | 9 |
| `~` and `!` | 1 each | 1 each | 1 each |

So a little under half of each map is wall, about 5% of the squares are monster squares,
and doors and objects are sparse. On levels 11 and deeper a pre-made map therefore brings
around 58 sleeping monsters before the normal placement adds its own; on levels 1 to 10 the
same map is nearly empty of monsters.

**Shared habits.**

* Walls are one square thick, except diagonal walls, which are drawn two squares thick
  (pairs of `##` stepping one column per row) so that they look solid.
* Corridors are one square wide. Nearly every map has a long, almost unbroken corridor along
  row 1 and another along row 15, with side corridors feeding rooms; halls and rooms are 3 to 6 squares
  across.
* Rooms are closed with single doors at their mouths; a few vault entrances use two doors
  side by side. Some rooms have no door, only a gap.
* Monster squares come in three patterns: lone sentries in corridors and doorways; straight
  lines of 3 to 7 along a passage; and solid blocks (3 by 4 or 3 by 5, "barracks") filling
  a room.
* Object squares sit at dead ends, in small closets behind a door, and inside the vaults,
  usually with one or more monster squares beside them.
* The two special squares are placed deep inside the layout, far from (33, 15) by walking
  distance: in roughly a third of the maps they are side by side or within two squares of
  each other in one vault (stacked in a one-wide cell, or as "`~`, monster, `!`" in a row); in
  the others they are in separate vaults at opposite ends of the map. They usually have
  several monster squares within two squares, but a few maps leave them unguarded apart
  from the fixed guardian that comes with the special square itself.

**The designs.** There are about eleven distinct designs; eight of the maps are reworked
copies of earlier ones (maps 13 to 20 in file order repeat maps 1 and 6 to 12 with 4 to 10
percent of the squares changed: a door moved, a wall opened, the special squares relocated),
and map 5 is map 4 with its rows reordered and a few squares edited. The designs are:

* An "office block": three horizontal bands separated by full-width walls with doors; a
  band of door-linked rooms on top, a band of nested rooms in the middle (including a small
  rectangular spiral and a long sealed gallery), and a row of identical five-wide cells with
  doors along the bottom corridor; the vault is a small walled block on the right.
* A rectangular spiral: concentric one-wide corridors wound inward from the outer ring,
  each ring entered through a single door at a corner, ending in a strip of tiny cells
  where the special squares sit.
* Diagonal ribbons: double-thick diagonal walls forming diamond and chevron chambers with
  doors where the diagonals meet.
* A diamond keep: nested diamonds in the centre with the special squares stacked at the
  core, a hall of regularly spaced one-square pillars to one side and rectangular rooms to
  the other.
* A warren of small irregular rooms joined by many doors.
* Nested rectangular rings of corridor wrapped around blocks of rooms.
* A slanted hall: a mostly open map crossed by a few long double-thick diagonal walls
  forming large V shapes (this is the map with the fewest walls).
* A colonnade: a comb of one-square pillars on one side and an octagonal vault in the
  middle with a door at the bottom.
* Big rectangular blocks with wide streets between them.
* A winding layout of long bent corridors and pocket rooms.
* A cell block: a comb of one-wide dead-end corridors along the top and a row of six
  identical cells (5 wide, 4 high inside) along the bottom, each entered from the corridor
  above through a door or a one-square gap.
* (The unused 21st map is a dense comb maze of long one-wide corridors.)

An illustrative fragment in the same style (drawn for this document, not taken from the
file):

```
#####################
#  .  D     #-..#   #
#     #  #  ##D##   #
###D###  #    .     #
#.....   #####D######
#.....   D  ~.!  -  #
#####################
```

## 9. Object placement on a new level

### 9.1 The placement helpers

**Random free-square walk (place one object).** Given an object and an argument:

1. Start at x = rnd(65), y = rnd(15).
2. While the square at (x, y) is not empty floor: add rnd(3) - 2 (that is -1, 0 or +1) to x
   and, independently, rnd(3) - 2 to y. Then wrap: x above 65 becomes 1, x below 1 becomes
   65, y above 15 becomes 1, y below 1 becomes 15.
3. Put the object and its argument on the square.

It only looks at the object on the square. It ignores monsters (objects can land under
sleeping monsters), ignores the player's position, never uses the border ring, and happily
places things inside treasure rooms, sealed vaults or dead ends.

**Place n of the same.** Repeats the walk n times with the same object and argument; n of
zero or less places nothing. The argument is computed once and shared by all n copies.

**Place with a chance.** Given a threshold t: draw rnd(151); if it is less than t, do one
walk. The chance is therefore (t-1) in 151. A threshold of 1 can never succeed.

### 9.2 The full list, in order, for levels 1 to 20

"L" is the level number (1..20). Every line is executed in this order; each "if" is
evaluated as written.

| # | Condition | What is placed | Count | Argument |
|---|---|---|---|---|
| 1 | L = 16 | Base of the volcanic shaft ("the base of a volcanic shaft") | 1 | 0 |
| 2 | L is 1..14, 16 or 17 | Staircase going down | 1 | 0 |
| 3 | L is 2..14 or 16..20 | Staircase leading upwards | 1 | 0 |
| 4 | L is 4..14 or 17..19, and no up elevator has been created yet this game, and rnd(100) is above 85 (15%) | Elevator going up; the "up elevator created" counter is raised | 1 | 0 |
| 5 | L is 1..10, 15 or 20, and no down elevator has been created yet this game, and rnd(100) is above 85 (15%) | Elevator going down; the "down elevator created" counter is raised | 1 | 0 |
| 6 | always | Book | rund(3) (0..2) | L |
| 7 | always | Fortune cookie | rund(3) | 0 |
| 8 | always | Altar | rund(3) | 0 |
| 9 | always | Statue | rund(3) | 0 |
| 10 | always | Fountain | rund(3) | 0 |
| 11 | always | Throne (the jewelled kind) | rund(2) (0..1) | 0 |
| 12 | always | Mirror | rund(2) | 0 |
| 13 | L is 18, 19 or 20 | Pit | 1 | 0 |
| 14 | always | Pit | rund(3) | 0 |
| 15 | L is 18, 19 or 20 | Hidden trap door | 1 | 0 |
| 16 | always | Hidden trap door | rund(2) | 0 |
| 17 | always | Hidden arrow trap | rund(2) | 0 |
| 18 | always | Hidden teleport trap | rnd(3) - 2, so one with chance 1 in 3, else none | 0 |
| 19 | always | Hidden dart trap | rnd(3) - 2, so one with chance 1 in 3, else none | 0 |
| 20 | L = 1 | Chest | exactly 1 | 1 |
| 21 | L is not 1 | Chest | rund(2) | L |
| 22 | L is 1..15 | Diamond | rund(2) | rnd(10 L + 1) + 10 |
| 23 | L is 1..15 | Ruby | rund(2) | rnd(6 L + 1) + 6 |
| 24 | L is 1..15 | Emerald | rund(2) | rnd(4 L + 1) + 4 |
| 25 | L is 1..15 | Sapphire | rund(2) | rnd(3 L + 1) + 2 |
| 26 | always | Potion | rnd(4) + 3 (4..7) | each one rolled separately from the potion table (section 11.5) |
| 27 | always | Scroll | rnd(5) + 3 (4..8) | each one rolled separately from the scroll table (section 11.5) |
| 28 | always | Gold pile (plain, argument = gold pieces) | rnd(12) + 11 (12..23) | each one separately: 12 times rnd(L + 1), plus 8 L, plus 10 |
| 29 | L = 8 | Bank branch ("the 8th branch of the Bank of Larn") | 1 | 0 |
| 30 | L is 4 or more, the dealer's pad has not been created yet this game, and rnd(100) is above 75 (25%) | Dealer McDope's Pad; the "pad created" counter is raised | 1 | 0 |
| 31 | chance 1 in 151 | Ring mail | 1 | 0 |
| 32 | chance 0 (threshold 1: never) | Studded leather armor | - | 0 |
| 33 | chance 2 in 151 | Splint mail | 1 | 0 |
| 34 | chance 4 in 151 | Shield | 1 | rund(3) (0..2) |
| 35 | chance 1 in 151 | Battle axe | 1 | rund(3) |
| 36 | chance 4 in 151 | Longsword | 1 | rund(3) |
| 37 | chance 4 in 151 | Flail | 1 | rund(3) |
| 38 | chance 6 in 151 | Spear | 1 | rnd(5) (1..5) |
| 39 | chance 3 in 151 | Ring of regeneration | 1 | rund(3) |
| 40 | chance 0 (threshold 1: never) | Ring of protection | - | rund(3) |
| 41 | chance 1 in 151 | Ring of strength | 1 | rund(5) (0..4) |
| 42 | chance 1 in 151 | Ring of extra regeneration | 1 | 0 |
| 43 | see section 10 | Unique items, first group (at most one) | 0 or 1 | 0 |
| 44 | see section 10 | Unique items, second group (each independently) | 0 to 6 | 0 |
| 45 | see below | "Deep" weapons and armour | | |

Line 45: this block runs if the difficulty value is below 3, or else if rnd(4) equals 3
(1 chance in 4; the draw is made only at difficulty 3 or more). Inside the block, only if
L is 4 or more:

| Chance | Object | Argument |
|---|---|---|
| 2 in 151 | Sunsword | rund(6) (0..5) |
| 4 in 151 | Two-handed sword | rnd(6) (1..6) |
| 2 in 151 | Belt of striking | rund(7) (0..6) |
| 2 in 151 | Energy ring | rund(6) (0..5) |
| 3 in 151 | Plate mail | rund(8) (0..7) |

Notes on the list:

* Arguments of weapons, armour and rings are their enchantment ("+n"). Book and chest
  arguments are the level they were made on, which decides their contents when read or
  opened (Part 06). Gem arguments are their value. The argument expressions of lines 22 to
  25 and 34 to 41 are evaluated whether or not the object ends up being placed.
* The two "never" lines (studded leather armor and ring of protection) are upstream
  behaviour: those two can still appear through the random-object table of section 11.
* The elevators and the pad are each created at most once per game because their counters
  are player attributes that are saved with the game (Part 02).
* The level-15 column of the table: no stairs, no up elevator, possibly the down elevator,
  everything else as usual, plus the Eye from the map. Level 20: up stairs, the forced pit
  and trap door, possibly the down elevator, no gems, plus the potion from the map.
* The home level uses section 6 instead of this list.

## 10. Unique items

Sixteen artifacts can each be created by the level scatter only while their per-game
counter (a player attribute, Part 02) is zero. When one is placed (random free-square walk,
argument 0) its counter is raised by one.

For each candidate, the test is: counter is zero, and the level is at least the minimum
level, and rnd(die) is less than (base + per-level bonus times (level - minimum level)).

**First group: at most one per level.** The ten candidates are tested in this order and the
group stops at the first one that is created.

| Order | Item | Minimum level | Chance |
|---|---|---|---|
| 1 | Brass lamp | 1 | 7 in 120 |
| 2 | Wand of wonder | 1 | 7 in 120 |
| 3 | Orb of dragon slaying | 1 | 7 in 120 |
| 4 | Scarab of negate spirit | 1 | 7 in 120 |
| 5 | Cube of undead control | 1 | 7 in 120 |
| 6 | Device of theft-prevention | 1 | 7 in 120 |
| 7 | The Talisman of the Sphere | 1 | 7 in 120 |
| 8 | The Hand of Fear | 1 | 7 in 120 |
| 9 | Orb of enlightenment | 1 | 7 in 120 |
| 10 | Elven chain | 1 | 7 in 120 |

(Die 120, base 8, no level bonus: rnd(120) below 8.)

**Second group: each tested independently**, after the first group, in this order.

| Order | Item | Minimum level | Die | Threshold | Chance |
|---|---|---|---|---|---|
| 11 | Sword of slashing | 1 | 120 | 8 | 7 in 120 |
| 12 | Bessman's flailing hammer | 1 | 120 | 8 | 7 in 120 |
| 13 | Slayer | 10 | 100 | 15 + (level - 10) | 14% on level 10, plus 1% per level deeper, 24% on level 20 |
| 14 | The Vorpal Blade | 1 | 120 | 8 | 7 in 120 |
| 15 | Staff of power | 8 | 100 | 15 + (level - 8) | 14% on level 8, plus 1% per level deeper, 26% on level 20 |
| 16 | Amulet of life preservation | 5 | 100 | 15 | 14% |

Volcano levels count by their level number (16 to 20), so they are the richest source of
Slayer and the staff.

Related behaviour outside this module: when the brass lamp's genie leaves for good, the
lamp's counter is reset to zero so that another lamp can be generated later; the orb of
enlightenment's counter also goes up while the orb is carried and down when it is dropped
(Part 06).

## 11. Random items

### 11.1 The random-object table

"Pick a random object for item depth d" returns an object and its argument. The object is
entry number rnd(N) of the table below, where N depends on the depth: N = 32 when d is 3 or
less, N = 35 when d is 4, 5 or 6, and N = 37 when d is 7 or more.

| Entries | Object | Weight | Argument |
|---|---|---|---|
| 1 to 4 | Scroll | 4 | scroll table roll (section 11.5) |
| 5 to 8 | Potion | 4 | potion table roll (section 11.5) |
| 9 to 12 | Gold pile | 4 | rnd(10 (d + 1)) + 10 d + 10 gold pieces |
| 13 to 16 | Book | 4 | d |
| 17 to 19 | Dagger | 3 | dagger bonus table |
| 20 to 22 | Leather armor | 3 | leather bonus table |
| 23 | Ring of regeneration | 1 | rund(d/3 + 1) |
| 24 | Ring of protection | 1 | rnd(d/4 + 1) |
| 25 | Energy ring | 1 | rund(d/4 + 1) |
| 26 | Ring of dexterity | 1 | rnd(d/4 + 1) |
| 27 | Ring of strength | 1 | rnd(d/2 + 1) |
| 28 | Spear | 1 | rund(d/3 + 1) |
| 29 | Belt of striking | 1 | rund(d/2 + 1) |
| 30 | Ring mail | 1 | rund(d/2 + 1) |
| 31 | Studded leather armor | 1 | rund(d/2 + 1) |
| 32 | Shield | 1 | rund(d/3 + 1) |
| 33 (depth 4 and more) | Flail | 1 | rund(d/2 + 1) |
| 34 (depth 4 and more) | Chain mail | 1 | chain bonus table |
| 35 (depth 4 and more) | Two-handed sword | 1 | rund(d/3 + 1) |
| 36 (depth 7 and more) | Plate mail | 1 | plate bonus table |
| 37 (depth 7 and more) | Longsword | 1 | sword bonus table |

So at shallow depth 16 of 32 picks are scrolls, potions, gold or books.

Bonus tables: the argument is a uniformly chosen entry of a small list. For three of them
only the first part of the list is used when the difficulty value is non-zero.

| Item | List | Entries used at difficulty 0 | Entries used at difficulty 1 and more |
|---|---|---|---|
| Dagger | 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 5 | all 13 | all 13 |
| Leather armor | 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7 | all 15 | the first 12 (maximum +4) |
| Chain mail | 0, 0, 0, 1, 1, 1, 2, 2, 3, 4 | all 10 | all 10 |
| Plate mail | 0, 0, 0, 0, 1, 1, 2, 2, 3, 4 | all 10 | the first 3 (always +0) |
| Longsword | 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 3 | all 13 | the first 6 (always +0) |

Callers pass different depths: pre-made map object squares use (level + 1), treasure rooms
(level + 2); monster drops, chests and the create-artifact scroll pass their own (Parts 04,
06).

### 11.2 "Something": create random treasure around a square

Given a centre square and an item depth: if the depth is below 0 or above 20, do nothing.
Otherwise pick a random object (11.1) and create it next to the centre (11.3); then draw
rnd(101) and repeat the whole thing while the draw is below 8 (7 chances in 101 of each
further item).

### 11.3 Create an item next to a square

Given a centre (x, y), an object and an argument. Object ids outside the object table are
ignored. The eight neighbours of the centre are tried in the cyclic order of section 1.4;
the centre itself is never used.

**Anything that is not gold:** the item goes on the first neighbour that is usable and holds
no object (section 14, "is this square usable" with the object check on; monsters do not
matter). If none qualifies, nothing is created and nothing is reported.

**Gold.** There are four gold objects that differ in the unit of their argument: a plain
pile (gold pieces), a "tens" pile (argument times 10), a "hundreds" pile (argument times
100) and a "thousands" pile (argument times 1000). For gold the argument passed in is the
amount in gold pieces. For each neighbour in order, depending on what is already there:

1. A plain pile: if its amount plus the new amount is below 32,767, add and stop. Otherwise
   continue with rule 2 as if it were a tens pile.
2. A tens pile (or arriving from rule 1): if 10 times its argument plus the new amount is
   below 327,670, set the argument to (10 times argument + amount) / 10, make it a tens
   pile and stop. Otherwise continue with rule 3.
3. A hundreds pile (or arriving from rule 2): if 100 times its argument plus the amount is
   below 3,276,700, set the argument to (100 times argument + amount) / 100, make it a
   hundreds pile and stop. Otherwise continue with rule 4.
4. A thousands pile (or arriving from rule 3): if 1000 times its argument plus the amount is
   at most 32,767,000, set the argument to (1000 times argument + amount) / 1000 and make
   it a thousands pile; otherwise set the argument to 32,767. Stop either way.
5. Anything else: if the square is usable and holds no object, put the requested gold
   object there and stop; the stored argument is the amount divided by 100 when the
   requested object is the hundreds pile, and the amount itself otherwise. If the square is
   not usable, go on to the next neighbour.

Quirks that follow from these rules and should be kept:

* Divisions truncate, so gold is lost in rounding: creating a hundreds pile from 299 gold
  stores 2 (worth 200), and merging 7 gold into a tens pile of 50 leaves it at 50.
* When a plain pile overflows into rule 2 its argument is reinterpreted as tens, multiplying
  what was already there by ten. This needs a plain pile near 32,767 and is rare.
* The gold rules look at the neighbour's object before any bounds check. A recreation should
  treat an off-map neighbour as "anything else, not usable".

### 11.4 Drop gold around the player

Given an amount: if it is more than 250 create a hundreds pile, otherwise a plain pile, next
to the player's square with the rules of 11.3. Used for monster gold drops (Part 04) and for
the gold that spills out of an opened chest (Part 06).

### 11.5 Potion and scroll kinds

The kind of a randomly created potion or scroll is drawn uniformly from a weighted list.
These lists belong to the potion and scroll modules (Part 06 is authoritative for names and
effects); the weights are repeated here because they decide what a new level contains.

Potions, 41 entries in total. The potion of cure dianthroritis is not in the list.

| Potion | Weight | Potion | Weight |
|---|---|---|---|
| sleep | 2 | water | 2 |
| healing | 3 | blindness | 1 |
| raise level | 1 | confusion | 1 |
| increase ability | 2 | heroism | 1 |
| wisdom | 2 | sturdiness | 1 |
| strength | 2 | giant strength | 1 |
| raise charisma | 2 | fire resistance | 1 |
| dizziness | 2 | treasure finding | 2 |
| learning | 1 | instant healing | 2 |
| gold detection | 3 | poison | 2 |
| monster detection | 3 | see invisible | 2 |
| forgetfulness | 2 | | |

Scrolls, 81 entries in total.

| Scroll | Weight | Scroll | Weight |
|---|---|---|---|
| enchant armor | 4 | spirit protection | 3 |
| enchant weapon | 5 | undead protection | 4 |
| enlightenment | 6 | stealth | 2 |
| blank paper | 5 | magic mapping | 2 |
| create monster | 3 | hold monsters | 3 |
| create artifact | 5 | gem perfection | 2 |
| aggravate monsters | 5 | spell extension | 2 |
| time warp | 4 | identify | 3 |
| teleportation | 3 | remove curse | 4 |
| expanded awareness | 4 | annihilation | 1 |
| haste monsters | 4 | pulverization | 3 |
| monster healing | 3 | life protection | 1 |

Wild Edition hook: the scroll roll may be replaced by a special Wild scroll, see Part 12.

## 12. Monster placement

### 12.1 First visit

1. The stealth grid is cleared for the whole map: every monster already on the level (from
   a pre-made map, volcano rooms or treasure rooms) becomes asleep and unseen. This also
   cancels the "starts awake" rule that the next-to-a-square monster creator applies to
   rothes, poltergeists and vampires in treasure rooms.
2. On level 0 nothing more happens.
3. Otherwise the number of ordinary monsters is rnd(12) + 2 + level/2 (for example 3 to 14
   on level 1, 10 to 21 on level 15, 13 to 24 on level 20). For each, a kind is picked with
   the depth rule for the level number and one placement attempt is made (12.5). A failed
   attempt is not retried, so the real number can be lower.
4. On levels 11 to 15: (level - 10) demon lords are added, so 1 on level 11 up to 5 on
   level 15. Each is one of the seven demon lord kinds chosen uniformly (the kind is
   redrawn on every retry), and placement is retried until it succeeds.
5. On levels 16 to 20: (level - 15) demon princes are added, 1 on V1 up to 5 on V5, each
   retried until placed.

Together with the map guardians, level 15 therefore holds 5 demon lords plus the demon
prince on the Eye, and level 20 holds 5 demon princes plus Lucifer on the potion.

All of these start asleep with their kind's full starting hit points.

### 12.2 Return visits

Every time the player arrives on an already visited level other than level 0,
(level/2 + 1) ordinary monsters are attempted (1 on level 1, 8 on levels 14 and 15, 11 on
level 20), each with one placement attempt and a kind from the depth rule. Demon lords and
princes are never added again. Nothing depends on how long the player was away.

The periodic wandering-monster spawn while the player stays on a level (one placement
attempt every 120 - 4 times level turns) is in Part 04; it uses the same placement helper.

### 12.3 Which monsters (summary of the depth rule)

The rule itself is specified in Part 04; in short, for a depth d (clamped to 1..12) a kind
is drawn uniformly from a window of the monster table that slides deeper with d, the water
lord is never drawn, a genocided kind is replaced by the next kind in the table, and
finally, if the level currently in play is 15 or shallower, the result is replaced by a
lemming with probability 9 in 100. Because of the clamp, levels 12 to 20 all draw from the
same deepest window. Wild Edition monsters are never drawn (Part 12).

Depths used in this part: the level number for normal placement and volcano rooms;
(level + 1) for monster squares on pre-made maps; (level + 2), or (level + 4) at
difficulty 3 or more, for treasure rooms.

### 12.4 Sleeping, seen, and the stealth quirk

* Every monster placed by this module starts with both stealth bits clear.
* The stealth grid is a single working grid that is neither stored per level nor saved. On
  a first visit it is cleared; on a return visit it is not. Monsters on a level the player
  returns to therefore take whatever bits are lying on their squares from the level just
  left. Since bits are set on a square when a monster moves onto it or is seen there and
  are not cleared when it moves away, a level that has been played on for a while leaves
  many squares flagged, and monsters that were awake when the player left may be found
  asleep and vice versa. Newly added return-visit monsters have their own square's bits
  cleared. After loading a saved game the grid starts all clear: every monster on the
  current level is asleep and unseen until disturbed.
* A recreation that wants identical behaviour keeps one global grid as described. One that
  stores the flags per level changes how levels feel on return (monsters would stay awake).

### 12.5 The placement helper (put a monster on a random free square)

Given a monster kind: up to 10 times, draw x = rnd(65), y = rnd(15); if that square is empty
floor, has no monster and is not the player's current position, put the monster there with
full starting hit points and both stealth bits clear, and report success. After 10 failures
report failure. Monsters placed this way never start on an object, never on the border, and
the helper does not look at genocide (the sweep afterwards does).

"The player's current position" during a level change is still the position carried over
from the old level, so a monster can be placed on the stairs the player is about to arrive
on; the stairs code then moves the player to the nearest empty square.

### 12.6 The genocide sweep

After monsters are placed (first visit) or added (return visit), every square whose monster
kind is marked genocided has its monster removed. Hit points and stolen items on the square
are left as they are.

### 12.7 A stale-data quirk

Creating a level clears only the monster id of each square, and placing a monster sets only
its id, hit points and stealth. The stolen-item count of the square is not reset in either
case. If the player left the previous level while a thief holding stolen items stood on some
square, a monster later created on the same coordinates of a brand-new level inherits that
count and those item slots and will drop copies when killed. This is extremely rare; a
recreation may clear the count when a level is created.

## 13. What the player sees and remembers

There is no line-of-sight or lighting model. The map memory is the per-square "known" value
of section 2.2, and it is updated by the front end's map routines (Part 09), summarised
here because the data lives with the level:

* After every player move, all squares within 1 of the player (a 3 by 3 block) become
  known: their "known" value is set to the real object, and any monster on them gets the
  "seen" bit. With expanded awareness the block is 7 by 7 (within 3). When blind, only the
  player's own square is updated. Walls do not block this; the block is clamped to the map.
* A single square can be revealed on its own (used when a monster moves on a known square,
  when a door opens, and so on): its "known" value becomes the real object and its monster,
  if any, becomes seen. Nothing is revealed while blind.
* A known square keeps showing the object last seen there until it is revealed again, so
  the remembered map can be out of date.
* Whole-level reveals: the home level and wizard mode at creation (section 4.1), the magic
  mapping scroll and similar effects (Parts 06, 07). The potion of forgetfulness and the
  alter reality spell reset squares to unknown (Parts 06, 07).
* A fresh level starts entirely unknown, so the player sees only the 3 by 3 block around
  the arrival square.
* Wall squares are drawn with their stored shape mask even when only remembered.

## 14. Helpers of the module

| Helper | Behaviour |
|---|---|
| Allocate level storage | Allocates the 21 storage records at program start; failure ends the game with the "malloc failure" death. |
| Free level storage | Releases them at exit. |
| Is this square usable (x, y, check objects?, check monsters?) | False if (x, y) is off the map. False if the current level is 1 and the square is the exit (33, 16). False if the square is a wall or a closed door. Otherwise true, unless "check objects" is on and the square holds any object other than empty floor, or "check monsters" is on and a monster stands there. Used by item creation (objects checked) and by create-monster-next-to (monsters checked), and by other modules. |
| Drop gold | Section 11.4. |
| Put a monster on a random free square | Section 12.5. Returns success or failure. |
| Maze carver | Section 7.2. Also used by the alter reality spell. |
| Store current level | Copies the five stored fields into the current level's record and refreshes its checksum. Also called when saving the game. |
| Fetch current level | Copies the record back into the working grids with the checksum test of section 2.3. Also called at the end of loading a saved game. |
| Wall shapes (x1, y1, x2, y2) | Section 7.7, for the given rectangle clamped to the map. |
| Change level | Section 4. |
| Clamp coordinates, reporting | Clamps x to 0..66 and y to 0..16 in place and returns how many of the four limits had to be applied (zero means the coordinates were already on the map). |
| Clamp coordinates, silent | The same clamping without a result (a macro used by monster and spell code). |
| On the map? | True when x is 0..66 and y is 0..16; changes nothing. |
| Create item next to a square | Section 11.3. |
| Something | Section 11.2. |
| Pick a random object | Section 11.1. |
| "Forget" the object under the player | Sets both the object and the "known" value of the player's square to empty floor (used when the player picks up or destroys what they stand on). |
| "Disappear" a monster | Removes the monster from a square and, if the square is known, reveals that one square again so the map updates. |
| Write levels / read levels | Section 16. |
| Internal: load pre-made map | Section 8. |
| Internal: build one treasure room, choose treasure rooms | Section 7.5. |
| Internal: random free-square walk, place n, place with a chance | Section 9.1. |
| Internal: scatter objects | Sections 6, 9, 10. |
| Internal: place monsters | Sections 12.1, 12.2. |
| Internal: genocide sweep | Section 12.6. |
| Internal: build layout | Section 5. |

There is no distance helper in this module; the only distance computed for placement is the
squared distance inside the nearest-empty-square rule (section 4.2).

## 15. Messages

The module prints almost nothing. The complete list:

| Text | When |
|---|---|
| "OH NO!!!! INTERNAL MEMORY CORRUPTION!!!!" (preceded and followed by a newline) | A level record fails its checksum when fetched. |
| "(sum A of level L does not match saved sum B)" followed by a newline, where A is the recomputed checksum, L the level number and B the remembered checksum, all as plain decimal numbers | Printed right after the previous line; then a beep, a 5-second pause and the "internal complications" death. |
| "Failure in positionplayer" followed by a newline | Printed by the nearest-empty-square rule when the level has no empty square at all (in the player module). |

A read error on the map file is reported only on the process's error stream (the text
"fgets" plus the system error description), which the player never sees; the generator is
then used.

## 16. Levels in the save file

The levels section of a save file (its place in the whole file is in Part 03) is written as
follows, all in the machine's native byte order with no conversion:

1. The current level is first stored into its record (so the record is up to date).
2. The current level number as a 4-byte integer.
3. The 21 "visited" bytes, level 0 first.
4. For every level whose visited byte is non-zero, in level order, its storage record as one
   block of 36,450 bytes laid out exactly as in memory:

| Offset | Bytes | Content |
|---|---|---|
| 0 | 2,278 | Monster hit points, 16-bit each |
| 2,278 | 29,614 | Monster and stolen items: per square 26 bytes = monster id (1), stolen count (1), then 6 slots of 4 bytes (object id byte, one padding byte, 16-bit argument) |
| 31,892 | 1,139 | Objects, one byte each |
| 33,031 | 1 | padding |
| 33,032 | 2,278 | Object arguments, 16-bit each |
| 35,310 | 1,139 | Known values, one byte each |
| 36,449 | 1 | padding |

Within each grid the squares are ordered by x first and y second: the 17 squares of column
0 (y = 0..16), then column 1, and so on.

Reading does the reverse: level number, visited bytes, then one record per visited level.
After the rest of the file has been read and checked, the current level is fetched from its
record into the working grids. Checksums, the stealth grid and the two scratch grids are
not saved. A recreation that does not need to exchange save files with the original only
has to preserve the content, not this byte layout.

## 17. Checklist of quirks to keep

* A pre-made map is chosen 51% of the time (49% generator), and only the first 20 of the
  21 maps are ever used.
* Pre-made levels get no treasure rooms; generated levels 15 and 20 do not normally exist.
* Monster squares on pre-made maps are inert on levels 1 to 10 and live from level 11.
* The Eye exists only as the `~` square of whichever map is drawn for level 15, and the
  potion only as the `!` square of whichever map is drawn for level 20.
* The maze start square (1, 1) usually remains a wall.
* Every generated level has one full-width straight corridor.
* Volcano rooms are solid blocks of one monster kind.
* Treasure rooms on V4 hold monsters but no treasure; their seats stay empty; their door may
  sit on a corner.
* Studded leather armor and the ring of protection have a zero chance in the level scatter.
* Level 1 always has exactly one chest; gems never appear in the volcano.
* Level 15 has no stairs at all; V1's up staircase is a dead end; V3 to V5 have no down
  stairs but a guaranteed pit and trap door.
* Arriving through the dungeon entrance always puts the player on (33, 15) without checking
  what is there.
* Returning to any visited level adds level/2 + 1 sleeping monsters, however short the
  absence, and nothing else changes with time away.
* Awake and seen flags are not kept per level (section 12.4).
* Gold piles lose remainders when they are created as, or merged into, larger-unit piles.
