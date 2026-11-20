# Part 07 — Spells and spheres of annihilation

This part specifies everything the game does when the player casts a spell: the 39 spells and their data, the casting prompt, cost and failure rules, the shared helpers (adjacent-target strike, travelling projectile, burst around the player), the per-monster resistance table, every special spell, and the complete life cycle of spheres of annihilation. It also covers the two scroll effects that live in the same module (the pulverization ray and the annihilation burst), because they reuse the same helpers.

It leans on other parts for things it only calls: Part 02 (attributes, timers and what happens when a timer runs out, life protection), Part 03 (the known-spells screen, death screens, save file framing), Part 04 (the monster table, how monsters are damaged and killed, what the charm, scare, hold, cancel and invisibility timers do to monster behaviour, monster creation), Part 05 (maze carving, level re-entry, wall-shape analysis), Part 06 (books, the brass lamp, statues, thrones, altars, mirrors, the talisman of the sphere, the scrolls), Part 09 and Part 11 (the display and input contract), Part 12 (Wild Edition hooks) and Part 14 (tile art for the effect tiles named here).

## 1. Conventions used in this part

**Random helpers.** rnd(n) is a uniform integer from 1 to n inclusive. rund(n) is a uniform integer from 0 to n-1 inclusive. Both are a remainder of the C library generator. All divisions and halvings below are integer operations that discard the remainder.

**Map.** The map is 67 columns (x = 0..66) by 17 rows (y = 0..16). y grows downward (south). "On the map" means 0 <= x <= 66 and 0 <= y <= 16. In the dungeon and volcano the outer ring (x = 0, x = 66, y = 0, y = 16) is solid wall; on the home level it is ordinary ground.

**Levels.** Level 0 is the home (town) level, 1 to 15 are the dungeon, 16 to 20 are the volcano (V1 to V5). Several rules below say "walls cannot be destroyed below V2": the test is "level number less than 18", so wall destruction by spells works on levels 0 to 17 (home, dungeon 1 to 15, V1, V2) and never on levels 18, 19, 20 (V3, V4, V5).

**Directions.** Directions are numbered 0 to 8. The order matters (spheres store the number, projectiles use it to choose an animation tile, and "reverse" is a lookup).

| Number | Name printed | dx | dy | Reverse |
|---|---|---|---|---|
| 0 | None | 0 | 0 | 0 |
| 1 | South | 0 | +1 | 3 |
| 2 | East | +1 | 0 | 4 |
| 3 | North | 0 | -1 | 1 |
| 4 | West | -1 | 0 | 2 |
| 5 | Northeast | +1 | -1 | 8 |
| 6 | Northwest | -1 | -1 | 7 |
| 7 | Southeast | +1 | +1 | 6 |
| 8 | Southwest | -1 | +1 | 5 |

**Shorthand.** "clev" is the character's experience level. "Difficulty" is the game difficulty number (0 upward). INT, WIS, CHA, DEX, STR are the current attribute values. "Beep" is the front end's alert sound call. Messages are quoted exactly; `\n` inside a quoted message is a line break in the message window, `%s` and `%d` are slots filled in as described. Many messages deliberately start with two spaces and no line break, because they continue the line that holds the prompt and the echoed answer.

**Attribute loss.** "INT -1" means the attribute is lowered by one and then, if it is below 3, set to 3.

**Life protection.** The death routine returns instead of ending the game when the character has a charge of life protection (Part 02 and Part 03). Every "the player dies" step below is followed, in that case, by the stated "then" behaviour (usually the spell simply ends).

## 2. Spell data

There are exactly 39 spells, ids 0 to 38. The id order is fixed: it is used by the experience check, by book learning, by the resistance table and by the save file.

The source groups the spells into six "levels" (tiers). The tier has no mechanical effect of its own; what matters is the id. Two columns in the table are derived from rules given later: "Min clev" is the lowest experience level at which the spell does anything (section 4.3), and "Book levels" is the range of book level numbers (the level a book was generated on, 0 to 20) that can teach it (section 3.2).

| Id | Code | Name | Description shown to the player | Tier | Min clev | Book levels |
|---|---|---|---|---|---|---|
| 0 | pro | protection | Generates a +2 protection field | 1 | 1 | 0-3 |
| 1 | mle | magic missile | Creates and hurls a magic missile equivalent to a +1 magic arrow | 1 | 1 | 0-3 |
| 2 | dex | dexterity | Adds +2 to the caster's dexterity | 1 | 1 | 1-3 |
| 3 | sle | sleep | Causes some monsters to go to sleep | 1 | 1 | 1-3 |
| 4 | chm | charm monster | Some monsters may be awed at your magnificence | 1 | 1 | 1-3 |
| 5 | ssp | sonic spear | Causes the caster's hands to emit a screeching sound | 1 | 1 | 2-3 |
| 6 | web | web | Causes strands of sticky thread to entangle an enemy | 2 | 2 | 2-3 |
| 7 | str | strength | Adds +2 to the caster's strength for a time | 2 | 2 | 2-3 |
| 8 | enl | enlightenment | The caster becomes more aware of things around them | 2 | 2 | 3 |
| 9 | hel | healing | Restores some of the caster's health | 2 | 3 | 3 |
| 10 | cbl | cure blindness | Restores sight to one so unfortunate as to be blinded | 2 | 3 | 3-20 |
| 11 | cre | create monster | Creates a monster near to the caster | 2 | 3 | 3-20 |
| 12 | pha | phantasmal forces | Creates illusions which, if believed, cause monsters to die | 2 | 4 | 4-20 |
| 13 | inv | invisibility | The caster becomes invisible | 2 | 4 | 4-20 |
| 14 | bal | fireball | Creates a ball of fire that burns whatever it hits | 3 | 4 | 4-20 |
| 15 | cld | cold | Sends forth a cone of cold which freezes whatever it touches | 3 | 5 | 4-20 |
| 16 | ply | polymorph | You can find out what this does for yourself | 3 | 5 | 5-20 |
| 17 | can | cancellation | Stops a monster from using its special abilities | 3 | 5 | 5-20 |
| 18 | has | haste self | Speeds up the caster's movements | 3 | 6 | 5-20 |
| 19 | ckl | cloud kill | Creates a fog of poisonous gas which kills all that is within it | 3 | 6 | 5-20 |
| 20 | vpr | vaporize rock | Changes rock to air | 3 | 6 | 5-20 |
| 21 | dry | dehydration | Dries up water in the immediate vicinity | 4 | 7 | 6-20 |
| 22 | lit | lightning | Causes the caster's finger to emit lightning bolts | 4 | 7 | 6-20 |
| 23 | drl | drain life | Subtracts hit points from both you and a monster | 4 | 7 | 6-20 |
| 24 | glo | invulnerability | This globe helps to protect the player from physical attack | 4 | 8 | 6-20 |
| 25 | flo | flood | Creates a deluge of water, flooding the immediate chamber | 4 | 8 | 7-20 |
| 26 | fgr | finger of death | A holy spell calling on your god to back you up | 4 | 8 | 7-20 |
| 27 | sca | scare monster | Terrifies the monster so that it may not hit the caster | 5 | 9 | 7-20 |
| 28 | hld | hold monster | Freezes monsters in their tracks | 5 | 9 | 7-20 |
| 29 | stp | time stop | All movement in the caverns ceases for a limited duration | 5 | 9 | 8-20 |
| 30 | tel | teleport away | Teleports a monster | 5 | 10 | 8-20 |
| 31 | mfi | magic fire | Creates a curtain of fire around the caster | 5 | 10 | 9-20 |
| 32 | mkw | make a wall | Makes a wall in the specified place | 6 | 10 | 9-20 |
| 33 | sph | sphere of annihilation | Anything caught in this sphere is instantly killed. | 6 | 11 | 10-20 |
| 34 | gen | genocide | Eliminates a species of monster from the game | 6 | 11 | 11-20 |
| 35 | sum | summon demon | Summons a demon who may help you out | 6 | 11 | 12-20 |
| 36 | wtw | walk through walls | Allows the player to walk through walls for a short period of time | 6 | 12 | 13-20 |
| 37 | alt | alter reality | God only knows what this will do | 6 | 12 | 14-20 |
| 38 | per | permanence | Makes a character spell permanent, e.g. protection, strength, etc. | 6 | 12 | 15-20 |

Notes on the text: only the description of spell 33 ends with a full stop. The descriptions of spells 2 and 7 say "+2" but the coded boost is 3 for both. The names are lower case exactly as shown.

