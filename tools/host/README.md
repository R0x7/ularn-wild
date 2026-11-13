# Host test harness

Plays the real game (upstream core + the Android front end) on Linux, no
Android SDK needed, driven by a key script. Built for the Wild Edition work
(docs/WILD_EDITION_PLAN.md) but useful for any change to the C side.

```
tools/host/check.sh                 # gcc syntax check of every game source, Android flags
tools/host/build.sh                 # builds tools/host/out/ularn_host
tools/host/play.sh [opts] STEP...   # runs a game, feeding STEPs; prints state after each
tools/host/smoke.sh                 # regression: new game, walk, dungeon, save, restore
tools/host/second_wind.sh           # regression: the Second Wind perk works once per level
tools/host/treasure_map.sh          # regression: bottles hold treasure maps below ground only
```

`play.sh --help` lists the options. The essentials:

* `--wizard` sends `-` + the wizard password first, so the debug keys work:
  * `=` (Wild Edition, wizard only) opens a debug menu: **(o)** place any
    object by id *under you* (then the game asks to take it), **(m)** create a
    monster by id next to you, **(g)** +1000 gold, **(x)** gain a level,
    **(t)** run N `wild_turn()` ticks without moving, **(h)** heal,
    **(d)** take N damage (a blow through `losehp()`, so it can kill),
    **(f)** print the Wild attribute values.
  * `+` is upstream's create item (places it next to you), `_` max XP,
    `` ` `` debug mode.
* `--seed N` makes the run reproducible (`ULARN_SEED`).
* `--wild MASK` sets the feature mask (`ULARN_WILD`, default 4095 = all);
  `--wild 0` is the vanilla game.
* `--quiet` prints only changed messages, text pages and events.
* `--home DIR` reuses a HOME so you can save (`S`) in one run and restore in
  the next (`play.sh --home DIR ...`; add `-- -r` for the checkpoint).
* Steps: raw keys; `\n` enter, `\e` escape, `\s` space, `{N:keys}` repeats.
  Driver commands: `!map` (level overview), `!inv` (inventory as the side
  panel sees it), `!tiles` (tile ids around the player).

Object and monster ids are the enum positions in `itm.h` / `monster.h`;
`tools/host/ids.py` prints them.

Each run uses a fresh temp HOME and work dir (kept with `--keep` or after a
crash; `ularn.log` there has the game's stdout/stderr). Exit code 1 if the
game crashed.
