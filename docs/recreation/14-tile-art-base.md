# Part 14 — Tile art: the base sheet (ids 0-367) and every tile mapping

This part describes the game's map graphics: the format of the tile sheet, the look of every one of
the 368 original tiles (ids 0 to 367), and every table that turns game state into a tile id
(monsters, objects, walls, the player, spell effects, the cursor). It is written so that an artist
with no access to the original image can draw an equivalent sheet, and so that a programmer can
wire the ids up identically. It leans on Part 04 (monster table), Part 06 (object table), Part 05
(when wall shapes are recomputed), Part 07 (which spell shows which effect, and its timing), Part 09
(the shared display state the tile ids are written into) and Part 11 (how the view scales, scrolls
and layers the tiles). Tiles from id 368 upward (Wild Edition rows) are Part 15.

Random helpers used below: rnd(n) is uniform over 1..n and rund(n) is uniform over 0..n-1.

## 1. Sheet format

| Property | Value |
|---|---|
| Asset shipped in the app | one PNG, `ularn_tiles.png`, 512 x 896 pixels, 8-bit RGBA, non-interlaced |
| Original part of it | the top 512 x 736 pixels (23 rows of tiles): ids 0 to 367. A copy of just this part is kept in the tools folder as the "base sheet" and is pixel-identical to the top of the shipped sheet |
| Tile size | 32 x 32 pixels |
| Grid | 16 tiles per row, no gutters, no margin |
| Tile id to position | column = id mod 16, row = id div 16 (integer division); pixel origin = (column x 32, row x 32) |
| Number of base tiles | 368 (16 x 23) |
| Alpha | strictly binary: every pixel is either fully opaque (alpha 255) or fully transparent (alpha 0). No partial alpha anywhere |
| Distinct opaque colours in the base sheet | about 1,565 (most come from a few anti-aliased effect tiles; the hand-drawn tiles use a small recurring palette, section 3) |

### 1.1 Transparency rules (important and easy to get wrong)

Almost nothing in the base sheet is transparent. The rules are:

1. **Every creature, object, feature, player, spell-bolt and spell-animation tile is fully opaque
   and has the dungeon floor painted into it.** The "background" of these tiles is not black and
   not transparent: it is the flat floor colour **#426b6b** (a dull dark teal, RGB 66,107,107).
   A monster tile is therefore "monster standing on floor", an item tile is "item lying on floor".
   The floor colour alone fills 225,664 of the base sheet's 376,832 pixels.
2. **The empty-floor tile (id 191)** is 32 x 32 of pure #426b6b with no texture at all.
3. **The unknown/unexplored tile (id 175)** is 32 x 32 of pure opaque black #000000.
4. **Wall tiles (352-367), door/arch and building tiles** are opaque; they use near-black #111111
   for mortar, outlines and dark interiors, and show the teal floor only where the art leaves a gap
   (rounded wall corners, around free-standing buildings).
5. **Exactly nine tiles are transparent overlays:** the cursor (id 174) and the eight sparkle
   frames (ids 240-247). In these, every pixel that is not part of the drawing has alpha 0.
   Tile 174 has 80 opaque pixels, tiles 240-247 have 18, 42, 80, 171, 179, 100, 33 and 10.
6. **Nine tiles are leftover "mask partners"** of those overlays: 190 (the cursor on black) and
   248-255 (the sparkle frames on black). They are opaque, never drawn by this game, and exist
   only because of how the sheet was derived (section 2).

Consequence for a renderer: one base tile per map cell is enough; there is no separate floor layer.
Overlay tiles (174, 240-247 and the Wild overlays of Part 15) are alpha-blended on top.

## 2. Provenance: how the sheet was derived and what was baked in

The art is the graphics file of the upstream Ularn 1.6.3a X11/Windows/Amiga release, an XPM image
named `ularn_gfx.xpm` with the same 16-column layout of 32 x 32 tiles. The Android port converted
that XPM to PNG once and made one change, described in the project as "the cursor/sparkle masks
baked in":

1. In the upstream X11 front end, overlay tiles had no alpha. Instead each overlay existed twice
   in the sheet, as a pair "tile A / tile B": the same drawing on two different background
   colours (A on the floor-style background, B on black). The pairs were (240,248), (241,249),
   (242,250), (243,251), (244,252), (245,253), (246,254), (247,255) for the sparkle and (174,190)
   for the cursor.
2. At start-up upstream compared the two tiles of each pair pixel by pixel and built a 1-bit clip
   mask for tile A: a pixel is **opaque where A and B have the same colour** (that is the drawing)
   and **transparent where they differ** (that is the background).
3. The Android sheet has this mask applied permanently: in tiles 174 and 240-247 the background
   pixels were given alpha 0. The B partners (190, 248-255) were left exactly as upstream had
   them, on opaque black. A check confirms it: each of 240-247 equals its partner 248-255 with the
   black pixels made transparent, and 174 equals 190 likewise.

Nothing else was changed: no recolouring, no rescaling. Upstream's own tables (player tiles,
effect tiles, wall base, monster and object tile lists) are used unchanged, apart from the Wild
Edition additions (Part 15). Upstream listed a second tile per frame for overlay effects (the B
partner) and a second cursor tile; the Android tables keep only the first.

The Wild Edition tooling treats the base sheet as read-only: authored tiles must have ids of 368
or more, and the shipped sheet is rebuilt as "base sheet + authored tiles", growing by whole rows.

## 3. Palette character

The sheet mixes two hand-pixelled families plus a few scanned or rendered pieces.

**Family A (most monsters and items; a fixed "house" palette of roughly 45 colours).** Muted,
slightly earthy, with few steps per hue. The colours that recur most:

| Use | Colours |
|---|---|
| Floor | #426b6b |
| Shadow | #000000 |
| Dark red-brown / leather / wood | #631800, #422100, #2c1804, #420000 |
| Tan / mid brown | #846331, #6f562c |
| Red | #c72919 |
| Gold and yellow | #b99018 (dark gold), #de9c0d (orange gold), #eedc37 (yellow), #eaf4b2 (pale yellow highlight) |
| Olive | #7a7d24 |
| Warm and cool greys | #837e6c (warm grey), #525252 (dark grey), #b099a8 (lilac grey), #788da7 (slate blue-grey) |
| Whites | #e8f7f7 (off-white), #9fcfac (pale green-white, used as a "steel" highlight), #94deed (pale cyan) |
| Darks | #313124 (olive black), #051818 (teal black), #293952 (navy) |
| Skin | #f79f89, #d98e68 |
| Greens | #00e400, #108400, #73b644, #0c8c52 |
| Blues | #0000ff, #0000c6, #000084, #000063, #005ab5, #00b9c7 (cyan) |
| Teal-blacks (books, emerald, green dragon) | #0c2431, #063146, #214263 |
| Pinks / purples | #eeb1d7, #840084, #63004a, #f3007b |

**Family B (later additions: about a third of the monsters, all the demons, the player
portraits, altar/throne, some items).** Slightly brighter, built from ramps in steps of 0x20 per
channel: reds #400000, #600000, #800000, #a00000, #c00000, #e00000, #ff0000; browns #402000,
#603000, #804000, #a05000; khaki #606030, #808060, #a0a078, #c0c090; ice #608080, #78a0a0,
#90c0c0, #a8e0e0; greens #004000, #006000, #008000, #00a000; golds #604800, #806000, #a07800,
#c09000; blues #000080, #0000c0, #0000ff; neutral greys #101010 ... #f0f0f0; skin #7f5a48,
#a5765e, #ca8f72, #e5a282, #ffb691.

**Stone (walls, arches, buildings, wall-building animation).** A neutral grey ramp #5c5c5c,
#737373, #838383, #939393, #a2a2a2, #b3b3b3, #c4c4c4, #d3d3d3, #e2e2e2, with #111111 mortar and
outlines (not pure black).

**Effects.** Pure saturated primaries: #00ff00, #00ffff, #ff0000, #ff8000, #ffff00, #ffffff,
blues #0050a0, #0060c0, #0070e0, #0080ff. The diagonal versions of three bolt effects are
smoothly rotated copies whose edges blend into the floor colour (that is where most of the 1,565
colours come from); a redraw may use crisp pixels instead.

A second near-floor teal, #476c6c, appears in about ten tiles (wraith, fountain, statue, some
portraits) as dither or stray pixels next to the floor. It is visually indistinguishable from the
floor and has no meaning.

## 4. Style guide for an artist

- **Viewpoint.** Figures and items are drawn in front or three-quarter view as if standing on a
  floor seen slightly from above. Buildings and doors are flat front elevations. The pit, the
  round trap medallions, the teleporter and the yellow mold are top-down.
- **Scale and margins.** There is no common scale: each subject is drawn as large as it
  comfortably fits. Small creatures (gnome, lemming, gnome king) occupy about 22 x 22 px; ordinary
  humanoids about 22 x 30; large monsters, dragons and all demons run edge to edge (0-31 in both
  axes) and may be cropped by the tile edge. Small items (rings, gems, gold) sit centred in about
  16 x 14 px with 8-10 px of floor around them; weapons lie on a diagonal from lower left to
  upper right and may touch the corners; armour is 22-30 px wide. Buildings and arches fill the
  whole tile.
- **Outlines.** Family A and B creatures have **no outline**; shapes are separated from the floor
  by their own darkest shade. A few small items do have a 1-px black or #111111 outline (dagger,
  amulet chains, cannabis leaf, fortune cookie, spell-lightning). Stone structures are outlined
  and jointed in #111111.
