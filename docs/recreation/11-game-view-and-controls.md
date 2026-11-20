# Part 11 — Game view and controls: rendering, HUD, gestures, minimap, side panel, control editor

This part specifies everything the player sees and touches while a game is on screen: the
game view (HUD, tile map, message panel, answer buttons, text pages and their re-wrapping
for narrow screens), every touch gesture and keyboard mapping, the low-HP "danger cues",
the round minimap and the full-level overview, the side panel shown on wide windows, the
context-action button and its delayed follow-up keys, the Cast button's spell list, and the
customisable on-screen controls (model, default layouts, play behaviour, the editor, storage
format and key notation).

It leans on: Part 09 for the shared display state (what each field means and how the native
side fills it; this part only says which fields are read), Part 10 for the app shell around
the view (start panel, menu, settings dialog, accessibility preferences, TalkBack), Part 13
for the Wild Edition visual effects that hook into the view (weather, glitch, pet and bounty
status text), and Parts 14/15 for the tile sheet's contents.

Units used throughout: **dp** is a density-independent pixel (pixels = dp × screen density).
**sp** in this part means "dp multiplied by the text-scale accessibility factor" (the
factor comes from Part 10; its effective range is 0.75 to 1.9, default 1.0). Colours are
given as hex RGB, with alpha as a 0–255 number where it is not opaque.

---

## 1. View stack and data inputs

The activity's root is a black frame with four full-size layers, bottom to top:

1. the **game view** (this part, sections 2–12);
2. the **controls overlay** (sections 19–24), transparent, same size as the game view;
3. the **side panel** (section 16), on the right edge, only on wide windows; when it shows,
   layers 1 and 2 get a right margin equal to its width;
4. the edit toolbar (section 24) and the start panel (Part 10).

The root is padded by the display-cutout insets, so nothing is laid out under a notch.

Touches reach the controls overlay first. In play mode it claims a touch only if the finger
went down on a control; any other touch falls through to the game view underneath for the
whole gesture (including any further fingers that join it). In edit mode the overlay claims
every touch.

The game view draws only from the shared display state (Part 09). The fields it reads:

| Field | Use in this part |
|---|---|
| sequence number | 0 = nothing published yet (draw only the background); changes drive redraws and caches |
| display mode | 0 = map mode, 1 = text-page mode |
| input mode | 0 busy, 1 command, 2 prompt (one of a set of answer keys), 3 direction, 4 number, 5 password/text |
| answers | the string of keys the current prompt accepts (up to 64 characters) |
| caret flag, cursor x, cursor y | text cursor, 1-based column and row |
| beep count | a change triggers a vibration |
| player x, y; "player cursor" flag | camera, cursor tile, hat overlay |
| tiles (67×17), overlay tiles (67×17, −1 = none) | the map |
| hp, hp max, spells, spell max, gold, player level, level title, exp, AC, WC, six stats, effects bit mask, level name, game time, class number | HUD and side panel |
| message characters and formats (5 rows × 80 columns) | message panel |
| text characters and formats (25 rows × 80 columns) | text pages |
| minimap class per square (67×17 bytes) | minimap, overview, weather hook |
| inventory text (26 slots × 80 characters) | item answer buttons, side panel |
| context object/kind/flags/doors/name | context action |
| spell count, known flags, three-letter codes | spell picker |
| Wild fields: feature/perk bits, hat tile, glitch turns, weather, coin price, pet cell, pet info, bounty info | hooks (sections 5, 7, 16; details in Parts 12/13) |

Format codes for message and text cells: 0 normal, 1 standout, 2 standout-2, 3 standout-3,
4 bold, 5 inverse.

The 17 effect names, bit 0 first, exactly as displayed: "Stealth", "Undead Pro",
"Spirit Pro", "Charm", "Time Stop", "Hold Monst", "Giant Str", "Fire Resist", "Dexterity",
"Strength", "Scare", "Haste Self", "Cancel", "Invisible", "Protect 3", "Protect 2",
"Wall-Walk".

The six stat labels, in order: "STR", "INT", "WIS", "CON", "DEX", "CHA" (STR is the value
including its bonus).

---

## 2. Game view: content area and the "reserve"

The game view fills the window (minus the side panel). The activity tells it how much of
its left, right and bottom edges are covered by controls or the soft keyboard (the
"reserve"), and the view draws everything inside the remaining **content rectangle**:
left = reserve-left, top = 0, right = view width − reserve-right, bottom = view height −
reserve-bottom. If that rectangle would be less than 120 dp wide or less than 120 dp tall,
the reserve is ignored and the whole view is used.

How the reserve is computed (re-run after any layout size change, after editing controls,
after a settings change, and when the keyboard height changes; it measures each control's
on-screen rectangle):

