# Part 16 — Suggested improvements

Everything in this part is optional. Parts 01 to 15 describe the game as it is; a
rebuild that follows them and ignores this part is a faithful recreation. This part
collects what a rebuild could do better: a cleaner architecture, fixes for defects found
while the specification was written, usability changes and a few design suggestions.

Each item says what it changes for the player, so that the choice between "faithful"
and "better" is made deliberately, one item at a time. Section 3 is also the single
list of every defect the other parts report, with a recommendation for each.

## 1. Recommended policy

1. Build the faithful behaviour first, with each quirk behind its own named switch
   (Part 00 section 4).
2. Ship with the switches in section 3 set as recommended there: defects that only
   hurt or confuse the player are fixed, quirks that players of this game family know
   and rely on are kept.
3. Offer one setting, "Classic rules", that puts every switch back to the original
   behaviour. Store it with the character, as the Wild feature mask is, so a game
   plays by one set of rules from start to finish and the scoreboard can mark which
   set a score was earned under.

## 2. Architecture

These change nothing the player can see, unless noted. Part 00 section 7 already
assumes the first six.

| # | Change | Instead of | Benefit |
|---|---|---|---|
| A1 | One language, with the game core as a platform-neutral module | A C core, a bridge layer and a Kotlin app | No bridge, no hand-synchronised field lists, runs in unit tests, can target the browser |
| A2 | A game is an object; input requests suspend | Global state, a forked process per game, pipes and shared memory | Simpler lifecycle; nothing to leak between games; no process to lose when the app is killed |
| A3 | A seedable generator owned by the game, with its state saved | The C library generator, re-seeded from the clock on every load | A game can be replayed exactly from its seed and its key log; reloading no longer changes what happens next. Player-visible: reloading a checkpoint no longer re-rolls an outcome |
| A4 | Typed results and events | The app matching printed text ("The ... died!") to count kills, and reading the final score page to learn how the game ended | Kill counts include every kind of kill; no dependence on wording; translations become possible |
| A5 | A versioned, self-describing save with migration | A raw memory dump that differs between 32-bit and 64-bit builds and is refused and deleted when the format changes | Saves survive updates and move between devices. Player-visible: an update never destroys a game |
| A6 | Delays as cancellable requests scaled by a setting | A delay routine that ignores the "no delays" option | The existing setting starts working |
| A7 | Structured screens: each building, list and menu is described as data (title, rows with label, key, price, state, footer) and the 80 by 25 text page is one renderer of it | Fixed-width text pages plus a heuristic re-wrapper that guesses at paragraphs and price columns | Phone layouts can be native lists with tap targets; screen readers get real rows; the track picker can ask "is this the bank?" instead of searching the page for words. Keep the text renderer for the harness and for golden tests |
| A8 | Monster awake and seen flags stored with each level and saved | One working grid shared by all levels and dropped on save | Removes the leak described in Part 04 section 13. Player-visible, slightly: sleeping monsters stay asleep across level changes and reloads |
| A9 | "Ordinary monster", "demon", "Wild monster" as flags on the monster row | Ranges of identifier numbers | Adding a monster can no longer shift the meaning of arithmetic on identifiers (see W1 in section 4) |
| A10 | One scoreboard entry per character name, keeping the best | One entry per numeric user id, which on a phone means one for the whole device, and a harder game overwriting a better score | The scoreboard becomes useful with several save slots. Player-visible |
| A11 | Record the key log of every game alongside the seed | Nothing | A crash or a disputed outcome can be reproduced; regression tests can be recorded by playing |
| A12 | Tile sheet built entirely from text tiles plus a generated index file naming each tile | A hand-inherited image for ids 0 to 367 | The whole sheet is reviewable and editable by agents; tile ids stop being magic numbers in the renderer |

## 3. Defects in the base game, with a recommendation for each

"Keep" means the oddity is part of how the game plays and removing it would change
balance or surprise experienced players. "Fix" means it is recommended to ship with
the corrected behaviour and keep the original behind the "Classic rules" switch.
The part and section that describe each one are given.

### 3.1 Character and timers (Part 02)