- **Cast shadow.** Almost every creature and most items have a hard-edged, pure black (#000000)
  cast shadow painted on the floor: a sheared silhouette that starts under the feet and extends to
  the right and up-right (light from the front left), typically 40-190 pixels. Flying or floating
  subjects (floating eye, quasit) instead have a black ellipse on the floor below them with a gap
  of floor in between. Rings, gems and gold have a 1-2 px black shadow hugging their lower-right
  edge. Insubstantial creatures (wraith, poltergeist, invisible stalker, mimic, water lord has a
  normal one) mostly have none.
- **Shading.** 2 to 4 flat shades per material, lit from the upper left, no gradients. Texture is
  done with single-pixel checker dithering (chain mail, the wraith, the mimic, fading spell
  clouds). No anti-aliasing against the floor except in the rotated bolt tiles.
- **Saturation.** Creatures and items are muted (family A) to moderately bright (family B);
  spell effects and the cursor are fully saturated so they pop.
- **Monsters versus objects versus terrain.** Monsters: big, characterful, facing left or front,
  with shadow. Objects: smaller, centred, on bare floor, with a small shadow. Dungeon furniture
  (altar, throne, stairs, fountain, statue): medium, 20-28 px. Buildings: a grey stone arch or
  facade filling the tile, with a sign or symbol identifying it. Walls: grey brick filling the
  tile.
- **Lettering in tiles.** Several tiles contain readable text or symbols that identify them:
  "DND", "LRS", "McDope's", "$", "8th", "S", "LSD". Keep them.

## 5. Sheet layout at a glance

| Row | Ids | Content |
|---|---|---|
| 0-3 | 0-63 | Monsters: 56 ordinary monsters in monster-id order (0-55), then the demon tiles (56-63) |
| 4-10 | 64-162 | Dungeon features, buildings, items (not in object-id order; see sections 8 and 9) |
| 10 | 163, 164 | Blank floor (unused) |
| 10 | 165-172 | Player, female, classes 0-7 |
| 10 | 173 | Blank floor (unused) |
| 10 | 174 | Cursor overlay (transparent) |
| 10 | 175 | Unknown: solid black |
| 11 | 176-180 | Blank floor (unused) |
| 11 | 181-188 | Player, male, classes 0-7 (directly below the female tiles: male id = female id + 16) |
| 11 | 189 | Blank floor (unused) |
| 11 | 190 | Cursor on black (unused mask partner) |
| 11 | 191 | Empty floor ("nothing"); also used for invisible traps and for "no direction" effects |
| 12-14 | 192-231 | Five directional bolt effects, 8 direction tiles each |
| 14 | 232-239 | Blank floor (unused) |
| 15 | 240-247 | Sparkle overlay, 8 frames (transparent) |
| 15 | 248-255 | Sparkle on black (unused mask partners) |
| 16-21 | 256-351 | Fourteen spell animations, one per column 0-13, six frames each running down the column (frame f of column c is tile 256 + c + 16 x f); columns 14 and 15 are blank floor |
| 22 | 352-367 | Sixteen wall shapes indexed by a 4-bit neighbour mask |

## 6. How a map square gets its tile

The game core keeps, for every cell of the 67 x 17 map: the real object there, the object the
player remembers there (the "known" object, initially Unknown), the monster there (if any), a
per-cell argument value, and two monster visibility bits ("seen" and "awake"). The front end turns
a cell into one tile id with this procedure (run whenever a cell is revealed or redrawn):

1. If the cell is the player's position and the player is not blind: the tile is the player tile
   for the character's class and gender (section 10). Done.
2. Otherwise, if the remembered object is Unknown: the tile is the Unknown tile, **175**.
3. Otherwise, if there is a monster in the cell:
   1. The monster is drawn only if the player is not blind and the monster's "seen" or "awake"
      bit is set. If not, use the tile of the remembered object (step 4).
   2. If it is a **mimic**: when the game turn counter is a multiple of 10, re-roll its disguise:
      repeatedly pick rnd(M) where M is the id of the first demon lord (61 in this build) until
      the pick is neither the invisible stalker nor one of the four Wild Edition monsters. Then
      use the tile of the disguise monster. The disguise is one value shared by all mimics, starts
      as the mimic itself (the rainbow blob, tile 45), is not saved, and is re-rolled every time
      any visible mimic's cell is recomputed during such a turn. Because the pick range includes M
      itself, a mimic can look like a type I demon lord (tile 56), and it can also pick "mimic".
   3. If it is the **invisible stalker** and the player cannot see invisible: use the remembered
      object's tile instead (the stalker's own tile 38 is only seen with see-invisible).
   4. If it is any **demon** (type I demon lord through the God of Hellfire) and the player does
      not carry the Eye of Larn: use the remembered object's tile instead.
   5. Otherwise use the monster's tile from the monster table (section 7).
4. With no monster: use the tile of the remembered object from the object table (section 9).
5. Wild Edition hook: if the chosen tile is the remembered object's own tile, a glitch feature may
   substitute another item's tile (items only; see Part 12).
6. Finally, **if the chosen tile id equals the wall object's tile id (84), replace it by
   352 + the cell's argument value** (the wall-shape mask, section 15). The test is on the tile
   id, not on the object, so it also applies when a wall is what the player remembers under a
   hidden monster.