- **Landscape.** For each control: if its centre x is left of 25 % of the overlay width,
  left = max(left, the control's right edge); else if its centre x is right of 75 %,
  right = max(right, overlay width − the control's left edge). Controls centred in the
  middle half are ignored. Both values are capped at 30 % of the width. If the setting
  "Landscape: fit the map beside the controls" (default on) is off, both are 0 for the
  reserve. Reserve-bottom = the soft keyboard's height over the game (0 when hidden).
  Independently of that setting, the capped left/right strips are always given to the
  minimap as areas to avoid (section 14).
- **Portrait.** Find the smallest top edge among controls whose centre y is below 30 % of
  the overlay height (controls in the top 30 % are ignored). Reserve-bottom =
  max( min(height − that top edge + 4 dp, 65 % of the height), keyboard height ).
  With no qualifying control it is 4 dp. Left and right reserves are 0, and the minimap
  avoid strips are 0.

So in portrait the whole display sits above the block of controls; in landscape it sits
between the left and right control clusters.

---

## 3. Colours and fonts of the game view

| Name | Normal | High-contrast mode |
|---|---|---|
| background | #0C0C0E | #000000 |
| map background (also "unexplored") | #000000 | same |
| normal text | #DEDED8 | #FFFFFF |
| dim text | #969696 | #FFFFFF |
| red (standout) | #FF6E64 | #FFFFFF, underlined |
| green (standout-2) | #6EDC78 | #FFFFFF, underlined |
| blue (standout-3) | #78AAFF | #FFFFFF, underlined |
| bold | #FFFFFF | #FFFFFF |
| accent | #FFBE3C | (most uses become #FFFFFF) |
| panel | #16171B | #000000 with a 2 dp white outline |
| panel line / bar track | #2C2E36 | same |

Format code → text colour: 1 red, 2 green, 3 blue, 4 white, anything else (0 and 5) normal.
In high-contrast mode every format is white and the three standout formats are underlined
instead.

Fonts: the platform's monospace face (regular and bold) for text pages; the platform's
sans-serif face (regular and bold) for the HUD, messages and answer buttons. All
anti-aliased. Tiles are drawn with nearest-neighbour scaling (no filtering, no dithering).

A caret blink timer toggles every 530 ms for as long as the view is attached (on 530 ms,
off 530 ms) and redraws the view on each toggle.

---

## 4. Frame sequence

On every draw:

1. Fill the whole view with the background colour and compute the content rectangle.
2. Mark the minimap as "not shown this frame".
3. Copy the shared state. If there is no state, it is not initialised, or its sequence
   number is 0, stop (blank dark screen).
4. If the beep count differs from the last frame's, and this is not the first frame seen,
   and vibration is on, perform one "long press" haptic. Remember the count.
5. If the display mode changed since the last frame, reset the text-page scroll to (0, 0).
6. Run the per-frame hooks: danger cues (section 13), glitch and weather (Part 13), and the
   statistics observer (Part 10).
7. Build the answer buttons for the current prompt (section 10).
8. Decide the height of the answer-button strip at the bottom of the content rectangle:
   in map mode always exactly one button row (section 10.3), whether or not there are
   buttons, so the map never changes size when a prompt appears; in text mode the full
   height the buttons need (0 when there are none).
9. The rest of the content rectangle (above the strip) is the **area**. Draw the text page
   (section 8) or the map-mode screen (sections 5–7) into it.
10. Draw the answer buttons. In text mode, or when they need no more than the one reserved
    row, they go in the bottom strip. Otherwise (a multi-row item grid in map mode) they
    are drawn **over the bottom of the map**: a rectangle spanning the content width whose
    bottom is the map rectangle's bottom and whose top is max(map top, map bottom − full
    grid height), first filled with the panel colour. The reserved strip below the
    messages then stays empty.
11. Draw the swipe indicator if a swipe is active (section 11.4).
12. Weather lightning flash over the whole view (Part 13 hook).

---

## 5. Map mode: layout and HUD

Map mode stacks, top to bottom inside the area: three HUD text lines, a row of effect
pills, the map, the message panel. The reserved answer-button row is below the area.
Horizontal padding for HUD and message text is 8 dp (x0 = area left + 8 dp, x1 = area
right − 8 dp, w = x1 − x0). The first line starts 8 dp below the area top. Each HUD line
is 20 sp tall with its text vertically centred in the line.

**Line 1 — hit points and gold.**

- Left: "HP {hp}/{hp max}" in bold sans 13 sp, white.
- Right: "Gold {gold}" in sans 12 sp, accent colour (white in high contrast),
  right-aligned at x1.
- Between them a bar: left edge = x0 + the width of the fixed sample "HP 999/999" (bold
  13 sp) + 6 dp; right edge = x1 − width of the gold text − 10 dp. Drawn only if it would
  be more than 30 dp long. Track: rounded rectangle 8 dp tall (4 dp above and below the
  line's centre), corner radius 4 dp, colour #2C2E36. Fill: same shape from the left edge
  for the fraction hp / hp max clamped to 0..1 (0 if hp max ≤ 0). Fill colour: fraction
  above 0.6 → #5AC86E; above 0.3 → #F0BE3C; otherwise #EB5046; white in high contrast;
  and replaced by the pulsing red of section 13 while in danger. High contrast adds a
  2 dp white outline 1 dp outside the track.

**Line 2 — level line.** Text: "Lv {player level} {level title}  ·  Exp {exp}  ·  {where}"
(two spaces either side of each middle dot), where {where} is "Home" when the level name
is "H" and otherwise "Level {level name}". Sans 12 sp, normal colour. If it is wider than
the space available the font shrinks proportionally to fit, but not below 9 dp.

- *LarnCoin ticker (Wild Edition).* The coin-price field is 0 when the feature is off,
  negative when the ticker should be hidden, positive when shown; its absolute value is
  the price. Each frame with a non-zero value: if a previous price is remembered and the
  new absolute price differs, the trend becomes "up" or "down" (and stays until the next
  change); remember the price. When the value is positive, draw "Ⓛ {price}" followed by
  " ▲" (trend up), " ▼" (trend down) or nothing (no trend yet), right-aligned at x1 on
  line 2, in green (up), red (down) or accent (none); white in high contrast. The level
  text's available width shrinks by the ticker's width + 8 dp. The remembered price and
  trend live in the view and are not reset between games.
- *Weather icon (Wild Edition hook, Part 13).* If the weather module reports a sun or moon
  tile, it is drawn in a square one line high, 4 dp after the end of the level text, and
  the text's available width shrinks by the line height + 4 dp.
- *Glitch (Wild Edition hook, Part 13).* Lines 2 and 3 pass through the glitch module's
  text corruption before drawing.

**Line 3 — stats.** "STR {n}  INT {n}  WIS {n}  CON {n}  DEX {n}  CHA {n}   AC {ac}  WC
{wc}  Sp {spells}/{spell max}": the six stats joined by two spaces, then three spaces,
then AC, WC and Sp separated by two spaces. Sans 12 sp, dim colour, shrunk to fit like
line 2.

**Effects row.** One pill per set bit of the effects mask, in bit order, left to right
from x0. Text sans 11 sp in accent colour (white in high contrast); pill height 18 sp,
width = text width + 12 dp, fully rounded ends, fill #3C3214 at alpha 200 (black with a
white outline in high contrast), drawn 1 dp below the row top, text inset 6 dp; 4 dp
between pills. If a pill would cross x1, it and all later ones are not drawn. The row
always occupies 18 sp + 6 dp of height, even when empty, so the map does not jump when an
effect starts or ends.

Total HUD height = 14 dp + 78 sp.

**Message panel** (section 6) is anchored to the bottom of the area: its top is at
area bottom − 8 dp − 4 × 17 sp − 12 dp.

**Map rectangle**: the full width of the area (no side padding), from the bottom of the
effects row to 6 dp above the message panel's top.

After the map and minimap, the danger overlay (section 13) is drawn over the map
rectangle.

---

## 6. Map mode: message panel

A rounded rectangle (corner radius 8 dp, panel colour; black with a white outline in high
contrast) from 4 dp inside the area's left and right edges, from the panel top to 6 dp
above the area bottom. Inside, with 6 dp top padding and starting at x0, exactly four
text lines of 17 sp pitch, sans 13 sp.

The game keeps five 80-column message rows. They are re-wrapped to the panel width:

1. For each source row top to bottom: trim trailing spaces. Skip the row if it is empty,
   unless the caret is on it (caret flag set and cursor row − 1 equals this row's index).
2. The row's colour is decided once for the whole row: the format of the first
   non-space character whose format is not normal (normal if there is none), mapped by the
   colour table in section 3. Bold and inverse rows are not drawn in a different face.
3. Wrap by measured pixel width: take as many characters as fit in w; if the row does not
   fit, break at the last space inside the fitting part, or mid-word if there is no space.
   Each piece becomes one display line; leading spaces of a continuation are skipped. A
   piece is never empty (at least one character), except the single empty piece created
   for an empty row holding the caret.
4. The caret belongs to the piece that contains column (cursor x − 1), or to the last
   piece of its row if the column is at or past the end.
5. Only the **last four** display lines are shown (older ones scroll out of the top).
6. A line in a standout format is underlined in high-contrast mode.
7. The caret, when visible in the blink cycle, is an accent-coloured bar 8 dp wide and
   2 dp tall, starting 1 dp below the text baseline, at the x position reached after the
   caret's column within its piece.

Typed answers (numbers, names) are echoed by the game itself into these rows (Part 09),
so the panel is also where number and text entry appears in map mode.

---

## 7. Map mode: the tile map

### 7.1 Tile sheet

Tiles come from one image asset (named "ularn_tiles.png") of 32×32-pixel tiles arranged
16 per row; tile id n is at column n mod 16, row n div 16 (integer division). At start-up
every tile is cut into its own 32×32 bitmap (so scaled drawing never bleeds a neighbour's
edge pixels). If the sheet cannot be loaded, the map is simply not drawn. An id outside
the sheet draws nothing. Sheet contents: Parts 14 and 15.

### 7.2 Tile size and zoom

- fit-all = min(map rectangle width / 67, map rectangle height / 17): the tile size at
  which the whole 67×17 level fits.
- Preferred size = the saved zoom (a tile size in dp; 0 means automatic) if positive,
  otherwise max(fit-all, 30 dp).
- Tile size = preferred, clamped to the range fit-all .. 80 dp.

Pinch zoom: a two-finger scale gesture is accepted only in map mode and only if its focal
point starts inside the map rectangle. While scaling, the new tile size = current tile
size × the gesture's incremental scale factor, clamped to fit-all .. 80 dp, and is stored
as the zoom (converted to dp). When the gesture ends the zoom is saved to settings (key
"tileDp"). "Reset map zoom" in settings sets it back to 0 (toast "Zoom reset"). The zoom
has no focal-point panning: the view always follows the player.

### 7.3 Camera

With tile size t and map pixel size 67t × 17t, separately for each axis:

- if the map is no larger than the rectangle on that axis, it is centred (camera offset =
  (map size − rectangle size) / 2, a non-positive number);
- otherwise the camera offset = player coordinate × t + t/2 − rectangle size / 2, clamped
  to 0 .. (map size − rectangle size). The player stays centred until the view reaches a
  map edge.

There is no manual panning.

### 7.4 Drawing

The map rectangle is filled black and used as the clip. The visible tile range is the
columns and rows overlapped by the rectangle (clamped to 0..66 and 0..16). Each tile's
destination rectangle has its four edges rounded to whole pixels individually (left =
round(origin + column × t), right = round(origin + (column + 1) × t), likewise vertically)
so adjacent tiles meet exactly with no seams.

For each visible cell, in this order (each layer stretched over the cell):

1. the cell's base tile;
2. *hat (Wild Edition)*: at the player's cell, if the player-cursor flag is set and the
   hat tile id is greater than 0, that tile;
3. the cell's overlay tile if it is not negative;
4. *pet heart (Wild Edition)*: at the pet's cell (pet x ≥ 0), tile id 397;
5. glitch overlay tile for the cell, if the glitch module supplies one (Part 13);
6. the **cursor**: at the player's cell, if the player-cursor flag is set, tile id 174 (a
   highlight frame drawn over the player's sprite).

Glitch hooks bracket the tile loop (begin, a per-row horizontal shift of each destination
rectangle, end). After the tiles, if weather is active, a second pass over the same cells
draws rain, snow or stars per cell (it is given the cell's minimap class), then the
day/night tint covers the map rectangle (Part 13). Finally the minimap is drawn
(section 14), then the clip is released.

The view remembers the visible cell range for photo mode. *Photo hook (Part 13):* on
request the view renders exactly those cells to a bitmap at 32 × scale pixels per tile
(reduced in steps of 8 px per tile, never below 8, until the longer side is at most
2560 px), with layers 1–4 and the weather pass and tint, but without the cursor tile,
glitch, minimap or HUD. It can also hand out a single tile bitmap and the tile id under
the player.

---

## 8. Text-page mode

Used whenever the display mode is "text" (shops, inventory, help, scores, character
creation...). The page is the 25×80 character grid with a format per cell.

### 8.1 Choosing the size

1. used-columns = the largest row length after trimming trailing spaces, but at least 40.
2. Padding 10 dp on all sides of the area; avail-width and avail-height are what is left.
3. Size = the monospace font size at which used-columns characters exactly span
   avail-width (cell width is the width of the letter "M").
4. Page identity = display mode plus the non-space characters of the first three
   non-blank rows. When the identity changes (a new page rather than an updated one), the
   scroll offsets reset to the top-left.
5. If the size is below 11 sp, the page is **re-wrapped** (section 8.3). Otherwise it is
   drawn as a grid (section 8.2).

### 8.2 Grid drawing

- Size is capped at 20 sp. If 25 rows at the font's natural line spacing are taller than
  avail-height, the size is scaled down by avail-height / (25 × line spacing), but not
  below 8 dp. Vertical scroll is forced to 0.
- The page is used-columns × cell width wide. If that fits, it is centred horizontally;
  otherwise it starts at the left padding and can be dragged sideways (offset clamped to
  0 .. page width − avail-width). With the sizing rules above the page normally always
  fits, so this horizontal drag is a safety net. The page is top-aligned (first row
  starts at the top padding).
- Every row is drawn cell by cell on the fixed grid: an inverse cell first gets a
  filled background in the normal text colour spanning the font's ascent to descent (and
  0.5 px wider than the cell), and its character is drawn in the background colour; other
  cells use the format colour of section 3; bold cells use the bold monospace face;
  standout cells are underlined in high contrast. Spaces and NUL characters draw nothing
  (but an inverse space still gets its background).
- Caret (if the caret flag is set and the blink is on): accent bar one cell wide, 2 dp
  tall, 1 dp below the baseline of row (cursor y − 1), at column (cursor x − 1).
- When the page is horizontally scrollable, a scroll thumb is drawn along the bottom of
  the area: white at alpha 90, 3 dp thick (from 5 dp to 2 dp above the area bottom),
  corner radius 2 dp, length = avail-width × (avail-width / page width), positioned
  proportionally.

### 8.3 Re-wrapped drawing

Used when an 80-column page would need a font below 11 sp (a phone held upright, or a
narrow landscape strip between controls).

1. Start at font size 15 sp. Compute columns = floor(avail-width / cell width) and re-wrap
   the page to that many columns (section 9).
2. If the result's row count × line spacing fits avail-height, or the size is already
   11 sp, stop. Otherwise reduce the size by 1 dp (not below 11 sp) and repeat.
3. The result is cached until the state's sequence number, the available size or the text
   scale changes.
4. Draw the re-wrapped rows like grid rows (same cell rules), horizontally centred as a
   block of "columns" cells, starting at the top padding minus the vertical scroll offset.
   Rows entirely outside the area are skipped.
5. If the page is taller than avail-height it scrolls vertically by dragging (offset
   clamped to 0 .. page height − avail-height). A scroll thumb is drawn on the right
   edge: white at alpha 90, 3 dp wide (5 dp to 2 dp inside the area's right edge), track
   = area height − 20 dp, thumb length = track × (avail-height / page height).
6. The caret is drawn at the re-wrapped cursor position (section 9.6).

The scroll position is not moved automatically to reveal the cursor or a prompt at the
bottom; it only resets when the page identity or the display mode changes.

### 8.4 Interaction on a text page

- Dragging scrolls (when the page is scrollable in that direction).
- A tap anywhere that is not an answer button "continues": see section 11.2.
- Answer buttons sit in a strip at the bottom of the content rectangle; the strip's height
  varies with the buttons (section 10.3), so the page is laid out again when a prompt
  appears or disappears.
- Swipes, map taps and pinch do nothing.

---

## 9. Text re-wrapping algorithm

Input: the page's rows × columns (25 × 80) with a character and format per cell, the
cursor position (0-based row and column; a row outside the page means "no cursor"), and a
target width in columns. The working width W = max(target, 10).

Definitions:

- A cell is **blank** if its character is a space and its format is not inverse. NUL
  characters are read as spaces. An inverse space is content (it is a highlighted bar).
- Each source row is first trimmed of trailing blank cells.
- **Indent** of a row = number of leading blank cells.
- A row **has a gap of n** if, after its indent, there is a run of at least n blank cells
  followed by a non-blank cell.
- A row is **tabular** if it has a gap of 3.
- A row is a **rule** if it has at least 10 cells after its indent, the first of them is
  not a letter or digit, and all of them are that same character (for example a line of
  dashes).
- Every output cell remembers the source row and column it came from; padding spaces
  inserted by the algorithm carry a source row but no column.

### 9.1 Main pass

Go through the source rows top to bottom:

1. **Rule** row: output its first min(length, W) cells (indent included) as one line.
2. **Empty** row: output one empty line, unless the previous output line is already
   empty. (So any run of blank rows, including one at the very top of the page, becomes
   exactly one blank line.)
3. **Table**: if this row is tabular and the next row is tabular too, collect the maximal
   run of consecutive tabular rows and lay them out as a table (section 9.4).
4. Otherwise **prose**: starting with this row, keep appending the next row while it
   *joins* the current last row (section 9.2). If the paragraph is a single row that
   already fits in W, output it unchanged (indent kept). Otherwise lay it out as a
   paragraph (section 9.3).

Afterwards, remove empty lines from the end of the output (but keep at least one line),
then place the cursor (section 9.6).

### 9.2 Does row B continue the paragraph ending in row A?

All of the following must hold ("A is first" means A is the paragraph's first row):

1. neither row is empty;
2. B does not have a gap of 6 (that would be a table row);
3. neither row is a rule;
4. B's first non-blank cell is in normal format, or in the same format as A's last cell
   (a highlighted line starts its own paragraph: it is a heading);
5. not (B's first non-blank character is upper-case and A's last character is a letter or
   digit) — a capitalised line after one that did not end a sentence is a new item;
6. A is at least 50 cells long (indent included) and longer than W − 20 (A ran on to the
   original right margin);
7. B's indent equals A's indent, or A is first and B's indent is 0 and A's indent is 3
   to 8 (an indented first line followed by lines at the margin).

### 9.3 Paragraph layout

Given one or more rows and a width:

1. Special case — a single row whose content (length − indent) fits the width and whose
   indent is more than 4: it is only too far to the right (right-aligned or centred
   text). Drop leading blanks so that the line becomes exactly the width long; the content
   therefore ends flush with the right edge. Done.
2. Split the rows into words (maximal runs of non-blank cells). Record the gap before
   each word: the number of blank cells before it on its row, where the break between two
   source rows counts as one blank; clamp the gap to 1..2 (so the double space after a
   full stop survives and wider runs shrink to two). The first word's gap is 0.
3. First-line indent = min(first row's indent, 4, width div 4). Continuation indent = if
   there are several rows, min(second row's indent, 4); else the row's own indent if it
   is at most 4, else 0.
4. Start a line with the first-line indent. For each word: if it is not the first on the
   line and line length + gap + word length exceeds the width, emit the line and start a
   new one with the continuation indent. Unless the word is first on its line, add its
   gap as spaces. Add the word's cells; whenever the line is already full (length ≥
   width) before adding a cell, emit it and continue on a fresh, unindented line (a word
   longer than the line is hard-broken).
5. Emit the last line if it is not empty.

### 9.4 Table layout

Given a run of tabular rows and a width:

1. A column position is a **gutter** if it is blank in every row (rows that have ended
   count as blank there). **Columns** are the maximal spans between gutters at least 2
   positions wide; a single all-blank position does not separate columns. A span runs
   from its first non-gutter position to just after its last non-blank position.
2. If fewer than two columns are found: each row is output unchanged if it fits,
   otherwise laid out as a one-row paragraph.
3. A row's **cell** in a column is its part inside the span with trailing blanks removed
   (leading blanks are kept, which preserves right-aligned numbers). Column width = span
   width. Lead = the first column's start.
4. **Keep the columns.** Original width = last column's end − lead. Tight width = sum of
   column widths + 2 per gap between columns. If the original width fits, each row is
   output with its cells at their original positions shifted left by the lead; else if
   the tight width fits, with the columns packed 2 apart. In both cases the whole table
   is indented by min(lead, width − total). One output line per row.
5. **Hanging last column.** Otherwise, let hang-at = sum of all but the last column's
   widths + 2 per such column. If width − hang-at ≥ max(16, width div 3): each row's
   leading columns are packed 2 apart with no indent, and the last column's cell (leading
   blanks dropped) is word-wrapped into the remaining width with every wrapped line
   starting at hang-at (a list of names with descriptions).
6. **Bands.** Otherwise the columns are split into groups ("bands") printed one under the
   other for each source row. A column is **right-aligned** if in every row its cell is
   empty or ends at the span's last position (prices).
   1. *Equal bands*: for k from (number of columns − 1) down to 2, if the column count is
      divisible by k: slot j's width = the widest column among those at position j of
      each group of k; if the k slots packed 2 apart fit the width, use groups of k
      consecutive columns, each cell placed at its slot's offset (right-aligned columns
      pushed to the right of their slot). Take the largest k that works.
   2. *Greedy with original spacing* (if no equal split works): fill a band with
      consecutive columns for as long as (end of the newest column − start of the band's
      first column) fits the width. If every band then fits: band width = the widest
      band; each cell is placed at its original offset relative to its band's first
      column, except that a right-aligned column that is last (and not first) in its band
      is placed so that it ends at the band width — so prices line up across bands.
      Lines with no content are skipped.
   3. *Greedy packed* (last resort): fill a band while the column widths plus 2 per gap
      fit. Cells are packed 2 apart; every band after the first is indented by
      min(2, width div 8).
   For 6.1 and 6.3, a band line that still exceeds the width (one column wider than the
   screen) is laid out as a one-row paragraph instead.

### 9.5 Expected behaviour (the unit tests)

All examples use an 80-column source.

| Case | Input | Width | Expected output |
|---|---|---|---|
| Rows that fit are kept | "    a short line", blank, "another" | 40 | identical three lines |
| Nothing is wider than the target | a shop page: two long prose lines, a blank, two rows of "letter) item … price" pairs side by side, a far-right "You have 500000 gold pieces", and a prompt line | 30, 40, 46, 60 | every output line is at most the width long |
| Paragraph joined and wrapped | "Welcome to the Ularn Trading Post.  We buy items that explorers no longer find" / "useful.  Since the condition of the items you bring in is not certain," | 40 | 4 lines; line 1 "Welcome to the Ularn Trading Post.  We", line 2 "buy items that explorers no longer find" |
| Side-by-side lists become one aligned list | "a) an energy ring                1800   b) a ring of increase damage     1250" / "c) a ring of regeneration        2200   d) a ring of extra regeneration 10000" | 46 | "a) an energy ring                1800" / "b) a ring of increase damage     1250" / "c) a ring of regeneration        2200" / "d) a ring of extra regeneration 10000" |
| Highlighted heading starts a new paragraph | an indented long prose line not ending a sentence, then "Charm Monster (chm) [Self] {Short}" whose first 13 cells are standout | 46 | the heading is the last output line, on its own |
| Rule is cut to width | 63 dashes / "Score: 1" | 20 | 20 dashes / "Score: 1" |
| Cursor follows the prompt | two blank rows, then "Your wish? [(d) deposit, (w) withdraw, (s) sell a stone, or escape] " with the cursor just after the trailing space | 40 | the cursor is on the line that ends "escape]", one column past that line's end |
| Blank runs shrink | 60 "x" characters, four blank rows, "prompt?" | 40 | 40 "x" / 20 "x" / blank / "prompt?" |

How the shop example resolves: the two rows form a table whose columns are the left
names, the left prices, and (because only one blank position separates the longest right
name from its 5-digit price) the right name-and-price as a single third column. Neither
"keep the columns" nor the hanging column fits 46, three columns cannot split into equal
bands, so greedy-with-original-spacing makes two bands (columns 1–2, column 3), both 37
wide, and the right-aligned price column ends at 37 in the first band.

### 9.6 Cursor placement

1. Scan all output cells in order. Every cell that came from the cursor's source row at a
   source column before the cursor column sets the position to (that line, that cell's
   index + (cursor column − the cell's source column)); the last such cell wins. So the
   cursor lands just after the last character before it, plus any spaces it was beyond.
2. If the cursor row is outside the page: end of the last output line.
3. Else if no cell qualified: column 0 of the first output line containing a cell from the
   cursor's row; if there is none, a new empty line appended at the end.
4. If the column is at or beyond W, move to column 0 of the next line (appending an empty
   line if needed).

---

## 10. Answer buttons

Whenever the game waits for input the view offers tap targets derived from the input
mode and the answers string. They are rebuilt when the triple (input mode, answers,
display mode) changes or high-contrast mode is toggled.

### 10.1 By input mode

| Input mode | Buttons |
|---|---|
| busy, command | none |
| prompt | item buttons if it is an item prompt (10.2); otherwise one per accepted key: take the answers, keep printable ASCII (0x20–0x7E), Enter (13) and Esc (27), remove duplicates; if 1 to 14 keys remain, show them in the given order but with Enter and then Esc moved to the end; if more than 14 (for example a prompt accepting any letter) show none |
| number | "All" (sends "*"), "0" … "9", "⌫" (sends byte 8), "Enter" (sends byte 13) |
| password / text | "⌨ Type" (shows or hides the soft keyboard; sends nothing), "Enter" (byte 13) |
| direction | a single inert label "Swipe or press a direction" |

Labels for prompt keys: Enter → "Enter"; Esc → "Esc"; space → "Space"; "*" → "All (*)";
any other key → the character itself. Each button sends its one byte.

### 10.2 Item prompts

A prompt is an item prompt when its answers string is exactly one of: the 26 lower-case
letters followed by "*" and Esc (eat/quaff/read/wear...); the letters, "*", "." and Esc
(drop); the letters, "*", "-" and Esc (wield).

1. Search the message rows from the bottom up for the first containing
   "What do you want to ". In that row, if a "[" follows and "* for all" follows the
   "[", the text between them is the game's list of fitting slots.
2. Slots = the lower-case letters in that list; if no list was found, every letter whose
   inventory text is non-empty. Only the 26 inventory slots count.
3. Buttons, in order: for a drop prompt, if no list was found or the list contains ".",
   "Gold" (key "."); one button per slot labelled with the slot's inventory text as the
   game words it (or "item {letter}" if that is empty), key = the letter; for a wield
   prompt "Nothing" (key "-"); "Show all" (key "*"); "Cancel" (sends Esc).
4. These are laid out as a grid (below). Every button except "Cancel" shows its key
   letter in front of the label.

### 10.3 Layout and look

- Row height = max(44 dp, 40 sp).
- Grid columns (item prompts) = floor(content width / max(150 dp, 140 sp)), clamped to
  2..6. Strip height = one row for ordinary buttons; for a grid, rows × row height capped
  at half the content height.
- Gap 6 dp. Ordinary buttons share one row, each (strip width − gap × (count + 1)) /
  count wide but at most 110 dp, the row centred horizontally. Grid buttons are
  (strip width − gap × (columns + 1)) / columns wide, left-aligned in columns, rows of
  equal height filling the strip.
- Each button is inset vertically by half a gap, plus another half gap at the top of the
  first row and the bottom of the last row. Corner radius 10 dp.
- Active buttons: fill #343640 at alpha 235 with a 1 dp white border at alpha 80 (high
  contrast: black fill, 2 dp solid white border). The inert direction label has no fill
  or border and its text is white at alpha 170.
- Ordinary label: bold sans 14 sp, white, centred, shrunk to fit the button width − 8 dp.
- Item button: font size min(14 sp, 42 % of the button height); the key letter in bold
  accent colour (white in high contrast) 8 dp from the left; the name in regular white
  starting one bold "M" width + 4 dp after that; if the name does not fit before the
  right padding (8 dp) it is cut and ends with "…" (trailing spaces removed first).

Tapping a button sends its byte(s) with a key-tap haptic (if vibration is on).

---

## 11. Touch on the game view

### 11.1 Dispatch order

For every touch event that reaches the game view:

1. The minimap gets first refusal (section 14.3). If it claims the event, stop.
2. The pinch detector sees every event. A "pinching" flag is cleared on each new
   finger-down and set whenever more than one finger is down (it stays set until the next
   fresh touch).
3. The swipe handler (11.4). If it consumes the event, the tap/long-press detector is
   sent a cancel so it cannot also fire, and processing stops.
4. Otherwise the tap / long-press / drag detector (11.2, 11.3, 8.4) handles it, using the
   platform's standard tap slop and long-press timeout.

### 11.2 Tap

On a completed single tap (ignored while pinching):

1. If it is inside an answer button: send the button's bytes and/or run its action. Done.
2. If no game is running (or there is no state): notify the shell (it shows the start
   panel, Part 10). Done.
3. If the display is a text page, or the tap is outside the map rectangle (HUD, message
   panel, empty strip): **tap to continue**. Only in prompt mode: send space if the
   answers contain a space; else Enter (byte 13) if they contain it; else, if there is
   exactly one accepted key, that key; otherwise nothing.
4. Otherwise (on the map), if "tap to move" is enabled and the input mode is command or
   direction: find the tapped square (column = (x − map left + camera x) / tile size,
   row likewise, truncated), take the difference to the player's square, and if it is not
   (0, 0) send the lower-case direction key for it (11.5). In command mode this is one
   step toward the square; in direction mode it answers the prompt. There is no
   path-finding: only the compass sector of the tapped square matters.

### 11.3 Long-press

When the platform's long-press fires (not while pinching), if a game is running, "tap to
move" is on, the display is the map, the point is inside the map rectangle and the input
mode is command: send the **upper-case** direction key toward the pressed square (a run
in that direction). Nothing happens on the player's own square or in other input modes.

### 11.4 Swipe, hold-to-walk, flick

Constants: threshold 28 dp; flick time 180 ms; hold delay 320 ms; repeat interval 150 ms.

Swipes are allowed when "swipe to move" is on, a game is running, the display is the map
and the input mode is command or direction. A swipe must start inside the map rectangle
and not on an answer button.

1. **Finger down**: remember the start point and time; clear any swipe state. Not
   consumed (so it can still become a tap or long-press).
2. **Move** with more than one finger down, or while pinching: any swipe is ended; not
   consumed.
3. **Move**, no swipe yet: ignore unless swipes are allowed and the start point
   qualifies. While the finger is less than 28 dp (straight-line) from the start point
   nothing happens. Once it is 28 dp or more away, the swipe becomes active with the
   direction of the vector from the start point to the finger (11.5), and:
   - with "quick flick runs" off (the default): the **first step** is sent immediately;
   - with it on: the first step is scheduled 180 ms later (so that a flick can become a
     run instead).
4. **First step**: if the swipe is still active and has not stepped yet, send the
   lower-case direction key and mark it stepped. If the input mode is command, schedule
   the first repeat 320 ms later.
5. **Repeat** (every 150 ms after the hold delay, for as long as the swipe is active): if
   the input mode is command, send the current direction key again. While the game is in
   any other mode (busy, a prompt) the tick sends nothing but keeps ticking.
6. **Steering**: while active, the direction is re-evaluated on every move that is at
   least 28 dp from the start point (moves closer than that keep the current direction).
   When it changes, and the swipe has already stepped, and the input mode is command, the
   new direction key is sent at once and the repeat timer restarts with the 320 ms hold
   delay. The start point never moves: the finger works like a joystick around it.
7. **Finger up** while active: if no step has been sent yet (only possible with flick-run
   on, or in rare races): in direction mode send the lower-case key (answering the
   prompt); else if flick-run is on and the time since finger-down is under 180 ms send
   the **upper-case** key (run); else send the lower-case key (one step). Then end the
   swipe. Note the flick test is measured from finger-down, while the first-step delay is
   measured from the moment the 28 dp threshold was crossed.
8. **Cancel**: end the swipe.

In a direction prompt the first step answers the prompt (one key); no repeat is scheduled
by it, though steering afterwards, once the game is back in command mode, behaves as in
step 6.

While a swipe is active an indicator is drawn: a ring of radius 34 dp around the start
point (2 dp stroke, white at alpha 150) and, from the centre to the ring in the current
direction, a 4 dp line with a 7 dp-radius dot at its end, both accent colour at alpha 220.

Every key sent by tap, long-press, swipe or answer button performs a key-tap haptic when
vibration is on. Vibration here follows the controls layout's "Vibrate on press" option
(section 19).

### 11.5 Direction from a vector

For a vector (dx, dy) in screen orientation (y grows downward): angle = atan2(−dy, dx) in
degrees, normalised to 0..360 (0 = east, counter-clockwise). Sector = floor(((angle +
22.5) mod 360) / 45). Keys by sector 0..7: l (east), u (north-east), k (north), y
(north-west), h (west), b (south-west), j (south), n (south-east). Each direction
therefore owns a 45° wedge centred on it. The same rule is used for swipe vectors (in
pixels) and tap/long-press targets (in whole squares).

### 11.6 Interaction with the controls overlay

A finger that goes down on a control never reaches the map. A finger that goes down
elsewhere belongs to the game view for the whole gesture, so a pinch may end with a
finger over a control without pressing it. Controls and map gestures can be used at the
same time with different fingers only if the control was touched first (the overlay keeps
handling later fingers of its own gesture; fingers landing off a control during that
gesture do nothing). In landscape with "fit the map beside the controls" off, controls
float over the map and simply shadow it for touches.

---

## 12. Keyboard input

The game view is focusable and acts as a minimal text editor target so the soft keyboard
can be shown over it ("no suggestions" raw input, no full-screen/extract mode). The shell
toggles it from the keyboard control, the "⌨ Type" button and the menu; the keyboard's
height becomes the bottom reserve (section 2) so the prompt stays visible.

Soft keyboard text: each committed character is sent as its code (newline → 13; codes
above 0x7F are sent as 0x7F). A delete request sends byte 8 once per character to delete
(at least once).

Key events (hardware keyboard, or key events from the soft keyboard), only while a game
is running and the controls are not being edited:

| Key | Sent |
|---|---|
| Enter, numpad Enter | 13 |
| Backspace | 8 |
| Escape | 27 |
| Arrow left / right / up / down | h / l / k / j — only if "Arrow keys / numpad move" is on |
| Home / Page Up / End / Page Down | y / u / b / n (always) |
| Numpad 7 8 9 / 4 5 6 / 1 2 3 with NumLock off | y k u / h . l / b j n — only if the setting is on |
| Ctrl + letter | control code 1..26 |
| any key producing printable ASCII 0x20–0x7E | that character |

With Shift held, the movement letters above are sent upper-case (run). Keyboard input
does not vibrate.

---

## 13. Danger cues

### 13.1 Tracker logic

State: in-danger flag, a latch, last HP (−1 = none). Parameters: threshold percent
(setting; choices 10, 15, 20, 25, 30, 35, 40, 50; default 25) and big-hit percent (fixed
20). Fed once per drawn frame with (hp, hp max), in both display modes:

1. If hp max ≤ 0: reset everything (not in danger, latch off, no last HP); no events.
2. below = hp × 100 < threshold × hp max (strictly below). in-danger = below.
3. If there is no last HP (first reading of a game, or after a reset): latch = below,
   remember hp, no events. A game resumed while already hurt shows the state but
   announces nothing.
4. Otherwise: if below and the latch is off, set the latch and raise **crossed-below**;
   if not below, clear the latch. (So it fires once and not again until HP has been back
   at or above the threshold.)
5. If hp < last HP and (last HP − hp) × 100 ≥ 20 × hp max, raise **big-hit**.
6. Remember hp.

Worked examples (threshold 25, max 100): 100 → 30 nothing; → 24 crossed; → 20, → 24,
→ 10 nothing; → 25 leaves danger; → 24 crossed again. 100 → 81 no big hit (19); 81 → 61
big hit (20); 61 → 100 nothing; 100 → 10 big hit and crossed together. A max of 0 (new
process, state zeroed) resets, and the next reading is again a "first reading".

### 13.2 Cues

Four independent toggles (all default on): pulse the HP bar, vignette, vibrate, big-hit
flash. Vibration additionally requires the controls' "Vibrate on press" option.

| Cue | Trigger | Effect |
|---|---|---|
| Vibration | crossed-below | pattern: on 70 ms, off 90 ms, on 70 ms, once |
| Big-hit flash | big-hit (toggle on) | the map rectangle is covered with #FF1E14 at alpha 90 × (1 − elapsed / 350 ms), fading to nothing over 350 ms |
| Big-hit buzz | big-hit (toggle on), vibration allowed, and not also crossed-below in the same reading | one 45 ms vibration |
| HP bar pulse | while in danger (toggle on) | the bar's fill colour is replaced by red interpolated with the pulse p: red = 140 + 115p, green = 20 + 50p, blue = 20 + 40p (truncated to integers) |
| Vignette ("edge glow") | while in danger (toggle on) | an elliptical radial gradient over the map rectangle: the ellipse touches the middle of each edge; fully transparent from the centre out to 62 % of the radius, then rising to #C80000 at alpha 150 at the ellipse and beyond (so the corners are reddest). Its overall opacity is multiplied by (150 + 105p) / 255 when the HP-bar-pulse toggle is on, or by a constant 220 / 255 when it is off |

Pulse: p = 0.5 + 0.5 × sin(2π × (t mod 1100) / 1100), t = milliseconds of monotonic
uptime; period 1.1 s, range 0..1.

The vignette and flash are drawn only in map mode, over the map rectangle (and over the
minimap). While the view is in map mode and either (in danger with pulse or vignette
enabled) or a flash is running, the view redraws every animation frame; otherwise it only
redraws when the game changes (or the caret blinks).

Settings wording (the dialog itself is Part 10): section "Danger cues (low HP)", "Warn
when HP is below", choices "{n}% of max HP", "Pulse the HP bar red", "Red vignette around
the map", "Vibrate when HP falls below it", "Flash red on a big hit (20% of max HP or
more)", note "Vibration also needs "Vibrate on press" (Edit controls → Look) to be on."

---

## 14. Minimap

### 14.1 The dot-map renderer (shared by minimap, overview and side panel)

Everything comes from the per-square minimap class byte written by the game (Part 09):
low 5 bits = class, bit 0x80 = a monster is shown there. The game only fills squares the
player has revealed and only flags monsters the main map is actually showing, so nothing
unseen can appear.

| Class | Meaning | Terrain colour | Marker |
|---|---|---|---|
| 0 | unknown | not drawn | — |
| 1 | floor | #807E74 | — |
| 2 | wall | #3A3834 | — |
| 3 | door | #8C643C | — |
| 4 | item | floor colour | red dot #EB3732 |
| 5 | gold | floor colour | red dot #EB3732 |
| 6 | stairs / way up | floor colour | upward triangle #64E66E |
| 7 | stairs / way down | floor colour | downward triangle #46D2FF |
| 8 | volcanic shaft | floor colour | diamond #FF6E1E |
| 9 | dungeon entrance | floor colour | downward triangle #E664FF |
| 10 | building | floor colour | square #508CFF |
| 11 | fountain | floor colour | ring #96D7FF |
| 12 | altar | floor colour | cross #FFA0D2 |
| 13 | throne | floor colour | diamond #FFCD3C |
| 14 | trap | floor colour | X #B96EFF |
| 15 | statue, mirror | floor colour | small square #B9B9B9 |
| +0x80 | monster here | — | yellow dot #FFE128 |

Drawing a range of squares with cell size c and an overall alpha:

1. Terrain: each known square as a flat rectangle with whole-pixel edges (no
   anti-aliasing, no seams).
2. Landmarks (classes 6–15): markers of radius R = max(0.75c, 2.6 dp) centred on the
   square. Shapes: triangle = apex R from the centre, base corners R to each side and
   0.8R on the other side; diamond = four points at distance R; building square = 1.6R
   across; statue square = 1.0R across; fountain = a circle outline of radius 0.75R and
   stroke 0.5R; altar = a vertical bar 0.64R wide and 2R tall plus a horizontal bar 1.6R
   wide and 0.64R thick centred 0.45R above the centre; trap = two diagonal strokes
   reaching ±0.75R, stroke 0.55R, round ends. Filled shapes have a 1.2 dp black (alpha
   210) outline; the stroked ones (fountain, trap) are drawn first 1.2 dp thicker in that
   black.
3. Items and gold: red dots of radius D = max(0.42c, 1.7 dp).
4. Monsters: yellow dots of radius 1.1D.

Each layer is drawn completely before the next, so monsters are on top. Dots sit on a
black (alpha 200) disc 0.6 dp larger. The player is a white dot on a black (alpha 230)
disc 1 dp larger.

### 14.2 The round corner minimap

Shown only when the "Minimap" setting is on (default) and a game is running, in map mode.

- Usable width = the map rectangle narrowed to exclude the avoid strips (section 2);
  height = the map rectangle's height.
- Diameter d = min(112 dp, 34 % of the usable width, 60 % of the height). If d < 56 dp
  the minimap is not drawn.
- Position: 6 dp inside a corner of the usable area. Candidate corners in order:
  top-right, top-left, bottom-right, bottom-left. The first one whose bounding square,
  grown by (tile size / 2 + 2 dp) on every side, does not contain the centre of the
  player's square on screen is used. If all four would cover the player it is not drawn.
  In practice it sits top-right and hops away when the player walks under it.
- Content: a disc of colour #08080C at alpha 150; clipped to the disc, the level around
  the player at overall alpha 205 with the player's square exactly at the centre; range
  10 squares each way, so the diameter spans 21 squares (cell = d / 21). The player dot
  has radius max(0.55 cell, 2.6 dp). Rim: a 3 dp black (alpha 160) ring at radius + 1 dp,
  then a 1.5 dp ring of #C8B478 at alpha 210 at the radius.

### 14.3 Touch

A finger-down within the disc's radius + 4 dp (while it is shown) is claimed by the
minimap, as are the rest of that gesture's events. On release within radius + 24 dp:
key-tap haptic (if on) and the level overview opens. Touches starting anywhere else go to
the map as usual.

---

## 15. Level overview dialog

A full-screen black dialog (no title bar; keeps the screen on if that setting is on),
opened from the minimap or the side panel's map. Any touch release, or Back, closes it.
It re-reads the state every 250 ms and redraws when the sequence number changed, so it is
live. Sizes here are plain dp (not text-scaled).

- Background #0C0C0E, 16 dp padding.
- Title, bold sans 18 dp, white, top-left: "Home — map" when the level name is "H";
  "Level {name} — map" otherwise; "Level map" if the name is empty; and only "No map
  yet" (nothing else drawn) when there is no usable state.
- Hint, sans 12 dp, #969696, right-aligned on the title's baseline: "Tap or Back to
  close".
- The legend's height is measured first for the full width. The map area is what remains
  between the title (12 dp below it) and the legend (12 dp above it); if it is less than
  20 dp tall, neither map nor legend is drawn.
- Map: the whole 67×17 level with square cells, cell = min(area width / 67, area height /
  17), centred in the area on a #08080C background, at full alpha, player dot radius
  max(0.6 cell, 2.6 dp) (only if the player's coordinates are on the map), and a 1 dp
  border of #C8B478 at alpha 120.
- Legend: drawn 12 dp below the map's actual bottom edge (not pinned to the screen
  bottom), text 13 dp.

Legend entries, in order: "You" (player dot), "Monster" (yellow dot), "Item / gold" (red
dot), "Stairs up", "Stairs down", "Volcanic shaft", "Dungeon entrance", "Building",
"Fountain", "Altar", "Throne", "Trap", "Statue / mirror" (each with its marker), "Door"
(a filled square in the door colour). Layout for text size T: icon radius 0.42T, row
height 1.6T, each entry = icon (2 × radius) + 0.4T + label; entries flow left to right
with 1.1T between them and wrap to a new row when the next would pass the right edge;
text colour #C8C8C3. Icon sizes: player and monster dots 0.8 × radius, item dot 0.75 ×,
door square 1.4 × radius across, markers at the full radius.

---

## 16. Side panel (wide windows)

### 16.1 When it shows

Evaluated from the root's current size in dp (minus cutout padding; before the first
layout, from the configuration's screen size), and again whenever the root's size changes
(rotation, fold/unfold, entering or resizing split-screen):

- wide = (min(width, height) ≥ 600 and width ≥ 600) or (width ≥ 900 and height ≥ 480);
- shown when wide and the setting "Tablets / foldables: side panel with stats, map and
  inventory" is on (default on).

Width = 28 % of the window width, clamped to 240..340 dp. The panel is docked to the
right edge at full height; the game view and controls overlay are narrowed by a right
margin of the same width (so default control layouts and the reserve are computed for the
narrower area). On phone-sized windows nothing changes. A phone on its side (height about
360–440 dp) never qualifies; an unfolded foldable or a wide enough split-screen pane
does.

### 16.2 Content

Redrawn on every game change. Sizes are plain dp (not affected by text scale or high
contrast). Colours: background #121317, a 1 dp line #2C2E36 down the left edge, text
#DEDED8, dim #969696, accent #FFBE3C. Padding 12 dp; content starts 13 dp from the left;
content width w = panel width − 25 dp. Regular rows are 19 dp tall; a text row that is
too wide shrinks its font to fit, not below 9 dp.

If there is no usable state, or the class number is negative (no character yet), or w is
under 60 dp: only "No character yet" (13 dp, dim).

A section header is its label in bold 11 dp dim capitals with a 1 dp line under it,
taking 22 dp. Sections, top to bottom (8 dp before each header after the first):

1. **CHARACTER**
   - "Level {player level} {level title}" — bold 14 dp, white.
   - "Home" (level name "H") or "Dungeon level {level name}" — 13 dp.
   - "HP {hp}/{max}" with a bar; "Spells {spells}/{max}" with a bar. A bar starts at the
     row's left + the width of the sample "Spells 999/999" + 8 dp and runs to the right
     edge (omitted if under 24 dp long); 8 dp tall, radius 4 dp, track #2C2E36, filled
     for value/max clamped 0..1 (nothing drawn at 0). HP colour by fraction: above 0.6
     #5AC86E, above 0.3 #F0BE3C, else #EB5046 (no danger pulse here). Spells colour
     #6E96FF.
   - "AC {ac}   WC {wc}   Exp {exp}" (three spaces between items).
   - "Gold {gold}" in accent.
   - Stats in two columns of three rows: left column STR, INT, WIS; right column CON,
     DEX, CHA. Each: label in dim at the column's left, value right-aligned at the
     column's width (w/2) minus 8 dp.
   - "Turn {game time}" in dim.
   - Active effects (if any; no header): pills as on the HUD but 20 dp tall with 11 dp
     text, fill #3C3214 at alpha 200, accent text, 4 dp gaps, wrapping onto further rows.
2. **PERKS** (Wild Edition; only if any perk bit is set): pills like the effects but fill
   #1E3246 at alpha 200 and text #8CC8FF. Names by perk bit 0..10: "Magnet Hands", "Thick
   Skin", "Vampiric", "Bookworm", "Lucky", "Nimble", "Haggler", "Glass Cannon", "Second
   Wind", "Night Owl", "Cartographer". Perk bits are the high 16 bits of the Wild flags
   field; bits 11–15 are not perks. (What the perks do: Part 12.)
3. **PET** (Wild Edition; only when the pet info's first value, the species, is
   positive): the pet's title in bold 13 dp white; an "HP {hp}/{max}" bar coloured like
   the player's; the growth line in dim; and, if the pet is on another level, its
   whereabouts in accent. The three strings come from the pet status helper (Part 13):
   title "{Baby|Young|Grown|Fearsome} {species name}", growth "{n} kill(s) · grows at
   {m}" or "{n} kill(s) · fully grown", whereabouts "Waiting in town" or "Waiting on
   level {name}".
4. **BOUNTY** (Wild Edition; only when the bounty info's species is positive): title in
   bold 13 dp white, reward in accent, outlook in #78DC78 if claimed, otherwise dim. From
   the bounty status helper (Part 13): "WANTED: {species name}", "Reward {gold with
   thousands separators} gold", and "Done! Collect it in town" / "Comes down on your
   next town visit" / "Comes down in {n} turn(s)".
5. **MAP**: the whole-level overview (section 15's map drawing) in a box w wide and
   w × 17 / 67 tall, then 6 dp below it the legend at text size 10 dp wrapped to w.
   Tapping the box opens the full-screen overview.
6. **INVENTORY**: one entry per non-empty slot a–z: the slot letter and ")" in accent,
   then the game's own item text in 13 dp, word-wrapped with a hanging indent (the width
   of "m) " + 2 dp); breaks prefer the last space that fits. If the pack is empty:
   "Nothing" in dim.

If the content is taller than the panel it scrolls by vertical drag (after the platform's
touch slop), and a thumb (white at alpha 70, 2.5 dp wide at the right edge, length
proportional) shows the position. A tap (no drag) on the map box opens the overview.

---

## 17. Context-action button

A control with the "context" action (default label "Act") shows what can be done where
the player stands and does it with one press. Background: this game has no pick-up, climb
or enter commands; each turn it asks about the object underfoot. So most actions are
"send the rest key '.', which makes the game ask again, then answer that question" — and
the answer must be sent only once the question is really on screen.

### 17.1 Button state

Recomputed on the main thread after every game change (coalesced):

1. Advance any pending follow-up first (17.3).
2. No state, no running game, or not in map mode → neutral (own label, disabled).
3. Input mode busy → keep whatever is shown (avoids flicker).
4. Otherwise compute the plan (17.2). No plan → neutral. Else label = the plan's label and
   enabled = the plan is enabled **and** the input mode is command.

A disabled context button is drawn at 45 % of its normal opacity, does not light up or
vibrate when pressed, and pressing it does nothing. In the control editor the button
always shows its own label.

Pressing it (on release) re-checks: game running, map mode, command mode, plan enabled
with something to send; then starts the plan.

### 17.2 Plans

Inputs: context kind, flags (1 = enhanced interface on, 2 = blocked: the game will not
offer the object now, e.g. a monster adjacent or time stopped; 4 = time stopped), a door
bit mask, and the object's name. "Look-plan" below means: enabled unless the blocked flag
is set; send "."; then, if an answer key is given, wait for a prompt and send it.

| Kind | Label | Enabled | Keys |
|---|---|---|---|
| 2 item to take | "Pick up" | not blocked | "." then "t" when a prompt whose answers contain both "t" and "i" appears |
| 3 gold | "Gold" | not blocked | "." |
| 4 stairs up | "Stairs ▲" | not blocked | "." then "u" when the prompt's answers are exactly s, u, d, i, Esc |
| 5 stairs down | "Stairs ▼" | not blocked | "." then "d" (same prompt) |
| 6 volcanic shaft down | "Climb ▼" | not blocked | "." then "c" when the answers are exactly c, i, Esc |
| 7 shaft up | "Climb ▲" | not blocked | "." then "c" (same prompt) |
| 8 building / dungeon entrance | "Enter" | not blocked | "." then "g" when the answers are exactly g, i, Esc |
| 9 dealer's pad | "Visit" | not blocked | "." then "c" when the answers are exactly c, i, Esc |
| 10 chest, enhanced interface | "Open chest" | always (the blocked flag is not consulted) | "O" |
| 10 chest, classic interface | "Open chest" | not blocked | "." then "o" when the answers are exactly o, t, i, Esc |
| 11 fountain | "Fountain" | not blocked | "." (the game's own drink/wash menu follows) |
| 12 altar | "Altar" | not blocked | "." |
| 13 throne | "Throne" | not blocked | "." |
| 14 brass lamp | "Lamp" | not blocked | "." |
| 15 vending machine (Wild) | "Vending" | not blocked | "." |
| 16 LarnCoin ATM (Wild) | "ATM" | not blocked | "." |

If the kind is none of those (0 nothing, 1 inspect-only), closed doors next to the player
are considered. Door bit i (0..7) means the neighbour in direction key h, j, k, l, y, u,
b, n respectively, with arrows "←", "↓", "↑", "→", "↖", "↗", "↙", "↘":

| Situation | Label | Enabled | Keys |
|---|---|---|---|
| time stopped | "Open door" | no | — |
| enhanced interface, exactly one door | "Open {arrow}" | yes | "o", then the door's direction key once the game is in direction-input mode |
| enhanced interface, several doors | "Open door" | yes | "o" (the player answers the direction with a swipe or D-pad) |
| classic interface, exactly one door | "Open {arrow}" | yes | the direction key (walk into the door), then "o" when a prompt with answers exactly o, i, Esc appears |
| classic interface, several doors | "Doors" | no | — |

With no doors: kind 1 (a feature with nothing to do, such as a statue or open door) shows
a disabled button labelled with the last word of the object's name, capitalised ("a great
marble statue" → "Statue"; an empty name → "Act"). Anything else is neutral.

### 17.3 Delayed follow-up keys

Sending "." and "u" blindly would be dangerous: if the stairs prompt does not appear, "u"
would be read as "move north-east". So:

1. **Start**: cancel anything pending. Read the state and remember its sequence number.
   Send the first key(s). If there are follow-up steps, queue them, set a deadline 3000 ms
   ahead, and poll every 60 ms (the check also runs on every game change).
2. **Check** (while a step is pending): if there is no state or the deadline has passed,
   drop everything. If the state's sequence number is still the one remembered at
   sending, or the input mode is busy, keep waiting (the game only publishes when it
   runs, so a new sequence number means it has reacted to our key). Otherwise: if the
   input mode is the one the step expects and its answers test passes, send the step's
   keys, remember the new sequence number, extend the deadline to 3000 ms from now, and
   move to the next step; if not, drop everything (the game went somewhere else, e.g.
   straight back to command mode).

Each step = keys to send, the expected input mode, and a test on the answers string. The
same mechanism is shared with the Cast button (section 18); starting a new sequence
cancels an unfinished one.

---

## 18. Cast button and spell list

A control with the "cast" action (the default "Cast" button):

- If no game is running: nothing. If the game is not waiting for a command: it just sends
  "c" like a plain key button.
- Otherwise it opens a dialog:
  - heading "Cast a spell" (bold, 20 sp); below it "Spell points: {spells} / {spell
    max}";
  - if spell points are 0: "You have no spell points left. They regenerate over time." in
    #FFBE3C, and the list rows are disabled and shown at 40 % opacity;
  - if no spell is known: "You haven't learned any spells yet. Reading books teaches
    them.";
  - one row per known spell in the game's spell order (from the known flags): name (bold
    16 sp) with its three-letter code right of it (monospace 15 sp, #FFBE3C) and a
    description under it (13 sp, 75 % opacity). An unknown code shows the code as its
    name and no description;
  - buttons "Type code…" and "Cancel".
- Choosing a spell: if the game is still waiting for a command, send "c", then the
  three-letter code once a prompt appears whose answers string starts with the 26
  lower-case letters (the game's spell-code prompt). If that prompt does not come (for
  instance the game says there are no spells), the letters are never sent. A following
  direction question is answered by the player as usual.
- "Type code…": dialog titled "Spell code" with a three-character monospace field (hint
  "e.g. pro") and buttons "Cast" / "Cancel". The entry is trimmed and lower-cased; if it
  is exactly three letters it is cast as above, otherwise a toast: "A spell code is three
  letters, e.g. pro".

Names and descriptions shown, in game order (39 spells):

| Code | Name | Description |
|---|---|---|
| pro | Protection | Raises your armor class by 2 for a while. |
| mle | Magic missile | Fires a magic missile in a chosen direction. |
| dex | Dexterity | Raises your dexterity by 3 for a while. |
| sle | Sleep | Puts an adjacent monster to sleep; you hit it 2–4 times. |
| chm | Charm monster | Monsters may be awed by you and leave you alone for a while. |
| ssp | Sonic spear | Hurls a screeching sound blast in a chosen direction. |
| web | Web | Entangles an adjacent monster; you hit it 3–5 times. |
| str | Strength | Raises your strength by 3 for a while. |
| enl | Enlightenment | Reveals the map in a wide area around you. |
| hel | Healing | Restores 20 + 2 × your level hit points. |
| cbl | Cure blindness | Cures blindness. |
| cre | Create monster | Creates a monster next to you. |
| pha | Phantasmal forces | An illusion that kills an adjacent monster if it believes it (needs wisdom). |
| inv | Invisibility | Makes you invisible for a while (longer with an amulet of invisibility). |
| bal | Fireball | Hurls a ball of fire in a chosen direction. |
| cld | Cold | Sends a cone of cold in a chosen direction. |
| ply | Polymorph | Turns an adjacent monster into a random other monster. |
| can | Cancellation | For a few turns, monsters can't use their special attacks. |
| has | Haste self | Speeds up your movements for a few turns. |
| ckl | Cloud kill | A cloud of poison gas hurts every monster around you. |
| vpr | Vaporize rock | Turns the walls around you into open floor. |
| dry | Dehydration | Dries up an adjacent monster for heavy damage. |
| lit | Lightning | Shoots a lightning bolt in a chosen direction. |
| drl | Drain life | Costs you up to half your max HP to deal double that to an adjacent monster. |
| glo | Invulnerability | A globe that raises your armor class by 10 for a while (costs 1 INT). |
| flo | Flood | A deluge of water hurts every monster around you. |
| fgr | Finger of death | Kills an adjacent monster if your wisdom holds (rarely kills you). |
| sca | Scare monster | Frightens monsters so they may not hit you (longer with the Hand of Fear). |
| hld | Hold monster | Freezes monsters in their tracks for a few turns. |
| stp | Time stop | Stops time for a while: monsters can't act. |
| tel | Teleport away | Teleports an adjacent monster elsewhere on the level. |
| mfi | Magic fire | A curtain of fire burns every monster around you. |
| mkw | Make a wall | Builds a wall in a chosen direction. |
| sph | Sphere of annihilation | Sends a sphere that destroys whatever it meets (costs 1 INT; risky). |
| gen | Genocide | Wipes out one kind of monster (costs 1 INT; forgotten after use). |
| sum | Summon demon | A demon strikes an adjacent monster, but may turn on you. |
| wtw | Walk through walls | Lets you walk through walls for a few turns. |
| alt | Alter reality | Rebuilds the level, scattering its items and monsters (costs 1 INT; forgotten after use). |
| per | Permanence | Makes your active spell effects last almost forever (costs 1 INT; forgotten after use). |

(These are the front end's own summaries; the authoritative spell rules are in Part 07.)

---

## 19. Controls: the model

A **layout** is an ordered list of controls plus two global options:

| Layout field | Default | Meaning |
|---|---|---|
| controls | empty | drawing order = list order; the last is on top and is hit-tested first |
| opacity | 0.55 | 0.1..1.0, multiplies every control's alpha |
| haptics ("Vibrate on press") | true | key-tap haptic on control presses; also copied to the game view as its vibration switch (answer buttons, map gestures, beeps, minimap) and gates the danger vibrations |

A **control** has these fields:

| Field | Default | Meaning |
|---|---|---|
| id | — | unique string within the layout |
| type | — | button or D-pad |
| label | "" | text on a button; empty = show a description of the keys (or "Act" for the context action) |
| keys | "" | key notation (section 20) sent by a "send keys" button |
| action | send keys | what a button does (table below) |
| centre x, centre y | 0.5, 0.5 | position of the centre as a fraction of the overlay's width / height |
| width, height | 56 dp, 56 dp | size in dp (so shapes survive different screens) |
| repeat | false | auto-repeat while held |
| shape | rounded | rounded rectangle or circle (buttons) |
| D-pad keys | y, k, u, h, ., l, b, j, n | nine key notations, row by row from the top-left: NW, N, NE, W, centre, E, SW, S, SE; empty = that zone does nothing |
| eight-way | true | D-pad: allow diagonals, or 4-way only |

New ids are made as prefix + n, where n starts at (number of controls + 1) and increases
until unused.

On screen a control's rectangle is its size centred on (centre x × overlay width, centre
y × overlay height), with the centre clamped so the whole control stays inside the
overlay.

Button actions, in the order of the editor's drop-down, with their titles there:

| Action | Title | Behaviour |
|---|---|---|
| send keys | "Send keys" | sends the parsed key notation when pressed |
| run | "Run modifier (next D-pad move runs)" | section 22.3 |
| keyboard | "Show / hide keyboard" | toggles the soft keyboard |
| menu | "Open app menu" | opens the in-game menu (Part 10) |
| edit | "Edit controls" | enters edit mode (section 24) |
| context | "Context action (take, stairs, enter, open…)" | section 17 |
| cast | "Cast spell (opens the spell list)" | section 18 |
| photo | "Take a photo (Wild Edition photo mode)" | takes a photo if a game is running (Part 13) |

---

## 20. Key notation

Human-editable text converted to the bytes sent to the game. Processing is left to right:

| Notation | Byte(s) |
|---|---|
| backslash + e or E | 0x1B (Escape) |
| backslash + n or r | 0x0D (Enter) |
| backslash + s | 0x20 (space; a literal space also works) |
| backslash + t | 0x09 (Tab) |
| backslash + b | 0x7F (labelled Backspace; note this is DEL, whereas the view's own ⌫ button and the keyboard send 0x08) |
| backslash + backslash | a literal backslash |
| backslash + ^ | a literal caret |
| backslash + < | a literal "<" |
| backslash + anything else, or a backslash at the very end | the backslash itself is sent literally; the following character is then processed normally |
| ^ + a letter (either case) | control code 1..26 |
| ^ + @, [, backslash, ], ^, _ | 0, 27, 28, 29, 30, 31 |
| ^ + ? | 127 |
| ^ + anything else, or ^ at the very end | a literal caret |
| <name> (case-insensitive) | esc → 0x1B; ret, enter → 0x0D; spc, space → 0x20; tab → 0x09; del, bs → 0x7F |
| < not forming a known name | a literal "<" |
| any other printable ASCII character 0x20–0x7E | itself |
| anything else (non-ASCII, control characters) | dropped |

Examples: "i" sends i; "Hl" sends H then l; "c" + backslash + "ea" sends c, Escape, a;
"^R" sends Ctrl+R (18).

**Describing bytes** (used for auto-labels and the editor's preview): each byte becomes a
word, joined by single spaces: 0x1B "Esc"; 0x0D or 0x0A "Enter"; 0x20 "Space"; 0x09
"Tab"; 0x7F "Bksp"; other values 1..26 "Ctrl+" and the letter; everything else the
character itself.

---

## 21. Default layouts

Used when no layout has been saved for the current orientation (and by Reset). They are
computed for the overlay's actual size in dp, w × h, on an 8 dp grid: gap G = 8, screen
margin M = 12, snap(v) = v rounded to the nearest multiple of 8 (halves round up).
Positions are then stored as fractions like any user layout. Unless stated, buttons are
rounded rectangles, do not repeat, and send keys.

The buttons:

| Id | Label | Keys / action |
|---|---|---|
| esc | "Esc" | Escape |
| ret | "Enter" | Enter |
| spc | "Space" | space |
| inv | "Inv" | i |
| cast | "Cast" | cast action (its keys field holds c) |
| quaff | "Quaff" | q |
| read | "Read" | r |
| wield | "Wield" | w |
| wear | "Wear" | W |
| drop | "Drop" | d |
| eat | "Eat" | e |
| run | "Run" | run-modifier action |
| kbd | "⌨" | keyboard action |
| help | "?" | ? |
| disc | "Known" | a single backslash (the game's "list what is known" command) |
| rest | "Rest" | . |
| save | "Save" | S |
| tele | "Tele" | Z |
| menu | "☰" | menu action |
| ctx | "Act" | context action |
| dpad | — | D-pad with the default keys, 8-way, repeat on |

### 21.1 Portrait (opacity 0.55)

1. Button width bw = snap(((w − 2M − G) × 0.5 − 2G) / 3) clamped to 48..72; button height
   48. Grid width = 3bw + 2G; grid height = 4 × 48 + 3G = 216. bottom = h − M.
2. **Action grid**, 3 columns × 4 rows, its bottom-right corner at (w − M, bottom), gaps G,
   filled row by row: Esc, Enter, Space / Inv, Cast, Quaff / Read, Wield, Wear / Drop,
   Eat, Run.
3. **D-pad** diameter dia = snap(min(w − 2M − grid width − 2G, 160, 200)) (160 = grid
   height − one button height − G). Centre at (M + dia/2, bottom − 80).
4. **Act** button above the D-pad: centre (M + dia/2, bottom − 216 + 24), size dia × 48
   (so its top lines up with the grid's top row).
5. **Utility row** above both: centre y = bottom − 216 − G − 20; seven buttons 40 tall and
   uw = snap((w − 2M − 6G) / 7) clamped to 40..60 wide, spread evenly from x = M to
   x = w − M (first flush left, last flush right): ⌨, ?, Known, Rest, Save, Tele, ☰. The
   ⌨ and ☰ buttons are circles.

Example, 360 × 740 dp: bw 48; grid columns centred at x = 212, 268, 324 and rows at
y = 536, 592, 648, 704; D-pad 160 across centred (92, 648); Act 160 × 48 centred (92,
536); utility buttons 40 × 40 at y = 484, x from 32 to 328 in equal steps.

### 21.2 Landscape (opacity 0.5)

1. Button width 60; height bh = snap((h − 2M − 4G) / 5) clamped to 40..52.
2. **Grid** of 15 buttons, 3 columns × 5 rows, right edge at w − M, vertically centred on
   the screen (bottom at h/2 + grid height/2, grid height = 5bh + 4G): Esc, Enter, Space /
   Inv, Cast, Quaff / Read, Wield, Wear / Drop, Eat, Known / Rest, Save, Tele.
3. **D-pad** bottom-left: dia = snap(min(h − 2M − 56 − G, 190)) (so the 190 cap becomes
   192); centre (M + dia/2, h − M − dia/2).
4. Above it, on one line with centre y = h − M − dia − G − 22: **Run**, 64 × 44, centre
   x = M + 32; and **Act**, (dia − 72) × 44, filling the rest of the D-pad's width
   (centre x = M + 72 + (dia − 72)/2).
5. Top-left: three 40 × 40 circles at centre y = M + 20, spread evenly across the D-pad's
   width (x from M to M + dia): ⌨, ?, ☰.

Example, 740 × 360 dp: bh 52; grid columns at x = 562, 630, 698, rows at y = 60, 120,
180, 240, 300; D-pad 192 across centred (108, 252); Run centred (44, 126); Act 120 × 44
centred (144, 126); circles at (32, 32), (108, 32), (184, 32).

---

## 22. Controls overlay: play mode

### 22.1 Hit testing

Controls are tested from the top-most (last in the list) down. Each control's rectangle
is grown by 6 dp on every side. D-pads and circular buttons are hit if the point is
within half the grown rectangle's smaller side from its centre; rectangular buttons if
the point is inside the grown rectangle. Every finger is tracked separately, so several
controls can be held at once.

### 22.2 Buttons

- **Send keys**: on finger-down, haptic and send the keys. With repeat on, the keys are
  re-sent after 320 ms and then every 110 ms while held. Repeating stops for good when
  the finger slides more than 24 dp outside the button (sliding back does not restart
  it) or lifts.
- **Keyboard, menu, edit, context, cast, photo**: haptic on finger-down (for the context
  action only if it is enabled); the action fires on **release**, and only if the finger
  is still within 24 dp of the button and the gesture was not cancelled.
- A button looks pressed while a finger that went down on it is within that 24 dp margin.

### 22.3 The run modifier

Two flags, armed and locked.

- Finger-down on a run button (with haptic): if locked → clear both; else if armed →
  disarm; else arm, and start a 550 ms timer. If the finger is still down when the timer
  fires, the modifier becomes **locked** (armed cleared) with another haptic. Lifting the
  finger cancels the timer. So: tap = next move runs; long-press = stay in run mode; tap
  again = off.
- Whenever a D-pad direction (not the centre) fires while armed or locked, its key is
  replaced by its run variant: a notation that is exactly one lower-case letter becomes
  the upper-case letter; anything else is unchanged. If only armed, the arm is consumed
  by that one key (so with hold-to-repeat the first move runs and the repeats are plain
  steps); if locked, every move runs.

### 22.4 D-pad

Zone from the finger's offset (dx, dy) from the D-pad's centre, with radius r = half the
D-pad's width:

1. If the distance is under 0.28 r: the centre zone.
2. Four-way: if |dx| > |dy| then east (dx > 0) or west; otherwise south (dy > 0) or
   north.
3. Eight-way: the same 45° sectors as section 11.5, mapped to the zones E, NE, N, NW, W,
   SW, S, SE.

Zone numbering matches the key list: 0 NW, 1 N, 2 NE, 3 W, 4 centre, 5 E, 6 SW, 7 S,
8 SE.

- Finger-down: fire the zone's key (if it has one: haptic, run substitution, send). If
  repeat is on and the zone is not the centre, re-fire after 320 ms and then every
  110 ms.
- Finger move: the zone is recomputed from the angle wherever the finger is, even outside
  the pad. When it changes, the repeat stops; if the new zone is not the centre its key
  fires at once and (with repeat on) the repeat restarts with the 320 ms delay. Sliding
  into the centre only pauses. The centre key (default "." = rest) fires only on a
  finger-down in the centre, never repeats, and is never turned into a run.
- Finger-up: stop.

### 22.5 Haptics, opacity, accessibility

- Control haptics are the platform "keyboard tap" effect, only if the layout's "Vibrate
  on press" is on.
- Opacity: every colour's alpha is multiplied by the layout opacity; in edit mode by at
  least 0.75; in high-contrast mode by at least 0.92.
- TalkBack exposes each button and each D-pad direction as a virtual button (Part 10).
  Activating one fires it once: a D-pad zone fires its key; a send-keys button sends; a
  run button toggles (on → off, off → armed; no lock); other actions run immediately.

---

## 23. Controls overlay: drawing

Accent #FFBE3C; "dark ink" #1E1600. Labels use the default typeface with synthetic bold,
centred.

**Button.** "Lit" = pressed (and, for the context action, enabled) or a locked run
button. "Active" = a run button while armed or locked.

- Corner radius 10 dp, or a circle of half the smaller side.
- Body: accent if lit; else #785A1E at alpha 230 if active; else #262830 at alpha 215
  (opaque black in high contrast). A disabled context button (outside edit mode) uses
  45 % of the overall alpha for everything.
- Outline, inset 0.5 dp: 1 dp wide; black at alpha 120 if lit, else white at alpha 230 if
  active, else white at alpha 70. High contrast: 2 dp (4 dp when active), solid black if
  lit, else solid white.
- Label: the live context label for a context button outside edit mode if there is one;
  else the control's label; else "Act" for the context action or the description of its
  keys. Colour: dark ink if lit, else white, at min(1, overall alpha + 0.35); high
  contrast: black if lit, else white. Size: min(42 % of the height × text scale, 16 dp ×
  text scale, 70 % of the height), then shrunk if needed to fit 82 % of the width.

**D-pad.**

- Base: a disc #22242C at alpha 205 (black in high contrast) with a 1 dp white outline at
  alpha 70 (2 dp solid white in high contrast).
- For each direction zone that has a key (diagonals skipped when 4-way): if it is the
  zone under the finger, an accent wedge: the 45° annular sector centred on the
  direction, from 0.3 r to r. A chevron (small triangle) pointing outward with its tip
  at 0.8 r, 0.195 r long and 0.26 r wide at the base: dark ink on the active zone; else
  accent while the run modifier is armed or locked; else #EBEBF0 at alpha 235 (white in
  high contrast).
- A zone's key is written (at 0.52 r from the centre, at most 0.3 r wide, height 0.16 r ×
  text scale; white at alpha 200, or dark ink on the active zone) only in edit mode or
  when the key differs from the default for that zone.
- Centre, if it has a key: a disc of radius 0.255 r, #373A44 at alpha 200 (dark grey in
  high contrast; accent while pressed), labelled "•" if the key is "." and otherwise
  with the key's description (white, or dark ink while pressed).
- While the run modifier is armed or locked: the word "RUN" in accent, centred 0.38 r
  below the centre (height 0.14 r, at most 0.5 r wide).

---

## 24. Edit mode

### 24.1 Entering and leaving

Entered from the start panel's or in-game menu's "Edit controls" entry, or a control with
the edit action. On entry: the start panel and soft keyboard are hidden, a copy of the
current layout is kept for Cancel, the toolbar appears, and a long toast shows: "Drag to
move · corner handle to resize · tap a selected control to edit it". The game keeps
running underneath but receives no touches or hardware keys.

- **Done**, the Back button, or an orientation change: save the layout for the
  orientation it was loaded for, and leave.
- **Cancel**: restore the copy taken on entry, and leave.

On leaving, the reserve is recomputed and, if no game is running, the start panel
returns.

### 24.2 The editing surface

The screen is dimmed with black at alpha 70 and a guide grid is drawn every 24 dp (1 px
lines, white at alpha 28). Every control gets a dashed frame around its rectangle: white
at alpha 160 and 1.2 dp for unselected ones; accent and 2.5 dp for the selected one, plus
a resize handle — an accent disc of radius 14 dp centred on the bottom-right corner with
a short black diagonal stroke. D-pads show all their key labels. Controls are drawn with
at least 75 % opacity.

Touch handling (exact rectangles, no slop):

1. Finger-down within 25.2 dp of the selected control's bottom-right corner starts a
   **resize**.
2. Otherwise finger-down on a control selects it, brings it to the front (moves it to the
   end of the list, permanently changing the stacking order) and starts a **move**.
   Finger-down on empty space clears the selection.
3. Nothing changes until the finger has moved more than the platform touch slop.
4. Move: the control's centre follows the finger, keeping the original grab offset; with
   snapping on, the centre's pixel position is rounded to the nearest multiple of 8 dp;
   then stored as fractions clamped to 0..1.
5. Resize: the top-left corner stays fixed; width and height follow the finger (snapped
   to 8 dp multiples of screen position when snapping is on), minimum 28 dp each, at most
   the overlay's size; D-pads and circular buttons stay square (the larger of the two).
6. Release without having moved, on a control that was **already selected** before this
   touch, within 500 ms of touching down: open its properties dialog.

### 24.3 Toolbar

A horizontally scrollable strip (background #0A0A0C at alpha 200; 8 dp / 6 dp padding)
across the top of the screen, holding pill buttons (36 dp tall, corner radius 18 dp, 14 dp
side padding, 6 dp apart, white text on #2D2F37 at alpha 220; the accent pill is black
text on #FFBE3C), in this order:

| Pill | Effect |
|---|---|
| "Done" (accent) | save and leave |
| "+ Button" | add a button: id "btn{n}", label "New", no keys, centre (0.5, 0.6), 56 × 46 dp; select it and open its properties |
| "+ D-pad" | add a D-pad: id "dpad{n}", centre (0.5, 0.6), 160 × 160 dp, repeat on, default keys; select it and open its properties |
| "Edit selected" | open the selected control's properties (disabled and at 40 % opacity when nothing is selected) |
| "Snap: on" / "Snap: off" | toggle snapping to the 8 dp grid (default on) |
| "Look" | appearance dialog |
| "Profiles" | profiles dialog |
| "Reset" | confirmation "Reset the portrait layout to the default?" (or "landscape"), buttons "Reset" / "Cancel"; on Reset the saved layout for this orientation is deleted immediately and the freshly computed default is loaded into the editor |
| "⇅" | move the toolbar to the other edge (top ↔ bottom); spoken description "Move toolbar to the other edge" |
| "Cancel" | discard and leave |

A control added with "+" stays even if its properties dialog is cancelled.

**Look** — dialog "Controls appearance": "Opacity: {n}%" with a slider from 10 % to 100 %
in 1 % steps, applied live; a checkbox "Vibrate on press"; button "OK". Both edit the
layout being edited.

**Profiles** — dialog "Layout profiles", a list:

1. "Save current layout as…" → dialog "Save profile" with a field (hint "Profile name")
   and "Save" / "Cancel". A non-blank trimmed name stores the layout under it at once
   (overwriting a profile of the same name) and toasts: Saved "{name}".
2. "Load: {name}" for every stored profile, sorted by name → loads it into the editor;
   toast "Loaded — tap Done to keep it".
3. "Copy layout to clipboard (JSON)" → puts the layout's JSON text (section 25) on the
   clipboard (clip label "Ularn controls"); toast "Layout copied".
4. "Paste layout from clipboard" → parses the clipboard text; on failure toast "Clipboard
   doesn't contain a layout"; on success loads it into the editor; toast "Pasted — tap
   Done to keep it".
5. "Delete a profile…" (only if any exist) → dialog "Delete profile" listing the names;
   tapping one deletes it at once; toast "Deleted".

Profiles are not tied to an orientation. Loading or pasting only changes the editor's
working layout: Done keeps it, Cancel drops it. Saving and deleting profiles, and Reset's
deletion of the saved layout, take effect immediately regardless of Done/Cancel.

### 24.4 Properties dialog

Title "Edit button" or "Edit D-pad". It works on a copy; nothing changes until Save.
Sections (bold headers), top to bottom:

- **"Label"**: a single-line field, hint "Shown on the button (blank = show keys)".
- Buttons only — **"Action"**: a drop-down of the eight action titles (section 19).
  Under it a note for two actions: context — "Changes with what you stand on: Pick up,
  Stairs, Enter, Open chest/door, Fountain, Altar… Dimmed when there's nothing to do. The
  label above is shown then."; cast — "Opens a list of the spells you know. When the game
  isn't waiting for a command it just sends 'c'."
- Buttons only, visible only for the send-keys action — **"Keys to send"**: a monospace
  single-line field; a live preview "Sends: {description}" or "Sends: (nothing)";
  quick-insert buttons "Esc", "Enter", "Space", "Ctrl", "Bksp" that insert, at the text
  cursor, backslash-e, backslash-n, backslash-s, "^" and backslash-b respectively; and
  a two-line help text: "Type the keys the game should receive, in order. e.g.  q
  (quaff)  ·  c\ea  ·  ^R" / "\e Esc   \n Enter   \s Space   \b Backspace   ^X Ctrl+X
  \\ backslash".
- D-pads only — **"Direction keys"**: note "NW  N  NE / W  centre  E / SW  S  SE. Same
  notation as buttons. Leave the centre blank to disable it."; a 3 × 3 grid of centred
  monospace fields (at least 72 dp wide) holding the nine keys; a button "Reset to y k u
  / h . l / b j n"; a checkbox "8-way (diagonals)"; note "Tip: the Run button makes the
  next move a run (H J K L...). Long-press Run to keep it on."
- **"Behaviour"**: checkbox "Repeat while held" (buttons) or "Keep moving while held"
  (D-pads); for buttons also "Round button".
- **"Size"**: sliders from 24 to 300 dp with live captions — "Diameter: {n} dp" for a
  D-pad (sets both sides), "Width: {n} dp" and "Height: {n} dp" for a button.
- Dialog buttons: "Save", "Cancel", and "More…" which offers "Duplicate" and "Delete".

Save: the label and keys text are taken as typed, a D-pad takes its nine fields, a round
button's sides are both set to the larger of the two, and the edited copy replaces the
control. Delete removes the control and clears the selection. Duplicate closes the dialog
and adds a copy of the control **as it was before this dialog's edits**, with a new id
("btn{n}" or "dpad{n}"), shifted by +0.06 of the width and +0.04 of the height (not past
1.0), and selects it.

---

## 25. Persistence

Private preferences store named "controls":

| Key | Value |
|---|---|
| "layout_port" | JSON text of the portrait layout (absent = use the computed default) |
| "layout_land" | JSON text of the landscape layout |
| "profiles" | set of profile names |
| "profile:{name}" | JSON text of that profile |

A layout is loaded once per orientation when the overlay first has a size (and again
after an orientation change). If the stored text is missing or cannot be parsed, the
default for the current overlay size is computed; the default is not written back until
the user leaves edit mode with Done. The loaded layout's haptics option is copied to the
game view.

JSON format (written pretty-printed with 2-space indentation; this is also the clipboard
format):

| Member | Type | Written | Read |
|---|---|---|---|
| version | number | always 1 | ignored |
| opacity | number | layout opacity | default 0.55; clamped to 0.1..1 |
| haptics | boolean | layout option | default true |
| controls | array of objects | every control in order | **required**; anything without it is not a layout |

Each control object:

| Member | Written | Read (default; clamp) |
|---|---|---|
| id | the id | "c" + array index if missing |
| type | "BUTTON" or "DPAD" | BUTTON if missing or unknown |
| label | text | "" |
| keys | key notation | "" |
| action | "KEYS", "RUN", "KEYBOARD", "MENU", "EDIT", "CONTEXT", "CAST" or "PHOTO" | KEYS if missing or unknown |
| cx, cy | centre fractions | 0.5; clamped to 0..1 |
| w, h | size in dp | 56; clamped to 24..400 |
| repeat | boolean | false |
| shape | "ROUNDED" or "CIRCLE" | ROUNDED |
| dpad | array of nine key notations — D-pads only | used only if it has exactly nine entries; otherwise the default keys |
| eightWay | boolean — D-pads only | true |

Related settings live in the general settings store (Part 10): "tapMove" (true),
"swipeMove" (true), "flickRun" (false), "fitBeside" (true), "arrows" (true), "minimap"
(true), "sidePanel" (true), "tileDp" (0 = automatic), "dangerPct" (25), "dangerPulse",
"dangerVignette", "dangerVibrate", "bigHitCue" (all true).

---

## 26. When no game is running, and the end of a game

- Before any game has published, the view is just the dark background (the start panel
  sits over it).
- The game itself shows its final pages (death or victory text, score, scoreboard) as
  ordinary text pages and waits for a key on each; they are continued by tapping, like
  any page. When the game process then exits, the shell marks the view as not running,
  hides the keyboard and returns straight to the start panel (or finishes the app if the
  player chose "Save game & quit" or "Leave"); if controls are being edited at that
  moment the start panel waits until editing ends.
- While not running, the view keeps drawing the last published screen behind the start
  panel, but: swipes, map taps and long-presses are off, the minimap is hidden, controls
  send nothing, and a tap that is not on a leftover answer button asks the shell to show
  the start panel.
- The end-state, final-score and end-reason fields of the shared state are not drawn by
  the game view; they feed statistics, music and the automatic photo (Parts 10 and 13).

---

## 27. Wild Edition hooks in this part (summary)

| Where | Hook | Owner |
|---|---|---|
| per frame | glitch and weather modules are updated with the fresh state | Part 13 |
| HUD line 2 | LarnCoin ticker (specified in section 5) | this part; price rules in Part 12 |
| HUD line 2 | sun/moon tile after the level text | Part 13 |
| HUD lines 2–3 | glitch text corruption | Part 13 |
| map tiles | hat overlay on the player; heart tile 397 on the pet; glitch row shift, overlay tiles and colour pass | Parts 12, 13, 15 |
| after tiles | weather per-cell pass, day/night tint; lightning flash over the whole view | Part 13 |
| photo | render-visible-map, single-tile and player-tile services; the "photo" button action | Part 13 |
| side panel | PERKS, PET, BOUNTY sections (section 16) | text helpers in Part 13, rules in Part 12 |
| context action | kinds 15 (vending machine) and 16 (LarnCoin ATM) | Part 12 |

---

## 28. Quirks worth reproducing

1. In map mode the one-row answer strip is always reserved, yet a multi-row item grid is
   drawn over the bottom of the map and leaves that strip empty.
2. A message row is coloured as a whole by its first highlighted non-space character.
3. Only the last four wrapped message lines are visible, although the game has five rows.
4. Text pages narrower than 40 columns are sized as if they were 40 wide; a fitting page
   is top-aligned and horizontally centred.
5. The re-wrapper keeps one blank line even at the very top of a page, and turns an
   over-wide centred or right-aligned single line into a right-flush line.
6. A re-wrapped page does not auto-scroll to its prompt.
7. With flick-to-run on, the flick is timed from finger-down, but the delayed first step
   from the threshold crossing.
8. A held D-pad direction after a single tap on Run runs once and then walks.
9. The key notation's "backspace" is byte 0x7F, while the built-in ⌫ button and keyboards
   send 0x08.
10. With the enhanced interface the chest action ignores the "blocked" flag.
11. Selecting a control in the editor permanently moves it to the top of the stacking
    order.
12. The editor's guide grid is 24 dp, but snapping is to 8 dp.
13. Editor Reset deletes the saved layout at once: a later Cancel shows the old layout
    again only until it is next loaded, when the default appears.
14. Duplicate copies the control as it was before the open dialog's unsaved edits.
15. The landscape D-pad's 190 dp cap snaps up to 192 dp.
16. The danger vignette's pulsing is governed by the "Pulse the HP bar red" toggle, not
    by its own.
17. The side panel and the overview dialog ignore the text-scale and high-contrast
    options.

---

## 29. Index of user-visible strings in this part

HUD: "HP {hp}/{max}", "Gold {n}", "Lv {n} {title}  ·  Exp {n}  ·  Home", "…  ·  Level
{name}", "STR", "INT", "WIS", "CON", "DEX", "CHA", "AC {n}", "WC {n}", "Sp {a}/{b}",
"Ⓛ {price}", " ▲", " ▼", and the 17 effect names (section 1).

Answer buttons: "Enter", "Esc", "Space", "All (*)", "All", "0"–"9", "⌫", "⌨ Type", "Swipe
or press a direction", "Gold", "item {letter}", "Nothing", "Show all", "Cancel".

Minimap / overview: "No map yet", "Level map", "Home — map", "Level {name} — map", "Tap
or Back to close", and the 14 legend labels (section 15).

Side panel: "No character yet", "CHARACTER", "Level {n} {title}", "Home", "Dungeon level
{name}", "HP {a}/{b}", "Spells {a}/{b}", "AC {n}   WC {n}   Exp {n}", "Gold {n}", "Turn
{n}", "PERKS" and the 11 perk names, "PET", "BOUNTY", "MAP", "INVENTORY", "{letter})",
"Nothing".

Context button: "Act", "Pick up", "Gold", "Stairs ▲", "Stairs ▼", "Climb ▼", "Climb ▲",
"Enter", "Visit", "Open chest", "Fountain", "Altar", "Throne", "Lamp", "Vending", "ATM",
"Open door", "Open ←" (and ↓ ↑ → ↖ ↗ ↙ ↘), "Doors".

Spell list: "Cast a spell", "Spell points: {a} / {b}", "You have no spell points left.
They regenerate over time.", "You haven't learned any spells yet. Reading books teaches
them.", "Type code…", "Cancel", "Spell code", "e.g. pro", "Cast", "A spell code is three
letters, e.g. pro", and the table in section 18.

Controls: default labels (section 21), "RUN", "•", key descriptions "Esc", "Enter",
"Space", "Tab", "Bksp", "Ctrl+{letter}".

Editor: the entry toast, the toolbar pills, and every dialog string quoted in section 24;
action titles in section 19.

Settings labels that govern this part (dialog in Part 10): "Swipe to step, hold to keep
walking", "Quick flick runs (instead of one step)", "Tap a square to step towards it
(long-press to run)", "Landscape: fit the map beside the controls", "Minimap (tap it to
see the whole level)", "Tablets / foldables: side panel with stats, map and inventory",
"Reset map zoom", "Zoom reset", "Pinch the map to zoom in or out; the view follows your
character.", "Arrow keys / numpad move (hardware keyboard)", and the danger-cue labels in
section 13.