| # | Original behaviour | Recommendation |
|---|---|---|
| B1 | The dexterity spell adds 3 on casting and adds 3 again when it runs out, so each cast is a permanent +6 | Fix: remove 3 on expiry |
| B2 | Heroism adds 11 to every ability and removes 10; giant strength adds 21 and removes 20; cocaine adds 33 and removes 34, and a second dose while the first is active adds another 33 with only one removal | Fix: remove exactly what was added, once per active effect |
| B3 | Protection timers that run out do not recalculate armour class until something else does | Fix: recalculate on expiry |
| B4 | Awareness never wears off once the orb of enlightenment has been generated anywhere in the game, carried or not | Fix: only while the orb is carried |
| B5 | Scroll enchantment never raises armour past +9 but raises a weapon past +9 one time in ten | Keep |
| B6 | The armour enchant message reads "Your a shield glows for a moment." | Fix the wording |
| B7 | Going back in time, and the permanence spell, lengthen harmful timers too | Keep |
| B8 | Gender is not saved | Fix: save it |
| B9 | The ring scan reads one slot beyond the pack | Fix (no visible change) |

### 3.2 Screens, score and saves (Part 03)

| # | Original behaviour | Recommendation |
|---|---|---|
| B10 | Net worth is always halved and the depth bonus never counts, because a penalty flag is set and never cleared | Keep the scores as they are for comparability, or fix and mark scores with the rule set (section 1). Do not silently change it |
| B11 | Every empty pack slot subtracts 1 from the score | Fix |
| B12 | The final score page shows the title of the next experience level | Fix |
| B13 | Loading a game lower-cases the class name ("ogre") | Fix |
| B14 | The "eat" list is always empty; the "wield" and "wear" lists never show objects with high identifiers, which includes every Wild object | Fix: build each list from what the command can actually use |
| B15 | Saving ends the session even when writing the save failed, and the checkpoint is deleted as well | Fix: report the failure and keep playing |
| B16 | Quitting leaves no scoreboard entry | Keep |

### 3.3 Monsters and combat (Part 04)

| # | Original behaviour | Recommendation |
|---|---|---|
| B17 | Fire resistance changes only the message; hell hound and dragon fire still do full damage | Fix: resistance prevents the damage, as the message says |
| B18 | The Vorpal Blade never beheads, and the Slayer test misfires, when the weapon is in pack slot "a" | Fix |
| B19 | The hand of fear works 40% of the time although its own note says 60% | Keep the coded value |
| B20 | Teleporting a monster restores it to full hit points | Keep |
| B21 | A mimic's disguise is one shared value for all mimics and can be the first demon lord | Fix: one disguise per mimic, ordinary monsters only |
| B22 | "Asleep" only matters while the player has stealth | Keep |

### 3.4 Dungeon (Part 05)

| # | Original behaviour | Recommendation |
|---|---|---|
| B23 | Only 20 of the 21 hand-drawn maps can ever be chosen | Fix: choose among all of them |
| B24 | Studded leather armour and the ring of protection can never appear in the level scatter, because their thresholds are too low to pass the test | Keep (they are sold in the store), or fix with a small non-zero chance |
| B25 | Treasure rooms on volcano level 4 contain guards but no treasure | Fix: clamp the treasure depth instead of rejecting it |
| B26 | The dungeon bottom (level 15) has no stairs at all | Keep: it is reached and left by other means, and this is part of the challenge |

### 3.5 Objects (Part 06)

| # | Original behaviour | Recommendation |
|---|---|---|
| B27 | Giving money at an altar subtracts the gift twice on two branches, so gold can go negative | Fix |
| B28 | A chest gives up its contents only when its trap explodes; otherwise "Nothing happens." and it stays shut | Keep: this is the long-standing behaviour, but review it against player expectation before shipping |
| B29 | The book-reading intelligence bonus is 1 in 10 although its note says 40% | Keep the coded value |
| B30 | Gold detection ignores two of the four kinds of gold pile | Fix |
| B31 | Running never checks the squares it crosses, so hidden traps passed over while running do not fire | Keep |

### 3.6 Spells (Part 07)