Each computed tile is stored in the shared display grid and the cell's overlay slot is cleared
(set to "none"). Blindness: while blind the player tile is not shown (step 1 is skipped, so the
player's own cell shows what is remembered there), the cursor is not drawn, newly seen cells are
not revealed, and no spell effect tiles are written.

The classic one-character map symbols still exist in the monster and object tables (they are
listed in sections 7 and 9 for completeness) but the Android display never uses them for the map;
they are only used by the diagnostic dump. There is no character fallback: if the sheet fails to
load, no map is drawn.

## 7. Monster tiles (ids 0-63)

Mapping rule: ordinary monsters 1 to 56 use tile (monster id - 1). Monster id 0 ("unseen
attacker", never placed on the map) is listed as tile 0. The Wild Edition monsters 57-60 use tiles
384-387 (Part 15). The demons use tiles 56-63, with **type I and type II demon lords sharing tile
56**.

"Art box" is the bounding box of non-floor pixels as x-range, y-range (0-31, origin top left).
All tiles are opaque on floor #426b6b; "shadow" means the black cast shadow of section 4.

### 7.1 Row 0 (tiles 0-15)

| Tile | Monster id and name | Map char | Art box | Depiction |
|---|---|---|---|---|
| 0 | 1 lemming (also id 0) | l | 1-26, 15-29 | Small grey rodent crouched low, facing left, in the lower half of the tile. Fur #837e6c with #525252 bands, navy #293952 underside, a 2-px white tooth #e8f7f7. Thin black shadow under the body. |
| 1 | 2 gnome | G | 5-27, 6-29 | Tiny front-facing garden gnome: tall pointed green hat (#00e400, shade #108400) with a red tip #c72919, pink face #f79f89, white beard (#e8f7f7, #eaf4b2), green tunic and legs, gold belt #b99018, brown boots #846331/#631800. Shadow to the right. |
| 2 | 3 hobgoblin | H | 2-29, 3-31 | Red-skinned warrior, front view: grey iron helmet (#837e6c/#525252), red jerkin #c72919 over dark red limbs (#631800, #420000), round wooden shield with gold rim on the viewer's left, short sword with pale pink blade held out to the right. |
| 3 | 4 jackal | J | 1-27, 8-30 | Lean khaki dog standing side-on, head to the left, pointed ears, tail raised at right. Three khaki shades #606030, #808060, #a0a078; red tongue #ff0000 (4 px). |
| 4 | 5 kobold | K | 7-27, 0-30 | Gaunt upright dog-eared humanoid, front view, arms hanging: dark browns #202000, #402000, #603000, dark green ragged tunic #004000/#006000, two glowing red eyes. |
| 5 | 6 orc | O | 3-30, 2-30 | Pig-faced orc, front view: pink head (#ca8f72/#a5765e) with flat snout, tusks and pointed ears; grey breastplate; dark red studded kilt #800000 with gold dots; round black shield (#101010/#303030) with a red boss on the left; curved white scimitar raised on the right. |
| 6 | 7 snake | S | 1-31, 7-26 | Green snake in a tall S-coil, head at upper left with a red forked tongue, tail trailing left along the floor. Body #108400 with #00e400 highlights, a yellow belly line #eedc37 and regular red spots #c72919. |
| 7 | 8 giant centipede | c | 6-27, 1-30 | Segmented centipede curving from upper right down to lower left, seen from above: banded greens #024202, #026603, #029a02, tan legs #996530 sticking out both sides, red eyes, orange pincers #ff6500 at the lower-left head end. |
| 8 | 9 jaculi | j | 0-30, 0-31 | A hunched, vulture-like biped filling the tile: bright red body and legs #c72919 patterned with six white squares, shaggy navy and olive-black feathers (#293952, #313124) forming a crest and drooping wings, an orange-gold shoulder patch #de9c0d, brown clawed feet #846331 and a brown clawed hand at lower left. |
| 9 | 10 troglodyte | t | 8-31, 3-30 | Stocky horned cave-man, front view: dark red-brown skin (#631800, #422100, #2c1804), two short horns, red eye slit, ragged olive loincloth (#6f562c, #7a7d24). Large shadow to the right. |
| 10 | 11 giant ant | A | 0-31, 10-31 | Black ant side-on, head to the left, six long thin angled legs spanning the full tile width: #051818 with #313124 and navy #293952 highlights, two green eyes (#00e400/#108400). |
| 11 | 12 floating eye | E | 7-20, 9-27 | A 14-px eyeball hovering in the upper middle: mottled grey-pink ball (#837e6c, #b099a8), pale #9fcfac white of the eye and a blue iris #005ab5 with black pupil looking left; separate black ellipse shadow on the floor below. |
| 12 | 13 leprechaun | L | 6-28, 1-31 | Jaunty little man, front view: wide green hat (#00a000/#008000) with a tall blue feather, red beard and red curled shoes (#800000/#600000), green coat with yellow cuffs, skin #e5a282, holding a small gold pot at his hip. |
| 13 | 14 nymph | N | 6-30, 2-31 | Dancing young woman in three-quarter view: long blonde hair (#de9c0d/#b99018) streaming to the right, royal-blue dress (#0000ff/#0000c6) with gold hem bands, pale skin #eaf4b2, one arm extended left. |
| 14 | 15 quasit | Q | 0-29, 1-29 | Small blue imp hovering: blue body in four shades (#0000ff, #0000c6, #000084, #000063), grey horns and grey bat wings #837e6c spread to both edges, red eyes, thin barbed tail; black ellipse shadow below. |
| 15 | 16 rust monster | R | 0-31, 2-29 | Armadillo-like beast crouched facing left: plated body in dark reds (#400000, #600000, #800000, #a00000), two long grey antennae #909090 sweeping back, a long tail arching up to the top right ending in a grey propeller-shaped paddle. |

### 7.2 Row 1 (tiles 16-31)

| Tile | Monster id and name | Map char | Art box | Depiction |
|---|---|---|---|---|
| 16 | 17 zombie | Z | 1-29, 0-30 | Shambling corpse with both arms stretched to the left: green face #73b644 with black hair, torn gold-brown shirt #b99018 splashed with red #c72919, blue-grey trousers #788da7 with cyan patches, green hands, brown shoes. |
| 17 | 18 assassin bug | a | 2-31, 1-31 | Mantis-like black insect, body a long diagonal from upper left to lower right on stilt legs: #313124, #051818, navy #293952, a green eye, a few white glints on the wing case. |
| 18 | 19 bugbear | b | 5-31, 0-31 | Upright orange-brown bear in profile facing left (#603000, #804000, #a05000), snout open, yellow claws, khaki harness and loincloth (#606030, #a0a078), a spiked club raised behind its head at top right. |
| 19 | 20 hell hound | h | 1-31, 0-31 | Blood-red hound lunging toward the lower left, tail whipping up to the top right: #800000, #a00000, #c00000 with a glowing #ff0000 eye. |
| 20 | 21 ice lizard | i | 3-31, 0-31 | Pale blue-grey lizard, head at lower left with open jaws, white teeth #e0e0e0 and a red tongue #c03000, body and tail curving up to the top: #608080, #78a0a0, #90c0c0. |
| 21 | 22 centaur | C | 1-31, 0-31 | Bare-chested man's torso (skin #e5a282, black hair) on a chestnut horse body (#603000, #804000, #a05000) rearing to the left, holding a bow on the left, a green quiver on his back. |
| 22 | 23 troll | T | 0-31, 0-31 | Massive bare-chested brute filling the whole tile from edge to edge: tan muscles (#7f5a48, #a5765e, #ca8f72), wild black hair, scowling face with dark red mouth, green trousers (#006000/#008000). |
| 23 | 24 yeti | Y | 3-28, 2-29 | White ape standing front-on: fur #e8f7f7 with slate #788da7 shading down its right side and cyan #00b9c7 accents, two black eyes, red mouth #c72919; on its black shadow at lower right a red-orange flame-like shape (reads as a torch or bloody club). |
| 24 | 25 white dragon | d | 1-31, 0-31 | Dragon rearing to the left with wings spread to both top corners: body in whites and pale greys (#e8f7f7, #9fcfac, #b099a8, #837e6c), wing membranes slate #788da7, a blue eye. Same pose as the other four dragons. |
| 25 | 26 elf | e | 1-30, 0-30 | Elf archer, front view: brown hair, pink face, red-brown tunic #631800 with green collar and gold hem, a gold bow #b99018 on the right and a sword angled down to the lower right, brown boots. |
| 26 | 27 gelatinous cube | g | 6-31, 9-28 | A translucent block in three-quarter view: warm grey #837e6c, lilac #b099a8 and pale green #9fcfac faces around a darker #525252 core. |
| 27 | 28 metamorph | m | 1-30, 4-31 | Featureless gingerbread-man shape, arms out: off-white #e8f7f7 with pale green #9fcfac shading and a little slate #788da7; black shadow to the right. |
| 28 | 29 vortex | v | 3-29, 4-31 | Brown whirlwind funnel, wide at the top right and narrowing to the lower left: #631800 and #422100 bands with a gold edge #b99018. |
| 29 | 30 ziller | z | 0-28, 0-30 | Hunched black brute with long arms to the floor: #051818, #313124 and dark grey #525252, a red horned face #c72919, lilac claws #b099a8. |
| 30 | 31 violet fungus | F | 2-31, 1-30 | Purple toadstool with writhing tentacle roots: magenta-purple stem and tentacles (#840084, #63004a) with hot pink #f3007b highlights, a red-and-pink mottled cap. |
| 31 | 32 wraith | W | 1-27, 2-27 | Hooded ghost drawn entirely as a one-pixel checkerboard of greys (#909090, #a0a0a0, #b0b0b0) and floor, so it looks see-through; two solid red eyes. No shadow. |

### 7.3 Row 2 (tiles 32-47)

| Tile | Monster id and name | Map char | Art box | Depiction |
|---|---|---|---|---|
| 32 | 33 forvalaka | f | 1-30, 0-30 | Snarling leopard prowling to the left, tail curled over its back: orange-gold coat (#de9c0d, #b99018) with black spots, red mouth with white fangs, grey-lilac belly. |
| 33 | 34 lama nobe | l | 1-31, 2-31 | A giant snail: a long brown ribbed cone shell (#402000, #804000, #a05000) lying from the top left to the lower right, with a cream-khaki body (#a0a078, #c0c090) and two eye stalks tipped with red eyes at the lower right. |
| 34 | 35 osequip | o | 0-31, 3-30 | Low scaly quadruped facing left: grey-brown hide (#837e6c, #846331, #631800) with orange-gold spikes and legs #de9c0d, a long thin tail curling up into a hook at the top right. |
| 35 | 36 rothe | r | 1-31, 4-28 | Shaggy golden ox-like beast charging to the left: straw and gold fur (#846331, #b99018, #de9c0d) with dark #313124 hatching, white tusks. No black. |
| 36 | 37 xorn | X | 0-31, 0-31 | Squat rocky mound filling the tile: mottled charcoal (#202020, #303030, #404040, #505050), a wide red mouth with white teeth near the top, yellow eyes on the sides and crown, grey arms with claws reaching out to both sides. |
| 37 | 38 vampire | V | 1-29, 3-30 | Caped vampire, arms spread wide: pale green skin #9fcfac, black hair, red-brown cape (#631800/#422100) hanging like wings, navy trousers #293952. |
| 38 | 39 invisible stalker | (space) | 4-25, 3-31 | A human silhouette in near-black greys (#101010 to #404040) that breaks up into scattered pixels from the waist down. No shadow. Only ever drawn when the player can see invisible. |
| 39 | 40 poltergeist | p | 0-29, 4-30 | Wispy slate-blue spectre (#788da7, #636384): round head at upper left with red eyes and white fangs, thin tendrils trailing to the right ending in white claws. |
| 40 | 41 disenchantress | q | 1-31, 0-31 | Blonde sorceress in a long white gown (#e8f7f7 with pale cyan #94deed folds) spreading to both lower corners, holding a blue orb at her chest, pale blue sparkles around her head. |
| 41 | 42 shambling mound | s | 1-31, 2-31 | A haystack-shaped heap of vegetation: dithered greens (#006000, #008000) and golds (#604800, #a07800, #c09000) with orange-red blossoms. |
| 42 | 43 yellow mold | y | 2-26, 6-24 | Top-down oval patch of amber slime #de9c0d with a darker rim #b99018 and five dark brown holes #422100; black shadow below. |
| 43 | 44 umber hulk | U | 0-31, 0-31 | Hulking beetle-ape filling the tile: red-brown and olive plates (#631800, #7a7d24, #b99018), a red-capped head with a single staring eye and mandibles, pale claws #9fcfac. |
| 44 | 45 gnome king | k | 5-27, 9-29 | The gnome again, crowned: gold crown with blue gems, closed eyes, long white beard, royal-blue coat (#0000ff/#000084), gold belt and boots. Sits low in the tile. |
| 45 | 46 mimic | M | 0-30, 0-31 | A shapeless rainbow blob: horizontal bands from red at the top through orange, yellow, green and cyan to blue at the bottom, breaking into loose single pixels around the edge. No shadow. |
| 46 | 47 water lord | w | 4-30, 1-28 | A robed figure made of water: cone-shaped body in blue #005ab5 and cyan #00b9c7 with pale #9fcfac streaks, one arm held out to the right. |
| 47 | 48 bronze dragon | D | 0-30, 0-31 | The dragon pose (see tile 24) in bronze: body #631800, #846331, #b99018, #de9c0d with cream wing membranes on brown ribs. |

### 7.4 Row 3 (tiles 48-63)

| Tile | Monster id and name | Map char | Art box | Depiction |
|---|---|---|---|---|
| 48 | 49 green dragon | D | 0-30, 0-31 | The dragon pose in dark teal-green (#0c2431, #063146, #0c8c52) with a green striped belly and red-brown wings #631800. |
| 49 | 50 purple worm | P | 0-31, 0-31 | A huge magenta worm seen head-on, a near-circular body filling the tile (#400000, #600030, #800040, #a00050, #c00060): a round maw ringed with white teeth at the top, four black pores with pink rims. |
| 50 | 51 xvart | x | 1-31, 0-31 | Lumpy red-brown humanoid (#420000, #631800, #c72919), small head, long arms ending in gold claws #de9c0d at both lower corners. |
| 51 | 52 spirit naga | n | 1-31, 0-30 | Golden serpent (#b99018, #eedc37, #6f562c) coiled in a large S, with a woman's head at the top right: wild brown hair, red eyes, fangs. |
| 52 | 53 silver dragon | D | 0-30, 0-31 | The dragon pose in shimmering pale blue-white: #e8f7f7, #94deed, #788da7, #b099a8 heavily dithered. |
| 53 | 54 platinum dragon | D | 2-27, 0-31 | A different dragon: standing upright in profile facing left, slender, ice-white and pale teal (#608080, #78a0a0, #90c0c0, #a8e0e0, #b0b0b0), grey belly, purple tongue. |
| 54 | 55 green urchin | u | 0-31, 0-31 | A bright green ball filling the tile (#026c12, #039418, #04aa1d, #05c923, #07f82c) with four black dots ringed in light green. |
| 55 | 56 red dragon | D | 0-30, 0-31 | The dragon pose in red #c72919 and dark red #631800 with a gold belly #b99018 and orange-brown wings. |
| 56 | 61 type I demon lord and 62 type II demon lord | 1, 2 | 0-31, 0-31 | Hulking brown horned demon (#420000, #631800) filling the tile: gold horns, red mane, white eyes and fangs, lilac-grey wings behind, a sword held low at the left. |
| 57 | 63 type III demon lord | 3 | 0-31, 0-31 | Golden multi-limbed fiend (#604800, #806000, #a07800) with a white-horned dragon head, grey blade-like arms crossing the body, a red loincloth, a staff at the far left. |
| 58 | 64 type IV demon lord | 4 | 0-30, 0-31 | A woman in a red dress (#c00000/#ff0000) with white feathered wings and grey hair, a black shield bearing a white skull and crossbones on the left, a sword on the right. |
| 59 | 65 type V demon lord | 5 | 0-31, 0-31 | Blond bare-chested man with red eyes (skin #ca8f72, #e5a282, #ffb691), green bat wings edged in red in the top corners, and a green-and-gold serpent tail instead of legs. |
| 60 | 66 type VI demon lord | 6 | 2-30, 1-31 | Bulky armoured monster in mottled greys and navy (#837e6c, #525252, #313124, #293952) with white horns, dark red legs #631800, a cleaver at the left and a small flame at the right. |
| 61 | 67 type VII demon lord | 7 | 0-31, 0-31 | Classic bright red devil (#a00000, #c00000, #e00000, #ff0000) crouching, with orange-brown bat wings #a05000 and a yellow eye. |
| 62 | 68 demon prince | 9 | 0-31, 0-31 | Half-length figure in a royal-blue robe (#000080, #0000c0, #0000ff) with gold trim and a high red collar; pale lilac face, red eyes, white horns; a tall cyan flame in the hand at the left edge, a red sceptre at the right. |
| 63 | 69 God of Hellfire | 0 | 0-31, 0-31 | Same composition as tile 62 in red: red robe (#c00000, #e00000, #ff0000) with gold trim and blue lining, tan face with a black goatee and red swept hair, cyan flame at the left, a golden cross-topped staff at the right. |

Notes: the map characters come from the monster table; the invisible stalker's is a space and is
changed to "I" when a see-invisible potion is drunk (characters are not shown on the Android map).
The app's radio feature treats any displayed tile in the range 56-63 as "a demon is in view"
(Part 13), so demon tiles must keep those ids.

## 8. Feature, building and item tiles (ids 64-162)

Listed in tile-id order with the object(s) that use each tile. Object ids are the enumeration
order of Part 06 (also tabulated in section 9). All tiles are opaque on floor #426b6b unless a
tile is said to fill the whole square.

### 8.1 Row 4 (tiles 64-79)

| Tile | Used by (object id) | Art box | Depiction |
|---|---|---|---|
| 64 | altar (2) | 3-31, 2-31 | Red arched altar slab (#800000, #c00000) carved with a tall white cross, white pillars at its sides, standing on a stepped orange-gold plinth (#a05000, #ffc000). Shadow at lower right. |
| 65 | throne (3) | 1-29, 3-29 | Ornate chair, three-quarter view: gold frame (#c0a040, #f2c44d), red upholstery (#a00000, #e00000), a blue cross #0000ff on the back, curled gold arms and legs. |
| 66 | orb of enlightenment (59) | 5-26, 6-25 | Glossy blue sphere (#004080, #0050a0, #0060c0, #0070e0) with a white glint, resting on a crimson cushion #800040. |
| 67 | pit (6) | whole tile | Top-down square shaft drawn in one-point perspective: a 2-px black border, four trapezoid walls (top black, left floor-teal, right cyan #00b9c7, bottom slate #788da7) around a black-framed inner square of teal. 454 black pixels. |
| 68 | staircase up (7) | 5-23, 1-30 | A grey stone spiral stair winding upward, in dark warm greys (#231f1f, #2f2b2b, #575757, #787878). |
| 69 | elevator up (8) | 5-30, 4-31 | Grey lift doors #ababab in a black frame, a row of five small indicator lamps above (the second one red), and a green up-pointing triangle #afd373 outlined in white and black to the right. |
| 70 | fountain (9) | 7-28, 5-29 | A round grey stone basin with a spray of dark blue-violet water (#47334f, #473f6b, #4f4f8f, #474787) rising like a small tree. |
| 71 | statue (10) | 10-28, 0-31 | A dark grey-brown stone man (#433b3b, #4b4343, #534b4b, #6f6363) standing on a rough rock base. |
| 72 | teleport trap, visible (11) | 4-27, 2-29 | Round flame-coloured medallion seen from above: red rim #c72919, orange #de9c0d, yellow #eedc37, pale centre #eaf4b2, with a black eye-shaped symbol (a vertical lens with notches top and bottom). Black shadow at lower right. Same medallion as the trap tiles 129, 136, 138, different symbol. |
| 73 | College of Larn (75) | whole tile | Grey classical temple front: triangular pediment, four fluted columns, steps, a black doorway (#9a9a9a, #cbcbcb, #696969, white highlights, #111111 lines). |
| 74 | mirror (12) | 3-24, 2-28 | A tilted cheval mirror in a brown wooden stand (#603000, #a05000), the glass silver #c0c0c0 with white diagonal reflections. |
| 75 | DND store (76) | whole tile | Arch of grey stone blocks around a #111111 interior, the letters "DND" in white, a burning torch (brown handle, orange flame) below them. |
| 76 | staircase down (13) | 5-28, 5-29 | A pale green down arrow #afd373 outlined in white and black, above a shallow grey stone hole. |
| 77 | elevator down (14) | 5-30, 4-31 | Tile 69 with the triangle pointing down and placed lower. (Differs from 69 in only 29 pixels.) |
| 78 | Bank of Larn (78) | whole tile | Stone arch with closed grey double doors, a yellow "$" on the left door. |
| 79 | 8th branch of the bank (77) | whole tile | Tile 78 plus the yellow text "8th" across the bottom of the doors. |

### 8.2 Row 5 (tiles 80-95)

| Tile | Used by (object id) | Art box | Depiction |
|---|---|---|---|
| 80 | dead fountain (15) | 9-26, 19-29 | The empty stone basin alone, low in the tile, pinkish greys (#534b4b, #6f6363). |
| 81 | gold, small pile (19) | 9-27, 12-26 | A heap of gold nuggets: #6f562c, #b99018, #eedc37, #eaf4b2, with a black shadow at the right. Identical to tiles 133, 134, 135. |
| 82 | open door (16) | whole tile | Stone-block arch around an empty #111111 opening, a grey gradient threshold at the bottom. |
| 83 | closed door (17) | whole tile | The same arch filled by a brown plank door (#663300, #7b4800) with a four-pane gold-framed window, two iron strap hinges on the left, a handle on the right. |
| 84 | wall (18): sentinel only | whole tile | Grey brick wall filling the tile. Never drawn for a wall (step 6 of section 6 always replaces it); identical to tile 347 and to tile 367 except for 4 corner pixels. |
| 85 | Eye of Larn (23) | 3-30, 2-29 | A large pitted grey stone sphere (#525252, #737373, #a2a2a2, #b3b3b3) with a red eye #ff0000 and black slit pupil set into its left side. |
| 86 | plate mail (24) | 3-29, 2-29 | An empty suit of silver-white plate (torso with pauldrons and tassets): #525252, #837e6c, #b099a8, #e8f7f7. |
| 87 | chain mail (25) | 0-31, 0-30 | A sleeveless mail shirt with wide shoulder guards, drawn as a white, lilac and pale green checker, gold collar, brown belt with gold buckle. |
| 88 | leather armor (26) | 4-25, 5-26 | A dark red-brown leather cuirass (#2c1804, #422100, #631800). |
| 89 | sword of slashing (34) | 5-23, 0-31 | A curved scimitar blade (pale green-white #9fcfac) arcing down the right side, with white motion streaks to its left. |
| 90 | Bessman's flailing hammer (35) | 0-31, 0-30 | A massive two-headed maul: riveted blue-black heads (#051818, #313124, #293952), brown haft running to the top right corner. |
| 91 | sunsword (36) | 0-31, 0-31 | Broad sword on the diagonal, gold hilt at lower left: white-pink blade (#e8f7f7, #eeb1d7) wrapped in a yellow-green glow (#8faf10, #539d22, #37885b). |
| 92 | two-handed sword (37) | 0-30, 1-31 | Long plain blade on the diagonal with the point at lower left and a gold-and-brown hilt at upper right. |
| 93 | spear (38) | 0-31, 0-31 | A thin brown shaft corner to corner with a small grey head at upper right. |
| 94 | dagger (39) | 6-25, 6-25 | A small black-outlined dagger, orange hilt #ff9900 at upper right, grey blade pointing lower left. |
| 95 | ring of extra regeneration (46) | 8-22, 9-22 | A silver band with a white stone on top; black shadow below. |

### 8.3 Row 6 (tiles 96-111)

Rings 95-102 share one template: a band about 15 x 13 px seen from slightly above, centred,
with a 1-2 px black shadow along the lower right.

| Tile | Used by (object id) | Art box | Depiction |
|---|---|---|---|
| 96 | ring of regeneration (47) | 8-22, 9-22 | Silver band with a pale iridescent stone (#eaf4b2, #e8f7f7). |
| 97 | ring of protection (48) | 8-23, 10-22 | Plain dark steel band (#293952, #525252, #b099a8). |
| 98 | energy ring (49) | 8-23, 10-22 | Olive-gold band (#7a7d24, #b99018) with a pink top #f79f89 and red-brown inside. |
| 99 | ring of dexterity (50) | 8-23, 10-22 | Red-brown band #631800 with an orange-gold top #de9c0d. |
| 100 | ring of strength (51) | 10-26, 10-21 | A thin braided gold-and-brown hoop (#b99018, #631800), lighter than the others. |
| 101 | ring of cleverness (52) | 10-25, 9-21 | Silver band over navy (#293952, #0c2431). |
| 102 | ring of increase damage (53) | 8-23, 10-22 | Bright silver band (#837e6c, #e8f7f7, #9fcfac). |
| 103 | belt of striking (54) | 3-27, 5-28 | A black strap #313124 lying in an S-curve with a white square buckle at the lower left. |
| 104 | magic scroll (55) | 1-31, 0-30 | A parchment half unrolled: brown roll #631800 at the top left, sheet in white, pink and pale green (#e8f7f7, #eeb1d7, #9fcfac) covered in dark glyph marks. |
| 105 | magic potion (56) | 8-29, 3-28 | Round-bottomed glass flask with a brown cork, filled with yellow liquid (#de9c0d, #eedc37, #7a7d24); shadow to the right. |
| 106 | book (57) | 0-31, 1-30 | A closed tome lying at an angle: navy tooled cover (#214263, #063146), pink-white page edges, a gold clasp. |
| 107 | chest (58) | 0-31, 9-29 | Wooden treasure chest (#631800, #846331) bound in gold bands (#b99018, #eedc37) with a gold lock plate. |
| 108 | amulet of invisibility (60) | 8-25, 9-24 | A gold chain loop (#cc9900, #996600) with a dark-set orange stone hanging at the lower right. |
| 109 | orb of dragon slaying (61) | 1-31, 2-30 | A red crystal ball (#a00000, #e00000) on a silver stand, with a pair of gold-brown dragon wings spread above it. |
| 110 | scarab of negate spirit (62) | 4-26, 0-31 | A scarab beetle seen from above: blue and green wing cases, tan head and six tan legs (many small shades). |
| 111 | cube of undead control (63) | 1-30, 0-27 | A blue box (#004080, #0060c0, #0070e0) whose front shows a white skull inside a red "prohibited" circle and slash. |

### 8.4 Row 7 (tiles 112-127)

| Tile | Used by (object id) | Art box | Depiction |
|---|---|---|---|
| 112 | device of theft-prevention (64) | 8-23, 6-26 | A gold padlock (#846331, #b99018, #de9c0d) with a silver shackle and black keyhole. |
| 113 | diamond (71) | 5-23, 9-24 | A faceted marquise-cut gem tilted up to the right with a white glint at the top: greys #525252, #837e6c, #b099a8. Tiles 113-116 are the same drawing recoloured. |
| 114 | ruby (72) | 5-23, 9-24 | The gem in red (#631800, #c72919). |
| 115 | emerald (73) | 5-23, 9-24 | The gem in dark green (#0c2431, #063146, #0c8c52). |
| 116 | sapphire (74) | 5-23, 9-24 | The gem in blue (#000063, #000084, #0000a5, #0000ff). |
| 117 | dungeon entrance (79) | whole tile | The stone arch around a passage shaded from #434343 to #222222, as if descending into darkness. |
| 118 | volcanic shaft down (80) | 0-31, 3-31 | A grey rocky volcano cone with an orange-red glowing crater at the top, black outline. |
| 119 | base of volcanic shaft (81) | 1-30, 0-31 | A grey rock face with a cave mouth rimmed in red and orange lava #f69908 that trickles down. |
| 120 | battle axe (40) | 0-30, 1-31 | Double-bitted axe: navy blades #293952 edged in pink at the upper centre, brown haft to the lower left corner. |
| 121 | longsword (41) | 0-31, 0-31 | Slim sword corner to corner, point at upper right, gold crossguard, brown grip at lower left. |
| 122 | flail (42) | 2-30, 0-31 | A brown handle standing vertically at the left, a chain looping down and across to a spiked iron ball at the right. |
| 123 | ring mail (27) | 0-29, 1-31 | Short-sleeved mail shirt in a grey, slate and pale green ring pattern, gold jewelled collar, brown belt. |
| 124 | studded leather armor (28) | 2-23, 6-27 | The leather cuirass of tile 88 with rows of pale studs. |
| 125 | splint mail (29) | 6-31, 1-29 | A barrel-shaped cuirass of vertical splints (#837e6c, #b099a8) with broad lilac-blue shoulder plates. |
| 126 | plate armor (30) | 1-28, 3-28 | Winged pauldrons over an X-shaped breast-and-tasset assembly in grey with green and navy tints. |
| 127 | stainless plate armor (31) | 1-28, 3-28 | Tile 126 recoloured in clean neutral greys and white (#404040, #656565, #999999, #d2d2d2, #ffffff). |

### 8.5 Row 8 (tiles 128-143)

| Tile | Used by (object id) | Art box | Depiction |
|---|---|---|---|
| 128 | lance of death (43) | 2-31, 1-30 | A grey lance on the diagonal wrapped in a blue glow (#000063, #0e7dd8, #2b91b7). |
| 129 | arrow trap, visible (87) | 4-27, 2-29 | The flame medallion of tile 72 with a black upward arrow symbol (arrowhead at top, two white fletchings). |
| 130 | unused | 4-27, 2-29 | Exact duplicate of 129. |
| 131 | shield (32) | 3-28, 3-28 | Round wooden shield seen face-on: brown quarters (#663300, #996633), iron rim and cross bands #666666 with rivets, #110000 outline. |
| 132 | your home (82) | 1-26, 3-31 | A log cabin: yellow thatched roof, grey chimney with a wisp of smoke, plank door, stone footings and steps. |
| 133 | gold, x10 pile (20) | 9-27, 12-26 | Exact duplicate of 81. |
| 134 | gold, x1000 pile (21) | 9-27, 12-26 | Exact duplicate of 81. |
| 135 | gold, x10000 pile (22) | 9-27, 12-26 | Exact duplicate of 81. |
| 136 | dart trap, visible (89) | 4-27, 2-29 | The flame medallion with a black dart symbol (a narrow spike with white fins over a small triangle). |
| 137 | unused | 4-27, 2-29 | Exact duplicate of 136. |
| 138 | trapdoor, visible (90) | 4-27, 2-29 | The flame medallion with a black horizontal hatch symbol (a wide bar with a raised centre and a white notch below). |
| 139 | unused | 4-27, 2-29 | Exact duplicate of 138. |
| 140 | trading post (83) | whole tile | Stone arch, #111111 interior showing stacks of gold coins, two white exchange arrows and a sword. |
| 141 | unused | 4-27, 2-29 | Exact duplicate of 72. |
| 142 | throne that has summoned its gnome king (4) | 1-29, 3-29 | Exact duplicate of 65. |
| 143 | sphere of annihilation (93) | 7-23, 9-25 | A matt black ball (#111111, #323232, #3b3b3b) with one white glint. No shadow. |

The duplicates 130, 137, 139 and 141 sit right after the visible trap tiles and were evidently
reserved for the invisible versions; the game instead shows invisible traps as plain floor.

### 8.6 Rows 9-10 (tiles 144-162)

| Tile | Used by (object id) | Art box | Depiction |
|---|---|---|---|
| 144 | dead throne, jewels pried off (5) | 1-29, 3-29 | Exact duplicate of 65. All three throne states look the same. |
| 145 | Larn Revenue Service (84) | whole tile | Stone arch, #111111 interior, the letters "LRS" in white above stacks of gold coins. |
| 146 | fortune cookie (94) | 5-25, 7-24 | A golden-brown folded cookie (#b99018, #de9c0d) with a wedge-shaped opening at the right, thin dark outline. |
| 147 | golden urn (95) | 9-22, 9-28 | A small yellow-gold vase (#b99018, #e9c707, #eedc37). The object is not implemented in play but has a tile. |
| 148 | brass lamp (65) | 2-29, 14-27 | A golden genie lamp (#eedc37, #de9c0d) with pink highlights, spout pointing left. |
| 149 | Hand of Fear (66) | 7-22, 6-24 | A severed grey hand (#666666, #aaaaaa, #cccccc), fingers pointing up-left. |
| 150 | Talisman of the Sphere (67) | 7-24, 10-25 | The gold chain of tile 108 with a black ball pendant. |
| 151 | wand of wonder (68) | 10-23, 4-27 | A thin grey rod #474747 with a lilac sparkle tip (#9b67cf, #cfcfff) at the upper right. |
| 152 | staff of power (69) | 2-28, 2-28 | A red-brown staff from a red knob at the upper left down to the lower right, with tan bands. |
| 153 | Vorpal Blade (44) | 0-30, 0-31 | A heavy navy blade with a line of red dots, point at lower left, and a large spiked red-brown hilt with a gold centre at the upper right. |
| 154 | Slayer (45) | 1-30, 1-30 | A sword with its red grip at the lower right and grey blade pointing up-left, inside a jagged yellow starburst (#b99018, #eedc37, #eaf4b2). |
| 155 | elven chain (33) | 1-31, 0-30 | A short-sleeved mail shirt in an olive-grey and pale green checker (#837e6c, #9fcfac), gold collar and belt. |
| 156 | speed (96) | 5-28, 10-26 | A fat white tablet seen at an angle, embossed "S". |
| 157 | LSD (97) | 5-28, 10-26 | The same tablet embossed "LSD". |
| 158 | hashish (98) | 4-26, 3-27 | A seven-fingered dark green leaf #0c8c52 with a #111111 outline. |
| 159 | magic mushrooms (99) | 2-30, 4-27 | Three toadstools with bright green stems #00e400 and pink-speckled pale caps. |
| 160 | cocaine (100) | 5-27, 10-26 | A small heap of cream-white powder #fafbf3 with loose specks. |
| 161 | Dealer McDope's pad (85) | 0-30, 0-31 | The plank door of tile 83 in a wooden frame, under a black signboard reading "McDope's" in yellow. |
| 162 | amulet of life preservation (70) | 1-29, 4-31 | A heart-shaped gold chain holding a red-and-white lifebuoy ring at the bottom, with short yellow-green sparkle strokes around it. |

## 9. Object id to tile: the complete table

Object ids in enumeration order (Part 06). "Char" is the classic map symbol from the object
table (unused by the Android map). Tiles marked * are shared with another object.

| Id | Object | Char | Tile |
|---|---|---|---|
| 0 | Unknown (unexplored marker) | space | 175 |
| 1 | Nothing (empty floor) | space | 191 * |
| 2 | altar | A | 64 |
| 3 | throne | T | 65 * |
| 4 | throne (gnome king already summoned) | T | 142 * (same picture as 65) |
| 5 | dead throne | T | 144 * (same picture as 65) |
| 6 | pit | P | 67 |
| 7 | staircase up | % | 68 |
| 8 | elevator up | ~ | 69 |
| 9 | fountain | F | 70 |
| 10 | statue | & | 71 |
| 11 | teleport trap (visible) | ^ | 72 |
| 12 | mirror | = | 74 |
| 13 | staircase down | % | 76 |
| 14 | elevator down | ~ | 77 |
| 15 | dead fountain | f | 80 |
| 16 | open door | O | 82 |
| 17 | closed door | D | 83 |
| 18 | wall | # | 84, always replaced by 352 + mask |
| 19 | gold (amount = argument) | * | 81 * |
| 20 | gold (argument x 10) | * | 133 * (same picture as 81) |
| 21 | gold (argument x 1000) | * | 134 * (same picture as 81) |
| 22 | gold (argument x 10000) | * | 135 * (same picture as 81) |
| 23 | Eye of Larn | o | 85 |
| 24 | plate mail | [ | 86 |
| 25 | chain mail | [ | 87 |
| 26 | leather armor | [ | 88 |
| 27 | ring mail | [ | 123 |
| 28 | studded leather armor | [ | 124 |
| 29 | splint mail | [ | 125 |
| 30 | plate armor | [ | 126 |
| 31 | stainless plate armor | [ | 127 |
| 32 | shield | [ | 131 |
| 33 | elven chain | [ | 155 |
| 34 | sword of slashing | ( | 89 |
| 35 | Bessman's flailing hammer | ( | 90 |
| 36 | sunsword | ( | 91 |
| 37 | two-handed sword | ( | 92 |
| 38 | spear | ( | 93 |
| 39 | dagger | ( | 94 |
| 40 | battle axe | ( | 120 |
| 41 | longsword | ( | 121 |
| 42 | flail | ( | 122 |
| 43 | lance of death | ( | 128 |
| 44 | Vorpal Blade | ( | 153 |
| 45 | Slayer | ( | 154 |
| 46 | ring of extra regeneration | = | 95 |
| 47 | ring of regeneration | = | 96 |
| 48 | ring of protection | = | 97 |
| 49 | energy ring | = | 98 |
| 50 | ring of dexterity | = | 99 |
| 51 | ring of strength | = | 100 |
| 52 | ring of cleverness | = | 101 |
| 53 | ring of increase damage | = | 102 |
| 54 | belt of striking | ) | 103 |
| 55 | magic scroll (all kinds) | ? | 104 |
| 56 | magic potion (all kinds) | ! | 105 |
| 57 | book | B | 106 |
| 58 | chest | C | 107 |
| 59 | orb of enlightenment | o | 66 |
| 60 | amulet of invisibility | " | 108 |
| 61 | orb of dragon slaying | o | 109 |
| 62 | scarab of negate spirit | . | 110 |
| 63 | cube of undead control | . | 111 |
| 64 | device of theft-prevention | . | 112 |
| 65 | brass lamp | . | 148 |
| 66 | Hand of Fear | . | 149 |
| 67 | Talisman of the Sphere | . | 150 |
| 68 | wand of wonder | / | 151 |
| 69 | staff of power | / | 152 |
| 70 | amulet of life preservation | " | 162 |
| 71 | diamond | < | 113 |
| 72 | ruby | < | 114 |
| 73 | emerald | < | 115 |
| 74 | sapphire | < | 116 |
| 75 | College of Larn | M | 73 |
| 76 | DND store | S | 75 |
| 77 | 8th branch of the Bank of Larn | $ | 79 |
| 78 | Bank of Larn | $ | 78 |
| 79 | dungeon entrance | E | 117 |
| 80 | volcanic shaft down | V | 118 |
| 81 | base of volcanic shaft | V | 119 |
| 82 | your home | H | 132 |
| 83 | trading post | T | 140 |
| 84 | Larn Revenue Service | L | 145 |
| 85 | Dealer McDope's pad | P | 161 |
| 86 | arrow trap, invisible | space | 191 * (plain floor) |
| 87 | arrow trap, visible | ^ | 129 |
| 88 | dart trap, invisible | space | 191 * (plain floor) |
| 89 | dart trap, visible | ^ | 136 |
| 90 | trapdoor, visible | ^ | 138 |
| 91 | trapdoor, invisible | space | 191 * (plain floor) |
| 92 | teleport trap, invisible | space | 191 * (plain floor) |
| 93 | sphere of annihilation | s | 143 |
| 94 | fortune cookie | c | 146 |
| 95 | golden urn | u | 147 |
| 96 | speed | : | 156 |
| 97 | LSD | : | 157 |
| 98 | hashish | : | 158 |
| 99 | magic mushrooms | : | 159 |
| 100 | cocaine | : | 160 |
| 101-116 | Wild Edition objects, in order: vending machine, can of Fizz, slice of pizza, energy drink, rubber duck, traffic cone, propeller beanie, wizard hat, party hat, tinfoil hat, egg, fishing rod, fish, old boot, bottle, cassette | (Part 15) | 368-383 in the same order |
| 117 | Wild Edition LarnCoin ATM | (Part 15) | 388 |

Sharing summary: tile 191 serves empty floor and all four invisible trap kinds (so an undetected
trap is indistinguishable from floor); the three throne states use three tile ids with identical
pictures; the four gold denominations use four tile ids with identical pictures; all scrolls share
one tile, all potions share one tile, all books share one tile. An item's enchantment or identity
never changes its tile.

## 10. Player tiles (165-172 and 181-188)

The player tile is chosen by character class number (0-7, the order of the class menu) and
gender flag (0 = female, 1 = male; male is the default): tile = 165 + class for a female
character, 181 + class for a male character.

| Class | Name | Female tile | Male tile |
|---|---|---|---|
| 0 | Ogre | 165 | 181 |
| 1 | Wizard | 166 | 182 |
| 2 | Klingon | 167 | 183 |
| 3 | Elf | 168 | 184 |
| 4 | Rogue | 169 | 185 |
| 5 | Adventurer | 170 | 186 |
| 6 | Dwarf | 171 | 187 |
| 7 | Rambo | 172 | 188 |

All are opaque full-figure portraits on floor, front view, mostly with a black cast shadow, drawn
in the brighter family B palette with many small shades (17-34 colours each).

| Tile | Art box | Depiction |
|---|---|---|
| 165 Ogre, female | 8-23, 1-31 | A tall cave-woman wearing a white horned animal skull as a headdress (horns yellow), dark hair, a yellow leopard-spotted fur dress (#eedc37, #de9c0d with black spots), brown boots, a small cyan-bladed knife in her lowered hand. Skin #cb833b, #eba377. |
| 181 Ogre, male | 6-27, 2-31 | A burly bare-chested man with the same horned skull helmet, arms folded across his chest, a yellow spotted fur loincloth with hanging tails. Skin #f79f89, #d98e68. |
| 166 Wizard, female | 5-27, 0-31 | A black-haired sorceress in a long royal-blue gown (#0000a0, #0000c0, #0000ff) with a gold belt and a red cloak #a00000 over one shoulder, a puff of grey-white sparkles leaving her outstretched hand at the right. |
| 182 Wizard, male | 1-30, 0-31 | A grey-robed wizard (#909090, #a0a0a0, #b0b0b0) with a wide-brimmed pointed grey hat with a red band, a long white beard, a red-brown staff held across his body, and a blue cloak lining dotted with gold stars at the right. |
| 167 Klingon, female | 1-31, 1-31 | A warrior woman with long tawny hair and a black visor across her eyes, a short gold tunic (#b99018, #de9c0d) with a white hem band, a red shield on her arm at the right, a pale-bladed short sword held low at the left, brown boots. |
| 183 Klingon, male | 3-30, 1-31 | A bald, grey-bearded old warrior kneeling on one knee: gold cloak (#403000, #806000) down his back, bare arms, a grey buckle at his chest, a red-brown weapon across his lap, a dark red-brown round shield at the right. |
| 168 Elf, female | 1-30, 1-31 | A blonde elf in a bright green tunic #00c000 with yellow trim and a brown belt, a gold longbow held at the left, a sword hanging down at the right. |
| 184 Elf, male | 1-29, 0-31 | An elf lord: gold crown, long pale pink-white hair, green shoulder guards, pale mail, a dark teal cloak (#0c2431, #063146), a blue shield with a green leaf emblem, a gold-hilted sword held point down at the left. |
| 169 Rogue, female | 7-28, 0-31 | A brown-haired woman in a short brown leather dress (#804000, #a05000), one arm raised overhead, the other holding a curved dagger out to the left, gold bracers, caught mid-stride. |
| 185 Rogue, male | 5-31, 0-31 | A sneaking thief in a golden-brown hood and clothes (#a05000, #a07800), a white-cyan dagger in the hand at the left, a bulging white loot sack on his back. |
| 170 Adventurer, female | 3-29, 0-31 | A valkyrie: white winged helmet with a red crest, long blonde hair, white dress with a red-patterned hem, a blue shield with gold edging at the left, a long sword point-down at the right. |
| 186 Adventurer, male | 3-29, 0-29 | A knight in grey plate with gold edging (#525252, #837e6c, #b099a8, #eedc37), a winged helm with a gold crest, a blue shield with a gold cross at the left, a sword angled down at the right. |
| 171 and 187 Dwarf | 0-31, 0-31 | **The two tiles are pixel-identical**; the Dwarf looks the same for both genders. A red-bearded dwarf in a horned iron helmet, blue armour with gold trim (#0000c0, #0000ff, #c0a040), a red cape, a double-bladed axe raised at the top left, a brown-and-gold round shield at the right. Fills the tile. |
| 172 Rambo, female | 9-24, 1-30 | A dark-haired muscular woman with a grey headband, brown leather two-piece and boots, a cyan-bladed knife in her lowered hand. |
| 188 Rambo, male | 8-24, 1-31 | A bare-chested muscleman with a red headband and dark hair, brown loincloth and boots, gold wristbands, a white-bladed knife held low. Skin #cb833b, #eba377, #fbcbb3. |

Wild Edition hook: a hat overlay tile may be drawn over the player tile (Part 15).

## 11. Cursor, unknown, floor and blank tiles

| Tile | Name | Description |
|---|---|---|
| 174 | Cursor (overlay) | Transparent except for four pure red #ff0000 corner brackets. Each bracket is an L made of a 6 x 2 bar along the tile's outer edge and a 2 x 6 bar along the adjacent outer edge, meeting in the corner (20 pixels per corner, 80 in total); the middle 20 pixels of every edge and the whole interior are transparent. Drawn on top of the player's cell whenever the player is not blind, as the last layer of that cell. |
| 175 | Unknown | Solid black #000000. Shown for every cell the player has not yet seen. The map area's own background is also black, so unexplored space and off-map space look the same. |
| 190 | (unused) | The cursor brackets on solid black; mask partner of 174. |
| 191 | Floor / nothing | Solid #426b6b. Used for empty floor, for the four invisible trap objects, and as the "direction 0" entry of every bolt effect. |
| 163, 164, 173, 176-180, 189, 232-239, 270, 271, 286, 287, 302, 303, 318, 319, 334, 335, 350, 351 | (unused) | 29 spare tiles, each solid #426b6b, identical to 191. |

## 12. Directional bolt effects (tiles 192-231)

Five projectile effects each have eight tiles, one per compass direction, laid out clockwise
starting from west: base + 0 = west, + 1 = north-west, + 2 = north, + 3 = north-east, + 4 = east,
+ 5 = south-east, + 6 = south, + 7 = south-west. "Direction" is the direction of travel; the head
of the bolt leads and the tail trails behind.

The game's direction numbers are 0 = none, 1 = south, 2 = east, 3 = north, 4 = west,
5 = north-east, 6 = north-west, 7 = south-east, 8 = south-west (y grows downward). The lookup
table, per effect and game direction:

| Effect (index) | Used by (Part 07) | dir 0 | 1 S | 2 E | 3 N | 4 W | 5 NE | 6 NW | 7 SE | 8 SW |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 Magic missile | magic missile spell | 191 | 198 | 196 | 194 | 192 | 195 | 193 | 197 | 199 |
| 1 Sonic spear | sonic spear spell | 191 | 206 | 204 | 202 | 200 | 203 | 201 | 205 | 207 |
| 2 Fireball | fireball spell | 191 | 214 | 212 | 210 | 208 | 211 | 209 | 213 | 215 |
| 3 Lightning | lightning spell; scroll of pulverization | 191 | 222 | 220 | 218 | 216 | 219 | 217 | 221 | 223 |
| 4 Cone of cold | cone of cold spell | 191 | 230 | 228 | 226 | 224 | 227 | 225 | 229 | 231 |

How they are shown: as a bolt travels, for each cell it enters (and only if the player is not
blind and the cell is on the map) the cell's tile is **replaced** by the effect tile and its
overlay cleared; the game pauses for the spell's per-step delay and then redraws the cell
normally. Because each effect tile has the floor painted in, a bolt passing over a wall, item or
monster momentarily hides it behind floor colour.

What each looks like (all opaque on #426b6b, no shadow). The four orthogonal tiles of an effect
are exact 90-degree rotations of one drawing; the four diagonal tiles are 45-degree rotations.

| Effect | Tiles | Size of the west-facing drawing | Depiction and colours |
|---|---|---|---|
| Magic missile | 192-199 | 16 x 9, centred | A bright green crescent (opening backward, like an open parenthesis for the west tile) with a white core, followed by a green teardrop with a white centre tapering to a 1-px tail. #00ff00 (37 px), #ffffff (17), #00c600 (8). Diagonals are smooth rotations with soft blended edges. |
| Sonic spear | 200-207 | 23 x 13, centred | A wide cyan bow-wave crescent with a white leading core, a smaller crescent and a small bracket behind it, then a spear shaft drawn as a cyan checker that narrows to a line. #00ffff (45 px), #01a9a9 (24), #ffffff (10). Diagonals are smooth rotations. |
| Fireball | 208-215 | 25 x 11, centred | A round 11-px ball of red #ff0000 with a dithered orange #ff8000 and yellow #ffff00 core, trailing a pointed dark red #9d0000 tail flecked with orange and yellow. Diagonals are smooth rotations. |
| Lightning | 216-223 | 26 x 13, centred | A jagged black #111111 spindle with short spikes all round, containing a zig-zag bolt in light greys (#aaaaaa, #bbbbbb, #cccccc, #dddddd, #eeeeee). It has no obvious head or tail, so opposite directions look almost the same. Diagonals are hand-drawn with crisp pixels (7 colours). |
| Cone of cold | 224-231 | 29 x 15, centred | A white #ffffff bullet shape with a rounded nose, a 1-px light grey #c0c0c0 rim and a few grey #808080 speckles, whose rear half breaks up into horizontal streaks and loose pixels. Diagonals are smooth rotations. |

## 13. Sparkle overlay (tiles 240-247) and its leftovers (248-255)

The sparkle is the only base effect drawn as an overlay: the cell keeps its normal tile and the
sparkle frame is alpha-blended on top. It has 8 frames, tiles 240, 241, 242, 243, 244, 245, 246,
247 in order. It is played on the player's own cell for self-affecting spells (protection,
dexterity, charm monster, strength, healing, cure blindness, invisibility, haste self, globe of
invulnerability, scare monster, walk through walls, permanence) and on the target monster's cell
for polymorph (Part 07). Each frame is held for 75 ms.

Appearance: a handful of small four-pointed stars scattered irregularly over the tile on a
transparent background. Each star is a plus shape with a white #ffffff centre pixel, an inner
ring in a light tint and arm tips in a darker tint, in one of three colour families:
pink (#ffa8a8, #ff9b9b with #ff0080 tips), cyan-blue (#80ffff, #00ffff with #0080ff tips) and
green (#00ff80, #00ff00 with #009d00 tips). Star sizes run from a single pixel through 3 x 3
pluses to 13 x 13 stars with 1-px arms.

| Frame | Tile | Opaque pixels | Content |
|---|---|---|---|
| 0 | 240 | 18 | Three tiny 3 x 3 pluses (pink upper right, green centre-left, cyan lower right) and three single dots. |
| 1 | 241 | 42 | Six small stars, 3-5 px across. |
| 2 | 242 | 80 | Six stars 5-7 px across plus two dots. |
| 3 | 243 | 171 | Eight stars, the largest 13 px (pink upper right, green centre, cyan lower right), touching the tile edges. |
| 4 | 244 | 179 | Peak: nine stars in new positions, four of them 13 px. |
| 5 | 245 | 100 | Seven stars 5-7 px. |
| 6 | 246 | 33 | Five small stars 3-5 px. |
| 7 | 247 | 10 | Two 3 x 3 pluses (cyan upper centre, green lower left). |

The stars move between frames (they do not simply grow in place), giving a twinkling effect.
Tiles 248-255 hold the same eight drawings on opaque black and are never drawn.

## 14. Spell animations (tiles 256-349)

Fourteen effects, each with 6 frames, stored one effect per sheet column: for effect column c
(0-13) and frame f (0-5) the tile is 256 + c + 16 x f. Together with the sparkle they form the
magic-effect table, indexed by the effect numbers below.

| Effect number | Name | Frames | Mode | Tile ids, frame 0 to last |
|---|---|---|---|---|
| 0 | Sparkle | 8 | overlay on the cell's normal tile | 240, 241, 242, 243, 244, 245, 246, 247 |
| 1 | Sleep | 6 | replace | 256, 272, 288, 304, 320, 336 |
| 2 | Web | 6 | replace | 257, 273, 289, 305, 321, 337 |
| 3 | Phantasmal forces | 6 | replace | 258, 274, 290, 306, 322, 338 |
| 4 | Cloud kill | 6 | replace | 259, 275, 291, 307, 323, 339 |
| 5 | Vaporize rock | 6 | replace | 260, 276, 292, 308, 324, 340 |
| 6 | Dehydrate | 6 | replace | 261, 277, 293, 309, 325, 341 |
| 7 | Drain life | 6 | replace | 262, 278, 294, 310, 326, 342 |
| 8 | Flood | 6 | replace | 263, 279, 295, 311, 327, 343 |
| 9 | Finger of death | 6 | replace | 264, 280, 296, 312, 328, 344 |
| 10 | Teleport away | 6 | replace | 265, 281, 297, 313, 329, 345 |
| 11 | Magic fire | 6 | replace | 266, 282, 298, 314, 330, 346 |
| 12 | Make wall | 6 | replace | 267, 283, 299, 315, 331, 347 |
| 13 | Summon demon | 6 | replace | 268, 284, 300, 316, 332, 348 |
| 14 | Annihilate | 6 | replace | 269, 285, 301, 317, 333, 349 |

Display rules:

1. A frame request is ignored if the frame number is greater than the effect's frame count, or
   outside 0-7, or the player is blind, or the cell is off the map.
2. **Overlay mode** (sparkle only): the cell's tile is recomputed normally (section 6) and the
   frame's tile id is written into the cell's overlay slot.
3. **Replace mode** (all others): the frame's tile id is written as the cell's tile and the
   overlay slot is cleared, so the monster, item or wall in that cell is hidden for the duration
   and the animation appears on bare floor.
4. The game core plays frames 0 to count-1 with a 75 ms pause after each, on one cell (single
   target spells, on the target), or on all eight cells around the player at once (cloud kill,
   flood, magic fire, annihilate, each via the same frame loop), or on the qualifying cells of the
   3 x 3 block around the player (vaporize rock: walls, statues, thrones, altars, xorns and
   trolls). Afterwards the cells are redrawn normally. Which spell uses which effect: sleep,
   web, phantasmal forces, cloud kill, vaporize rock, dehydration, drain life, flood, finger of
   death, teleport away, magic fire, make wall, summon demon and annihilation respectively
   (details in Part 07).

Frame-by-frame appearance (everything opaque on #426b6b; none has a cast shadow):

| Effect | f0 | f1 | f2 | f3 | f4 | f5 |
|---|---|---|---|---|---|---|
| Sleep (white #ffffff letters "Z" in 2-px strokes, drifting up and to the right) | 256: one 7-px Z in the bottom-left corner | 272: one 8-px Z a little higher and further right | 288: two Zs, a small one bottom-left and a 9-px one mid-left | 304: two Zs, small at mid-left and a 10-px one upper-centre | 320: three Zs, small bottom-left, medium centre-left, a 13-px one cut off by the top-right corner | 336: two Zs, the medium one and the large one at the top right; the smallest is gone |
| Web (1-px white lines) | 257: a 7 x 5 grey-white silk blob (#808080, #c0c0c0, #ffffff) at the centre | 273: a 16-px spider web, eight spokes and one ring | 289: a 23-px web with two rings | 305: a full-tile web: spokes to all edges with three concentric hexagonal rings | 321: the same web with the innermost ring gone | 337: only the two outer rings and spoke stubs remain; the centre is empty |
| Phantasmal forces (a black-cloaked, hooded spectre rising; cloak #232323, #2b2727 with brown tints, hood rimmed in grey-lilac, lower edge fading out through a checker dither) | 258: a small 12 x 22 figure at the centre with a red eye slit | 274: larger, 19 px wide | 290: larger again, shoulders 24 px wide | 306: a bust filling the tile, thin red eye line | 322: the same with larger glowing red eyes | 338: the same with a broad red visor of eyes with yellow centres (the tile differs from 322 only around the eyes) |
| Cloud kill (the magic fire drawings recoloured green: dark green #031b03, #114b15 rims, lime #95bf15, #96d117, #8ee719, pale yellow cores #f0e074, #f1efbe) | 259: a 6-px lime spark | 275: an 18 x 15 green puff with a dark rim | 291: a 23-px cloud with a bright lime centre | 307: a billowing cloud filling the tile with a cream core and lime fringe | 323: a ring of loose pale-yellow and lime pixels with a clear centre | 339: sparse scattered lime and yellow pixels near the edges |
| Vaporize rock | 260: a grey boulder filling the tile, veined with glowing white-orange cracks and a scorched brown rim | 276: an orange-tan dust burst (#a77327, #c38b2f, #c79737) with a pale grey rock stump at lower left | 292: a darker brown and maroon debris cloud (#572b07, #5b1300, #6f3707) with a grey stump | 308: thinning brown specks around a small grey remnant | 324: a few brown specks and a thin grey sliver | 340: a faint vertical brown-grey wisp and half a dozen specks |
| Dehydrate (steam in four greys #707070, #909090, #b0b0b0, #d0d0d0) | 261: a small low puff, 14 x 11 | 277: a lumpy cloud 17 px across | 293: a mushroom cloud with a tapering stem, 20 x 24 | 309: the cap risen and flattened, stem thinner | 325: a few detached puffs higher up | 341: four tiny puffs, 42 pixels in all |
| Drain life (blood drops: #800000, #c90b01, #ff0000 with yellow #ffff00 tips; only 37-50 pixels per frame) | 262: a little red "tree": a dark stem with a spray of yellow-tipped drops above it | 278: the spray opening into a V, no stem | 294: drops curling outward left and right | 310: two loose groups of drops beginning to fall | 326: short vertical streaks falling, yellow at their lower ends | 342: the streaks near the bottom edge |
| Flood (an expanding water ring in four blues #0050a0, #0060c0, #0070e0, #0080ff with light grey #c0c0c0 glints) | 263: a 6-px blue blob | 279: a 12-px faceted disc | 295: an 18-px disc with a 3-px hole | 311: a 24-px ring, hole 8 px | 327: a 30-px ring, hole 14 px | 343: a ring touching the tile edges, 4-5 px thick |
| Finger of death (a skull) | 264: a dull dark-brown skull with a short spine (#2f1b07, #472b0b, #5f3b0f), 15 x 22 | 280: the skull lit to tan and gold (#8f5f1f, #c79737) | 296: bright gold with red eye sockets | 312: the skull swallowed by a cream-white glow (#f3f3f3, #fbf3cf, #f7dfbb) with orange-red eyes and a ragged brown fringe | 328: a white-cream blob with a grey centre, same outline | 344: the blob hollowed into a ring with floor showing through the middle |
| Teleport away (a shimmering blue curtain in vertical stripes of the four flood blues, with a white-cyan flare) | 265: a 7 x 18 blue bar with a pale vertical glint | 281: a 13 x 22 panel with a lens-shaped flare | 297: a 23 x 28 panel with a four-pointed white star | 313: stripes covering almost the whole tile behind a large four-pointed star (only 102 floor pixels left) | 329: the curtain parting down the middle, star smaller | 345: two blue bands at the left and right edges and a small vertical flare in the bare centre |
| Magic fire (dark red #4f0000, #83170b rims, orange #ff7b07, #ff9b1b, #ffb733, yellow-white cores #ffe35f, #fffb97, #ffffff) | 266: a 6-px orange spark | 282: an 18 x 15 fire burst | 298: a 23-px fireball, yellow at the centre | 314: a white-yellow explosion filling the tile, orange fringe | 330: a ring of loose pale-yellow and white pixels | 346: sparse scattered orange-yellow pixels |
| Make wall (the grey brick wall of section 15 rising from the bottom of the tile, floor above it) | 267: 5 pixel rows of brick (y 27-31) | 283: 9 rows (y 23-31) | 299: 13 rows (y 19-31) | 315: 21 rows (y 11-31) | 331: 29 rows (y 3-31) | 347: the complete wall, identical to tile 84 |
| Summon demon (a bright red devil, #b70000, #df0000, #ff0000, white horns, black eyes, climbing out of a black hole in the floor) | 268: just the white horn tips and the top of the red head over a black slit near the bottom centre | 284: head and horns above a black oval | 300: head, shoulders and chest; the tip of a weapon at the right | 316: down to the waist, holding a brown-shafted scythe with a white blade at the right, the hole spreading behind | 332: down to the knees | 348: standing full height (20 x 31) on white clawed feet, scythe across the body; the hole is gone |
| Annihilate (a spinning pinwheel: yellow #eedc37 and pale yellow #eaf4b2 arms with olive-lime #8faf10 and green #539d22, #37885b toward the centre) | 269: a tight 15-px pale yellow whorl | 285: 19 px, six to eight curved arms, greener | 301: 24 px, arms separating | 317: 27 px | 333: 31 px, eight detached curved arms around a green swirl | 349: arms at the tile edges, the centre fading to thin dark-green traces |

## 15. Wall tiles (352-367)

A wall cell never shows tile 84; it shows 352 + mask, where mask (0-15) is stored in the wall
cell's argument value and is the sum of:

| Bit value | Set when the neighbouring cell in this direction is a wall, an open door or a closed door |
|---|---|
| 1 | west (x - 1) |
| 2 | south (y + 1) |
| 4 | east (x + 1) |
| 8 | north (y - 1) |

A neighbour that would be off the map counts as "not connected". Diagonal neighbours are ignored.
The mask is recomputed by a routine that scans a rectangle of cells and rewrites the argument of
every wall in it: over the whole map when a level is created or loaded into play, and over the
cells around a change when walls are made, vaporised, annihilated or eaten by a sphere of
annihilation (Parts 05 and 07). A newly made wall keeps whatever mask its argument last held until
that rescan, which follows immediately.

**The common drawing.** A grey brick wall filling the tile: brick courses 3 pixels tall separated
by 1-px #111111 mortar lines every 4 pixels (8 courses per tile, mortar on rows 0, 4, 8, ... 28),
bricks 3 to 7 px long with staggered vertical joints. Each brick is shaded light on top (#e2e2e2,
#d3d3d3, #c4c4c4) to darker below (#b3b3b3, #a2a2a2, #939393, #838383), and the wall darkens
slightly toward the right edge (#737373, #5c5c5c). Tile 367 (mask 15) is this drawing with no
border, so adjoining walls join seamlessly.

**The border.** On every side where the wall has **no** connecting neighbour the tile gets a
4-pixel kerb of flat dressed stones: a 1-px dark line on the tile edge, then 3 x 3 px squares of
flat #a2a2a2 separated by 1-px black lines (a row of eight squares per side), then a 1-px black
line, with the brickwork inset inside it. Where two bordered sides meet, the corner is cut off at
45 degrees: a small triangle of floor colour #426b6b (4, 3, 2 and 1 pixels on successive rows, 10
pixels) shows at the tile's corner, with a black diagonal line behind it, so free-standing wall
ends look rounded. Sides that do connect have no kerb; the bricks run to the edge.

| Tile | Mask | Connected neighbours | Kerb on sides | Cut corners |
|---|---|---|---|---|
| 352 | 0 | none (isolated pillar) | top, right, bottom, left | all four |
| 353 | 1 | W | top, right, bottom | top-right, bottom-right |
| 354 | 2 | S | top, left, right | top-left, top-right |
| 355 | 3 | W, S | top, right | top-right |
| 356 | 4 | E | top, left, bottom | top-left, bottom-left |
| 357 | 5 | W, E (horizontal run) | top, bottom | none |
| 358 | 6 | S, E | top, left | top-left |
| 359 | 7 | W, S, E | top | none |
| 360 | 8 | N | left, right, bottom | bottom-left, bottom-right |
| 361 | 9 | N, W | right, bottom | bottom-right |
| 362 | 10 | N, S (vertical run) | left, right | none |
| 363 | 11 | N, S, W | right | none |
| 364 | 12 | N, E | left, bottom | bottom-left |
| 365 | 13 | N, E, W | bottom | none |
| 366 | 14 | N, S, E | left | none |
| 367 | 15 | all four | none (solid brick) | none |

Colour counts for reference: the wall tiles use 11-12 colours each; #111111 is the most common
(236-370 px), then #a2a2a2, #b3b3b3, #939393, #838383, #e2e2e2. Tiles with kerbs also contain pure
black (42-213 px). Only tiles 352, 353, 354, 355, 356, 358, 360, 361 and 364 contain any floor
colour (10 px per cut corner).

Doors count as wall connections for their neighbours, but the door tiles themselves (82, 83) are
single fixed drawings of a free-standing stone arch and do not vary with their surroundings.

## 16. Complete list of tiles the base game never draws

| Tiles | What they contain |
|---|---|
| 84 | The plain brick wall. Its id is used only as the "this is a wall" marker (section 6 step 6); the same picture is seen as tile 347, the last make-wall frame. |
| 130, 137, 139, 141 | Duplicates of the arrow trap, dart trap, trapdoor and teleport trap medallions. |
| 163, 164, 173, 176-180, 189, 232-239, 270, 271, 286, 287, 302, 303, 318, 319, 334, 335, 350, 351 | Blank floor. |
| 190 | Cursor on black. |
| 248-255 | Sparkle frames on black. |

Tile 147 (golden urn) and tile 38 (invisible stalker) are reachable only in unusual circumstances
(the urn object is not generated in normal play; the stalker needs see-invisible), but they are
wired up.

## 17. What a renderer needs to know

1. **Slicing.** Cut the sheet into one 32 x 32 bitmap per tile at load time (number of tiles =
   sheet width / 32 x sheet height / 32, read without any density scaling). Drawing from separate
   bitmaps rather than sub-rectangles of the sheet avoids neighbouring tiles bleeding in at the
   edges when scaled. Requests for an id beyond the sheet draw nothing.
2. **Scaling.** Tiles are stretched to the on-screen cell size with nearest-neighbour sampling (no
   bitmap filtering, no dithering). Cell edges are rounded to whole pixels so that each cell ends
   exactly where the next begins. The on-screen cell size and camera are Part 11.
3. **Background.** The map rectangle is filled with black before tiles are drawn; black is also
   the Unknown tile, so the unexplored area needs no special handling.
4. **Layer order within one cell.** (a) the cell's tile; (b) on the player's cell, when the player
   is not blind, the Wild Edition hat overlay if any; (c) the cell's overlay tile if it is not
   "none" (the sparkle, or Wild Edition overlays); (d) Wild Edition pet marker; (e) Wild Edition
   glitch overlay; (f) on the player's cell, when the player is not blind, the cursor tile 174.
   Weather and day/night tint (Wild Edition) are drawn over the whole map afterwards.
5. **Floor versus empty.** There is no "empty" tile with transparency: explored empty floor is
   tile 191 (flat teal) and unexplored is tile 175 (black). The floor has no texture, so large
   rooms are flat teal fields and all contrast comes from walls, shadows and sprites.
6. **Everything carries its own floor.** Because the floor is baked into the tiles, a monster
   standing on an item shows only the monster on bare floor, and a monster in a doorway or on
   stairs hides them; replace-mode effects likewise blank the cell to floor behind the animation.
   Any new tile meant to be a cell's base tile must also be opaque with #426b6b where floor should
   show (a transparent pixel there would show the black map background).
7. **Tile ids carry meaning in three places outside the lookup tables:** the cursor id 174 is a
   constant of the view; a displayed id in 56-63 means "a demon is visible" to the radio feature;
   id 84 is the wall marker. Keep these ids.
8. **The map grid** is 67 columns by 17 rows of signed 16-bit tile ids plus a parallel grid of
   overlay ids where -1 means none (Part 09).
9. **Other users of the sheet:** the trading-card renderer cuts monster tiles straight from the
   sheet using the same monster-to-tile rule (ids 1-56 use id - 1; Wild monsters 384-387; type I
   demon lord 56; later demons id - 6, that is 62 to 56, 63 to 57, ... 69 to 63), and photo mode
   uses item tiles and the player tile as stickers (Part 13).

## 18. Quirks worth reproducing

- Type I and type II demon lords look identical (both tile 56); tiles 57-63 serve types III-VII,
  the demon prince and the God of Hellfire.
- The Dwarf has one portrait for both genders (171 and 187 are the same picture).
- The three throne states and the four gold pile sizes each look identical, although each has its
  own tile id.
- Invisible traps are plain floor; the spare trap tiles next to the visible ones are never used.
- The elevator-up and elevator-down tiles differ only in the small green triangle.
- The cloud kill animation is the magic fire animation recoloured green, frame for frame.
- A mimic's disguise is global, changes only on turns divisible by 10, can be a type I demon
  lord or a mimic, and is never a Wild Edition monster or the invisible stalker.
- The wall test is made on the tile id, after monster and item substitution.
- Bolt and spell-animation tiles are opaque, so effects wipe the cell to floor rather than being
  composited over what is there; only the sparkle is a true overlay.
- The lightning bolt effect is black with a grey core rather than bright, and looks nearly the
  same in opposite directions.
- The project's Wild Edition planning notes describe the original tiles as having "1px black
  outlines" on "a transparent or solid-black background"; that is not what the base sheet
  contains. The base sheet's creature and item tiles are outline-free sprites with hard black
  shadows on an opaque teal floor, as described above.
