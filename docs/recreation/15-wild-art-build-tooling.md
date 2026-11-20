# Part 15 — Wild tiles, icons, resources, build, CI and developer tooling

This part covers everything in the repository that is not game logic: the 48 Wild Edition tiles (ids 368 to 415) and the tools that author them, the launcher icon, the Android resources and manifest, the Gradle and CMake builds, the three continuous-integration workflows, the two-repository working process, the Linux host test harness with its regression scripts, and a consolidated inventory of every non-code asset in the app.

It leans on Part 14 for the base tile sheet (ids 0 to 367) and the object/monster-to-tile mapping rules, on Part 12 for what the Wild objects, monsters and features do, on Part 13 for the full weather, glitch, card and photo rendering algorithms (this part only says which tile goes where and how it lines up), on Part 09 for the process model and shared state that the host harness reads, and on Parts 10 and 11 for the app shell and game view.

Nothing in this part uses random numbers from the game. The game's helpers, for reference when reading the test scenarios: rnd(n) is uniform 1..n and rund(n) is uniform 0..n-1 (both are used in the Wild code read for this part, for example "rnd(100) <= 6" and "rund(4)" when a pet species is chosen).

## 1. Conventions

- A tile is a 32 x 32 pixel cell. Coordinates are written (x, y) with x = column 0 (left) to 31 (right) and y = row 0 (top) to 31 (bottom). All ranges are inclusive: "x 6–25" means columns 6 through 25.
- "Black" always means #000000, used for the 1-pixel outline of every sprite.
- "Floor" means #426b6b, a dull teal. It is the opaque background colour of almost every tile in the base sheet (323 of the 368 base tiles have it in their top-left pixel, and the plain floor/grass tile 191 is a solid square of it). Wild object and monster tiles paint it into every pixel that is not part of the sprite, so they sit seamlessly on the floor.
- "Ground shadow" means #2f4c4c, a darker teal used for a 1-pixel-high horizontal bar under most objects (and a 1-pixel column to the right of the two cabinet tiles).
- "Transparent" means alpha 0. Only the cassette (383), the three coin/arrow icons (389–391) and all overlays (392–415) have transparent pixels.
- Style of the whole sheet, to be matched: chunky sprites, 1-pixel black outlines, 3 to 4 shades per material, slightly desaturated colours, no anti-aliasing, light from the upper left (highlights on the left/top, shade on the right/bottom). Objects should leave a 2 to 3 pixel margin.

## 2. The text tile format

Every Wild tile is authored as a plain text file, one file per tile, in the folder `tools/tiles/src/`. The sheet builder turns them into pixels. The format:

- **File name:** decimal tile id, an underscore, a name made of letters, digits, underscores and hyphens, and the extension `.txt` (for example `372_rubber_duck.txt`). The id is the only part that matters; the name is for humans.
- **Comment and palette lines:** any line whose first character is `#` is a comment. Every comment line is also scanned for palette entries of the form "one non-space character, an equals sign, a hash, six hexadecimal digits" (for example `k=#000000`). Any number of entries may share a line, and prose may surround them. Each entry binds that character to that opaque colour (alpha 255). Later entries for the same character replace earlier ones.
- **The transparent character:** the full stop `.` is predefined as transparent and needs no palette entry.
- **Blank lines** (empty or whitespace only) are ignored everywhere.
- **Pixel rows:** every other line is a pixel row. There must be exactly 32 pixel rows, each exactly 32 characters long (the line's trailing newline is removed first; nothing else is trimmed). Each character must be `.` or a character defined in the palette.
- **Consequences of the parsing rules** (quirks an equivalent tool should keep or consciously drop): a pixel row can never begin with `#`, so `#` is unusable as a palette character; whitespace cannot be a palette character; palette characters are case-sensitive (most tiles use a lower-case letter for the dark shade and the upper-case letter for the light shade of the same material); a palette entry may be declared and never used (tiles 383, 385 and 399 each declare one unused colour); files with Windows line endings fail the 32-character check.

The 48 files each begin with one to four comment lines: a title line naming the tile and the feature that owns it, sometimes a one-line description, and the palette. The hat tiles and glitch tiles carry the note "generated shapes, hand-tuned palette" or "generated from fixed-seed shapes, hand-picked palette": they were produced by a throw-away generator script that is not in the repository, then committed as text like the rest.

## 3. The sheet-building tool

`tools/tiles/build_sheet.py` (Python 3, needs the Pillow imaging library) rebuilds the shipped tile sheet `app/src/main/assets/ularn_tiles.png` from two inputs: the untouched original sheet `tools/tiles/base_sheet.png`, and every authored text tile. It takes no arguments. Its constants: tile size 32, 16 columns, 368 base tiles, highest reserved id 447.

Process:

1. Open the base sheet and convert to RGBA. It must be exactly 512 x 736 pixels (16 columns by 23 rows = 368 tiles). If not, stop with a message stating the actual and expected size.
2. List the source folder in sorted name order. For each entry:
   - If the name does not match the file-name rule, record the error "<name>: file name must be <id>_<name>.txt", unless the name starts with a full stop (hidden files are skipped silently).
   - If the id is below 368, record "<name>: id <id> is inside the original sheet (< 368)".
   - If the id was already defined by an earlier file, record "<name>: id <id> defined twice" (possible because leading zeros are allowed in the id).
   - Otherwise parse the tile. Parse errors are recorded as "<path>:<line>: row has <n> chars, want 32", "<path>: <n> pixel rows, want 32", or "<path>:<row>: '<char>' not in palette".
3. If any error was recorded, print all of them, one per line, and exit with status 1. The sheet is not written.
4. Compute the sheet height: rows = (the larger of 447 and the highest authored id, integer-divided by 16) + 1. With ids up to 447 that is 28 rows, so the output is 512 x 896 pixels. The sheet grows only by whole rows of 16 tiles.
5. Create a fully transparent RGBA image of that size and paste the base sheet at the top-left corner, unchanged (the first 23 rows of the output are pixel-identical to the base sheet).
6. For every id from 368 to (rows x 16) - 1: if a text tile exists, paste it; otherwise, if the id is at most 447, paste a "missing" placeholder; otherwise leave the cell transparent.
7. Save as an optimised PNG and print one line: "wrote <path relative to the current directory>: <rows> rows, <count> authored tiles (<comma-separated sorted ids, or 'none'>)".

**Id-to-position rule** (shared by the tool, the dump tool and the app): column = id mod 16, row = id divided by 16 (integer), pixel origin = (column x 32, row x 32). The app (Part 11) loads the sheet unscaled, cuts it into (width / 32) x (height / 32) separate 32 x 32 bitmaps indexed by that rule, 448 in all, and draws them with bitmap filtering and dithering off.

**The placeholder** for a reserved id with no file, so a forgotten tile is obvious in the game: a transparent cell with a rectangle from (2, 2) to (29, 29), 1-pixel outline bright magenta #ff00ff, fill dark purple #500050, and the id written in white in the imaging library's small default font with its top-left at (5, 10).

Resulting sheet layout of the Wild area:

| Sheet row | Ids | Content |
|---|---|---|
| 23 | 368–383 | Wild objects (vending items, hats on the floor, egg, fishing items, cassette) |
| 24 | 384–399 | Wild monsters, ATM, coin and arrow icons, hat overlays, pet heart, egg-crack frames |
| 25 | 400–415 | Weather overlays, sun, moon, eight glitch overlays |
| 26 | 416–431 | Spare: placeholders |
| 27 | 432–447 | Spare: placeholders |

## 4. The tile dump tool

`tools/tiles/dump_tile.py <id> [--base]` prints one tile of the current shipped sheet (or, with `--base` anywhere on the command line, of the untouched base sheet) in the text tile format, so an author can study the original art style or check a new tile.

1. Locate the tile by the id-to-position rule and read its 32 x 32 pixels as RGBA.
2. A pixel with alpha exactly 0 prints as `.`. Any other pixel is keyed by its red, green and blue only (alpha ignored); each distinct colour gets the next unused character, in order of first appearance scanning left to right, top to bottom, from this fixed sequence of 55 characters: k w r g b y c m o K W R G B Y C M O a n p q s t u v x z A D E F H I J L N P Q S T U V X Z 0 1 2 3 4 5 6 7 8 9. A 56th and later colour all print as `?`.
3. Output: a comment line "# tile <id> from <file name>", then the palette as comment lines of at most six "char=#rrggbb" entries each (lower-case hex), then the 32 rows.

Because the first pixel of nearly every base tile is the floor colour, dumped base tiles almost always have `k` = #426b6b; the Wild object tiles follow that habit.

## 5. Wild tile catalogue: overview

Object ids and monster ids below are the enum positions used by the game (Parts 04 and 06 give the tables; the Wild objects are ids 101 to 117 appended at the end of the object list, the Wild monsters are ids 57 to 60 inserted between the red dragon, 56, and the first demon lord, 61). The object tile list maps objects 101..117, in order, to tiles 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383, 388. The monster tile list maps monsters 57..60 to tiles 384..387.

| Id | File name stem | Owner feature | What it shows | Used by | Background | Sprite bounding box (x, y) |
|---|---|---|---|---|---|---|
| 368 | vending_machine | Vending | red vending machine | map tile of object 101 "a humming vending machine" | floor | 6–26, 1–30 |
| 369 | can_of_fizz | Vending | green soda can | object 102 "a can of Fizz" | floor | 10–22, 5–29 |
| 370 | slice_of_pizza | Vending | pizza slice, point down | object 103 "a slice of pizza" | floor | 4–27, 4–30 |
| 371 | energy_drink | Vending | tall navy can with a bolt | object 104 "an energy drink" | floor | 11–21, 3–30 |
| 372 | rubber_duck | Vending | yellow duck facing right | object 105 "a rubber duck" | floor | 3–28, 5–28 |
| 373 | hat_cone | Hats | traffic cone on the floor | object 106 "a traffic cone" | floor | 5–29, 4–26 |
| 374 | hat_propeller | Hats | propeller beanie on the floor | object 107 "a propeller beanie" | floor | 6–28, 8–22 |
| 375 | hat_wizard | Hats | wizard hat on the floor | object 108 "a pointy wizard hat" | floor | 3–29, 3–25 |
| 376 | hat_party | Hats | party hat on the floor | object 109 "a party hat" | floor | 9–25, 5–25 |
| 377 | hat_tinfoil | Hats | tinfoil hat on the floor | object 110 "a tinfoil hat" | floor | 6–28, 7–25 |
| 378 | egg | Pets | speckled egg with a glow | object 111 "a mysterious egg" | floor | 5–26, 3–30 |
| 379 | fishing_rod | Fishing | rod, reel, line, bobber | object 112 "a fishing rod" | floor | 2–29, 3–31 |
| 380 | fish | Fishing | silver-blue fish facing left | object 113 "a fish" | floor | 3–29, 6–25 |
| 381 | old_boot | Fishing | worn brown boot | object 114 "an old boot" | floor | 6–29, 5–28 |
| 382 | bottle | Fishing | green bottle with a note | object 115 "a message in a bottle" | floor | 4–29, 5–26 |
| 383 | cassette | Radio | cassette tape | object 116 "a cassette tape" | transparent | 3–28, 8–25 |
| 384 | null_pointer | Glitch | scrambled block creature | monster 57 "null pointer"; its trading card | floor | 2–28, 3–29 |
| 385 | fountain_kraken | Fishing | purple kraken in a pool | monster 58 "fountain kraken"; its card | floor | 1–31, 1–30 |
| 386 | tax_auditor | LarnCoin | man in a grey suit with a clipboard | monster 59 "tax auditor"; its card | floor | 5–26, 1–30 |
| 387 | roomba | Vending | robot vacuum disc | monster 60 "roomba"; its card | floor | 2–29, 12–28 |
| 388 | coin_atm | LarnCoin | cash machine | object 117 "a LarnCoin ATM" | floor | 6–26, 3–29 |
| 389 | coin_icon | LarnCoin | gold coin with an L | not referenced by any code | transparent | 4–27, 4–27 |
| 390 | arrow_up | LarnCoin | green up arrow | not referenced by any code | transparent | 3–28, 3–28 |
| 391 | arrow_down | LarnCoin | red down arrow | not referenced by any code | transparent | 3–28, 3–28 |
| 392 | overlay_cone | Hats | small cone, worn | drawn on the player while object 106 is worn | transparent | 9–22, 0–8 |
| 393 | overlay_propeller | Hats | small beanie, worn | worn object 107 | transparent | 9–21, 0–7 |
| 394 | overlay_wizard | Hats | small wizard hat, worn | worn object 108 | transparent | 7–22, 0–9 |
| 395 | overlay_party | Hats | small party hat, worn | worn object 109 | transparent | 11–19, 0–8 |
| 396 | overlay_tinfoil | Hats | small foil hat, worn | worn object 110 | transparent | 9–21, 0–7 |
| 397 | pet_heart | Pets | tiny red heart | drawn on the pet's cell | transparent | 24–30, 1–7 |
| 398 | egg_crack1 | Pets | whole egg with a crack | hatching animation, frame 1 | transparent | 6–24, 5–28 |
| 399 | egg_crack2 | Pets | broken shell and fragments | hatching animation, frame 2 | transparent | 3–28, 2–28 |
| 400 | rain1 | Atmosphere | rain streaks, frame A | rain and storm on surface floor cells | transparent | scattered |
| 401 | rain2 | Atmosphere | rain streaks, frame B | as 400 | transparent | scattered |
| 402 | snow1 | Atmosphere | snowflakes, frame A | snow on surface floor cells | transparent | scattered |
| 403 | snow2 | Atmosphere | snowflakes, frame B | as 402 | transparent | scattered |
| 404 | stars1 | Atmosphere | night stars, frame A | unexplored surface cells at night | transparent | scattered |
| 405 | stars2 | Atmosphere | night stars, frame B | as 404 | transparent | scattered |
| 406 | sun | Atmosphere | sun with eight rays | HUD icon by day | transparent | 3–28, 3–28 |
| 407 | moon | Atmosphere | crescent moon | HUD icon by night | transparent | 5–25, 5–26 |
| 408 | glitch_noise | Glitch | static noise | corruption overlay | transparent | whole cell |
| 409 | glitch_scanlines | Glitch | broken scanlines | corruption overlay | transparent | whole cell |
| 410 | glitch_inverted | Glitch | solid blocks | corruption overlay | transparent | whole cell |
| 411 | glitch_missing | Glitch | "MISSIN" text and checker | corruption overlay | transparent | whole cell |
| 412 | glitch_tear | Glitch | red/green/cyan tear lines | corruption overlay | transparent | 2–30, 8–22 |
| 413 | glitch_hexdump | Glitch | dripping green bytes | corruption overlay | transparent | 1–29, 2–25 |
| 414 | glitch_checker | Glitch | checker corner and "0x0" | corruption overlay | transparent | whole cell |
| 415 | glitch_question | Glitch | broken yellow "?" | corruption overlay | transparent | whole cell |

Notes on use:

- The object tiles 368–383 and 388 are what the map shows for a known Wild object lying on a cell, exactly like base objects (Part 14). While the glitch effect runs, the game may draw any carriable item as another item's tile (Part 12); the Wild object tiles take part in that like any other.
- The monster tiles 384–387 are also the portraits on those monsters' trading cards (Part 13).
- **Tiles 389, 390 and 391 are authored and shipped but nothing draws them.** The plan allowed the LarnCoin ticker to be "tiles 389–391 or just text", and the HUD ticker is text coloured green or red (Part 11/13). A recreation may omit them or keep them as spares; they are described below for completeness.
- The plan's ownership table calls 416–431 "Cards: this row is spare" (cards are drawn in code, Part 13) and 432–447 "Spare: ask before using". Both rows hold only placeholders.

## 6. The Wild tiles one by one

Each entry lists the palette (character meanings are omitted; colours are named by role), then the silhouette row by row, then shading and details. Where an interior is irregular by design it is described statistically rather than pixel by pixel.

### 368 — vending machine (Vending)

Palette: floor #426b6b; ground shadow #2f4c4c; black #000000; cabinet dark red #7c1a1a, mid red #b02828, light red #e05a4a; window glass dark #1e2e3c; glass edge highlight #5a7a8c; grey #9a9a9a (shelves and control panel); product and light colours yellow #e8c040, green #40d060, orange #e07020, cyan #3cb0c8, pink #c84090.

- **Cabinet outline (black):** top edge row 1, x 7–24; bottom edge row 29, x 7–24; left edge x 6 and right edge x 25, rows 2–28 (so the four corners are cut by one pixel).
- **Shadow:** a column at x 26 from row 4 to row 29, and a bar on row 30 from x 6 to x 26.
- **Top band:** rows 2–3, x 7–24, light red (the lit top of the cabinet).
- **Side bands:** rows 4–28, x 7–8 and x 23–24, dark red.
- **Front panel:** x 9–22 is mid red on rows 4, 21 and 26; on rows 5–20 x 9 and x 19 are mid red (frame of the window), and on rows 22–25 x 9 and x 22 are mid red.
- **Window:** x 10 is the glass-edge highlight on rows 5–20; x 11–18, rows 5–20, is dark glass with three grey shelves (full-width grey lines on rows 8, 13 and 18). Products are 1 pixel wide and 2 rows tall, sitting on each shelf at x 11, 13, 15, 17 with glass between: top shelf (rows 6–7) yellow, orange, yellow, orange; middle shelf (rows 11–12) cyan, pink, cyan, pink; bottom shelf (rows 16–17) orange, yellow, cyan, orange.
- **Control strip:** x 20–22, rows 5–20, grey; its centre column x 21 carries a yellow light on rows 6, 10 and 12, a green light on rows 8 and 19, and a black coin slot on rows 15–17.
- **Dispensing tray:** row 22, x 10–21, dark red lip; rows 23–25, x 10–21, black opening.
- **Base:** rows 27–28, x 7–24, dark red.

### 369 — can of Fizz (Vending)

Palette: floor; ground shadow; black; can dark green #1f6a2c, mid green #35a044, light green #7ad078; label white #e8e8e0, label shade #b0b0a8; red #c42020; metal #909090, metal highlight #d8d8d8.

- **Tab and lid:** row 5 black at x 16–17. Row 6: black x 12–15, metal x 16–17, black x 18–19. Row 7: black x 11, metal x 12, highlight x 13–15, metal x 16–19, black x 20. Row 8: black x 11, metal x 12–19, black x 20.
- **Body:** rows 9–26, black at x 10 and x 21. Green rows (9–14 and 20–26) read, from x 11: dark, dark, mid, light, light, mid, mid, dark, dark, dark (a vertical light stripe at x 14–15).
- **Label band:** rows 15–19: label shade at x 11–12 and x 19–20, white at x 13–18. A small red pennant on it: row 16 x 14–16, row 17 x 14–15, row 18 x 14.
- **Bottom rim:** row 27 black x 11, metal x 12–19, black x 20; row 28 black x 12–19.
- **Shadow:** row 29, x 10–22.

### 370 — slice of pizza (Vending)

Palette: floor; ground shadow; black; cheese #f0c838, cheese highlight #ffe880, cheese shade #c89020; crust #b06828, crust highlight #d89048; pepperoni #c42820, pepperoni highlight #f06048; green herb #2c8c2c.

- **Crust:** black top edge row 4, x 5–26. Rows 5–8 have black at x 4 and x 27; inside (x 5–26) row 5 is crust highlight, rows 6–7 crust, row 8 crust at x 5 and x 26 with cheese shade between.
- **Slice:** row 9: black x 5–6, cheese highlight x 7, cheese x 8–23, shade x 24, black x 25–26. Below that a triangle narrowing by one pixel per side every two rows, with black edges at: rows 10–11 x 7 and x 24; 12–13 x 8 and x 23; 14–15 x 9 and x 22; 16–17 x 10 and x 21; 18–19 x 11 and x 20; 20–21 x 12 and x 19; 22–23 x 13 and x 18; rows 24–27 x 14 and x 17; tip row 28, black x 15–16.
- **Shading:** on every row the pixel just inside the left edge is cheese highlight and the pixel just inside the right edge is cheese shade, the rest cheese. At the tip (rows 24–25) the two interior pixels are highlight then shade; on rows 26–27 they swap (shade then highlight).
- **Toppings:** four pepperoni slices, each 3 wide by 2 tall with the highlight colour on its top-left pixel: at x 11–13 rows 11–12; x 19–21 rows 12–13; x 15–17 rows 17–18; x 15–17 rows 22–23 (this last one fills the slice width, replacing the shade pixel). Three single green herb pixels at (9, 10), (22, 10) and (18, 19).
- **Shadow:** row 30, x 8–24 (note the one-row gap of floor between the tip and the shadow).

### 371 — energy drink (Vending)

Palette: floor; ground shadow; black; can navy #1c2448, darker navy edge #0e1230, blue highlight #3c4c80; bolt yellow #f0e030, bolt dark yellow #b0a018; green band #40d040; metal #909090, metal highlight #d8d8d8.

- **Shape:** a tall slim can. Row 3 black x 13–18. Row 4: black x 12, metal x 13, highlight x 14–15, metal x 16–18, black x 19. Row 5: black x 12, metal x 13–18, black x 19. Rows 6–27: black at x 11 and x 20. Row 28: black x 12, metal x 13–18, black x 19. Row 29: black x 13–18. Shadow row 30, x 11–21.
- **Body columns** (rows 6–27): x 12 dark edge, x 13 navy, x 14 blue highlight stripe, x 15–18 navy, x 19 dark edge. Rows 22–23 are instead a solid green band across x 12–19.
- **Lightning bolt** (overrides the body, including the highlight stripe where it crosses): row 9 dark yellow x 16, yellow x 17; row 10 yellow x 16; row 11 dark yellow x 15, yellow x 16; row 12 yellow x 15; row 13 dark yellow x 14, yellow x 15–17; row 14 yellow x 14–17; row 15 yellow x 16; row 16 dark yellow x 15, yellow x 16; row 17 yellow x 15; row 18 dark yellow x 14, yellow x 15; row 19 yellow x 14. (A zig-zag from upper right to lower left with a wide bar in the middle.)

### 372 — rubber duck (Vending)

Palette: floor; ground shadow; black; yellow #f0c830, shaded yellow #c09018, highlight #fff090; beak orange #e07018, beak dark #a04808; eye glint white #ffffff.

The duck faces right: round head upper right, beak pointing right, body below with a raised tail at the left.

- **Head:** black outline row 5 x 16–21; row 6 x 15 and x 22; row 7 x 14 and x 23; rows 8–13 x 13 on the left, and x 24 on the right for rows 8–10 only (below that the beak joins the head). Fill yellow, with highlight pixels at x 17–18 on row 7 and x 16 on row 8. Eye: black at x 20–21 on row 9 and x 20 on row 10, white glint at (21, 10).
- **Beak:** row 10 black x 24–27 (top edge); rows 11–12 orange x 24–27 with black at x 28; row 13 dark orange x 24–27 with black at x 28; row 14 black x 23–27 (bottom edge, continuing the neck line).
- **Tail:** row 13 black x 5–8; row 14 black x 4, yellow x 5–8, black x 9.
- **Neck join:** row 14 black x 11–14, yellow x 15–22.
- **Body:** row 15: black x 3, yellow x 4–9, black x 10, yellow x 11–21, black x 22. Row 16: black x 3, yellow x 4–21, black x 22. Row 17: black x 3 and x 23. Row 18: black x 3 and x 24. Rows 19–22: black x 4 and x 25. Row 23: black x 5 and x 24. Row 24: black x 6 and x 23, interior all shaded yellow. Row 25: black x 7 and x 22, interior shaded. Row 26: black x 8–10, shaded x 11–18, black x 19–21. Row 27: black x 11–18.
- **Details:** a highlight streak on row 17, x 10–13. The wing is shaded yellow: row 18 x 9–16, row 19 x 9–10 and x 15–16 (yellow between), rows 20–21 x 9–16.
- **Shadow:** row 28, x 6–26.

### 373 — traffic cone on the floor (Hats)

Palette: floor; black; orange light #f09a48, mid #d8701c, dark #9c4a12, darkest #5c2a0a; stripe white #eeeae0, stripe shade #bdb8ad. No teal ground shadow: all five floor-hat tiles instead have a hard black cast shadow, a three-row wedge under the hat that extends to the right.

- **Cone:** apex row 4, black x 15–16. Each row has black at both ends; widths grow downward: rows 5–6 span x 14–17; rows 7–9 x 13–18; rows 10–11 x 12–19; rows 12–14 x 11–20; rows 15–16 x 10–21; rows 17–19 x 9–22; row 20 x 8–23; row 21 x 6–25 (black x 6–8 and x 23–25, the top of the base plate).
- **Bands:** orange rows are 5–9, 13–15 and 19–21; white stripe rows are 10–12 and 16–18. Within an orange row the left part is light orange, the middle mid orange, the right part dark orange (about one third each; rows 5–6 are just light then dark). Within a stripe row the left two thirds are white and the right third stripe shade.
- **Base plate:** rows 22–23: black x 5, mid orange x 6–11, dark orange x 12–19, darkest x 20–25, black x 26.
- **Cast shadow (black):** row 24 x 6–29, row 25 x 8–29, row 26 x 11–26.

### 374 — propeller beanie on the floor (Hats)

Palette: floor; black; blue #4870c8, dark blue #2c4890; green #4cac54, dark green #2c7834; red #d8443c, dark red #a42820; yellow #eedc37, dark yellow #b99018; stalk silver #c8ccd0, dark silver #7c8088; button white #eeeae0, button shade #bdb8ad.

- **Propeller:** row 8 black x 9–14 and x 17–22. Row 9: black x 8, red x 9–14 (left blade), black x 15–16, blue x 17–22 (right blade), black x 23. Row 10: black x 9–10, dark red x 11–14, silver x 15, dark silver x 16, dark blue x 17–20, black x 21–22. Row 11: black x 11–14, silver x 15, dark silver x 16, black x 17–20.
- **Cap:** a low dome of four vertical panels, left to right red, yellow, blue, green, the green panel shaded dark green on its right. Row 12: black x 11–12, red x 13–14, white button x 15, button shade x 16, green x 17, dark green x 18, black x 19–20. Row 13: black x 9–10, red x 11–13, yellow x 14–15, blue x 16–17, green x 18, dark green x 19–20, black x 21–22. Rows 14–15: black x 8, red x 9–12, yellow x 13–15, blue x 16–18, green x 19–20, dark green x 21–22, black x 23. Row 16: black x 7, red x 8–11, yellow x 12–15, blue x 16–19, green x 20, dark green x 21–23, black x 24. Rows 17–18: black x 6, red x 7–11, yellow x 12–15, blue x 16–19, green x 20–21, dark green x 22–24, black x 25.
- **Rim:** row 19: black x 6, then the dark shade of each panel (dark red x 7–11, dark yellow x 12–15, dark blue x 16–19, dark green x 20–24), black x 25–26.
- **Cast shadow (black):** row 20 x 7–28, row 21 x 9–28, row 22 x 11–26.

### 375 — pointy wizard hat on the floor (Hats)

Palette: floor; black; hat light blue #5068c0, mid blue #2c3c8c, navy #18204c; yellow #eedc37, dark yellow #b99018.

- **Cone:** leans so its right edge is vertical: the right outline is black at x 19 for rows 4–14 and x 20 for rows 15–18, with the tip a single black pixel at (18, 3). The left outline steps left going down: x 17 (row 4), 16 (5), 15 (6), 14 (7–8), 13 (9), 12 (10–11), 11 (12), 10 (13–14), 9 (15–16), 8 (17–18).
- **Cone shading:** each row is light blue on the left, mid blue in the middle, navy on the right (the navy part is 1 pixel wide near the tip and 3 wide from row 13 down).
- **Stars:** two plus-shaped stars, each a yellow centre with four dark-yellow arms: centres at (13, 13) and (16, 17).
- **Hat band:** row 19: black x 7, yellow x 8–16, dark yellow x 17–20, black x 21.
- **Brim:** row 20: black x 5–7, light blue x 8–11, mid blue x 12–16, navy x 17–20, black x 21–24. Row 21: black x 4, light blue x 5–10, mid blue x 11–18, navy x 19–24, black x 25. Row 22: black x 3, mid blue x 4–18, navy x 19–25, black x 26.
- **Cast shadow (black):** row 23 x 4–29, row 24 x 6–29, row 25 x 9–26.

### 376 — party hat on the floor (Hats)

Palette: floor; black; cyan #58d0e8; pink #f080b8, dark pink #c84888, plum #80285c; blue #2c4890; pom-pom yellow #eedc37, dark yellow #b99018.

- **Pom-pom:** row 5 black x 15–16; row 6 black x 14, yellow x 15–16, black x 17; row 7 black x 14, yellow x 15–16, dark yellow x 17, black x 18; row 8 black x 14, yellow x 15, dark yellow x 16, black x 17.
- **Cone:** a narrow cone, both edges black. Spans: rows 9–10 x 14–17; rows 11–12 x 13–18; rows 13–15 x 12–19; rows 16–18 x 11–20; rows 19–20 x 10–21; row 21 x 9–22; row 22 x 9–23 (two black pixels at the right end).
- **Pattern:** diagonal candy stripes about 2 pixels thick running from lower left to upper right, alternating pink and cyan. The left two thirds of each row use the bright colours (pink, cyan, with dark pink where a stripe turns away from the light); the right third is in shadow and uses plum and blue for the same stripes. Any equivalent stripe pattern obeying that light/shadow split matches the look.
- **Cast shadow (black):** row 23 x 10–25, row 24 x 12–25, row 25 x 14–23.

### 377 — tinfoil hat on the floor (Hats)

Palette: floor; black; foil bright #f4f8fa, mid #c4ccd2, shaded #8c949c, dark crease #5a6066.

- **Silhouette:** a lumpy, crumpled cone with a small point. Black outline pixels at: row 7 x 15–16; row 8 x 14 and x 17; row 9 x 13 and x 18; row 10 x 12 and x 18; row 11 x 11 and x 19; row 12 x 10 and x 20; row 13 x 10 and x 21; row 14 x 9 and x 21; rows 15–16 x 10 and x 21; rows 17–18 x 9 and x 22; row 19 x 8 and x 23; row 20 x 8 and x 23–24; row 21 x 7 and x 25; row 22 x 6 and x 25–26.
- **Interior:** deliberately irregular. Bright foil dominates the left third, mid foil the middle, shaded foil the right, but each region is broken by one or two pixels of a neighbouring shade per row (stray bright glints on the right, shaded creases on the left) to read as crumpled metal. The bottom row of the hat (row 22) is a rim that alternates dark crease pixels with shaded and mid pixels.
- **Cast shadow (black):** row 23 x 7–28, row 24 x 9–28, row 25 x 11–26.

### 378 — mysterious egg (Pets)

Palette: floor; black; shell highlight #fff6d8, shell #e8dcb0, shell shade #c4b48a, shell dark #8c8060; speckle purple #8a5a9a, speckle dark purple #5c3a6c; glow inner mint #9ad8c0, glow outer teal #5f9488. No ground shadow.

- **Shell outline (black):** row 5 x 13–17; then pairs of edge pixels: row 6 x 12 and 18; row 7 x 11 and 19; row 8 x 10 and 20; row 9 x 9 and 21; row 10 x 8–9 and 21–22; rows 11–12 x 8 and 22; rows 13–16 x 7 and 23; rows 17–19 x 6 and 24; rows 20–22 x 7 and 23; row 23 x 7–8 and 22–23; row 24 x 8 and 22; row 25 x 9 and 21; row 26 x 9–10 and 20–21; row 27 x 11–12 and 18–19; row 28 x 13–17. (An egg 19 wide at its widest, narrower at the top.)
- **Shell shading:** light from the upper left. The highlight colour fills the upper-left region: 5 pixels wide on rows 6–9 (starting just inside the left outline), about 4 wide on rows 10–13, 3 on row 14, 2 on row 15 and a single pixel on rows 16 and 17, ending at (7, 17). The plain shell colour fills the middle. Shell shade enters from the right at row 13 and grows to cover the lower right; shell dark starts at the right edge on row 17 and by rows 23–27 covers the right half of the bottom.
- **Speckles:** nine light purple pixels at (17, 9), (12, 10), (15, 13), (19, 13), (11, 15), (14, 18), (10, 20), (11, 20), (22, 21); five dark purple pixels at (20, 18), (21, 21), (16, 22), (19, 24), (13, 25).
- **Glow halo:** two concentric 1-pixel rings hugging the outline: mint directly outside the black, teal outside the mint. The halo spans x 5–26 and rows 3–30 (row 3 and row 30 are just teal at x 14–16; rows 4 and 29 are teal, mint x 14–16, teal). The mint ring is missing where the outline bulges (rows 10, 13, 17–19 and 23), where the teal touches the black directly.

### 379 — fishing rod (Fishing)

Palette: floor; ground shadow; black; rod brown #8a5a2c, rod highlight #b88050; cork #c8a070, cork highlight #e0c090; reel metal #9a9aa8, reel dark #55556a; white #f0f0f0; line pale grey #c8d0d0; bobber red #c42020.

- **Rod:** a straight 45-degree diagonal from the tip, a black pixel at (26, 3), down and left to the reel. On each row from 4 to 20 the rod moves one pixel left: its left black outline is at x 25, 24, 23, 22, 21, 20, 19, 18, 17, 17, 16, 15, 14, 13, 12, 11, 10 for rows 4 to 20 (one pixel left per row, with one repeat at rows 12–13), followed by 1 to 4 pixels of brown with the highlight colour running down the centre, then a black right outline (from row 7 down; on rows 5–6 the fishing line touches the rod instead).
- **Line:** a vertical pale grey line at x 27 from row 5 to row 16, hanging straight down from the tip.
- **Bobber:** red at (27, 17) and x 26–28 on row 18; white x 25–29 on row 19, x 26–28 on row 20, (27, 21). Below it a hook in line colour: x 26–27 on row 22 and (26, 23).
- **Reel:** a block just above the handle: row 21 metal x 10–12; row 22 metal x 9–11, dark x 12–13; row 23 metal x 9–10, a white glint at (11, 23), dark x 12–13; row 24 metal x 9, dark x 10–13; row 25 dark x 10–12; black outline at x 14 on rows 21–24, x 13 on row 25 and x 10–12 on row 26.
- **Cork handle:** a diagonal band continuing the rod to the lower left, 4 to 5 pixels wide, with a 1-pixel cork-highlight stripe along its centre: it occupies x 8 on row 23, x 7–8 on row 24, x 6–9 on row 25, x 5–9 on row 26, x 4–8 on row 27, x 3–7 on row 28, x 3–6 on row 29, x 3–5 on row 30, outlined black on both sides (left outline at x 7, 6, 5, 4, 3, 2, 2, 2 for rows 23–30) and closed by black x 3–5 on row 31.
- **Shadow:** only a short bar, row 30, x 7–12. The sprite touches the bottom edge of the cell, breaking the usual margin.

### 380 — fish (Fishing)

Palette: floor; ground shadow; black; back dark blue #2c4a70; mid blue #4a78a8; flank light blue #8ab0d0; belly silver #d8e8f0.

The fish faces left, with a forked tail at the right.

- **Dorsal fin:** a dark-blue triangle with black edges: apex black at (15, 6); row 7 black x 14 and 16; row 8 black x 13 and 17; row 9 black x 12 and 17.
- **Body outline (black):** row 10 x 10–11 and x 18; row 11 x 7–9 and x 19–20; row 12 x 6 and x 21; row 13 x 5 and x 22; row 14 x 4 and x 23; rows 15–18 x 3 (the snout); row 19 x 4 and x 23; row 20 x 5 and x 22; row 21 x 6 and x 21; row 22 x 7–9 and x 18–20; row 23 x 10–17.
- **Tail:** joined to the body on rows 14–18 (no outline between). Upper lobe: black (28, 9); row 10 black x 27, mid blue x 28, black x 29; row 11 black x 26, mid x 27–28, black x 29; row 12 black x 25, mid x 26–28, black x 29; row 13 black x 24, mid x 25–27, black x 28. Rows 14–19 the tail is filled to x 27 with black at x 28. Lower lobe mirrors it in light blue: row 20 black x 24, light x 25–27, black x 28; row 21 black x 25, light x 26–28, black x 29; row 22 black x 26, light x 27–28, black x 29; row 23 black x 27, light x 28, black x 29; row 24 black (28, 24).
- **Horizontal colour bands:** rows up to 13 dark blue; rows 14–16 mid blue; rows 17–19 light blue; rows 20–22 silver.
- **Details:** a vertical dark-blue gill line at x 11 on rows 14–20. Eye: black at (8, 15) and (8, 16) with a silver glint at (9, 15). Mouth: one black pixel at (5, 18). Belly fin: mid blue at x 13–16 on row 20 and x 14–15 on rows 21–22.
- **Shadow:** row 25, x 6–24.

### 381 — old boot (Fishing)

Palette: floor; ground shadow; black; leather light #8a6040, mid #6a4628, dark #4a3018; sole #2a2018; laces tan #c8b890; water drip light blue #8ac8e0.

The boot stands upright, toe pointing right.

- **Shaft:** black top row 5, x 8–16; rows 6–16 black at x 7 and x 17. Inside, x 8–10 is light leather (a lit left edge that continues down to row 23) and x 11–16 mid leather; row 7 is light leather all the way across (a cuff line).
- **Laces:** tan 2-pixel dashes zig-zagging down the front: x 13–14 on rows 9, 12 and 15; x 14–15 on rows 10, 13 and 16.
- **Foot:** the upper edge steps out to the right: row 16 black x 17–18; row 17 mid leather to x 18, black x 19–23; row 18 mid to x 23, black x 24–25; rows 19–20 mid to x 25, black x 26; row 21 black x 26; rows 22–23 black x 27. Rows 21–23 are dark leather from x 11 (the lower part of the foot).
- **Patch:** a black plus sign on the toe: (21, 20), x 20–22 on row 21, (21, 22).
- **Sole:** rows 24–26: black x 6, sole colour x 7–27, black x 28, with one mid-leather pixel at (12, 26). Bottom outline row 27: black x 7–23 and x 26–27, leaving a 2-pixel gap of floor at x 24–25 (the sole flapping loose at the toe).
- **Drips:** two light-blue pixels at (29, 21) and (29, 22), in front of the toe.
- **Shadow:** row 28, x 6–28.

### 382 — message in a bottle (Fishing)

Palette: floor; ground shadow; black; glass light #80c890, mid #3a8a50, dark #1e5a30; cork #b08850; paper #f0e6c0, paper shade #c8b888; red #c42020.

The bottle lies on a diagonal, base at the lower left, neck and cork at the upper right.

- **Cork:** black x 26–28 on row 5; rows 6–7 black x 25 (row 6) or x 24 (row 7), cork to x 28, black x 29; row 8 black x 23, one mid-glass pixel x 24, cork x 25–28, black x 29; row 9 black x 22, mid x 23, light x 24, cork x 25–27, black x 28.
- **Neck and shoulder:** row 10 black x 15–21, mid x 22, light x 23, mid x 24–26, black x 27. Row 11 black x 12–14, light x 15–18, mid x 19–21, light x 22, mid x 23–25, black x 26.
- **Body:** the outline then runs down-left: left outline black at x 11 (row 12), 10 (13), 8–9 (14), 7 (15–16), 6 (17), 5 (18–19), 4 (20–22), 5 (23), 6 (24); right outline at x 25 (row 12), 24 (13), 23 (14–15), 22 (16–17), 21 (18), 20 (19–20), 18–19 (21), 17 (22), 16 (23), 13–15 (24); bottom row 25 black x 7–12.
- **Glass shading:** light glass along the upper-left side (5 to 6 pixels wide on rows 12–15, narrowing to 2 by row 20), mid glass in the middle and along the bottom-left, dark glass along the lower-right side from row 15 down.
- **Note:** a rolled cream note seen through the glass, a diagonal band 3 to 4 pixels thick running from about (8, 20) up to (18, 14), paper colour with paper shade on its lower-right side, and one red pixel at (13, 18) as its ribbon.
- **Shadow:** row 26, x 5–22.

### 383 — cassette tape (Radio)

Palette: black #000000; shell dark grey #3a3a40; shell edge light grey #6a6a72; shell shadow #24242a; label cream #e6e0c8; red #c42020; orange #d8862c; blue #3c4c8c; tape brown #5a3a22. (A tenth colour, #18181c, is declared and unused.)

**This is the only Wild object tile with a transparent background.** Everything outside the cassette is transparent, so on the map it appears on the black map background rather than on teal floor. A recreation that wants consistency could add the floor colour; to match the original, leave it transparent.

- **Shell:** a rectangle x 3–28, rows 8–25, black 1-pixel border. Inside the border: light grey along the top (row 9, x 4–27) and left (x 4, rows 9–23); shell shadow along the right (x 27, rows 10–24) and bottom (row 24, x 5–27); the rest of the frame (x 5 and x 26, rows 11–22) dark grey, with light grey corner pixels at (5, 10), (26, 10), (5, 23), (26, 23).
- **Label** (x 6–25): rows 10–11 red; rows 12 and 18 cream; row 13 cream with blue "handwriting" pixels at x 8, 10–11, 13–14, 16–17 and 19; rows 14–17 cream at x 6–9 and x 22–25 around the window, except row 17 where those pixels are orange (an orange stripe either side of the window's bottom edge).
- **Window:** black frame x 10–21 on rows 14 and 17, and black at x 10 and x 21 on rows 15–16. Inside, on rows 15–16: cream x 11, black x 12, cream x 13 (left spool), tape brown x 14–17, cream x 18, black x 19, cream x 20 (right spool).
- **Lower section:** row 19 dark grey x 5–26. Rows 20–23: a trapezoid-like inset outlined in shell shadow (row 20 x 8–23; x 8 and x 23 on rows 21–23) filled dark grey, with four black holes on row 22 at x 11, 14, 17 and 20 and two on row 23 at x 14 and 17.

### 384 — null pointer (Glitch)

Palette: floor; ground shadow; black; magenta #c43cb4, dark magenta #7a1f70; yellow #e0c848, dark yellow #9c8424; cyan #48c8c0, dark cyan #2a7c78; white #f0f0f0.

A humanoid figure assembled from a mosaic of "data blocks", with parts missing or displaced. The building block is 3 pixels wide by 3 rows tall in one hue: its top two rows are light, light, dark and its bottom row is dark, dark, dark (so every block has a dark right column and dark bottom row). Hues are magenta, yellow or cyan, chosen irregularly so neighbouring blocks usually differ.

- **Layout:** head x 9–19, rows 3–12 (black top edge row 3, x 10–16; black left edge x 9, rows 4–12). Torso rows 13–21, about x 6–26, with a stub of a left arm at x 7–9, rows 13–15, and a longer right arm reaching x 27 on rows 15–16. Two legs on rows 22–27: left leg x 9–13, right leg x 19–25, with floor showing between them on rows 26–27. Feet closed by black on row 28 (x 10–12 and x 21–24).
- **Face:** the left eye is a 3 x 3 white block at x 11–13, rows 7–9, with a black pupil at (13, 8). The right eye is displaced one row down: white at x 16–17 on rows 8 and 10 and at (16, 9), with a black pupil at (17, 9).
- **Glitching:** the black outline is jagged and broken, following block edges rather than a smooth contour; a few blocks are cut by black pixels; and loose pixels float off the body: yellow at (2, 3) and (28, 22), magenta at (24, 5) and (26, 19), cyan at (5, 16).
- **Colour budget** (for an equivalent): of the 1024 pixels about 612 are floor, 96 black, 49 yellow, 63 dark yellow, 41 magenta, 55 dark magenta, 35 cyan, 42 dark cyan, 13 white, 18 shadow.
- **Shadow:** row 29, x 8–25.

### 385 — fountain kraken (Fishing)

Palette: floor; black; purple #7a4aa8, light purple #a880d0, dark purple #4a2a70; eye yellow #e8d040; water light #5a90d0, water mid #3a68b0, water dark #24447a; sparkle #c8e8f8. (The ground-shadow colour is declared and unused.)

The largest Wild sprite: it fills the cell from x 1 to x 31.

- **Head dome:** black top row 1, x 12–19; then the black outline widens: row 2 x 11 and 20; row 3 x 10 and 21; rows 4–5 x 9 and 22; rows 6–13 x 8 and 23; row 14 x 9 and 23.
- **Head shading:** the upper-left of the dome (rows 2–6, from the left edge to about x 16) is light purple; the right 4 to 5 columns (x 19–22) are dark purple all the way down; the rest purple. Two single dark-purple "spots": (14, 5) and (12, 7).
- **Eyes:** black brows on row 10 at x 11–13 and x 18–20. Each eye is a yellow diamond with a black slit pupil: left eye yellow at x 11–13 (row 11), x 10–11 and x 14 (row 12), x 11 and x 13 (row 13), (12, 14), with black pupil at x 12–13 on row 12 and (12, 13); the right eye is the same shape shifted 7 pixels right (yellow x 18–20 on row 11, and so on; pupil x 19–20 on row 12 and (19, 13)).
- **Lower face:** rows 15–17, x 15–21 dark purple (a shadowed mouth area) between purple cheeks.
- **Tentacles:** six, all purple with black outlines.
  - Two outer arms rise at the far left and right and curl: each has a rounded top at rows 15–17 (left arm x 1–7, right arm x 25–31, black outline around purple), a "sucker" section on rows 18–19 where purple and black alternate pixel by pixel (left: black x 1, purple x 2, black x 3, purple x 4, black x 5; mirrored on the right at x 27–31), then sweeps inward on rows 20–22 to join the body, ending in a black-outlined base on row 23 (x 4–7 and x 25–28). One light-purple highlight pixel on each: (4, 21), (6, 22) on the left; (28, 21), (26, 22) on the right.
  - Four inner tentacles hang down from the face, separated by black. The outer pair are 3-pixel purple columns at x 8–10 and x 22–24 on rows 18–19; from row 20 they merge into the in-sweeping outer arms. The middle pair are 2 pixels wide on rows 18–19 (x 13–14 and x 18–19) and 3 wide on rows 20–22 (x 12–14 and x 18–20), each ending on row 23 as two pixels, one of them light purple: light at (12, 23) with purple at (13, 23), and purple at (19, 23) with light at (20, 23). Between the middle pair, at x 16 on rows 19–22, a single column of floor shows through.
- **Water pool:** an ellipse below: row 24 light water x 9–22 with sparkles at x 11 and x 21; row 25 light water x 5–26 with sparkles at x 9 and x 23; row 26 mid water x 4–27 with a sparkle at x 16; row 27 mid water x 3–28; row 28 mid water x 4–27; row 29 dark water x 5–26; row 30 dark water x 9–22. The pool has no black outline.

### 386 — tax auditor (LarnCoin)

Palette (26 colours, the richest tile): floor; ground shadow; black; suit grey #7a7a84, suit dark #4e4e56, suit light #a2a2ac; shirt white #e8e8e0; tie red #b02828, tie dark red #7c1a1a; skin #e0b08c, skin shade #b07c5c; hair dark #3c2a1c, hair highlight #5c4430; lens blue #4c6c8c, lens glint #c8e0f0; pencil gold #e8c040, pale yellow #f8f0d0; shoe black #202024, shoe highlight #5c5c64; clipboard brown #a87838, clipboard dark brown #7c5428; paper #ecead8, paper shade #bcbaa8; writing #6c6c78; clip metal #c0c0c8, clip metal dark #8a8a94.

A man standing facing the viewer, x 5–26, rows 1–28.

- **Hair:** black top row 1, x 13–18; row 2 black x 12 and x 19 with hair between; rows 3–4 black x 11 and x 20 with hair x 12–19; dark brown with four or five highlight pixels scattered on rows 2–3. On row 5 the hair is only the two side pixels x 12 and x 19, with skin x 13–18 between (the forehead).
- **Face:** rows 5–10 between black edges at x 11 and x 20. Skin, with the skin-shade colour down the right side (x 19) and across the chin (row 10, x 13–19). Row 7 is the glasses row and is 2 pixels wider for the ears: black x 10, ear x 11 (skin shade), then black, glint, lens, black, black, glint, lens, black across x 12–19, ear x 20, black x 21. Row 9 has a black 3-pixel mouth/moustache at x 15–17.
- **Neck and shoulders:** row 11: black x 9–13, skin shade x 14–17, black x 18–22.
- **Jacket:** rows 12–23, grey, with the left side lit (a suit-light column at x 7 and a lapel line of suit light at x 10 with a suit-dark line at x 9 beside it) and suit dark on the right side. Outline black at x 6 (rows 13–17) and x 5 (rows 18–24) on the left.
- **Shirt and tie:** a white V at the chest (row 12 x 13–18; row 13 x 13–14 and 17–18; row 14 x 14 and x 17), with the tie in the centre: red at x 15–16 on rows 13–17, dark red at x 15–16 on row 18 and (15, 19).
- **Pencil hand (viewer's left):** a vertical pencil at x 6: pale yellow on row 18, gold on rows 19–23, a shoe-black point at (6, 24); the hand is skin at x 7–8 on rows 22–23 (skin shade at (8, 23)).
- **Clipboard (viewer's right):** black-outlined board from x 17 to x 25, rows 15–25. Board edge: clipboard brown column at x 18 (rows 16–24) and bottom row 24 (x 18–24); dark brown at x 19 and x 23–24 on row 16. Metal clip: x 20–22 on rows 15–16, with one dark pixel at (21, 16). Paper: x 19–23, rows 17–23, with paper shade down x 24; three lines of "writing" on rows 18, 20 and 22 (x 20–23, the last one x 20–22). The holding hand is skin at x 24–25 on rows 19–20 (shade at (25, 20)), which pushes the outline out to x 26 on those rows.
- **Legs and shoes:** row 23 is the jacket hem, suit dark across x 9–16; two legs each 3 pixels wide (dark, grey, dark): left leg x 12–14 on rows 24–27, right leg x 17–19 on rows 26–27, black between and around. Shoes on row 28: black x 10, shoe black x 11, highlight x 12, shoe black x 13–14, black x 15–16, shoe black x 17, highlight x 18, shoe black x 19–20, black x 21.
- **Shadow:** two rows forming an ellipse: row 29 x 9–23, row 30 x 11–21.

### 387 — roomba (Vending)

Palette: floor; ground shadow; black; underside near-black #202024; body grey #4c4c54; top edge light #7c7c88; lid dark #2e2e34; button ring #8a8a94; button centre #c8c8d0; LED green #40e050; bristle grey #a0a0a0.

A low robot-vacuum disc seen from the front and slightly above, x 2–29, rows 12–26, sitting in the lower half of the cell.

- **Outline (black):** row 12 x 9–22; row 13 x 6–8 and x 23–25; row 14 x 4–5 and x 26–27; row 15 x 3 and x 28; rows 16–22 x 2 and x 29; row 23 x 3 and x 28; row 24 x 4–5 and x 23–27; row 25 x 6, x 8 and x 23; row 26 x 9–22.
- **Top face:** row 13, x 9–22, top-edge light. Rows 14–20 body grey, with a two-pixel green LED at x 16–17 on row 14. A darker oval lid in the centre: lid dark at x 10–21 (rows 15 and 20), x 8–23 (rows 16 and 19), x 7–24 (rows 17–18). The round button in the middle of the lid: button ring at x 14–17 (rows 16 and 19) and x 12–19 (rows 17–18), with the button centre at x 15–16 on row 18.
- **Front rim:** row 21 body grey x 6–25 with underside colour at x 3–5 and x 26–28; row 22 body grey x 9–22 with underside x 3–8 and x 23–28; underside also at (3, 20) and (28, 20).
- **Underside:** rows 23–25 underside colour, with two black wheels at x 7–9 and x 23–25 on rows 23–24, and four bristle pixels on row 25 at x 12, 14, 18 and 20.
- **Shadow:** row 28, x 3–29 (with a row of floor between the body and the shadow).

### 388 — LarnCoin ATM (LarnCoin)

Palette: floor; ground shadow; black; casing grey #6c7480, casing highlight #9aa2ac, casing shadow #464c56; key grey-white #d8d8d8; sign yellow #f8d860, sign dark yellow #d0a020, logo brown #7c5410; screen dark green #123a1c, screen mid green #2a6a3a, bright green #40d060; red #d04040.

- **Cabinet outline (black):** a rectangle x 6–24, rows 3–28 (full square corners), plus a black divider on row 8, x 6–24, under the sign.
- **Shadow:** a column at x 25 from row 6 to row 28, and a bar on row 29 from x 7 to x 26.
- **Sign** (x 7–23): rows 4–6 sign yellow, row 7 sign dark yellow. In its centre a small coin logo in logo brown with a dark-yellow middle: row 4 brown x 14–16; row 5 brown x 13–14, dark yellow x 15–16, brown x 17; row 6 brown x 13–15, dark yellow x 16, brown x 17; row 7 brown x 14–16.
- **Casing** (rows 9–27): x 7 casing highlight, x 23 casing shadow, x 8–22 casing grey except as follows.
- **Screen:** black frame x 9–21 on rows 10 and 16 and at x 9 and x 21 on rows 11–15. Inside (x 10–20, rows 11–15) dark green with a jagged bright-green price line rising to the right: bright at x 10–11 on row 15; x 12–13 and x 15–16 on row 14; x 14 on row 13; x 17–18 on row 12; x 19–20 on row 11. Two mid-green pixels at (10, 11) and (10, 13).
- **Keypad:** three rows of keys on rows 18, 20 and 22: key-white pixels at x 10, 12 and 14, and one coloured key at x 16: red on row 18, sign dark yellow on row 20, bright green on row 22.
- **Card slot:** black x 18–21 on row 18, with a casing-highlight lip x 18–21 on row 19.
- **Cash slot:** casing shadow x 11–19 on row 24; row 25 black x 10–12, bright green x 13–17 (a banknote), black x 18–20; row 26 dark green x 13–17 (the note's lower edge).

### 389 — coin icon (LarnCoin; shipped, unused)

Palette: black; rim brown #8c5c10; dark brown #5c3a08; dark gold #c89020; gold #e8b830; pale gold #f8e090. Transparent outside the coin.

- **Disc:** a circle 24 pixels across, x 4–27, rows 4–27, drawn as an octagon-like pixel circle: black row 4 and row 27 at x 11–20; the outline is 1 pixel thick on the straight runs (x 4 and x 27 on rows 11–20) and 2 pixels on the diagonals.
- **Rings:** just inside the black, a 1 to 2 pixel ring of rim brown; inside that gold. The upper-left and centre of the face is pale gold (the lit face); the gold remains as a band around it.
- **Bevel:** a second, thinner rim-brown arc runs 2 to 3 pixels inside the outer rim from about the 2 o'clock position clockwise round to 7 o'clock (x 23 on rows 12–19, curving along the bottom at row 23, x 12–19); the band between this arc and the outer rim is dark gold, so the lower right edge reads as a shaded bevel. The bottom rows 24–25 inside the rim are dark gold.
- **Emblem:** an embossed capital L with a crossbar (it reads like a struck-through L). Stem: x 12–14, rows 9–20, with dark brown at x 12 (the shadow side) and rim brown at x 13–14. Crossbar: row 13 rim brown x 10–17, row 14 dark brown x 10–17. Foot: rows 19–20 rim brown x 13–20, with a dark-brown shadow line on row 21, x 13–20.

### 390 — up arrow (LarnCoin; shipped, unused)

Palette: black; green #38b048; dark green #1c6c28; light green #80e090. Transparent background.

- **Head:** apex black at x 15–16 on row 3. Rows 4–15 form a triangle that widens one pixel per side per row: on row r the black edge pixels are at x = 18 - r and x = 13 + r (x 14 and 17 on row 4; x 3 and 28 on row 15). Inside, the first pixel after the left edge is light green, the last before the right edge is dark green, the rest green (row 4 is just light, dark).
- **Underside of the head:** row 16: black x 4–10, light x 11, green x 12–19, dark x 20, black x 21–27.
- **Shaft:** rows 17–26: black x 10, light x 11, green x 12–19, dark x 20, black x 21. Row 27: black x 10, dark green x 11–20, black x 21. Row 28: black x 11–20.

### 391 — down arrow (LarnCoin; shipped, unused)

Palette: black; red #c43030; dark red #7c1a1a; light red #f08070. Transparent background.

Exactly tile 390 mirrored top to bottom (row r of 390 becomes row 31 - r), recoloured green to red, dark green to dark red, light green to light red. So: row 3 black x 11–20; row 4 black x 10, dark red x 11–20, black x 21; rows 5–14 the shaft (black, light, red x 12–19, dark, black); row 15 the shoulder (black x 4–10 and x 21–27 around the shaft colours); row 16 the widest row (black x 3, light x 4, red x 5–26, dark x 27, black x 28); rows 17–27 narrowing one pixel per side per row to light, dark at x 15–16 on row 27; row 28 black x 15–16. The light edge stays on the left and the dark on the right.

### 392–396 — hat overlays, worn (Hats)

All five are transparent except for a miniature of the hat in the top rows of the cell, horizontally centred around x 15, in the same palettes as the floor versions (373–377) but without stars on the wizard hat. Each has a black outline including a black bottom row (the brim line that sits across the character's head). None has a shadow.

**392 cone** (rows 0–8): row 0 black x 14–15. Row 1: black x 13, light orange x 14, dark orange x 15, black x 16. Row 2: black x 13, light x 14, mid x 15, dark x 16, black x 17. Row 3 (white stripe): black x 12, white x 13–15, stripe shade x 16–17, black x 18. Row 4: black x 12, light x 13–14, mid x 15, dark x 16–17, black x 18. Row 5 (white stripe): black x 11, white x 12–16, shade x 17–18, black x 19. Row 6: black x 10–11, light x 12–13, mid x 14–16, dark x 17–19, black x 20–21. Row 7 (base plate): black x 9, mid orange x 10–13, dark x 14–17, darkest x 18–21, black x 22. Row 8: black x 10–21.

**393 propeller beanie** (rows 0–7): row 0 black x 10–14 and x 16–19. Row 1: black x 9, red x 10–14, black x 15, blue x 16–19, black x 20. Row 2: black x 10–11, dark red x 12–14, silver x 15, dark silver x 16, dark blue x 17–18, black x 19. Row 3: black x 11–12, red x 13, yellow x 14, white x 15, button shade x 16, dark green x 17, black x 18–19. Row 4: black x 10, red x 11–12, yellow x 13–14, blue x 15–16, green x 17, dark green x 18–19, black x 20. Row 5: black x 9, red x 10–12, yellow x 13–14, blue x 15–17, green x 18, dark green x 19–20, black x 21. Row 6 (rim, dark shades): black x 9, dark red x 10–12, dark yellow x 13–14, dark blue x 15–17, dark green x 18–20, black x 21. Row 7: black x 10–20.

**394 wizard hat** (rows 0–9): row 0 black x 16–17. Row 1: black x 15, light blue x 16, navy x 17, black x 18. Row 2: black x 14, light x 15, mid blue x 16, navy x 17, black x 18. Row 3: black x 13, light x 14, mid x 15–16, navy x 17, black x 18. Row 4: black x 12, light x 13–14, mid x 15, navy x 16–17, black x 18. Row 5 (band): black x 11, yellow x 12–15, dark yellow x 16–17, black x 18. Row 6: black x 9–10, light x 11–12, mid x 13–15, navy x 16–18, black x 19–20. Row 7: black x 8, light x 9–12, mid x 13–16, navy x 17–20, black x 21. Row 8: black x 7, mid x 8–17, navy x 18–21, black x 22. Row 9: black x 8–21.

**395 party hat** (rows 0–8): the pom-pom touches the top edge, so it has no outline above it. Row 0: black x 14, yellow x 15–16, black x 17. Row 1: black x 14, yellow x 15–16, dark yellow x 17, black x 18. Row 2: black x 14, yellow x 15, dark yellow x 16, black x 17. Row 3: black x 13, pink x 14, dark pink x 15, plum x 16, black x 17. Row 4: black x 13, pink x 14, dark pink x 15, blue x 16, black x 17. Row 5: black x 12, pink x 13–14, cyan x 15, blue x 16, plum x 17, black x 18. Row 6: black x 12, pink x 13, cyan x 14–15, plum x 16–17, black x 18. Row 7: black x 11, pink x 12, cyan x 13–14, dark pink x 15–16, plum x 17, blue x 18, black x 19. Row 8: black x 12–18.

**396 tinfoil hat** (rows 0–7): row 0 black x 14–15. Row 1: black x 13, bright x 14, shaded x 15, black x 16–17. Row 2: black x 12, bright x 13–14, mid x 15, bright x 16, shaded x 17, black x 18. Row 3: black x 11, bright x 12–14, mid x 15–16, shaded x 17–18, black x 19. Row 4: black x 10, bright x 11–13, mid x 14, bright x 15, mid x 16, shaded x 17–18, bright x 19, black x 20. Row 5: black x 10, bright x 11, shaded x 12, bright x 13, mid x 14–16, bright x 17–18, shaded x 19–20, black x 21. Row 6 (rim): black x 9, mid x 10, dark crease x 11, mid x 12–13, crease x 14–15, shaded x 16–18, mid x 19, crease x 20, black x 21. Row 7: black x 10–20.

### 397 — pet heart (Pets)

Palette: black; highlight pink #ff9a9a; red #d02828; dark red #8a1010. Transparent except a 7 x 7 heart in the top-right corner of the cell, x 24–30, rows 1–7.

Row 1: black x 25–26 and x 28–29. Row 2: black x 24, pink x 25–26, black x 27, red x 28–29, black x 30. Row 3: black x 24, pink x 25, red x 26–29, black x 30. Row 4: black x 24, red x 25–28, dark red x 29, black x 30. Row 5: black x 25, red x 26–27, dark red x 28, black x 29. Row 6: black x 26, dark red x 27, black x 28. Row 7: black x 27.

### 398 — egg crack, frame 1 (Pets)

Palette: black and the six shell and speckle colours of tile 378. Transparent outside the shell.

An opaque copy of the egg of tile 378, at exactly the same position (same black outline, same shading, same speckles), without the glow halo and without the floor, plus a black zig-zag crack across its middle. Crack pixels (black, replacing shell pixels): (13, 13); (14, 14); (14, 15) and (20, 15); (9, 16), (13, 16), (15, 16), (19, 16), (21, 16); (8, 17), (10, 17), (12, 17), (16, 17), (18, 17), (22, 17); (11, 18), (17, 18), (23, 18); (19, 19); (20, 20). The effect is a sawtooth line running across rows 16–18 from the left outline to the right outline, with one branch climbing to (13, 13), one to (20, 15) and one dropping to (20, 20).

### 399 — egg crack, frame 2 (Pets)

Palette as 398 (the light speckle colour is declared and unused). Transparent except:

- **Bottom half of the shell,** in place: rows 24–28 are identical to the egg's rows 24–28. Above them a jagged broken edge with four teeth: tooth tips are single black pixels on row 20 at x 8, 12, 16 and 20; on row 21 each tooth is black, shell, black (x 7–9, x 11–13, x 15–17, x 19–21, the shell pixel being shell colour for the first two, shell shade for the third and shell dark for the fourth) plus a black pixel at x 23; row 22: black x 7, shell x 8–9, black x 10, shell x 11, shade x 12–13, black x 14, shade x 15, dark speckle x 16, shade x 17, black x 18, shell dark x 19–21, black x 22–23; row 23: black x 7–8, shell x 9–10, shade x 11–16, dark x 17–21, black x 22–23.
- **Two flying shell fragments:** upper left, a small wedge: black x 5–6 on row 3; black x 4, highlight x 5, shell x 6, black x 7 on row 4; black x 3, highlight x 4, shell x 5, shade x 6, black x 7 on row 5; black x 4–6 on row 6. Upper right: black x 26–27 on row 2; black x 25, shell x 26, shade x 27, black x 28 on row 3; black x 25, shade x 26–27, black x 28 on row 4; black x 26–27 on row 5.
- **Four chips:** single shell-shade pixels at (10, 8), (22, 9), (25, 11) and (8, 12).

The middle of the cell (rows 7–19 apart from the chips) is transparent so the newborn pet shows through.

### 400 and 401 — rain, frames 1 and 2 (Atmosphere)

Palette: streak dark blue #3c5c8c; streak mid blue #7896c4; drop head pale blue #c8dcf4. Transparent background; row 31 is empty in both.

Each frame holds eight streaks. A streak is a 1-pixel-wide line 6 to 8 pixels tall that leans slightly right as it falls (it steps one pixel to the right once or twice along its length). Its colours from top to bottom: three to five dark-blue pixels, two mid-blue pixels, one pale pixel at the bottom.

Streaks of tile 400, given as (x, y) of the top pixel and of the bottom pale pixel: (27, 0) to (28, 5); (3, 1) to (5, 7); (17, 4) to (19, 11); (9, 13) to (11, 20); (22, 16) to (24, 22); (2, 21) to (4, 27); (14, 23) to (16, 30); (27, 24) to (28, 29).

Streaks of tile 401: (21, 0) to (23, 6); (7, 3) to (9, 10); (12, 9) to (13, 14); (28, 11) to (30, 17); (4, 15) to (6, 22); (18, 19) to (20, 25); (9, 25) to (10, 30); (25, 25) to (26, 30).

The two frames place their streaks in the gaps of each other so that alternating them reads as motion.

### 402 and 403 — snow, frames 1 and 2 (Atmosphere)

Palette: white #f8f8f8; pale grey-blue #d0d8e0; grey #8c96a4. Transparent background.

Two flake shapes. A big flake is 3 x 3: white centre, pale on the four edge-middles, grey on the four corners. A small flake is 2 pixels on one row: pale on the left, grey on the right.

Tile 402: big flakes with top-left corners at (4, 3), (19, 8), (10, 18), (25, 22); small flakes with their left pixel at (14, 2), (28, 4), (3, 13), (24, 15), (7, 27), (17, 28).

Tile 403: big flakes at (12, 5), (26, 11), (3, 21), (18, 24); small flakes at (22, 2), (3, 3), (9, 12), (16, 15), (29, 28), (11, 29).

### 404 and 405 — night stars, frames 1 and 2 (Atmosphere)

Palette: white #ffffff; warm white #e8e8d0; dim yellow #c8c090; dim blue-grey #5c6078. Transparent background.

Each frame has one big star and eight single-pixel stars. The big star is a plus sign 5 pixels across: white centre, dim yellow on the four pixels next to it, dim blue-grey on the four tips.

Tile 404: big star centred (8, 9); warm-white singles at (23, 4), (27, 18), (4, 24), (16, 27); dim singles at (12, 2), (18, 13), (2, 16), (29, 28).

Tile 405: big star centred (22, 21); warm-white singles at (5, 6), (28, 7), (15, 11), (9, 26); dim singles at (20, 3), (30, 14), (12, 18), (2, 30).

### 406 — sun (Atmosphere)

Palette: black; ray orange #c86c10; gold shade #d8a020; gold #f0c838; pale highlight #fcf0a0. Transparent background.

- **Disc:** a circle 18 pixels across, x 7–24, rows 7–24, with a black outline (top and bottom rows black at x 14–17; rows 8 and 23 black at x 11–14 and x 17–20; then two-pixel diagonal steps; x 7 and x 24 on rows 15–16).
- **Disc shading:** diagonal, light from the upper left. Pale highlight fills an upper-left crescent (on row 8 only x 15–16; 5 pixels wide on rows 9–12 starting just inside the left outline; 4 on row 13; 3 on rows 14–15; 2 on row 16; none below). Gold shade fills a lower-right crescent (1 pixel on row 14, growing to 6 pixels wide on rows 19–22; on row 23 only x 15–16). Gold in between.
- **Rays:** eight. The four straight rays are 2 pixels wide and 4 long, ray orange for the two pixels nearest the disc and gold for the two tips: up x 15–16, rows 3–6; down x 15–16, rows 25–28; left rows 15–16, x 3–6; right rows 15–16, x 25–28. The four diagonal rays are three 2-pixel horizontal dashes stepping diagonally, the outermost gold and the inner two orange: upper left x 5–6 (row 6), x 6–7 (row 7), x 7–8 (row 8); upper right x 25–26 (row 6), x 24–25 (row 7), x 23–24 (row 8); lower left x 7–8 (row 23), x 6–7 (row 24), x 5–6 (row 25); lower right x 23–24 (row 23), x 24–25 (row 24), x 25–26 (row 25).

### 407 — moon (Atmosphere)

Palette: black; bright #f0f0dc; mid #c8c8b0; dark tip #8c8c78; crater #6c6c5c. Transparent background.

A fat crescent open to the upper right: a top horn at the upper middle, a thick body down the left and along the bottom, and a thin lower horn reaching out to the right.

- **Top horn:** black x 13–14 on row 5; row 6 black x 11–12, dark tip x 13–14, black x 15; row 7 black x 9–10, mid x 11–12, dark tip x 13, black x 14.
- **Left body** (each row: black, fill, black): row 8 fill x 9–12; rows 9–10 x 8–12; row 11 x 7–12; row 12 x 7–11; rows 13–16 x 6–12; row 17 x 6–13; row 18 x 6–14. The inner (right) black edge is at x 13 on rows 8–16 except row 12 (x 12), then x 14 on row 17 and x 15 on row 18.
- **Bottom and lower horn:** row 19: black x 6, fill x 7–15, black x 16–17, and the horn tip black at (24, 19). Row 20: black x 6, fill x 7–17, black x 18–23, dark tip (24, 20), black (25, 20). Rows 21–22: black x 7, fill x 8–23, black x 24. Row 23: black x 8, fill x 9–22, black x 23. Row 24: black x 9–10, fill x 11–20, black x 21–22. Row 25: black x 11–12, fill x 13–18, black x 19–20. Row 26: black x 13–18.
- **Shading:** rows 8–12 are all mid. From row 13 down the left part of each row is bright and the right part mid; the bright part is 2 pixels wide on row 13 and grows by about one pixel per row to 7 wide on rows 20–24.
- **Craters:** two, each a crater-colour pixel followed by a dark-tip pixel: at x 9–10 on row 15 and x 12–13 on row 21.

### 408–415 — glitch corruption overlays (Glitch)

All eight have transparent backgrounds and share a harsh "broken display" palette: white #f0f0f0, magenta #e040d0, cyan #40e0e0, yellow #f0e050, black #000000, plus dark blue-black #202028 (tile 409), green #20c040 (412, 413) and red #e03030 (412). They are drawn semi-transparently over a few map cells at a time (section 8).

**408 static noise.** 78 isolated pixels scattered over the whole cell with no structure: 25 cyan, 21 magenta, 18 white, 7 yellow, 7 black (about 7.6% coverage), mostly single pixels with five or six 2-pixel clumps and one cyan 2 x 2-ish clump near the top (x 6–7, rows 0–1). Any random scatter with that colour mix is equivalent.

**409 broken scanlines.** Horizontal 1-pixel lines on every third row (rows 0, 3, 6, ..., 30); all other rows are empty. Each line is a broken run of segments: long solid magenta bars, strings of dark dashes of varying length with 1-pixel gaps, and two short cyan runs. Row by row: row 0 magenta x 1–23, dark x 24–27; row 3 dark x 6–8, 10, 12, cyan x 13–18, dark x 20–21; row 6 magenta x 0–16, dark x 17–26; row 9 dark x 5–8, 10, 12–16, 18–19; row 12 dark x 2–6, 8–12, 14, 19–21, 23–26, 28–30; row 15 dark x 8, 10–12, 14–24, 26; row 18 dark x 1, magenta x 3–21, dark x 22; row 21 dark x 10–12, cyan x 13–20, dark x 21–31; row 24 magenta x 4–29; row 27 dark x 7–16 and 19–20; row 30 dark x 9–10 and 12–14, magenta x 15–27.

**410 inverted blocks.** Five solid rectangles, each with a 1-pixel black line of the same width directly underneath: magenta x 17–27 on row 4 (black row 5); magenta x 5–11 on rows 5–6 (black row 7); yellow x 0–5 on rows 21–23 (black row 24); magenta x 21–25 on rows 22–24 (black row 25); magenta x 7–10 on rows 26–28 (black row 29).

**411 "MISSING" fragment.** Three elements:
1. The word MISSING in a white pixel font, cut off after the N: glyphs 3 pixels wide and 5 tall with 1-pixel gaps, on rows 5–9 starting at x 6 (letters at x 6–8, 10–12, 14–16, 18–20, 22–24, 26–28), each with a black drop shadow offset one pixel right and one pixel down (so shadow pixels reach row 10 and x 29).
2. The tail of the same word in magenta without shadow on rows 12–16, shifted to the left edge: a 1-pixel sliver of a cut-off letter at x 0, then S at x 2–4, I at x 6–8, N at x 10–12, G at x 14–16.
3. A "missing texture" checkerboard in the bottom-left corner, clipped to a staircase: on row r (18 to 31) it covers x 0 to x = 3 + 2 x (r - 18), so 4 pixels wide on row 18 and 30 wide on row 31. The checker uses 4 x 4 squares aligned to the cell origin: the square containing pixel (x, y) is magenta when (x div 4) + (y div 4) is even and opaque black when odd.

**412 RGB-split tear.** Two bands, each three consecutive rows: a red row, then a green row displaced 2 pixels to the right, then a cyan row displaced 2 pixels to the left, each broken by one to three 1-pixel gaps. Band 1: row 8 red x 4–6, 8, 10–12, 14–21; row 9 green x 6–13, 15–20, 22–23; row 10 cyan x 2–3, 5–19. Band 2: row 20 red x 4–7, 9–26, 28; row 21 green x 6–19, 21–24, 26, 28–30; row 22 cyan x 2–7, 9–19, 21–27.

**413 dripping bytes.** Six vertical "drips" of tiny byte marks, at x = 1, 6, 11, 16, 21 and 26 (a 5-pixel pitch). A mark is two pixels on one row starting at the drip's x: either green, green or green, white; about a third of the marks are followed, after a 1-pixel gap, by one more green pixel. Marks sit on every other row: the drips at x 1 and x 6 use odd rows, the other four even rows. Extents: x 1 rows 15–25 (six marks); x 6 rows 7–19 (six marks, none on row 15); x 11 rows 12–22 (five marks, none on row 18); x 16 rows 10–16 (four marks, all green-white); x 21 rows 4–22 (ten marks); x 26 rows 2–10 (five marks).

**414 missing-texture corner and "0x0".** Two elements:
1. Top right: the same 4 x 4 magenta/black checkerboard rule as in tile 411, covering rows 0–13 from a ragged left edge to x 31. The left edge (first opaque x) per row from row 0 to row 13 is 17, 16, 17, 14, 16, 14, 13, 11, 12, 14, 10, 11, 11, 9, so the patch leans left as it goes down.
2. Bottom left: the text "0x0" in yellow, 3 x 5 pixel glyphs at x 4–6, 8–10 and 12–14 on rows 22–26, with a black drop shadow offset one right and one down.

**415 broken question mark.** A large "?" assembled from yellow blocks roughly 4 x 4 pixels each with nibbled edges: a top bar of three blocks on rows 4–7 (x 9–12, x 14–17, x 19–22); the right-hand stroke, one block at x 19–22 on rows 9–12; a middle bar of two blocks on rows 14–17 (x 14–17 and x 19–22); and the dot, one block at x 14–17 on rows 24–27 with a 1-pixel hole column at x 15 on rows 25–26. About 100 yellow pixels in all; several blocks miss one or two corner or edge pixels. Around it 23 stray single pixels: 14 cyan (for example (25, 1), (22, 2), (31, 5), (9, 9), (0, 31) and a pair at x 20–21 on row 30), 5 white ((20, 8), (11, 12), (13, 16), (21, 21), (13, 26)) and 4 magenta ((8, 4), (3, 22), (3, 23), (23, 28)).

## 7. Spare ranges

Ids 416 to 447 (sheet rows 26 and 27) are reserved and unused. The shipped sheet contains the builder's magenta placeholder in each of those 32 cells. No code refers to them. A recreation can ship them transparent or omit the two rows entirely (the app sizes its tile array from the sheet's height and treats an out-of-range id as "draw nothing").

## 8. How the overlays register

All tiles, overlays included, are drawn into the same destination rectangle as the map cell they belong to, scaled from 32 x 32 to the current cell size with nearest-neighbour sampling. An overlay therefore lines up pixel for pixel with the tile underneath. The per-cell drawing order in the game view (Part 11 owns the full renderer) is:

1. The cell's own tile. On the player's cell this is the player sprite (base tiles 165–172 and 181–188, one per class and sex).
2. **Hat overlay** (392–396), only on the player's cell, only when the game reports the player as visible to themselves (the "player cursor" flag, which is off while blind), and only when the published hat tile is greater than 0. The game publishes hat tile = 392 + (worn hat's object id - 106), or 0 when no hat is worn, so cone 106 gives 392, propeller 107 gives 393, wizard 108 gives 394, party 109 gives 395, tinfoil 110 gives 396. The base player sprites have their heads in the top rows of the cell (head tops on rows 0 to 2, mostly between x 9 and x 23), so the overlays, drawn in rows 0 to 9 and centred near x 15, cover the top of the head. No per-class offset is applied; on the few classes whose heads are off-centre the hat simply sits where it sits.
3. The cell's effect overlay if one is set (spell animations from the base game, and the **egg-crack frames**). When an egg hatches, the newborn pet is first placed on a free cell next to the player; then, unless the player is blind, that cell's overlay is set to 398 for 250 ms and to 399 for another 250 ms, and then cleared. Frame 1's opaque egg covers the middle of the pet sprite; frame 2 leaves the pet visible above the broken lower shell.
4. **Pet heart** (397) on the cell whose coordinates equal the published pet position (a position of -1 means no pet on this level). It marks the top-right corner of the pet's sprite.
5. **Glitch overlay** (408–415): while the glitch counter is positive, each animation frame picks 3 to 6 distinct visible cells and a tile 408 + (hash mod 8) for each; the tile is drawn over the whole cell at alpha 200 of 255. (Selection, the 8 frames-per-second cadence, the row slicing and the hue rotation are in Part 13.)
6. The red cursor frame (base tile 174) on the player's cell.

After every cell has been drawn, a second pass draws the weather over the map when the player is on the surface level with the weather feature on:

- **Stars** (404 or 405) on about one in four never-seen ("unknown") cells, at night only (hour 21 or later, or before 6), drawn once over the whole cell. The two tiles are alternate twinkle states.
- **Rain** (400 or 401; storms use the same tiles) or **snow** (402 or 403) on a subset of floor-class cells (one in three; one in two in a storm; one in five with "reduce weather effects"). The tile is drawn twice: once offset inside the cell by the current drift, and once more one cell-height higher (and, for rain, one third of a cell to the left), so the pattern scrolls through the cell and wraps. The art is designed for that: streaks and flakes are spread evenly so the seam between the two copies is not noticeable. The two frames of each pair alternate every fourth animation frame.
- The day/night tint is multiplied over everything afterwards.

The photo-mode render repeats steps 1 to 4 and the weather pass, without the glitch overlay and the cursor.

**Sun and moon** (406, 407) are not map overlays. When weather is active, the HUD's second line (the one with the level name) draws the moon at night and the sun by day into a square whose side is the HUD line height, 4 density-independent pixels after the end of the text, with the same nearest-neighbour scaling.

## 9. The launcher icon

The icon is entirely vector artwork; there are no bitmap icon files at any density.

**Background** (`ic_launcher_background`): a plain rectangle shape filled with a single near-black colour, #16161A.

**Foreground** (`ic_launcher_foreground`): a vector drawable 108 x 108 density-independent pixels with a 108 x 108 viewport. It shows an amber "@" sign (the roguelike player glyph) standing above a row of five dim floor dots. All coordinates below are in viewport units, origin top left.

1. **Floor dots:** five filled circles of radius 1.6 on the line y = 73, centred at x = 34.6, 44.6, 54.6, 64.6 and 74.6 (10 units apart). Fill colour amber #FFBE3C at one-third opacity (alpha 0x55).
2. **Outer ring and tail of the "@":** one open path, stroked 5 units wide in opaque amber #FFBE3C with round end caps and no fill:
   - start at (67, 62), at the lower right;
   - a circular arc of radius 17, the long way round, clockwise on screen (down through the bottom, up the left side and over the top), ending at (71, 49). Its centre works out at about (54.1, 50.9), so the ring is a circle open over roughly 47 degrees at its right side, between the 3 o'clock and half-past-4 positions;
   - a straight line down to (71, 53);
   - a half-circle of radius 4 curving under, through (67, 57), to (63, 53);
   - a straight line up to (63, 45).
   The last three segments form the hook that joins the ring to the right side of the bowl.
3. **Inner bowl:** a full circle of radius 8 centred at (54, 49), stroked 5 units wide in the same amber, no fill, default (butt) caps.

The whole glyph spans about x 34.5–73.5 and y 31–71, inside the adaptive-icon safe zone (the central circle of diameter 66), with the dots just below it.

**Adaptive icon** (API 26 and later; resource `ic_launcher` in the `mipmap-anydpi-v26` folder): background layer = the background drawable; foreground layer = the foreground drawable; monochrome layer = the same foreground drawable (so themed icons on Android 13 and later show the "@" and dots as a silhouette).

**Legacy fallback** (API 24 and 25; resource `ic_launcher` in the plain `mipmap` folder): a layer list that stacks the background drawable and then the foreground drawable, giving a square dark icon with the same glyph.

The manifest uses this one `ic_launcher` resource for both the normal icon and the round icon. There is no separate round artwork.

## 10. Themes, colours and strings

There is one theme, one string, and no colour, dimension, layout, menu, font, raw or XML resource files. Every screen of the app is built in code (Part 10).

**Theme `Theme.Ularn`** (base variant, used on API 24 to 27):

| Attribute | Value |
|---|---|
| Parent | the platform's device-default theme, no action bar, fullscreen (`Theme.DeviceDefault.NoActionBar.Fullscreen`) |
| Window background | black |
| Accent colour | #FFBE3C (the same amber as the icon) |
| Navigation bar colour | black |
| Status bar colour | black |

**API 28 and later variant** (`values-v28`): identical, plus the display-cutout mode set to "short edges", so the window extends into a notch or camera cut-out on the short edges of the screen.

The only two colours defined in resources are therefore the amber accent #FFBE3C and the icon background #16161A; all other colours are constants in the code.

**Strings:** a single string, the application name: "Ularn Wild". Every other piece of text is in code or produced by the game.

## 11. The manifest

| Item | Declaration |
|---|---|
| Package | not in the manifest; the code namespace is `com.ularn.android` and the application id is set by the build (section 12) |
| Permission | vibrate |
| Permission | write external storage, limited to API 28 and below (photo mode saving to the Pictures folder on Android 7 to 9; Android 10 and later needs none) |
| Application: backup | allowed (`allowBackup` true). No backup-rules or data-extraction-rules file is declared, so the platform's default automatic backup applies to the whole app data |
| Application: icon and round icon | both the `ic_launcher` mipmap |
| Application: label | the "Ularn Wild" string |
| Application: theme | `Theme.Ularn` |
| Application: native libraries | `extractNativeLibs` false: the shared library stays uncompressed and page-aligned inside the APK and is loaded from there |
| Activities | exactly one: the main activity (`MainActivity`) |
| Main activity: exported | yes, with the standard launcher intent filter (action MAIN, category LAUNCHER) |
| Main activity: launch mode | single task |
| Main activity: configuration changes handled by the activity itself (no restart) | orientation, screen size, screen layout, smallest screen size, keyboard, keyboard hidden, navigation, UI mode, density, font scale |
| Main activity: soft-input mode | "adjust nothing" combined with "state hidden": the window is never resized or panned for the on-screen keyboard, and the keyboard is hidden on entry |
| Screen orientation | not declared. The activity follows the device by default and changes the requested orientation at run time from the settings (follow system, auto-rotate, lock portrait, lock landscape; Part 10) |

Nothing else is declared: no services, receivers, content providers, file-provider, meta-data, feature requirements, queries, network permission or network-security configuration. The app never uses the network.

The long configuration-change list matters to the design: the game runs in a forked child process tied to the activity's process (Part 09), and the views hold the live state, so the activity must survive rotation, folding, split-screen resizing, keyboard attachment, dark-mode switches and font-size changes without being recreated.

<!-- PART15-CONTINUES -->
