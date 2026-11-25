# Atmosphere: weather, day/night and Polaroid photos

## Weather and day/night (`WILDF_WEATHER`)

The town above the dungeon has a clock and a sky.

* **The clock:** a day is 1200 turns (an hour is 50 turns; a new game
  starts at 08:00). Night is 21:00 to 05:59; you are told when dusk falls and when
  dawn breaks. The HUD shows a sun or a moon after the level name, and the
  map is tinted: warm at dawn and dusk, deep blue at night (never more than
  40% darker).
* **Weather:** clear, rain, snow ("Snow! In Larn!") or a storm, changing
  every 200-600 turns ("It starts to rain.", "Thunder rolls over the
  mountains."). It keeps changing while you are underground; you hear about
  it when you climb back out. Rain and snow drift over the open ground, stars
  come out over unexplored squares at night.
* **What it does:** at night random monsters turn up twice as often on the
  surface, and now and then "You hear the pitter-patter of a thousand tiny
  feet": 8-12 lemmings enter at the east edge and march west across the map
  (they leave at the west edge, unless they find you first). In a storm,
  lightning occasionally strikes a monster near you for 20 damage (and the
  screen flashes). It is the sky's blow, not yours: it doesn't break your
  hold monster spell, and a half-damage curse doesn't weaken it.
* **Rainbows:** when rain or a storm clears in daylight while you are in
  town, a rainbow arches over it and the game says which way its end lies.
  There is a pot of gold there, already marked on your map: 26-75 gold if
  you have only been to dungeon level 1, more the deeper you have been
  (about 300-900 once you have reached level 12 or the volcano). Rain that stops
  at night, or while you are underground, leaves no rainbow.
* **Shooting stars:** on a clear night in town (not through rain, snow or
  storm clouds, and not while you are blind) a shooting star now and then
  streaks across the sky and falls to earth; the game says which way, and
  the spot is marked on your map. A gem lies there: a diamond, ruby, emerald
  or sapphire worth as much as one found on the deepest dungeon level you
  have reached. On average a bit more than one clear night in two brings one.
* **At a glance:** the side panel (wide screens) has a SKY section: the day
  and time ("Day 2, 14:24"), the weather while you are in town (underground
  you only find out when you climb back out), and how long until night falls
  or dawn breaks ("Night falls in 335 turns", shown in gold while it is
  night in town).
* **Settings:** "Reduce weather effects" keeps the tint and the HUD icon but
  stops the drifting rain and snow (thinner and still), the twinkling and
  the lightning flashes. "Weather and day/night on the surface" turns it all
  off; like every Wild feature it is stored with the character when created.

## Polaroid photo mode (`WILDF_PHOTO`)

* **Take a photo:** in-game menu → "Take a photo", or give an on-screen
  button the action "Take a photo" (Edit controls → a button → Action). The
  map as you see it (weather included) becomes an instant photo with a
  handwritten caption ("Ann the novice explorer — level 3, turn 1234"), the
  date, a vignette and a few pixel stickers. The screen flashes and clicks
  (through Dungeon Radio's sound effects when the Radio is on).
* **Where it goes:** Pictures/Ularn Wild in the gallery, then a Share
  button. Android 10 and later need no permission; Android 7-9 ask for
  storage access the first time.
* **Auto-photo:** with "Auto-photo on death / victory" (on by default) the
  final scene is snapped by itself. The newest photo shows under "Last
  photo" in Statistics & achievements.
* **Settings:** "Polaroid photo mode" in Wild features removes the menu
  entry, the button action and the auto-photo.