## 3. Learning spells and the known-spell counters

### 3.1 The counters

The game keeps one integer per spell (39 integers). A spell is "known" when its counter is not zero. Learning adds one to the counter, so reading the same spell twice gives a count of two. Three spells consume a count each time they are cast (genocide, alter reality and permanence, section 7); with a count of two they can be cast twice before they are forgotten. For every other spell the size of the count is irrelevant.

All counters are zero at character creation, before class spells are granted. The 39 integers are written to the save file as one block inside the player record (Part 03). The front end receives, for each spell, a known or not-known flag and its code so that it can offer a spell picker (Part 09).

### 3.2 Sources of spells

| Source | Effect on counters | Owner |
|---|---|---|
| Character class | Counter of each starting spell set to 1: Ogre mle; Wizard mle and chm; Klingon ssp; Elf mle; Rogue mle; Adventurer pro and mle; Dwarf pro; Rambo mle | Part 02 |
| Reading a book | One spell id is chosen as below and its counter is increased by 1 | Part 06 (the item), this part (the choice) |
| Brass lamp genie | The player types any three-letter code with the same code-entry routine as casting (prompt `\n  What spell would you like? : `); the counter of the matching spell is increased by 1, whether or not it was already known | Part 06 |
| Reaching experience level 100 | Every counter is set to exactly 1 | Part 02 |
| Becoming the wizard (debug mode) | Every counter is set to exactly 1 | Part 01 |

The college (Part 08) does not teach spells; its courses only raise attributes, refill spell points and advance time.

**Book choice.** A book carries a level number L (0 to 20). The "book ceiling" table, indexed by L = 0 to 20, is: 1, 4, 7, 11, 15, 20, 24, 28, 30, 32, 33, 34, 35, 36, 37, 38, 38, 38, 38, 38, 38.

1. If L is 3 or less: the spell id is uniform from 0 to ceiling(L) inclusive, that is rund(ceiling + 1). So level 0 books teach id 0 or 1, level 1 books ids 0 to 4, level 2 books ids 0 to 7, level 3 books ids 0 to 11.
2. If L is 4 or more (values above 20 are treated as 20): the spell id is rnd(ceiling(L) - 9) + 9, that is uniform from 10 to ceiling(L) inclusive. So deeper books never teach ids 0 to 9.
3. The counter of that spell is increased by one and the game prints `\nSpell "%s":  %s\n%s.` filled with the code, the name and the description (two spaces after the colon, a full stop appended).
4. With probability 1 in 10 (rnd(10) equal to 4) it also prints `\nYou feel clever!` and raises INT by one.

The genie prints the same announcement without the trailing full stop.

## 4. Casting

### 4.1 Command and time cost

Casting is the "cast spell" command (key `c` in this port). The command always uses up the player's turn, including when the player has no spell points, aborts the prompt with ESC, or the spell fails: after the command returns, the main loop runs regeneration and the monsters' move exactly as for any other time-consuming action (Part 01). Casting is not blocked while time is stopped, but the spell then fails (section 4.3).

Some spells take additional game turns or real time as described with each helper (a spell reflected by a mirror can put the caster to sleep for several monster turns; animations pause the game for fixed numbers of milliseconds).

### 4.2 Prompt flow

1. If the character's current spell points are zero or less, print `\nYou don't have any spells!` and stop.
2. Print the prompt `\nEnter your spell [D for list, ESC to abort]: `.
3. Read keys one at a time. Only lower-case letters a to z, capital D and ESC are accepted; any other key is ignored.
   - Capital D as the first character shows the list of known spells (below) and then prints the prompt again; nothing is stored.
   - ESC at any point prints `aborted.` and ends the command (no spell point is spent).
   - Any other accepted key is echoed and stored. There is no backspace. A capital D typed as the second or third character is stored like a letter (and can then never match).
   - After three stored characters the code is complete.
4. Search the spell codes in id order for an exact three-letter match whose spell is known.
   - Found: print ` (%s)` with the spell name, take one spell point, then perform the spell (section 4.3). Wild Edition hook: one perk can make the cast free and prints ` (free!)` first, and the cast is reported to the app as an event; see Part 12.
   - Not found (no such code, or the spell is not known): print ` Unknown spell! Try again.`
5. Recalculate derived statistics and refresh the status and effects panels.
6. If no spell was cast in step 4, go back to step 2. The loop ends only with a cast or with ESC.

Every spell costs exactly one spell point, whatever its id.

The list shown by D uses the text screen: it clears it, prints `The magic spells you have discovered to date are:` followed by a blank line, then each known spell in id order as the code, a space, the name padded to 20 characters and a space, three entries per line, with a "more" pause after 18 lines. It ends with a "more" pause and returns to the map (Part 03 owns the screen).

### 4.3 Checks made after the spell point is spent

These run in this order; each failure ends the cast with the spell point already lost.

1. **Time stopped.** If the time-stop counter is non-zero: `\n  It didn't seem to work.`
2. **Random failure.** If rnd(23) equals 7, or else if rnd(18) is greater than INT: `\n  It didn't work!` The second draw is only made when the first does not fail. The chance of failure is therefore 1/23 when INT is 18 or more, and 1 - (22/23) x (INT/18) when INT is below 18 (for example about 36% at INT 12, about 84% at INT 3). Spell id, experience level and difficulty do not enter the formula.
3. **Inexperience.** If clev x 3 + 2 is less than the spell id: `\n  Nothing happens.  You seem inexperienced.` (two spaces after the full stop). This gives the "Min clev" column of section 2.
4. Otherwise the spell's own effect runs (section 7). A spell id with no effect defined would print `  spell %d not available!` with the id and beep; with 39 defined spells this cannot happen.

There is no separate "fizzle" beyond these three messages; individual spells add their own failure rolls (phantasmal forces, finger of death, summon demon, sphere of annihilation).

## 5. Shared building blocks

### 5.1 Confusion check

Every aimed helper starts by checking the confusion timer. If the character is confused the game prints ` You can't aim your magic!` (one leading space), beeps, and the spell does nothing more. The check is made by the adjacent strike helper, the projectile helper and the polymorph, teleport-away and make-wall routines, so it applies to mle, sle, ssp, web, pha, bal, cld, ply, dry, lit, drl, fgr, tel, mkw, sum and the pulverization scroll. For pha, fgr and sum it is reached only after the spell's own success roll has passed. The sphere of annihilation asks for a direction without this check. Wild Edition hook: a worn hat can clear confusion just before the check; see Part 12.

### 5.2 Direction prompt

The direction routine prints `\nIn What Direction? ` and waits for one of the eight movement inputs. It then prints the direction's name from the table in section 1 (for example `North`) and returns the direction number (1 to 8) together with the map cell adjacent to the player in that direction.

In this port the input routine does not return until one of the eight directions is given: there is no way to answer "none" or to cancel, and direction 0 is never produced. (The sphere code nevertheless treats a stored direction of 9 or more as 0, "no movement".)

The adjacent cell can be off the map when the player stands on the map edge (possible on the home level and at the level 1 exit). Each helper says what it does then; in general the spell silently does nothing.

### 5.3 Animation primitives

The front end offers two drawing calls for spell effects and a millisecond sleep. Both drawing calls draw nothing while the character is blind and ignore cells off the map. Each call publishes the frame immediately; the sleep also publishes before waiting.

**Directional effect** (used by projectiles): draw effect E travelling in direction D at one cell. There are five effects, in this order: 0 magic missile, 1 sonic spear, 2 fireball, 3 lightning, 4 cone of cold. The cell's tile is replaced by the effect tile for (E, D) and any overlay on that cell is removed. In this port the tile ids are, for directions 1 to 8 in order: magic missile 198, 196, 194, 192, 195, 193, 197, 199; sonic spear 206, 204, 202, 200, 203, 201, 205, 207; fireball 214, 212, 210, 208, 211, 209, 213, 215; lightning 222, 220, 218, 216, 219, 217, 221, 223; cone of cold 230, 228, 226, 224, 227, 225, 229, 231. Direction 0 maps to tile 191 for all five (never used). Part 14 describes the art.

**Stationary magic effect**: a short animation at one cell. There are fifteen, in this order:

| No. | Effect | Frames | Drawing | Tile ids by frame |
|---|---|---|---|---|
| 0 | sparkle | 8 | overlay on top of the cell's normal tile | 240 to 247 |
| 1 | sleep | 6 | replaces the cell's tile | 256, 272, 288, 304, 320, 336 |
| 2 | web | 6 | replaces | 257, 273, 289, 305, 321, 337 |
| 3 | phantasmal | 6 | replaces | 258, 274, ... (each frame +16) |
| 4 | cloud | 6 | replaces | 259, 275, ... |
| 5 | vaporize | 6 | replaces | 260, 276, ... |
| 6 | dehydrate | 6 | replaces | 261, 277, ... |
| 7 | drain | 6 | replaces | 262, 278, ... |
| 8 | flood | 6 | replaces | 263, 279, ... |
| 9 | finger | 6 | replaces | 264, 280, ... |
| 10 | teleport | 6 | replaces | 265, 281, ... |
| 11 | fire | 6 | replaces | 266, 282, ... |
| 12 | wall | 6 | replaces | 267, 283, ... |
| 13 | demon | 6 | replaces | 268, 284, ... |
| 14 | annihilate | 6 | replaces | 269, 285, ... |

For effects 1 to 14 the tile of frame f (0 to 5) is 256 + 16 x f + (effect number - 1).

**Single-cell animation** ("play effect X at a cell"): for each frame in order, draw the frame at the cell and then sleep 75 ms. Total 600 ms for the sparkle and 450 ms for the others. The sleeps happen even when the character is blind. When this part says "sparkle on the player", it means this animation with effect 0 at the player's cell; nothing is redrawn afterwards by the spell itself (the normal end-of-turn redraw restores the cell).

**Multi-cell animation** (bursts and vaporize rock): for each frame, draw that frame on every affected cell, then sleep 75 ms once.

### 5.4 Resistance lookup

A table gives, for every monster and every spell, a reaction number from 0 to 18. 0 means the spell affects the monster. Any other number means the monster is immune: the game prints a line break and then the reaction message with the monster's real name filled in (the real name is used even when the character is blind), and the helper skips its effect. The full table is in section 6.

Whenever a monster resists, its cell is recorded as "the last monster the player hit". That marker makes the monster take its move even if it is outside the normal movement window (Part 04).

| No. | Message |
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

### 5.5 Damaging a monster with a spell

All spell damage goes through the general "damage the monster at this cell by N" routine with its "this is a spell" flag set. Part 04 owns it; the rules that matter here are:

1. If the character is under the half-damage curse, N is halved. If the result is zero or less, it becomes 1.
2. The monster is woken, and its cell becomes "the last monster hit".
3. If hold monster is active its counter is set to 1, so it ends at the next regeneration tick. Any damaging spell therefore breaks hold monster.
4. If the character carries the dragon-slaying bonus and the monster is a white, red, green, bronze, platinum or silver dragon, N is tripled.
5. Weapon effects (vorpal beheading, the lance-of-death cap against demons, the slayer sword) are skipped for spells.
6. If the monster's hit points are less than or equal to N it dies: `\nThe %s died!`, experience is awarded, and it drops its carried items, gold and loot as for any kill. The routine returns the hit points the monster had.
7. Otherwise its hit points are reduced by N and the routine returns the original, unhalved damage figure (or 1 in the case of rule 1's floor).

The name used in messages is the "current monster name", which the helpers set just before: the monster's real name, or the word `monster` when the character is blind.

### 5.6 Adjacent strike helper

Used by sle, web, pha, dry, drl, fgr and sum. Inputs: the spell id, a damage amount, a message, a number to show in the message, and a stationary effect.

1. Confusion check (5.1).
2. Direction prompt (5.2). If the target cell is off the map, stop silently.
3. **Mirror.** If the object on the target cell is a mirror (checked before looking for a monster):
   - sleep: print `You fall asleep! `, beep, then for (shown number + 2) iterations run one full "monsters' turn" (monsters move, twice if they are hasted; a random monster may appear; regeneration runs) followed by a 1000 ms sleep. The shown number is 2 to 4, so the caster sleeps 4 to 6 turns.
   - web: print `You get stuck in your own web! `, beep, then the same loop for (shown number + 2) iterations; the shown number is 3 to 5, so 5 to 7 turns.
   - any other spell: print the spell's message with `spell caster (that's you)` in place of the monster name (for example `  The spell caster (that's you) believed!`), beep, and the player loses the damage amount in hit points; if that kills, the cause of death is "hit by his own magic".
   - Stop.
4. If there is no monster on the target cell: print `  There wasn't anything there!` and stop.
5. Set the current monster name for that cell (5.5).
6. Play the spell's stationary effect at the target cell, then redraw that cell.
7. Resistance lookup (5.4). If immune, stop.
8. Print the message, filling in the monster name and the shown number. There is no line break before it.
9. Damage the monster by the damage amount (5.5).

### 5.7 Projectile helper

Used by mle, ssp, bal, cld, lit and by the scroll of pulverization. Inputs: the spell id (for resistance), a starting energy E, a message with one name slot, a per-cell delay in milliseconds, and a directional effect.

1. Confusion check (5.1).
2. Direction prompt (5.2). Remember the direction number D and the step (dx, dy). The position starts at the player's cell.
3. Repeat while E is greater than zero:
   1. Advance the position one step. If it is off the map, stop.
   2. If the position is the player's cell (only possible after a reflection): print `\nYou are hit by your own magic!`, beep, the player loses E hit points (cause of death "hit by his own magic"), and the spell ends.
   3. If the character is not blind: draw the directional effect for D at the cell, sleep for the per-cell delay, redraw the cell normally. When blind there is neither drawing nor delay.
   4. **Monster on the cell.** Set the current monster name. Then:
      - If the monster is the God of Hellfire, or it is any demon lord or the demon prince and rnd(100) is less than 10 (a 9% chance): the missile turns back. The step is negated (the stored direction number is not, so the animation tile keeps its original orientation) and the game prints a blank line and `\nthe %s returns your puny missile!` with the monster's real name. No damage is done to the monster.
      - Otherwise do the resistance lookup (5.4). If immune, the spell ends at once.
      - Otherwise print a line break and the message with the monster name. Damage the monster by E (5.5) and subtract the routine's return value from E. Redraw the cell, sleep 1000 ms, and move the position one step back (so that the same cell is visited again if any energy remains).
      - Consequence: a monster that survives absorbs all the energy and the missile stops. A monster that dies absorbs only its remaining hit points; the missile then re-enters that cell (now without the monster) and carries on.
   5. **No monster on the cell.** Look at the object there:
      - **Wall**: print a line break and the message with the word `wall` (for example `  The fireball hits the wall.`). If E is at least 50 + difficulty, and the level is below 18 (section 1), and the cell is not on the outer ring of the map: print `  The wall crumbles.`, the cell becomes empty floor, it is redrawn, and the wall shapes of the eight neighbours are recomputed and redrawn where already seen. In every case E becomes 0.
      - **Closed door**: line break, message with `door`. If E is at least 40: `  The door is blasted apart.` and the cell becomes empty floor (no door remains). E becomes 0.
      - **Statue**: line break, message with `statue`. If E is greater than 44 and difficulty is 3 or less and rnd(60) is less than 30 (29 chances in 60): `  The statue crumbles.` and the statue is replaced by a book whose level number is the current level. E becomes 0.
      - **Throne** (only the pristine kind, not one that has already produced its king and not a dead throne): line break, message with `throne`. If E is greater than 33: a gnome king with full hit points appears on the throne cell and the throne changes to the "king already summoned" kind. Nothing else is printed. E becomes 0.
      - **Mirror**: the step is negated and D is replaced by its reverse. Nothing is printed and E is not zeroed; the missile travels back toward the caster.
      - **Anything else** (floor, open door, items, traps, stairs, fountains, altars, spheres and so on): the missile passes over it unchanged.
   6. Subtract 3 + difficulty/2 from E.

The missile therefore loses 3 + difficulty/2 energy per cell travelled, hits a monster at distance d with E minus (d - 1) x (3 + difficulty/2), and in open ground covers E divided by (3 + difficulty/2) cells, rounded up. A missile that kills pays the per-cell cost twice on the victim's cell.

| Spell | Starting energy | Message | Delay per cell | Effect |
|---|---|---|---|---|
| mle | rnd(2 x (clev + 1)) + clev + 3 | `  Your missile hit the %s.` at clev 1, `  Your missiles hit the %s.` at clev 2 or more | 100 ms | magic missile |
| ssp | rnd(10) + 15 + clev | `  The sound damages the %s.` | 70 ms | sonic spear |
| bal | rnd(25 + clev) + 25 + clev | `  The fireball hits the %s.` | 40 ms | fireball |
| cld | rnd(25) + 20 + clev | `  The cone of cold strikes the %s.` | 60 ms | cone of cold |
| lit | rnd(25) + 20 + 2 x clev | `  A lightning bolt hits the %s.` | 10 ms | lightning |
| scroll of pulverization | 150 | `The ray hits the %s.` (no leading spaces) | 10 ms | lightning; resistance column of lit |

Despite the names, the fireball and the cone are single-file missiles like the others: there is no area of effect.

### 5.8 Burst helper

Used by ckl, flo and mfi. Inputs: spell id, damage, message, stationary effect. There is no confusion check, no direction prompt and no mirror rule.

1. The area is the 3 by 3 block centred on the player, clipped to the map.
2. Multi-cell animation (5.3) of the effect on every cell of the area except the player's own.
3. Redraw each of those cells.
4. For each cell of the area, column by column from west to east and within a column from north to south: if a monster is there, do the resistance lookup. If it is not immune: set the current monster name, print a line break and the message with the name, damage the monster by the full damage amount (5.5), and sleep 800 ms. Every monster in the area takes the full amount; nothing is shared or reduced.

## 6. The resistance table

The stored table has one row per monster id (70 rows, including four all-zero rows for the Wild Edition monsters, which resist nothing) and one column per spell id. Only seventeen columns are ever consulted, because only the helpers in section 5 and the two aimed special spells look the table up: mle, ssp, bal, cld, lit (projectiles), sle, web, pha, dry, drl, fgr, sum (adjacent strike), ckl, flo, mfi (bursts), tel and ply.

### 6.1 Live entries, by spell

Every monster not listed for a spell is affected normally. "Demon lords I-VII" means all seven types.

| Spell | Reaction no. | Immune monsters |
|---|---|---|
| mle | 6 | hell hound, silver dragon, red dragon |
| mle | 7 | troll, rothe, xorn, umber hulk, gnome king, bronze dragon, green dragon, platinum dragon |
| mle | 13 | gelatinous cube, metamorph, vortex, wraith, poltergeist, water lord, xvart, spirit naga, demon lords I-VII, demon prince, God of Hellfire |
| sle | 8 | floating eye, zombie, gelatinous cube, violet fungus, wraith, xorn, poltergeist, disenchantress, yellow mold, water lord, spirit naga, demon lords I-VII, demon prince, God of Hellfire |
| sle | 9 | silver dragon, platinum dragon |
| ssp | 4 | zombie, wraith, vampire, poltergeist, water lord, spirit naga |
| ssp | 10 | floating eye, gelatinous cube, vortex, shambling mound, yellow mold, demon lords I-VII, demon prince, God of Hellfire |
| web | 1 | vortex, invisible stalker, poltergeist, yellow mold, water lord, spirit naga, demon lords I-VII, demon prince, God of Hellfire |
| web | 2 | gelatinous cube, metamorph, vampire, mimic |
| web | 11 | ice lizard, green dragon, platinum dragon |
| web | 12 | hell hound, silver dragon, red dragon |
| pha | 4 | zombie |
| pha | 14 | white dragon, elf, wraith, vampire, spirit naga, platinum dragon, demon lords I-VII, demon prince, God of Hellfire |
| bal | 18 | God of Hellfire (never shown: he always reflects projectiles first) |
| cld | 4 | poltergeist, spirit naga |
| cld | 15 | ice lizard, yeti, white dragon |
| ply | 4 | demon lords VI and VII, demon prince |
| ply | 9 | gnome king, water lord, spirit naga, silver dragon, platinum dragon, demon lords I-V, God of Hellfire |
| ckl | 4 | zombie, gelatinous cube, metamorph, vortex, wraith, vampire, poltergeist, yellow mold, water lord, spirit naga, demon lords I-VII, demon prince, God of Hellfire |
| dry | 4 | zombie, troll, white dragon, metamorph, vortex, wraith, xorn, poltergeist, spirit naga, demon lords I-VII, demon prince, God of Hellfire |
| lit | 18 | God of Hellfire (never shown, as for bal) |
| drl | 4 | zombie, wraith, vampire, poltergeist, spirit naga, demon prince, God of Hellfire |
| drl | 9 | demon lords I-V (types VI and VII are not immune) |
| flo | 4 | vortex, xorn, poltergeist, xvart, spirit naga, demon lords I-VII, demon prince (the God of Hellfire is not immune) |
| flo | 16 | water lord |
| fgr | 4 | floating eye, zombie, gelatinous cube, metamorph, vortex, violet fungus, wraith, xorn, vampire, poltergeist, yellow mold, water lord, xvart, spirit naga, demon lords I-VII, demon prince, God of Hellfire |
| tel | 9 | demon lords I-VII, demon prince, God of Hellfire |
| mfi | 4 | demon lords I-VII, demon prince, God of Hellfire |
| sum | none | nobody is immune, including the God of Hellfire |

### 6.2 The same data, by monster

Monster ids follow Part 04's table (this build has four Wild Edition monsters at ids 57 to 60, so the demon lords are 61 to 67, the demon prince 68 and the God of Hellfire 69). Monsters not listed have no entries at all.

| Id | Monster | Immunities (spell: reaction no.) |
|---|---|---|
| 12 | floating eye | sle 8, ssp 10, fgr 4 |
| 17 | zombie | sle 8, ssp 4, pha 4, ckl 4, dry 4, drl 4, fgr 4 |
| 20 | hell hound | mle 6, web 12 |
| 21 | ice lizard | web 11, cld 15 |
| 23 | troll | mle 7, dry 4 |
| 24 | yeti | cld 15 |
| 25 | white dragon | pha 14, cld 15, dry 4 |
| 26 | elf | pha 14 |
| 27 | gelatinous cube | mle 13, sle 8, ssp 10, web 2, ckl 4, fgr 4 |
| 28 | metamorph | mle 13, web 2, ckl 4, dry 4, fgr 4 |
| 29 | vortex | mle 13, ssp 10, web 1, ckl 4, dry 4, flo 4, fgr 4 |
| 31 | violet fungus | sle 8, fgr 4 |
| 32 | wraith | mle 13, sle 8, ssp 4, pha 14, ckl 4, dry 4, drl 4, fgr 4 |
| 36 | rothe | mle 7 |
| 37 | xorn | mle 7, sle 8, dry 4, flo 4, fgr 4 |
| 38 | vampire | ssp 4, web 2, pha 14, ckl 4, drl 4, fgr 4 |
| 39 | invisible stalker | web 1 |
| 40 | poltergeist | mle 13, sle 8, ssp 4, web 1, cld 4, ckl 4, dry 4, drl 4, flo 4, fgr 4 |
| 41 | disenchantress | sle 8 |
| 42 | shambling mound | ssp 10 |
| 43 | yellow mold | sle 8, ssp 10, web 1, ckl 4, fgr 4 |
| 44 | umber hulk | mle 7 |
| 45 | gnome king | mle 7, ply 9 |
| 46 | mimic | web 2 |
| 47 | water lord | mle 13, sle 8, ssp 4, web 1, ply 9, ckl 4, flo 16, fgr 4 |
| 48 | bronze dragon | mle 7 |
| 49 | green dragon | mle 7, web 11 |
| 51 | xvart | mle 13, flo 4, fgr 4 |
| 52 | spirit naga | mle 13, sle 8, ssp 4, web 1, pha 14, cld 4, ply 9, ckl 4, dry 4, drl 4, flo 4, fgr 4 |
| 53 | silver dragon | mle 6, sle 9, web 12, ply 9 |
| 54 | platinum dragon | mle 7, sle 9, web 11, pha 14, ply 9 |
| 56 | red dragon | mle 6, web 12 |
| 61-65 | type I to V demon lords | mle 13, sle 8, ssp 10, web 1, pha 14, ply 9, ckl 4, dry 4, drl 9, flo 4, fgr 4, tel 9, mfi 4 |
| 66-67 | type VI and VII demon lords | mle 13, sle 8, ssp 10, web 1, pha 14, ply 4, ckl 4, dry 4, flo 4, fgr 4, tel 9, mfi 4 |
| 68 | demon prince | mle 13, sle 8, ssp 10, web 1, pha 14, ply 4, ckl 4, dry 4, drl 4, flo 4, fgr 4, tel 9, mfi 4 |
| 69 | God of Hellfire | mle 13, sle 8, ssp 10, web 1, pha 14, bal 18, ply 9, ckl 4, dry 4, lit 18, drl 4, fgr 4, tel 9, mfi 4 |

### 6.3 Dead entries

The stored table also holds values in columns that no code path reads. They have no effect on play and reaction messages 3, 5 and 17 are consequently never printed. They are listed so the table can be reproduced faithfully if wanted:

- chm column, reaction 3: gnome king, water lord, spirit naga, demon lords I-VII, demon prince, God of Hellfire.
- inv column, reaction 5: gnome, kobold, bugbear, elf, forvalaka, rothe, xorn, umber hulk, gnome king, spirit naga, demon lords I-VII, demon prince, God of Hellfire.
- sca column: reaction 3 for demon lords I-VII and the demon prince, reaction 4 for the God of Hellfire.
- hld column, reaction 4: God of Hellfire.
- mkw column, reaction 9: demon lords I-VII, demon prince, God of Hellfire.
- gen column, reaction 17: God of Hellfire.

Each stored row has only 38 values, so the last column (per) is zero everywhere, and the last six values of each row sit one column to the left of where the source's column headings put them. That is why the "resists" entry lands under make-wall rather than sphere, and "the demon is terrified" lands under genocide rather than summon demon. The observable result is that summon demon works on the God of Hellfire. How charm, invisibility, scare and hold actually treat particular monsters is decided in the monster code (Part 04), not by this table.

## 7. The spells one by one

Each entry describes what happens after the checks of section 4.3 pass. "Timer" means one of the character's timed-effect counters; all of them tick down by one per regeneration tick and act when they reach zero (Part 02). A timer is always increased by the stated amount, so recasting extends it.

### 0 pro: protection

If the protection timer is zero, the extra-defence bonus rises by 2. The timer increases by 250. Sparkle on the player. When the timer runs out the bonus drops by 2.

### 1 mle: magic missile

Projectile (5.7) with the mle row of the table there.

### 2 dex: dexterity

If the dexterity timer is zero, DEX rises by 3. The timer increases by 400. Sparkle on the player.

As coded, when the timer runs out DEX is raised by 3 again instead of being lowered. The net result of one full cast is a permanent +6 DEX (+3 immediately, +3 at expiry); casts made while the timer is running only extend it.

### 3 sle: sleep

Let n = rnd(3) + 1 (2 to 4). Adjacent strike (5.6) with damage "full hit of n", message `  While the %s slept, you smashed it %d times.` (n is the number shown) and the sleep effect.

"Full hit of n" is Part 04's routine: 10000 if the lance of death is wielded; otherwise n x (weapon class/2 + STR + extra strength - difficulty - 12 + extra damage), and if that is less than 1 the result is n. The monster is not actually put to sleep; the spell is n automatic blows.

### 4 chm: charm monster

The charm timer increases by 2 x CHA. Sparkle on the player. While it runs, attacking monsters other than demons and the platinum dragon may be "awestruck" and skip their attack (Part 04).

### 5 ssp: sonic spear

Projectile (5.7), ssp row.

### 6 web: web

Let n = rnd(3) + 2 (3 to 5). Adjacent strike with damage "full hit of n" (see sle), message `  While the %s is entangled, you hit it %d times.` and the web effect.

### 7 str: strength

If the strength timer is zero, extra strength rises by 3. The timer increases by 150 + rnd(100). Sparkle on the player. At expiry extra strength drops by 3.

### 8 enl: enlightenment

The rectangle from 15 columns west to 16 columns east of the player and from 5 rows north to 6 rows south (32 by 12 cells), with each edge clipped to the map, becomes known: for every cell in it the remembered object is set to the real object. The map is then repainted. No animation and no message. Monsters are not revealed by this (only the object layer is copied).

### 9 hel: healing

Hit points rise by 20 + 2 x clev, not beyond the maximum. Sparkle on the player.

### 10 cbl: cure blindness

The blindness timer is set to zero directly (so the usual "blindness lifts" message is not printed). Sparkle on the player, which is visible because blindness has just ended.

### 11 cre: create monster

In wizard mode only, the game first prints `Number: ` and reads a number; if one is entered, the monster with that id is created and the spell ends.

Otherwise the game asks Part 04's "random monster for a dungeon level" routine for a monster suited to (current level + 1), adds 8 to the returned monster id, and creates that monster. Creation (Part 04) places it on one of the eight cells around the player, starting from a random direction and trying each in turn, on the first cell that is on the map, is not a wall, a closed door or the level 1 exit, and holds no monster; if none qualifies nothing appears. There is no message and no animation.

Consequences of the "+8": the result is always 8 places later in the monster table than a normal wandering monster for the next level. The random routine's "lemming instead" rule (9% of the time on levels 0 to 15) therefore yields a jaculi (id 9). On deep levels the sum reaches beyond the red dragon: in this build ids 57 to 60 are the four Wild Edition monsters and 61 to 64 are demon lords I to IV, so the spell can create those (in the unmodified base game the same arithmetic gives demon lords I to VII and the demon prince). If the random routine has stepped past genocided species all the way to the first demon lord, the sum is the God of Hellfire's id, which creation accepts.

### 12 pha: phantasmal forces

If rnd(11) + 7 is less than or equal to WIS (never below WIS 8, always at WIS 18 or more): adjacent strike with damage rnd(20) + 20 + clev, message `  The %s believed!`, phantasmal effect. Otherwise print `  It didn't believe the illusions!` without asking for a direction.

### 13 inv: invisibility

Let j be the sum, over every inventory slot holding an amulet of invisibility, of (1 + that amulet's enchantment). The invisibility timer increases by 128 x j + 12 (12 with no amulet). Sparkle on the player. While it runs, monsters without infravision often miss (Part 04).

### 14 bal: fireball

Projectile (5.7), bal row.

### 15 cld: cold

Projectile (5.7), cld row.

### 16 ply: polymorph

1. Confusion check, direction prompt; stop silently if the target is off the map.
2. No monster there: `  There wasn't anything there!`
3. Set the current monster name; resistance lookup (immune monsters in section 6).
4. Play the sparkle at the target cell.
5. Pick a new monster id as rnd(first demon lord's id + 7), which in this build is 1 to 68: any monster from the lemming up to and including the demon prince, never the God of Hellfire. Redraw the pick while the species is genocided or is a Wild Edition monster (ids 57 to 60). The pick may equal the old species.
6. The monster on the cell becomes that species with that species' full starting hit points. Whatever it was carrying stays with it. Redraw the cell. No message.

There is no mirror rule.

### 17 can: cancellation

The cancellation timer increases by 5 + clev. No animation. While it runs monsters' special attacks are suppressed (Part 04; only 6% effective against the demon prince and the God of Hellfire), and any sphere of annihilation that moves explodes (section 9.5).

### 18 has: haste self

The haste timer increases by 7 + clev. Sparkle on the player. While it runs, monsters and spheres act only on turns where the timer value is odd, and game time advances every second turn (Part 02, Part 04).

### 19 ckl: cloud kill

Burst (5.8) with damage 30 + rnd(10), message `  The %s gasps for air!`, cloud effect.

### 20 vpr: vaporize rock

The area is the 3 by 3 block centred on the player, clipped so that it never includes the outer ring of the map (columns 1 to 65, rows 1 to 15). No direction, no confusion check, no resistance lookup.

1. Multi-cell animation of the vaporize effect on those cells of the area that hold a wall (only when the level is below 18), a statue, a pristine throne or an altar, or on which a xorn or a troll stands.
2. Then each cell is processed, columns west to east and rows north to south within a column:
   - **Wall**: on levels below 18 it becomes empty floor; on levels 18 to 20 nothing happens.
   - **Statue**: if difficulty is greater than 3 and rnd(60) is less than 30 (29 in 60), the statue survives and is merely redrawn. Otherwise it becomes a book whose level number is the current level. (At difficulty 3 or less it always becomes a book.)
   - **Pristine throne**: it changes to the "king already summoned" kind. As coded, no gnome king is actually placed: the routine only sets the hit-point value stored for that cell to the gnome king's starting hit points. If some monster happens to stand on the throne, that monster ends up with that many hit points, and it is also spared the xorn/troll damage below.
   - **Altar**: the altar stays. The hit-point value stored for that cell is set to the demon prince's starting hit points (with the same side effect on a monster standing there), and four demon princes are created next to the player, one after another, by the creation routine described under spell 11.
   - **Xorn or troll on the cell** (and the cell is not a throne or altar): set the current monster name and damage it by 200 (5.5).
3. Wall shapes are recomputed for the 5 by 5 block around the player and the already-seen walls in it are redrawn.

Closed doors and all other objects are unaffected. Nothing is printed except the kill message from the damage routine.

### 21 dry: dehydration

Adjacent strike with damage 100 + clev, message `  The %s shrivels up.`, dehydrate effect.

### 22 lit: lightning

Projectile (5.7), lit row.

### 23 drl: drain life

Let i be the smaller of (current hit points - 1) and (maximum hit points / 2). Adjacent strike with damage 2 x i, an empty message (nothing is printed on a hit), drain effect. After the helper returns, in every case, the caster's hit points are reduced by i with no death check: this happens even if the caster was confused, there was no monster, the monster was immune or the target was a mirror. Against a mirror the caster first loses 2 x i through the "own magic" rule (which can kill) and then i more.

### 24 glo: invulnerability

If the globe timer is zero, the extra-defence bonus rises by 10. The timer increases by 200. INT -1. Sparkle on the player. At expiry the bonus drops by 10.

### 25 flo: flood

Burst (5.8) with damage 32 + clev, message `  The %s struggles for air in your flood!`, flood effect.

### 26 fgr: finger of death

1. If rnd(151) equals 63 (1 in 151): beep, print `\nYour heart stopped!\n`, sleep 4000 ms, and the player dies with the cause "erased by a wayward finger". The spell ends.
2. Otherwise, if WIS is greater than rnd(10) + 10 (never at WIS 11 or less, always at WIS 21 or more): adjacent strike with damage 2000, message `  The %s's heart stopped.`, finger effect.
3. Otherwise print `  It didn't work.` (no line break, full stop; distinct from the general failure message).

### 27 sca: scare monster

The scare timer increases by rnd(10) + clev. Then, if any inventory slot holds the hand of fear, the whole timer (including time left over from earlier casts) is multiplied by 3. Sparkle on the player. Effect on monsters: Part 04.

### 28 hld: hold monster

The hold timer increases by rnd(10) + clev. No animation. While it runs monsters do not move (spheres still do). Damaging any monster, or a sphere explosion, cuts the timer to 1.

### 29 stp: time stop

The time-stop timer increases by rnd(20) + 2 x clev. No animation. While it runs the regeneration tick does nothing except count it down, monsters and spheres do not move, no monsters are generated, spells fail with `\n  It didn't seem to work.`, and several commands are refused (Part 01).

### 30 tel: teleport away

1. Confusion check, direction prompt; stop silently if the target is off the map.
2. No monster there: `  There wasn't anything there!`
3. Set the current monster name; resistance lookup (demons resist).
4. Play the teleport effect at the target cell.
5. Up to 10 attempts: pick x = rnd(65), y = rnd(15). If that cell has no object and no monster and is not the player's cell, the monster moves there with its carried items, its hit points are reset to its species' full starting value, the old cell is redrawn, and the spell ends. If all 10 attempts fail the monster stays where it is. Nothing is printed either way.

There is no mirror rule.

### 31 mfi: magic fire

Burst (5.8) with damage 35 + rnd(10) + clev, message `  The %s cringes from the flame.`, fire effect.

### 32 mkw: make a wall

Confusion check, direction prompt. If the target is off the map nothing happens. Otherwise, tested in this order:

1. The target is a wall: `\nthere's a wall there already!`
2. The target holds any object: `\nthere's something there already!`
3. A monster stands there: `\nthere's a monster there!`
4. The level is 1 and the target is the dungeon exit cell (column 33, bottom row): `\nyou can't make a wall there!`
5. Otherwise: play the wall effect on the cell, the cell becomes a wall, it is redrawn, and the wall shapes of it and its eight neighbours are recomputed and redrawn where already seen.

Walls made this way are ordinary walls. They can be made on any level, including the home level.

### 33 sph: sphere of annihilation

1. Unless in wizard mode: if rnd(23) equals 5 (1 in 23) and the character does not carry the talisman of the sphere, then beep, print `\nYou have been enveloped by the zone of nothingness!\n`, sleep 4000 ms, and the player dies with the cause "self - annihilated"; the spell ends. With the talisman the roll has no effect.
2. INT -1.
3. Direction prompt (no confusion check).
4. A sphere is created on the adjacent cell in that direction, travelling in that direction, with a lifetime of rnd(20) + 11 (12 to 31). Everything else is in section 9.

### 34 gen: genocide

The following loop runs until it reports success or failure:

1. Print `\nEnter the number of the monster to genocide [* for list]: ` and read a number. ESC, `*`, or Enter with nothing typed all give -1.
2. **-1: show the list.** Switch to the text screen, clear it, print `Monster numbers`, then list monster ids 1 up to the id before the first demon lord (1 to 60 in this build, so the four Wild Edition monsters are included) in three columns: entry k (counting from 0) is drawn at column (k mod 3) x 28 + 1 and row 3 + k/3, as the id left-justified in two characters, `) `, and the name (for example `1 ) lemming`, `12) floating eye`). The code intends to show `Genocided` in place of the name for species already removed, but it tests the wrong record (the one for the number just entered, -1, which is outside the table), so what is shown for already-genocided species is not dependable (unverified which way it falls; showing every name is the safe reading). Then a line break, and back to step 1 with the list still on screen.
3. **A number from 1 to 60:**
   - If the species is not yet genocided (or in wizard mode): return to the map if the list was showing, mark the species as genocided, print `\n  There will be no more %ss.` (the name with an `s` appended), then re-enter the current level exactly as if arriving on an already-visited level (Part 05: the level is stored and reloaded, the usual batch of "revisit" monsters is spawned, every monster of a genocided species is removed, and the player is repositioned if necessary), repaint the map and refresh the status panels. Success.
   - If it is already genocided and this is not wizard mode: nothing is printed and the loop goes back to step 1.
4. **Any other number** (0, a negative number other than -1, or 61 and above, so demons cannot be chosen): return to the map if needed, print `\n  You sense failure!`. Failure.

After the loop, whether it succeeded or failed: the known-spell counter for genocide drops by one, and INT -1.

A genocided species never appears again: random generation and creation skip to the next species, polymorph re-rolls, and it is purged from every level as the level is entered. The flags are saved with the monster data (Part 03, Part 04).

### 35 sum: summon demon

1. If rnd(100) is greater than 30 (70%): adjacent strike with damage 150, message `  The demon strikes at the %s.`, demon effect. No monster is immune.
2. Otherwise, if rnd(100) is greater than 15 (85% of the remaining cases, 25.5% overall): `  Nothing seems to have happened.`
3. Otherwise (4.5% overall): `  The demon turned on you and then vanished!`, beep, and the player loses rnd(40) + 30 hit points; death is recorded as "attacked by a revolting demon".

No direction is asked for in cases 2 and 3.

### 36 wtw: walk through walls

The walk-through-walls timer increases by rnd(10) + 5. Sparkle on the player. While it runs the player may move onto wall cells (Part 02).

### 37 alt: alter reality

The current level is rebuilt as a fresh random maze and everything that was on it is scattered.

1. Scan the whole map row by row (north to south, west to east within a row), building a list. For each cell: if its object is anything other than empty floor, a wall or a sphere of annihilation, record the object and its argument value; if a monster stands there, record its species and current hit points. Then make the cell a wall with no monster. (Things carried by monsters are not recorded; they stay attached to the cell.)
2. Carve a maze into the solid block with Part 05's corridor-eating routine started at cell (1, 1).
3. On level 1, reopen the exit cell (column 33, bottom row).
4. Clear one whole row to empty floor: row rnd(15), columns 1 to 65.
5. Put the recorded entries back, last recorded first:
   - an object: up to 100 picks of x = rnd(66), y = rnd(16); the first pick that lands on empty floor receives the object and its argument. If the first 99 picks all miss, the object is lost (the 100th pick is never used).
   - a monster: the same picks; it is placed on the first cell that is not a wall and holds no monster (it may share a cell with an object and may be put on the player's cell), with its recorded hit points. After 99 misses it is lost.
6. INT -1.
7. Recompute all wall shapes. Every cell becomes unknown to the player (in wizard mode every cell becomes known instead). Repaint the map.
8. Unless in wizard mode, the known-spell counter for alter reality drops by one.
9. If the player's cell now holds an object (including a wall) or a monster, the player is moved to the nearest cell that has neither (Part 02's repositioning: smallest squared distance, first found scanning columns west to east and rows north to south).

Stairs, shafts, buildings, altars, fountains and so on are objects and are scattered like everything else; this works on any level including the home level. The picks in step 5 can reach the east column and the bottom row, which are solid wall after step 1 except for the reopened level 1 exit, so on level 1 an object or a monster can end up on the exit cell. Sphere tiles are wiped but the sphere records survive (section 9.7).

### 38 per: permanence

Every timed effect that is currently active (non-zero) has 99999 added to its remaining time; inactive ones are untouched. One regeneration tick is then run immediately. The known-spell counter for permanence drops by one, INT -1, sparkle on the player.

The timers affected are all 28 that Part 02 ticks down, not only the helpful ones: heroism, altar protection, spell protection, dexterity, strength, giant strength, charm, invisibility, cancellation, haste self, aggravate monsters, scare monster, stealth, awareness, hold monster, haste monsters, fire resistance, globe, spirit protection, undead protection, half damage, see invisible, itching, clumsiness, walk through walls, the "coked" drug effect, blindness and confusion. Time stop is not in the list. The extension is plain arithmetic: a later event that subtracts time (a college course, Part 08) can still bring a timer back down.

### Things that are not spells

There is no spell for stealth, fire resistance, awareness, undead or spirit protection, slowing monsters or self-teleport; those come from scrolls, potions, items and experience levels (Parts 02 and 06).

## 8. Scroll effects that use this machinery

**Scroll of pulverization.** A projectile exactly as in 5.7 with the last row of the table there. It is subject to the confusion check, asks for a direction, and uses the lightning column for immunity.

**Scroll of annihilation.**

1. The area is the 3 by 3 block centred on the player, clipped to the map.
2. Multi-cell animation of the annihilate effect on every cell of the area except the player's.
3. For each monster in the area: if it is below the demon lords in the monster table, its experience value is added to a running total and it is removed with no drops (Wild Edition hook: it is counted as a kill, and one Wild monster releases what it had swallowed; see Part 12). If it is a demon lord, the demon prince or the God of Hellfire, print `\nThe %s barely escapes being annihilated!` and set its hit points to half (rounded down) plus 1.
4. If the total is greater than zero: `\nYou hear loud screams of agony!` and the total is awarded as experience.
5. Redraw the cells.

## 9. Spheres of annihilation

### 9.1 What a sphere is

A sphere of annihilation is a moving hazard that erases whatever it touches. Each live sphere has a record holding five 16-bit values: column, row, level number, direction of travel (section 1 numbering) and remaining lifetime. The records form a list; a new sphere goes to the front. Wherever a sphere currently sits on the level being displayed, the map's object layer holds the "sphere of annihilation" object, which is what the player sees and what the player and monsters react to when they step there.

A character counter ("spheres cast") is increased by one when a sphere is created and decreased by one when one is removed. Nothing reads it for game rules; it is saved with the character and shown only by the debug dump.

There is no limit on the number of spheres other than memory. (If memory for a record cannot be obtained the game ends with "died of internal complications".)

### 9.2 Creation

The only way a sphere comes into existence is the sph spell (section 7, spell 33). Creation:

1. Make the record with the target cell, the current level, the direction and the lifetime; put it at the front of the list; add one to the counter.
2. Run the placement step (9.4) in "just created" mode, which does not move the sphere but applies every arrival rule to the cell it starts on.
3. Sleep 500 ms so the player sees the sphere before it first moves.

Because the starting cell is clipped into the legal area (9.4 step 3), a sphere cast straight at the outer wall of a dungeon level from a cell next to that wall is placed on the caster's own cell, where rule d of 9.4 kills a caster who has no talisman.

### 9.3 When spheres move

Spheres are moved at the start of each "monsters move" phase (Part 04), which means:

- not at all while time is stopped;
- while the player is hasted, only on phases where the haste timer is odd;
- twice per player turn while monsters are hasted;
- also during the forced turns of a reflected sleep or web;
- regardless of hold monster (the spheres move before the hold check).

For each record in list order (newest first, though see 9.7 for the order after a reload):

1. Spheres whose level is not the current level are skipped entirely. They are in stasis: their lifetime does not run down, and they resume when the player returns to that level.
2. The lifetime is reduced by one. If it is now below zero the sphere is removed (9.6) and nothing else happens.
3. Otherwise, with probability 2 in N, where N is the larger of 7 and INT/2, the direction is replaced by rnd(8) (which may repeat the old one). At INT 15 or less this is 2 in 7 per move; at INT 40 it is 1 in 10.
4. Run the placement step (9.4) in "moving" mode.

A sphere created with lifetime L therefore makes L moves and vanishes on the following phase. The iteration must tolerate the removal of the sphere being processed and of the one that would have been processed next (a collision removes two).

### 9.4 The placement step

Inputs: a sphere and whether it was just created.

1. If the stored direction is 9 or more it is set to 0.
2. In moving mode: the sphere's old cell becomes empty floor, is marked as known empty floor and is redrawn; the position then advances one cell in the direction of travel.
3. Clip the position. On the home level it is clipped to the map (0 to 66, 0 to 16). On every other level it is clipped to the interior (1 to 65, 1 to 15): spheres never enter the outer wall ring, and one pushing against it stays where it is. Store the clipped position in the record.
4. Note whether the player carries the talisman of the sphere, which monster (if any) is on the cell, what object is on the cell, and whether that monster carries a talisman of the sphere among its own items.
5. Apply the first rule that matches:

| Order | Condition | Result |
|---|---|---|
| a | Player has no talisman, and the monster on the cell is a demon lord, the demon prince or the God of Hellfire | The cell is marked known and redrawn. `\nThe %s dispels the sphere!` (monster's name), beep. The sphere is removed (9.6). The demon is unharmed. |
| b | Player has no talisman, and the monster is a disenchantress | `\nThe %s cancels the sphere!`, beep. Explosion at the cell (9.5), then the sphere is removed. |
| c | Player has no talisman, and the cancellation timer is running | `\nAs the cancellation takes effect, you hear a great earth shaking blast!`, beep. Explosion at the cell, then the sphere is removed. This happens wherever the sphere is. |
| d | Player has no talisman, and the cell is the player's cell | `\nYou have been enveloped by the zone of nothingness!\n`, beep. The sphere is removed, the game sleeps 4000 ms, and the player dies with the cause "self - annihilated". |
| e | The cell already holds a sphere object (with or without talisman) | `\nTwo spheres of annihilation collide! You hear a great earth-shaking blast!`, beep. One sphere at the cell is removed, the explosion happens at the cell, then the other sphere at the cell is removed. |
| f | Otherwise | The cell's object becomes the sphere object; whatever object was there (wall, door, item, stairs, altar, anything) is destroyed without a message. If it was a wall, the wall shapes of the eight neighbours are recomputed and redrawn where seen. If a monster is on the cell: when it carries a talisman of the sphere, print `\nThe %s is unaffected by the sphere of annihilation!` and it stays; otherwise it is removed silently, with no experience and no drops. The cell is marked known and redrawn. |

With the talisman of the sphere in the player's inventory, rules a to d never apply: the player's spheres erase demons (including the God of Hellfire) like any other monster, cancellation and disenchantresses do not detonate them, and a sphere may pass over the player's own cell harmlessly.

Spheres destroy walls on every level, including volcano levels 18 to 20 where spells cannot.

### 9.5 The explosion

1. If hold monster is running its timer is set to 1; likewise cancellation. Both therefore end at the next regeneration tick.
2. The blast area is the 5 by 5 block centred on the explosion cell. On the home level it is clipped to the map; elsewhere it is clipped to the interior (columns 1 to 65, rows 1 to 15), so the outer wall survives.
3. For each cell, columns west to east and rows north to south within a column:
   - the object becomes empty floor (walls and everything else are destroyed, including other spheres' objects, though not their records);
   - a monster there is removed (no experience, no drops) unless it carries a talisman of the sphere, in which case `\nThe %s is unaffected by the blast!` is printed and it stays;
   - the cell is redrawn;
   - if the player has no talisman and stands on this cell: beep, `\nYou were too close to the sphere!`, sleep 3000 ms, and the player dies with the cause "annihilated by a sphere". (With life protection the loop simply continues.)
4. Wall shapes around the area are recomputed and seen walls redrawn.

Since a sphere is always adjacent to the caster when first placed, casting the spell while cancellation is running, onto a disenchantress, or onto another sphere kills a caster who has no talisman.

### 9.6 Removing a sphere

Removal is by position: the first record in the list that is on the current level at the given cell is taken out. The cell's object becomes empty floor, it is marked known and redrawn, the counter is reduced by one and the record is discarded. If no record matches, nothing happens. Removal is used when a lifetime expires, by rules a to e above, and by the monster code below.

Because removal empties the cell's object unconditionally, an item under a demon that dispels a sphere is destroyed too.

### 9.7 Other interactions

**The player walks onto a sphere** (Part 06's "what is on this cell" step): with the talisman, `\nThe Talisman of the Sphere protects you from annihilation!` and nothing else. Without it the player dies at once, with no message, with the cause "annihilated by a sphere".

**A monster walks onto a sphere** (Part 04's movement step):

- A demon lord, the demon prince or the God of Hellfire dispels it if the player has no talisman, and the God of Hellfire still dispels it 30% of the time (rnd(10) greater than 7) when the player has one: `\nThe %s dispels the sphere!` and the sphere at that cell is removed.
- In every other case the monster is destroyed together with what it carried: `\nThe %s is destroyed by the sphere of annihilation!` No experience is given.
- Either message is printed, with a beep, only if the player is not blind and the cell is one the player has seen.

**Projectiles and bursts** ignore the sphere object.

**Alter reality** does not record sphere objects, so they vanish from the map, but the records remain. On its next move such a sphere turns its current cell (now probably maze wall) into empty floor, moves on and reappears normally. In between it is invisible and harmless, because all contact rules are driven by the object on the map.

**An explosion** that wipes another sphere's object has the same effect on that sphere: its record survives and it reappears on its next move.

**Level changes.** Spheres stay on the level they were cast on. The sphere object is part of that level's stored map, and the record's level number keeps it frozen until the player comes back; a sphere can therefore outlast any amount of game time spent elsewhere.

**Saving.** After the monster data, the save file holds a 32-bit count of spheres followed by that many records in list order, each written as the raw in-memory record: a link field of pointer size (meaningless when read back), the five 16-bit values in the order column, row, level, direction, lifetime, and padding up to a multiple of the pointer size (24 bytes per record on a 64-bit build, 16 on a 32-bit build). On loading, each record read is put at the front of the list, so the processing order is reversed by every save and reload. The "spheres cast" counter is restored with the character, not recomputed. Part 03 owns the surrounding file layout and checksum.

**End of game.** All records are discarded when the game process ends.

## 10. Message catalogue

Every string printed by the spell and sphere code, grouped by where it comes from. Messages from other parts that appear during spell use are listed last for convenience.

**Casting**

- `\nYou don't have any spells!`
- `\nEnter your spell [D for list, ESC to abort]: `
- `aborted.`
- ` (%s)` (spell name)
- ` Unknown spell! Try again.`
- `\n  It didn't seem to work.` (time stopped)
- `\n  It didn't work!` (random failure)
- `\n  Nothing happens.  You seem inexperienced.`
- `  spell %d not available!`
- ` You can't aim your magic!`

**Helpers**

- `  There wasn't anything there!`
- `You fall asleep! `
- `You get stuck in your own web! `
- `spell caster (that's you)` (substituted for the monster name)
- `\nYou are hit by your own magic!`
- a blank line followed by `\nthe %s returns your puny missile!`
- the words `wall`, `door`, `statue`, `throne` (substituted for the monster name)
- `  The wall crumbles.`
- `  The door is blasted apart.`
- `  The statue crumbles.`
- the eighteen reaction messages of section 5.4, each preceded by a line break

**Spell messages**

- `  Your missile hit the %s.` and `  Your missiles hit the %s.`
- `  While the %s slept, you smashed it %d times.`
- `  The sound damages the %s.`
- `  While the %s is entangled, you hit it %d times.`
- `Number: ` (wizard mode, create monster)
- `  The %s believed!`
- `  It didn't believe the illusions!`
- `  The fireball hits the %s.`
- `  The cone of cold strikes the %s.`
- `  The %s gasps for air!`
- `  The %s shrivels up.`
- `  A lightning bolt hits the %s.`
- `  The %s struggles for air in your flood!`
- `\nYour heart stopped!\n`
- `  The %s's heart stopped.`
- `  It didn't work.`
- `  The %s cringes from the flame.`
- `\nthere's a wall there already!`
- `\nthere's something there already!`
- `\nthere's a monster there!`
- `\nyou can't make a wall there!`
- `\nYou have been enveloped by the zone of nothingness!\n`
- `\nEnter the number of the monster to genocide [* for list]: `
- `Monster numbers`
- list entries: the id left-justified in two characters, then `) ` and the name, or `) Genocided`
- `\n  There will be no more %ss.`
- `\n  You sense failure!`
- `  The demon strikes at the %s.`
- `  Nothing seems to have happened.`
- `  The demon turned on you and then vanished!`

**Scrolls handled here**

- `The ray hits the %s.`
- `\nThe %s barely escapes being annihilated!`
- `\nYou hear loud screams of agony!`

**Spheres**

- `\nThe %s dispels the sphere!`
- `\nThe %s cancels the sphere!`
- `\nAs the cancellation takes effect, you hear a great earth shaking blast!`
- `\nYou have been enveloped by the zone of nothingness!\n`
- `\nTwo spheres of annihilation collide! You hear a great earth-shaking blast!`
- `\nThe %s is unaffected by the sphere of annihilation!`
- `\nThe %s is unaffected by the blast!`
- `\nYou were too close to the sphere!`

**From other parts, seen during spell use**

- `\nIn What Direction? ` followed by `South`, `East`, `North`, `West`, `Northeast`, `Northwest`, `Southeast` or `Southwest` (direction prompt)
- `\nThe %s died!` (damage routine, Part 04)
- `\nAlas, you have died.\n` (hit-point loss, Part 02)
- `\nThe Talisman of the Sphere protects you from annihilation!` (Part 06)
- `\nThe %s is destroyed by the sphere of annihilation!` (Part 04)
- `The magic spells you have discovered to date are:` (Part 03)
- `\nSpell "%s":  %s\n%s.` and `\nYou feel clever!` (books)
- causes of death used: "hit by his own magic", "erased by a wayward finger", "self - annihilated", "attacked by a revolting demon", "annihilated by a sphere", "wasted by a malloc() failure", "died of internal complications" (Part 03)

## 11. Summary of quirks that must be reproduced

1. Every spell costs one spell point and one turn; aborting the prompt costs the turn but not the point.
2. An unknown or unlearned code re-prompts until a real spell is cast or ESC is pressed.
3. Failure depends only on INT and a flat 1 in 23; the experience requirement is "id no greater than 3 x clev + 2".
4. The dexterity spell raises DEX by 3 when cast and by 3 again when it expires.
5. The descriptions of dex and str say +2; the real boost is 3.
6. Sleep and web do not disable the monster; their damage is a multiple of the character's melee "full hit", and so is 10000 while the lance of death is wielded.
7. A killed monster absorbs only its remaining hit points from a projectile; a surviving one stops it.
8. A reflected projectile keeps its original animation orientation when a demon returns it but flips when a mirror does.
9. Statues crumble to projectiles only at difficulty 3 or less (29 in 60), but always yield to vaporize rock at difficulty 3 or less and 31 times in 60 above that.
10. Vaporize rock on a throne never produces the gnome king (a projectile does); on an altar it creates four demon princes beside the caster.
11. Drain life always costs the caster the hit points, hit or miss.
12. Create monster adds 8 to the monster id, which in this build can produce Wild Edition monsters and demon lords on deep levels.
13. Polymorph can produce anything up to the demon prince.
14. Summon demon is the only strike spell with no immune monsters.
15. Genocide and permanence are forgotten (one count) even when they fail or in wizard mode; alter reality is not forgotten in wizard mode.
16. Permanence also makes curses such as blindness, confusion, itching and half damage last 99999 turns longer.
17. The sphere spell has no confusion check, and a 1 in 23 chance of killing a caster who lacks the talisman before any direction is asked.
18. Cancellation detonates every moving sphere; the 5 by 5 blast kills a nearby player who lacks the talisman.
19. Spheres on other levels are frozen indefinitely.
20. The resistance table's last six stored columns are shifted by one, which leaves several entries without effect (section 6.3).
