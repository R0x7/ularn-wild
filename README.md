# Ularn for Android — Wild Edition (experimental)

> This repo is **Ularn: Wild Edition**: the same port with a pile of
> deliberately off-the-wall features bolted on (chiptune radio, monster
> trading cards, vending machines, hats, a parody cryptocurrency, a glitch
> realm, fishing, pets, level-up perks, weather, a Polaroid camera and a
> town bounty board). It installs beside the normal app as **"Ularn Wild"** with its own saves.
> The base app lives in [R0x7/android-ularn](https://github.com/R0x7/android-ularn);
> every QoL change made there is merged in here automatically.
>
> **Play in your browser:** https://r0x7.github.io/ularn-wild/ (no
> install; the same game compiled to WebAssembly, with a shared top-100
> leaderboard). It is built from the [`ularn-web`](https://github.com/R0x7/ularn-wild/tree/ularn-web/web)
> branch, which follows `main` automatically.
>
> **Install:** on your phone open
> https://github.com/R0x7/ularn-wild/releases/download/wild-latest/ularn-wild.apk
> (every push to `main` rebuilds it). Each feature is described in
> [`docs/wild/`](docs/wild/README.md) and can be switched off in
> Settings → *Wild features*; the design brief and agent plan is
> [`docs/WILD_EDITION_PLAN.md`](docs/WILD_EDITION_PLAN.md). The core game
> is unchanged with every feature off.
>
> For developers: `tools/host/` plays the real game on Linux from a key
> script (no device needed), `tools/tiles/` composes text-authored pixel
> art onto the tile sheet. See `tools/host/README.md`.


An Android port of **Ularn 1.6.3a** (Julian Olds' graphical version, from
[R0x7/ularn-1.6.3a](https://github.com/R0x7/ularn-1.6.3a)), with the original
tile graphics and fully customizable on-screen controls.

The upstream C game core runs unmodified, apart from a narrower
character-class screen so it fits a phone held upright. A new Android front end
(`ularn_winandroid.c`) implements the game's display/input interface
(`ularn_win.h`), the same way the X11, Windows and Amiga versions do.

## Building

**Android Studio (easiest):** open this folder, let it sync (it will offer to
install the NDK and CMake 3.22.1 if missing), then Run ▶ on a device.

**Command line:** with the Android SDK installed (`ANDROID_HOME` set):

```
./gradlew assembleDebug          # app/build/outputs/apk/debug/app-debug.apk
./gradlew installDebug           # build and install on a connected device
```

**Just want to install it:** on your phone, open
https://github.com/R0x7/android-ularn/releases/latest/download/ularn.apk
and tap the download to install. Every push to `main` builds a new APK
(`.github/workflows/build.yml`) and publishes it as the latest release, so
that link always gets the newest version.

Requires Android 7.0+ (API 24). Builds for arm64-v8a, armeabi-v7a and x86_64.

## Playing

- **Start screen:** New game / Continue saved game / Resume from checkpoint,
  scoreboard, settings and the control editor.
- **Back button** opens the in-game menu (keyboard, edit controls, settings,
  help, save & quit).
- **HUD:** HP bar, level and title, gold, stats and active effects above the
  map; the game's messages below it, reflowed to a readable size.
- **Map:** the view follows your character; pinch to zoom.
  - **Swipe** anywhere on the map to step in that direction (8-way);
    **keep holding** to keep walking, and slide your finger to steer.
    Optional: a quick **flick runs** (Settings → *Quick flick runs*).
  - **Tap** a square to step towards it, **long-press** to run.
  - A swipe also answers "In what direction?" prompts.
- **Portrait and landscape** both work, each with its own control layout.
  Settings → *Screen rotation* can follow the system, always auto-rotate
  (even with rotation lock on), or lock portrait / landscape; the menu also
  has *Switch to landscape / portrait*. In landscape the map fits between your
  side controls (toggle in Settings).
- **Prompts:** whenever the game asks a question (y/n, pick an item, a store
  menu, how much gold), buttons for the valid answers appear under the map.
  Tap a text page (inventory, help, scores) to continue.
- **Minimap:** a round map in the corner of the screen, centred on you.
  White dot = you, yellow = monsters, red = items and gold, with markers for
  stairs, the volcanic shaft, shops, fountains, altars, thrones and traps.
  It only shows what you have seen. Tap it for the whole level with a legend
  (toggle in Settings).
- **Action button:** changes with what you stand on or next to — *Pick up*,
  *Stairs ▲/▼*, *Climb*, *Enter*, *Open chest*, *Open ←* a door, *Fountain*,
  *Altar*, *Throne*… and is dimmed when there is nothing to do.
- **Spell picker:** the *Cast* button lists the spells you have learned this
  game with their codes and what they do (greyed out with no spell points);
  *Type code…* still lets you enter a code by hand.
- **Danger cues:** below a chosen share of your HP (default 25%) the HP bar
  pulses red, the map edge glows red and the phone vibrates once; a big hit
  flashes the map. Each cue can be turned off in Settings.
- **Statistics & achievements** (start screen and menu): lifetime totals,
  best depth and score, fastest win, causes of death, recent games and 17
  achievements, kept on the device.
- **Tablets and foldables:** on a wide screen a side panel shows your stats,
  effects, the level overview and your inventory; it follows fold/unfold and
  split screen (toggle in Settings).
- **Text pages** (help, shops, bank, scores) are laid out for an 80-column
  terminal. When a page is too wide to read on your screen (a phone held
  upright), it is re-wrapped to fit: paragraphs flow to the screen width and
  two-column price lists become one column. Drag to scroll if it is still
  taller than the screen.
- **Hardware / Bluetooth keyboards** work directly; arrow keys and the numpad
  move (toggle in Settings).
- **Save slots:** the start screen lists your slots with each character's
  name, level, depth and when it was last played. Tap a slot to continue it,
  resume its checkpoint, start a new game in it, rename, delete, or export it
  to a `.zip` file (and *Import a saved game…* to load one into an empty
  slot). Each slot has its own save and checkpoint; the scoreboard is shared.
  A save from before slots existed is moved into slot 1 automatically.
- **Accessibility** (Settings): game text size (85–150%, also following the
  system font size), a high-contrast mode, TalkBack labels for every
  on-screen button and D-pad direction, and an option to have TalkBack read
  new game messages.
- **Checkpoints:** the game checkpoints every 25 moves and whenever the app
  goes to the background, so if Android closes it you can *Resume from
  checkpoint*. Use *Save game & quit* (or `S`) for a full save.
- **Settings:** character name and gender, skip intro, no delays, no beep, and
  1.6's *enhanced interface* (open doors with `o`, chests with `O`).

## Customizing the controls

Open **Edit controls** from the start screen or the menu (or give any button
the *Edit controls* action). Portrait and landscape have separate layouts.

| In edit mode | |
|---|---|
| Drag a control | move it (snaps to an 8 dp grid; toggle *Snap*) |
| *Reset* | rebuild the default layout for this screen size |
| Drag the orange corner handle | resize |
| Tap a selected control | open its properties |
| **+ Button / + D-pad** | add a control |
| **Look** | overall opacity, vibration |
| **Profiles** | save/load named layouts, copy/paste layouts as JSON |
| **Reset** | restore the default layout for this orientation |
| **⇅** | move the toolbar to the other edge |
| **Done / Cancel** | keep or discard changes |

**Button properties:** label, action (*Send keys*, *Run modifier*,
*Show/hide keyboard*, *Open menu*, *Edit controls*, *Context action*,
*Cast spell*), keys to send, repeat
while held, round/rounded shape, width and height. Duplicate or delete from
*More…*.

**D-pad properties:** the key for each of the 8 directions and the centre,
4-way or 8-way, keep-moving-while-held, diameter.

**Key notation** (buttons and D-pad):

| Write | Sends |
|---|---|
| `i`, `q`, `Hl` | those characters, in order |
| `\e` or `<esc>` | Escape |
| `\n` or `<ret>` | Enter |
| `\s` or `<spc>` | Space |
| `\b` or `<del>` | Backspace |
| `^X` | Ctrl+X |
| `\\`, `\^` | a literal `\` or `^` |

So a button can be a macro, e.g. `c` + spell code: `cpro`. Ularn 1.6
commands: `hjklyubn` move (capitals run), `.` rest, `i` inventory, `c` cast,
`q` quaff, `r` read, `w` wield, `W` wear, `T` take off, `d` drop, `e` eat,
`\\` list known spells/items, `o`/`C` open/close door, `O` open chest,
`Z` teleport, `^` identify trap, `g` pack weight, `P` taxes, `S` save, `?` help.

**Run modifier:** tap *Run*, then a D-pad direction to run (sends `H J K L…`).
Long-press *Run* to keep it on; tap again to turn it off.

## How it works

```
app/src/main/cpp/
  ularn/                 Ularn 1.6.3a sources (upstream; only the game core is
                         built, not the TTY/X11/Win32/Amiga front ends)
  compat/
    ularn_winandroid.c   Android front end: implements ularn_win.h by writing the
                         display (map tiles, status, effects, messages, text
                         pages, what input is expected) into shared memory, and
                         reading keys from a pipe; adds checkpointing
    android_config.h     replaces upstream config.h (UNIX, LIBDIR ".")
    android_shim.h       renames main()/exit() so the game runs in a forked process
  shared_state.h         the shared display struct
  spawn.c                forks the game with the shared memory and two pipes
  ularn_jni.c            JNI bindings (shared buffer + field offsets)
app/src/main/assets/
  ularn_tiles.png        upstream ularn_gfx.xpm, with the cursor/sparkle masks baked in
  ularn/                 Uhelp, Umaps, Ufortune
app/src/main/java/com/ularn/android/
  game/                  GameState (reads the shared struct), GameView (renders it)
  controls/              layout model, overlay (play + edit mode), editor, persistence
  GameSession.kt         process lifecycle and I/O
  MainActivity.kt        UI, menus, settings
```

Each game is a fresh `fork()` of the app process, so the game's global state
always starts clean. Game files live in the app's private storage: the game
runs in `ularn163/` (data files and the shared `Uscore`), with `$HOME` set to
the save slot's folder `ularn163/slots/<n>/`, where Ularn keeps
`ularn_player.sav`, `ularn_player.ckp` and `ularn.opt`; the app adds
`slot.json` (name, level, depth, last played).

## History, credits and license

Ularn is the work of Noah Morgan (Larn, 1986), Phil Cordier (Ularn), David
Richerby, Josh Brandt and Josh Bressers (Ularn 1.5) and Julian Olds
(Ularn 1.6). [HISTORY.md](HISTORY.md) gives the game's history, from
[larn.org](https://larn.org/history/history), and a copyright note.

This project claims no copyright in the game and is shared free and
non-commercially only: Larn's original terms forbid making a profit from it.
The 1.6 sources are released under the GPL v2 (see `LICENSE` and
`app/src/main/cpp/ularn/COPYRIGHT`), but larn.org notes that this does not
lift Larn's no-profit condition; see [HISTORY.md](HISTORY.md#copyright).
The non-GPL modules in the 1.6.3a distribution (X11 menu library, Amiga helpers)
are not used and not included.