| # | Original behaviour | Recommendation |
|---|---|---|
| B32 | The resistance table has 38 columns for 39 spells and its last six columns are shifted by one, so several resistances apply to the wrong spell (for example a demon's "resists" lands on make wall instead of sphere of annihilation) | Fix: realign to the headings. This changes outcomes against demons, so it belongs under "Classic rules" |
| B33 | Vaporize rock aimed at a throne from next to it never releases the gnome king, while the projectile path does | Fix |
| B34 | Drain life costs the caster the hit points whether or not anything was hit | Keep |
| B35 | An aborted cast still uses the turn | Keep |
| B36 | A sphere cast at the outer wall lands on the caster | Fix: refuse the cast |
| B37 | The genocide list reads a flag from before the start of the monster table | Fix (show names normally) |

### 3.7 Buildings (Part 08)

| # | Original behaviour | Recommendation |
|---|---|---|
| B38 | The bank's appraisal of the Eye of Larn is skipped when the Eye is in pack slot "a" | Fix |
| B39 | The Eye can be sold twice, once through the appraisal and once through the gem list | Fix |
| B40 | Trading post prices for potions are read from the wrong store rows above a certain potion number, so several potions sell for the wrong amount | Fix |
| B41 | The "which stone" prompt does not accept "z" or Escape | Fix |
| B42 | Only lower-case "y" means yes | Fix: accept both cases |
| B43 | Escape at any "How much?" prompt accepts the default, which is everything: all gold deposited, the whole balance withdrawn, all gold donated or dropped | Fix: Escape cancels. This one costs players real games |

### 3.8 Front end (Parts 01, 09)

| # | Original behaviour | Recommendation |
|---|---|---|
| B44 | A direction prompt cannot be cancelled | Fix: Escape cancels and the action is abandoned at the usual turn cost |
| B45 | The "no delays" option is read and ignored | Fix (A6) |
| B46 | Debug mode identifies potions twice and scrolls never | Fix |
| B47 | The options file rejects the path syntax its own help documents | Not applicable: replace the options file with app settings |
| B48 | One help sentence loses a backslash to the style-marker parser and shows the key as two quote marks | Fix the help text |
| B49 | Refreshing the status line clears the "lost" flag as a side effect | Fix: clear it where the player learns the level |

## 4. Defects in the Wild Edition and the app

These are in the code this repository adds, so they are worth fixing in the
repository itself as well as in a rebuild.

| # | Behaviour | Where | Recommendation |
|---|---|---|---|
| W1 | The create monster spell and altar desecration add 8 to a monster number. The four Wild monsters were inserted just before the demon lords, so deep casts that used to give demon lords I to VII and a demon prince now give a null pointer, a fountain kraken, a tax auditor or a Roomba, and at most demon lord IV | Parts 04, 07 | Fix: number the Wild monsters after the base ones, or express the result as a table. This is a change to the base game made by accident, and it also happens with every Wild feature switched off |
| W2 | The tax auditor takes 10% of carried gold but only the amount owed is credited; the rest disappears | Part 12 | Fix: take no more than is owed |
| W3 | The pet acts during time stop and can heal on every action while time is stopped | Part 12 | Fix: the pet freezes too |
| W4 | A searched old boot lying on the floor is announced as "an old boot + 1" | Part 12 | Fix: do not use the enchantment field as the "searched" marker |
| W5 | A fountain can dry up after a cast on which nothing bit | Part 12 | Fix: roll only after a catch, as documented |
| W6 | The LarnCoin "How many" prompts default to the maximum, and Escape accepts the default | Part 12 | Fixed by B43 |
| W7 | The "died" event is sent before the life-protection test, and also on save and on quit | Parts 03, 09, 12 | Fix: send it only on an actual death (A4) |
| W8 | Sound and trading cards follow the app's current setting, not the flags stored with the character, so a tape picked up while the radio setting is off is lost | Part 13 | Fix: unlock regardless; let the setting only mute |
| W9 | The boss track starts when a demon tile is anywhere in the remembered map, not when a demon is in view | Part 13 | Fix: use the game's knowledge of what is displayed now |
| W10 | Opening the scoreboard from the start screen closes the tracked run as "quit" in the statistics | Part 10 | Fix |
| W11 | "Save game and quit" sets a flag that is cleared only when the next game starts, so a later exit can close the app unexpectedly | Part 10 | Fix: tie the flag to the session that set it |
| W12 | In the control editor, Reset deletes the saved layout immediately, even if the edit is then cancelled | Part 11 | Fix |
| W13 | The key notation for Backspace sends code 127 while the on-screen key and keyboards send code 8 | Part 11 | Fix: send 8 |
| W14 | Characters above code 127 from a soft keyboard are sent as code 127 | Part 10 | Fix: ignore them, or support text entry properly for names |
| W15 | The chest context action ignores the "blocked" state every other action respects | Part 11 | Fix |
| W16 | The planned "(old version)" label for save slots was never built; an old save is deleted when opened | Part 10 | Fixed by A5 |
| W17 | The project notes describe the base tiles as transparent with black outlines; they are opaque on the floor colour with cast shadows | Part 14 | Correct the notes |

## 5. Usability

None of these change a rule.

1. **Message history.** A scrollable log of every message of the session. The
   original shows five lines and the rest is gone.
2. **Confirm irreversible defaults.** Where a prompt's default is "all my gold",
   show the amount on the button.
3. **Identify on long-press.** Long-pressing a map square or an inventory row shows
   what the character knows about it, using the game's own item naming.
4. **Unknown squares in the level overview** could show how much of the level has
   been seen as a percentage.
5. **Time left on the HUD.** Mobuls remaining are shown only at the end of the
   inventory screen. A small indicator that turns amber and red would make the
   central pressure of the game visible. Offer it as a setting.
6. **A first-game guide.** Five or six one-time hints tied to events (first item
   underfoot, first spell point spent, first return to town, the first time the
   clock passes a quarter of the limit).
7. **Spell picker from the data module.** The picker's descriptions are a second
   copy of information the game already has. Generate them (Part 00 section 7.2).
8. **Translations.** With A4 and A7 in place, every string can come from a string
   table. Keep message identifiers stable.
9. **Controller and keyboard remapping** using the same action model as the
   on-screen controls.
10. **Colour-blind check of the minimap** (yellow monsters, red items): add shapes.
11. **Cloud-friendly export.** The slot archive already exists; add the seed and key
    log (A11) so a shared save can also be replayed.
12. **Screen reader map summary.** A spoken summary of what is adjacent in each
    direction, on request.

## 6. Design suggestions

These change the game and belong outside "Classic rules".

1. **Daily seed.** With A3, everyone can play the same dungeon on the same day, with
   a separate scoreboard column.
2. **Score rework.** Count depth, time remaining and difficulty in a way that
   rewards winning quickly (see B10). Keep the old formula for classic games.
3. **Pre-made map variety.** Mirror maps left to right at random, which doubles the
   apparent variety at no authoring cost.
4. **Wild balance pass.** Second Wind, Vampiric and a grown pet together remove
   much of the early danger; LarnCoin can make gold meaningless if the price model
   is exploited by reloading (A3 removes the reload exploit). Review once the
   distribution checks exist.
5. **Card album goals.** Small rewards for completing a rarity tier, cosmetic only.
6. **Weather below ground.** Dripping water on levels under a rainy town, as a
   purely visual effect.

## 7. Testing and tooling

1. Keep the command-line harness and its step notation (Part 15); add recording of
   key logs from real play (A11).
2. Make every probability table a named constant table and test its totals.
3. Add a "contact sheet" output to the tile builder (all tiles enlarged with their
   ids) for review.
4. Run the flags-off comparison (Part 00 section 10, check 7) in continuous
   integration; it would have caught W1.
5. Keep a single quirks file in the code listing every switch from section 3 with
   the part and section that describes it.

## 8. Suggested order of adoption

1. A1 to A6 from the start: they shape the code.
2. B43, B15, B14, B42 and B44 before the first playable release: they cost players
   games or hide working features.
3. W1 to W16 with the Wild milestone.
4. A7 when the phone layouts of the buildings are built.
5. The remaining fixes, then sections 5 and 6, by preference.
