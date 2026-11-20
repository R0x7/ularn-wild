# Part 10 — App shell: start screen, menus, settings, save slots, statistics, accessibility

This part specifies everything the Android application wraps around the game: the single
activity and its view stack, the start screen, the in-game menu, how a game process is
started and what happens when it ends, the settings dialog and every stored preference,
the save-slot system (including export, import and migration), the spell picker, run
statistics and achievements, the danger-cue settings, and the accessibility features.

It leans on these other parts:

- Part 01 for the game's command-line options (`-s`, `-r`, `-d`), the options file syntax
  and the in-game character-class screen.
- Part 03 for the scoreboard pages, the death/win pages and the save-file format.
- Part 07 for what the spells do in the game.
- Part 09 for the shared display state, the process model (fork, key pipe, notification
  pipe), the checkpoint key and the context action.
- Part 11 for how the game view and the on-screen controls are drawn and touched, the
  per-control editor dialog and the default control layouts.
- Part 13 for the Wild Edition app features (radio, trading cards, photo mode, weather and
  glitch effects) whose entry points appear in the menus described here.

All sizes written "dp" are density-independent pixels; "sp" are scaled text pixels. The
recurring accent colour is orange-amber RGB (255, 190, 60), hex FFBE3C. The app has no
layout resource files: every screen is built programmatically. All text is English only;
there is exactly one string resource, the application name "Ularn Wild".

No random numbers are drawn anywhere in this part.

## 1. Application manifest, theme and identity

| Item | Value |
|---|---|
| Application id | `com.ularn.android.wild` (so it installs beside the base app with separate data) |
| Launcher label | "Ularn Wild" |
| Version name | "3.0-wild (Ularn 1.6.3a)"; version code 100 plus the CI run number (100 locally) |
| Minimum / target Android | API 24 (Android 7.0) / API 35 |
| Permissions | Vibrate; write external storage, only requested up to API 28 (photo mode on Android 7 to 9, see Part 13) |
| Backup | Allowed (platform auto-backup) |
| Native libraries | Loaded straight from the APK (not extracted) |
| Activities | One, the launcher activity, launch mode "single task" |
| Configuration changes handled by the activity itself (no restart) | orientation, screen size, screen layout, smallest screen size, keyboard, keyboard hidden, navigation, UI mode, density, font scale |
| Soft-input mode | "adjust nothing" and "state hidden": the window is never resized or panned for the keyboard, and the keyboard is hidden at launch |

Theme: the platform's dark "device default, no action bar, fullscreen" theme, with a black
window background, accent colour FFBE3C, black navigation bar and black status bar. From
API 28 the theme (and the activity at creation) asks for layout into the display cutout on
the short edges. All dialogs are the platform's stock alert dialogs in that theme; none is
custom-styled beyond what is stated below.

## 2. Files the app shell owns

All paths are inside the app's private files directory.

| Path | Content |
|---|---|
| `ularn163/` | The game's working directory. The three data files `Uhelp`, `Umaps`, `Ufortune` are copied here from the APK assets folder `ularn/` on every game start (overwriting). The shared scoreboard lives here too (Part 03). When a run has no slot (the scoreboard), the options file `ularn.opt` is also written here. |
| `ularn163/slots/<n>/` | Save slot n (1 to 10): the game's `$HOME` for that slot. Holds `ularn_player.sav` (saved game), `ularn_player.ckp` (checkpoint), `ularn.opt` (options, rewritten on each start) and `slot.json` (app-side metadata). |
| `ularn163/slots/.legacy/` | Staging folder used while migrating a pre-slot save. |
| `ularn163/slots/.import-<nanosecond clock>/` | Staging folder used during an import; always removed afterwards. |
| `stats.json` | Run statistics and achievements. |
| `photos/last.png` | The newest photo-mode picture (Part 13); the statistics dialog shows it. |

Two shared-preferences files exist: `settings` (all settings, including the accessibility
ones) and `saves` (two integers remembering which slot a pending export or import is for).

The game process always runs with login name "player" (environment `USER` and `LOGNAME`),
which is why the save files are named `ularn_player.*`.

## 3. The activity

### 3.1 View stack

The content view is a frame container with a black background. Its children, bottom to
top:

1. **Game view**, filling the container. Draws the map, HUD, messages, text pages and
   answer buttons (Part 11). Its accessibility description is "Game screen". It is
   focusable (also in touch mode) and acts as the text-input target for the soft keyboard.
2. **Controls overlay**, filling the container. Draws the customisable buttons and D-pads
   (Part 11).
3. **Side panel** (tablets/foldables, Part 11), right-aligned, full height, initially
   hidden with zero width. It is inserted directly above the map and controls and below
   the two items that follow.
4. **Edit toolbar**: a horizontally scrolling strip, full width, wrap height, pinned to
   the top edge (or the bottom, see 3.9); hidden except in control-edit mode.
5. **Start panel**: a vertical scroller 300 dp wide, wrap height, centred; shown whenever
   no game is running and the control editor is not open.

The same stack is used for the start screen and for play: nothing is swapped. On the start
screen the game view and controls are simply visible behind the centred panel (the game
view draws nothing but its background until a game has published its first frame; after a
game has ended it keeps showing that game's last frame). Starting a game hides the start
panel; the game ending shows it again.

The container's padding is set to the display-cutout safe insets on all four sides
(API 28+), so nothing is drawn under a notch.

### 3.2 System bars

Whenever the window gains focus, the status and navigation bars are hidden in "immersive
sticky" fashion: a swipe from the edge shows them transiently and they hide again by
themselves.

### 3.3 Start-up sequence

On creation the activity, in this order:

1. Loads settings, the control-layout store, the run tracker (which loads `stats.json`)
   and the accessibility preferences.
2. Creates the game view, controls overlay, game session, the Wild Edition helpers (event
   bus, card drops, radio, photo mode, a listener that flashes the screen on a lightning
   event; all Part 13), the key follow-up helper, the context action (Part 09), the spell
   picker, the message announcer and the save-slot controller.
3. Runs the legacy-save migration (section 8.8) before anything reads the slots.
4. Builds the view stack, installs the inset listeners (3.6).
5. Applies settings, applies the rotation setting, loads the control layout for the
   current orientation, and shows the start panel.

### 3.4 Orientation and rotation

The activity handles rotation itself (no restart). It tracks one boolean, "landscape",
taken from the current configuration.

- When the configuration changes and the landscape flag flips: if the control editor is
  open it is closed **saving** the edits; the soft keyboard is hidden; the control layout
  for the new orientation is loaded (portrait and landscape each have their own stored
  layout; Part 11). The layout is only loaded once the overlay has a non-zero size, and
  never while editing; a layout-change listener on the overlay retries the load and
  recomputes the map reserve (3.7) whenever the overlay's size actually changes (rotation,
  split screen, fold/unfold).
- On every configuration change the accessibility text scale is re-applied (the system
  font scale may have changed).

The **rotation setting** (preference `orientation`, an index, default 0) maps to a
requested screen orientation:

| Index | Label | Behaviour |
|---|---|---|
| 0 | "Follow system rotation setting" | all four orientations, honouring the user's rotation lock |
| 1 | "Always auto-rotate" | all four orientations by sensor, ignoring the rotation lock |
| 2 | "Portrait" | portrait or reverse portrait by sensor |
| 3 | "Landscape" | landscape or reverse landscape by sensor |

An out-of-range stored index behaves as index 0. The in-game menu's "Switch to
portrait/landscape" entry stores index 2 if the screen is currently landscape, otherwise
index 3, and applies it at once. This is a persistent change of the setting, not a
temporary toggle.

### 3.5 Keyboard input

**Soft keyboard.** The app keeps its own flag "keyboard shown". Showing the keyboard
focuses the game view and asks the input method to show (implicit request); hiding asks it
to hide and clears the flag. The flag is not synchronised with the real keyboard state: if
the user dismisses the keyboard through the system, the flag stays set, the menu still
offers "Hide keyboard", and the next Back press only clears the flag.

The game view declares itself a text editor with a null input type (no suggestions or
composing) and the "no fullscreen" and "no extract UI" editor flags. Its input connection
behaves as follows:

- Committed text: each character is sent to the game as one byte; a line feed becomes
  byte 13; any character above code 127 is sent as byte 127.
- "Delete surrounding text": sends byte 8 once per character to delete before the cursor,
  at least once.
- Key events from the keyboard: key-down events go through the hardware key mapping
  below; key-up events are swallowed.

**Hardware (and Bluetooth) keyboards.** A key-down is translated by the first matching
rule; "Shift" below means the Shift modifier is held, which upper-cases the movement
letter (upper-case movement letters are the game's run commands).

| Order | Key | Bytes sent |
|---|---|---|
| 1 | Enter, numpad Enter | 13 |
| 1 | Backspace (the "delete backward" key) | 8 |
| 1 | Escape | 27 |
| 1 | Arrow left / right / up / down | `h` / `l` / `k` / `j` (with Shift `H` `L` `K` `J`), only if the setting "Arrow keys / numpad move" is on; if it is off the arrow keys are not consumed |
| 1 | Home / Page Up / End / Page Down | `y` / `u` / `b` / `n` (with Shift upper-case); always, regardless of the setting |
| 2 | Numpad 7 8 9 / 4 5 6 / 1 2 3, only when the setting is on **and** Num Lock is off | `y k u` / `h . l` / `b j n` (with Shift upper-case; the `.` for numpad 5 is unaffected by Shift) |
| 3 | Ctrl + a letter a to z | the control code 1 to 26 |
| 4 | Any key producing a printable ASCII character (codes 32 to 126) | that character |
| 5 | Anything else | not consumed |

With Num Lock on, the numpad digits fall through to rule 4 and are sent as digits.

Routing: the game view handles key-downs itself when it has focus; the activity applies
the same mapping as a fallback for unfocused key-downs, but only while a game is running
and the control editor is closed. Bytes are forwarded to the game only while a game
process is running (otherwise they are dropped, though the key still counts as consumed).

The activity treats two keys specially: **Menu** opens the menu (3.8 / section 5), and
**Back** follows the rules in 3.8.

### 3.6 Insets and the on-screen keyboard

Because the window never resizes for the keyboard, the app measures the keyboard itself
and keeps the game's display (and therefore its prompt line) above it:

- API 30 and later: on each inset pass, the keyboard height is the IME bottom inset minus
  the container's bottom padding (never below 0) if the IME is visible, otherwise 0.
- Before API 30: on each global layout pass, if the "keyboard shown" flag is clear the
  height is 0; otherwise it is the distance by which the game view's bottom edge on screen
  extends below the bottom of the window's visible display frame (never below 0).

When the measured height changes, the map reserve (3.7) is recomputed.

### 3.7 Keeping the map clear of the controls (the "reserve")

After every layout load, edit, settings change, size change or keyboard-height change, the
activity measures the control layout and tells the game view how much of each edge to
leave free. With w and h the overlay's pixel size, and for each control its on-screen
rectangle:

1. Landscape: a control whose centre x is left of 0.25 w contributes its right edge to
   "left reserve" (the maximum over such controls); a control whose centre x is right of
   0.75 w contributes (w minus its left edge) to "right reserve".
2. Portrait: a control whose centre y is below 0.3 h contributes its top edge to
   "top of the bottom block" (the minimum over such controls; starts at h).
3. The minimap is told to avoid, in landscape, min(left, 0.3 w) and min(right, 0.3 w); in
   portrait 0 and 0.
4. Landscape: if the setting "Landscape: fit the map beside the controls" is off, left and
   right are zeroed. The game view's reserve becomes left = min(left, 0.3 w),
   right = min(right, 0.3 w), bottom = keyboard height.
5. Portrait: the game view's reserve becomes left 0, right 0,
   bottom = max( min(h − top of the bottom block + 4 dp, 0.65 h), keyboard height ).

(The game view ignores a reserve that would leave it less than 120 dp in either
direction; Part 11.)

### 3.8 Back button and Menu key

Back, in priority order:

1. Control editor open: close it, saving the edits.
2. "Keyboard shown" flag set: hide the keyboard.
3. Otherwise: open the menu.

"Open the menu" (Back rule 3, the Menu key, or an on-screen button with the Menu action)
when **no game is running** means: if the start panel is hidden, show it; if it is already
showing, finish the activity (quit the app). When a game is running it shows the in-game
menu of section 5.

### 3.9 Control-edit toolbar and its dialogs

The editor's gestures and the per-control properties dialog belong to Part 11; the
toolbar and its helper dialogs are part of the activity and are specified here.

**Entering edit mode** (start screen "Edit controls", menu "Edit controls", or a control
with the Edit action): hide the start panel, hide the keyboard, take a deep copy of the
current layout (for Cancel), switch the overlay to edit mode, show the toolbar, and show a
long toast: "Drag to move · corner handle to resize · tap a selected control to edit it".

**Leaving edit mode**: with "save", the layout is stored for the current orientation;
without, the backup copy is restored. Edit mode ends, the map reserve is recomputed, the
toolbar hides, and if no game is running the start panel is shown again.

**Toolbar appearance**: background ARGB (200, 10, 10, 12); inner row padded 8 dp
horizontally and 6 dp vertically, vertically centred; no visible scrollbar. Buttons are
the "pill" style of section 4.1 with a 6 dp gap after each. In order:

| Button | Action |
|---|---|
| "Done" (accent) | leave edit mode, saving |
| "+ Button" | add a button: label "New", no keys, centre at (0.5, 0.6) of the overlay, 56 × 46 dp; select it and open its properties |
| "+ D-pad" | add a D-pad: centre (0.5, 0.6), 160 × 160 dp, repeat-while-held on; select it and open its properties |
| "Edit selected" | open the properties of the selected control; disabled and drawn at 40% opacity when nothing is selected |
| "Snap: on" / "Snap: off" | toggle grid snapping (default on); the label shows the current state |
| "Look" | the "Controls appearance" dialog below |
| "Profiles" | the "Layout profiles" dialog below |
| "Reset" | confirmation dialog, message "Reset the landscape layout to the default?" or "Reset the portrait layout to the default?", buttons "Reset" and "Cancel"; Reset rebuilds the default layout for this orientation and screen size |
| "⇅" (spoken as "Move toolbar to the other edge") | moves the toolbar between the top and bottom edge |
| "Cancel" | leave edit mode, discarding |

From the properties dialog (Part 11): saving replaces the control with the edited copy;
deleting removes it and clears the selection; duplicating adds a copy with a fresh id,
offset by +0.06 horizontally and +0.04 vertically (each capped at 1.0), and selects it.

**"Controls appearance" dialog** (title exactly that; button "OK"): a label
"Opacity: N%"; a slider with 91 positions where opacity = 0.10 + position/100 (10% to
100%); a checkbox "Vibrate on press". Both take effect immediately on the layout being
edited (the checkbox also immediately switches the game view's haptics).

**"Layout profiles" dialog** (title exactly that; a plain list, no buttons):

| Item | Action |
|---|---|
| "Save current layout as…" | dialog titled "Save profile" with a single-line field (hint "Profile name"), buttons "Save" and "Cancel"; a non-blank trimmed name stores the layout and toasts `Saved "<name>"` |
| "Load: <name>" (one per stored profile) | puts that layout into the editor; toast "Loaded — tap Done to keep it" |
| "Copy layout to clipboard (JSON)" | copies the layout's JSON text with clip label "Ularn controls"; toast "Layout copied" |
| "Paste layout from clipboard" | parses the clipboard text as a layout; on failure toast "Clipboard doesn't contain a layout", else loads it with toast "Pasted — tap Done to keep it" |
| "Delete a profile…" (only if at least one profile exists) | dialog titled "Delete profile" listing the names; tapping one deletes it; toast "Deleted" |

A loaded or pasted layout is only kept if the user then presses Done; Cancel restores the
backup.

### 3.10 Keep-screen-on

If the setting "Keep the screen on while the app is open" (default on) is set, the window
carries the keep-screen-on flag at all times the activity is in the foreground (start
screen included); otherwise the flag is cleared and the screen dims and locks normally.

### 3.11 Lifecycle

| Event | What the app does |
|---|---|
| Resume | Tells the radio to resume (Part 13). |
| Pause | Tells the radio to pause (no music in the background). |
| Stop (app no longer visible) | If a game is running: sends the checkpoint key (byte 0x1D, Ctrl+]) to the game, which writes a checkpoint at its next command prompt (Part 09), and records the slot's progress (8.4). Always: flushes statistics to disk and stops the play clock. |
| Destroy, when really finishing | "Leaves" the running game: sends the checkpoint key, then closes the game's input pipe so the game process exits. |
| Process death in the background | Nothing can run. On the next launch the start screen appears; the slot shows a checkpoint (or a save) and the top button offers to resume or continue it. The game also checkpoints every 25 commands by itself (Part 09). |

There is no saved-instance state: the activity is never recreated for configuration
changes, and after process death it starts fresh on the start screen.

The game session notifies the activity on every display change (from a background thread).
Each notification: schedules a redraw of the game view and side panel, lets the context
action re-evaluate, pokes the message announcer (section 12.4) and polls the Wild event
ring (Part 13). When the game process exits, the notification arrives on the main thread
followed by the exit handler of section 6.4; the process exit code is ignored.

## 4. The start screen

### 4.1 Common "pill" button style

Used by the start panel and the edit toolbar: a small platform button, text not
upper-cased; text white (black on accent buttons); background a rounded rectangle with
18 dp corner radius filled ARGB (220, 45, 47, 55), or solid RGB (255, 190, 60) for accent
buttons; minimum height 36 dp; 14 dp horizontal padding. In the start panel every such
button is full panel width, 44 dp tall, with an 8 dp top margin.

### 4.2 Panel

The scroller (300 dp wide, centred, no visible scrollbar, 12 dp top and bottom padding
that content may scroll through) holds one vertical panel: content centred horizontally;
padding 24 dp left/right and 20 dp top/bottom; background a rounded rectangle, 18 dp
corner radius, fill ARGB (235, 22, 22, 26), 1 dp outline ARGB (90, 255, 255, 255);
elevation 8 dp. The panel is rebuilt from scratch every time it is shown, and scrolled
back to the top.

Contents, top to bottom:

1. **Title** "ULARN": 30 sp, bold monospace, colour RGB (255, 190, 60), centred. Spoken
   by screen readers as "Ularn" and marked as a heading.
2. **Subtitle** "The Caverns of Ularn": platform default size, light grey (CCCCCC),
   centred, 14 dp bottom padding.
3. **Quick-start button** (accent), one of:
   - If at least one slot holds a game: take the slot whose metadata "last played" time is
     greatest (a slot without metadata counts as 0; ties go to the lowest slot number).
     Unless that slot is currently being exported, show "Continue: <slot title>" if it has
     a saved game, otherwise "Resume: <slot title>" (checkpoint only). Tapping plays the
     slot (8.3). If that slot is busy, no quick-start button is shown.
   - If no slot holds a game: "New game", which opens the new-game dialog (6.1) for the
     lowest-numbered empty slot (toast "No empty slot" if there were none).
4. **Heading** "Save slots": light grey, bold, padding 4 dp left, 16 dp top, 2 dp bottom;
   marked as a heading.
5. **Slot cards** (section 8.2), one per listed slot, each with a 6 dp top margin.
6. Button "Import a saved game…" (import into the first empty slot, 8.7).
7. Button "Scoreboard" (6.5).
8. Button "Statistics & achievements" (section 10.6).
9. Only if the trading-cards feature is enabled: button "Card album (C/69)" where C is the
   number of distinct cards collected (opens the album, Part 13).
10. Button "Edit controls" (3.9).
11. Button "Settings" (section 7).
12. Button "Quit" (finishes the activity).

Tapping the game view while no game is running shows the start panel (useful if it was
hidden).

## 5. The in-game menu

Shown by Back, the Menu key or a Menu-action control while a game is running. It is a
plain list dialog with no title and no buttons (dismiss by Back or tapping outside).
Entries in order:

| # | Label | Condition | Action |
|---|---|---|---|
| 1 | "Hide keyboard" if the keyboard-shown flag is set, else "Show keyboard" | always | toggle the soft keyboard |
| 2 | "Switch to portrait" if currently landscape, else "Switch to landscape" | always | store and apply the opposite fixed orientation (3.4) |
| 3 | "Edit controls" | always | enter control-edit mode (the game keeps running underneath) |
| 4 | "Settings" | always | the settings dialog (section 7) |
| 5 | "Game help (?)" | always | send the key `?` to the game |
| 6 | "Save game & quit" | always | set the "quit after exit" flag and send the key `S` to the game |
| 7 | "Leave (resume from checkpoint later)" | always | confirmation dialog (below) |
| 8 | "Statistics & achievements" | always | the statistics dialog |
| 9 | "Card album" | trading-cards feature enabled in Settings | open the album (Part 13) |
| 10 | "Radio" | radio feature enabled in Settings | open the radio dialog (Part 13) |
| 11 | "Take a photo" | photo feature enabled in Settings | take a photo (Part 13) |

The three Wild entries test the app's current Settings mask, not the mask stored in the
running game.

**Leave confirmation**: no title; message "Leave now? The game is checkpointed, so
"Resume from checkpoint" picks up from here (or from your last few moves if you're in the
middle of a menu)." (the inner phrase is in straight double quotes); buttons "Leave" and
"Cancel". Leave sets the "quit after exit" flag, sends the checkpoint key and closes the
game's input so it exits.

**Quit-after-exit flag**: when set, the app finishes (closes) as soon as the game process
exits instead of returning to the start screen. It is cleared only when the next game is
started. Consequence: "Save game & quit" merely types `S`; if the game is not at its
command prompt when the key arrives (so no save happens), the flag stays armed and the app
will close whenever that game process eventually ends.

## 6. Starting and ending games

### 6.1 The new-game dialog

Title "New game in slot N". Body (20 dp side padding, 8 dp top), top to bottom:

1. Label "Character name".
2. Single-line text field, hint "Name", capitalising each word, pre-filled with the
   Settings name.
3. Checkbox "Female character", initial state from Settings.
4. Label "Difficulty: k", with " (normal)" appended when k is 0 (14 dp top padding).
5. Slider with positions 0 to 10, initially the remembered difficulty (preference
   `difficulty`, default 0, always clamped to 0..10). Its spoken state is "Level k"
   (API 30+).
6. A 12 sp line at 75% opacity describing the difficulty (table below); updates live.
7. A 12 sp line at 60% opacity: "Once you have won, the game sets your next game at least
   one level harder."

Buttons "Start" and "Cancel".

Difficulty description: for k = 0, "Standard game: monsters at their normal strength."
Otherwise "Monsters: A% hit points, B% damage, armour class -k. They drop C% gold and give
D% experience." where A = (6+k)/6, B = (6+k)/5, C = 10/(10+k), D = 7/(7+k), each times 100
and rounded to the nearest whole number (halves round up). The resulting numbers:

| k | hit points | damage | armour class | gold | experience |
|---|---|---|---|---|---|
| 1 | 117% | 140% | -1 | 91% | 88% |
| 2 | 133% | 160% | -2 | 83% | 78% |
| 3 | 150% | 180% | -3 | 77% | 70% |
| 4 | 167% | 200% | -4 | 71% | 64% |
| 5 | 183% | 220% | -5 | 67% | 58% |
| 6 | 200% | 240% | -6 | 63% | 54% |
| 7 | 217% | 260% | -7 | 59% | 50% |
| 8 | 233% | 280% | -8 | 56% | 47% |
| 9 | 250% | 300% | -9 | 53% | 44% |
| 10 | 267% | 320% | -10 | 50% | 41% |

(This text mirrors the game's own difficulty scaling, Part 01/04; the game accepts up to
100, the app offers at most 10.)

On "Start":

1. Refuse if a game is running or an export/import is in progress (8.6 toasts).
2. Clean the name: replace every comma, colon, carriage return and line feed with a
   space; trim; keep at most 30 characters; if blank use "Adventurer".
3. Delete the slot's save and checkpoint files, and write fresh slot metadata: character
   = the cleaned name, female = the checkbox, last played = now; no label, level unknown.
   If this fails: toast "Couldn't prepare slot N: <error text>" and stop.
4. Remember the chosen difficulty as the default for next time.
5. Start the slot (6.2) with arguments: none for difficulty 0; otherwise `-d` followed by
   the difficulty number (capped at 10).

The name and gender typed here do not change the Settings defaults. The character class
is not chosen by the app: the game shows its own class-selection screen (Part 01/02).

### 6.2 Starting a slot

1. Read the slot's metadata (if missing, assume the Settings name and gender).
2. Set the game's home directory to the slot folder; build the options-file text (6.3)
   from the metadata's character name (falling back to the Settings name) and gender;
   mark the slot as the active slot; write the metadata back with "last played" = now.
3. Launch the game with the given arguments.

Arguments by purpose:

| Purpose | Arguments after the program name "ularn" |
|---|---|
| New game, normal difficulty | none |
| New game, difficulty k ≥ 1 | `-d` `k` |
| Continue a saved game | none (the game finds the save file in its home and restores it; Part 01/03) |
| Resume from a checkpoint | `-r` |
| Scoreboard | `-s` |

Launching (the internals are Part 09): hide the start panel; clear the quit-after-exit
flag; reset the message announcer's history; create the working directory and copy the
three data files into it; create the home directory and write `ularn.opt` there; fork the
game with working directory `ularn163`, and environment `HOME` = the home directory,
`USER` = `LOGNAME` = "player", `ULARN_WILD` = the Wild feature mask from Settings as a
decimal number (section 7.5). If launching fails: toast "Couldn't start the game" and show
the start panel again. On success the game view is marked running and given focus. If a
game process is already running, the launch request is treated as already successful and
nothing new is started.

Because the options file is rewritten on every start, the Settings options (skip intro,
no delays, no beep, enhanced interface) apply to continued and resumed games as well; the
name and gender in it always come from the slot.

### 6.3 The options file

Two lines:

1. The comment "# Written by the Ularn app; edit in Settings."
2. "OPTION " followed by a comma-separated list, in this order: "name:<name>";
   "gender:male" or "gender:female"; "nowelcome" if Skip introduction is on; "nonap" if No
   delays is on; "nobeep" if No beeping is on; and always either "enhanced_interface" or
   "noenhanced_interface".

The name is cleaned exactly as in 6.1 step 2. Example second line for defaults:
"OPTION name:Adventurer,gender:male,noenhanced_interface".

### 6.4 When the game process ends

The game shows its own final pages (death or victory text, score, scoreboard) and waits
for a key on each before exiting (Part 03), so the app adds no end-of-game screen. On
process exit the app, in order:

1. Gives the run tracker a last look at the shared state and tells it whether the game is
   resumable, i.e. whether a save file or a checkpoint file now exists in the game's home
   directory (section 10.3).
2. Records the slot's progress (8.4).
3. Marks the game view not running and hides the keyboard.
4. If the quit-after-exit flag is set, finishes the activity. Otherwise shows the start
   panel, unless the control editor is open (then the panel appears when editing ends).

Resulting behaviour by how the game ended:

| How it ended | Files left in the slot | What the user sees next |
|---|---|---|
| Death or victory | neither save nor checkpoint (the game removes them; Part 03) | start screen; the slot shows as empty; statistics record the outcome |
| In-game save (`S` key or a control) | save file | start screen with "Continue: …" |
| Menu "Save game & quit" | save file | the app closes |
| Menu "Leave" / app finishing | checkpoint (if the game reached a command prompt) | the app closes; next launch offers "Resume: …" |
| In-game quit (`Q`) | none | start screen; statistics record a quit |

### 6.5 The scoreboard entry

"Scoreboard" on the start screen first detaches from any slot (home directory back to the
working directory, options text back to the Settings defaults, no active slot), then
launches the game with `-s`. The game prints the scoreboard pages and exits (Part 03),
which returns to the start screen. The scoreboard file is shared by all slots.

Quirk: the exit handler's "resumable" test then looks for save files in the working
directory, where normally none exist, so the run tracker treats the exit as a quit of
whatever run it currently considers in progress (see 10.3, rule on game exit). Viewing the
scoreboard therefore closes the tracker's current unfinished run as "quit" even though its
save is intact in a slot.

## 7. Settings

### 7.1 The dialog

Title "Settings"; one button "OK"; the body scrolls. Body padding 20 dp left/right, 8 dp
top. Section headers are bold lines with 14 dp top and 2 dp bottom padding. Small notes
are 12 sp at 70% opacity.

Persistence rules, which matter for behaviour:

- Every checkbox, the danger-threshold chooser and the accessibility controls write their
  preference **immediately** when changed.
- The character name and the rotation choice are written only when "OK" is pressed.
- "OK" also re-applies all settings to the live views (gesture options, zoom, minimap,
  side panel, haptics, map reserve, danger cues, glitch/weather reduction, keep-screen-on,
  accessibility) and applies the rotation choice. Dismissing the dialog any other way
  keeps the already-written checkbox values but does not push them to the live views
  until the next "OK" or app start. Accessibility changes are the exception: they apply
  live as they are made.
- Character and Wild-feature options only influence the next game start.

### 7.2 Sections, in order

**"Character (default for new games)"**

| Control | Key | Type | Default | Effect |
|---|---|---|---|---|
| Text field, hint "Name", single line | `name` | text | "Adventurer" | Default name offered by the new-game dialog; used for slot-less runs and as a fallback when slot metadata is missing |
| "Female character" | `female` | boolean | off | Default gender in the new-game dialog; same fallbacks |
| "Skip introduction" | `skipIntro` | boolean | off | Adds "nowelcome" to the options file |
| "No delays (no-nap)" | `noNap` | boolean | off | Adds "nonap" |
| "No beeping" | `noBeep` | boolean | off | Adds "nobeep" |
| "Enhanced interface: open doors with 'o', chests with 'O'" | `enhanced` | boolean | off | Writes "enhanced_interface" instead of "noenhanced_interface" |

**"Screen"**

| Control | Key | Type | Default | Effect |
|---|---|---|---|---|
| Drop-down with the four rotation labels of 3.4 | `orientation` | integer index | 0 | Requested orientation |
| Note: "Portrait and landscape each keep their own control layout." | | | | |
| "Keep the screen on while the app is open" | `keepScreenOn` | boolean | on | 3.10 |

**"Map"**

| Control | Key | Type | Default | Effect (gesture details in Part 11) |
|---|---|---|---|---|
| "Swipe to step, hold to keep walking" | `swipeMove` | boolean | on | Swipe-to-move on the map |
| "Quick flick runs (instead of one step)" | `flickRun` | boolean | off | A fast flick sends a run command |
| "Tap a square to step towards it (long-press to run)" | `tapMove` | boolean | on | Tap-to-move |
| "Landscape: fit the map beside the controls" | `fitBeside` | boolean | on | 3.7 step 4 |
| "Minimap (tap it to see the whole level)" | `minimap` | boolean | on | Shows the round minimap |
| "Tablets / foldables: side panel with stats, map and inventory" | `sidePanel` | boolean | on | Allows the side panel on wide windows |
| Button "Reset map zoom" | `tileDp` | decimal | 0 | Sets the stored tile size to 0 (automatic) at once and toasts "Zoom reset". The same key is written whenever the user pinch-zooms the map. |
| Note: "Pinch the map to zoom in or out; the view follows your character." | | | | |

**"Keyboard"**

| Control | Key | Type | Default | Effect |
|---|---|---|---|---|
| "Arrow keys / numpad move (hardware keyboard)" | `arrows` | boolean | on | 3.5 |

**"Wild features (new games start with these; a saved game keeps its own)"** — see 7.5
for the twelve feature checkboxes, followed by:

| Control | Key | Type | Default | Effect (Part 13) |
|---|---|---|---|---|
| "Card drop toasts: show each card as it drops" | `cardToasts` | boolean | on | Pop-up when a card drops |
| "Reduce glitch effects (no shaking or colour cycling)" | `reduceGlitch` | boolean | off | Calms the Glitch Realm rendering |
| "Reduce weather effects (no drifting rain or snow, no lightning flashes)" | `reduceWeather` | boolean | off | Calms the weather rendering (the day/night tint stays) |
| "Auto-photo on death / victory" | `autoPhoto` | boolean | on | Photo mode snaps by itself at the end of a game |

**"Danger cues (low HP)"** — section 11.

**"Statistics"**

| Control | Key | Type | Default | Effect |
|---|---|---|---|---|
| "Show a message when an achievement unlocks" | `achToasts` | boolean | on | Enables the unlock toast (10.5) |

**"Accessibility"** — section 12.

### 7.3 Preferences without a control in the dialog

| Key | Type | Default | Meaning |
|---|---|---|---|
| `difficulty` | integer | 0 | Last difficulty chosen in the new-game dialog; clamped to 0..10 on both read and write |
| `wildFlags`, `wildKnown` | integers | see 7.5 | Wild feature mask and the set of bits known when it was saved |
| `radioMusic` | integer 0..100 | 60 | Radio music volume (set in the Radio dialog, Part 13) |
| `radioSfx` | integer 0..100 | 80 | Radio sound-effects volume |

### 7.4 Difficulty handling (summary)

- The stored value is always read and written clamped to 0..10.
- Difficulty 0 passes no argument to the game; k ≥ 1 passes `-d k`, with any larger
  request capped at 10 (a request of 99 yields `-d 10`).
- It applies to new games only; a continued or resumed game keeps the difficulty stored
  in its save.
- The description text is that of 6.1; for k = 6 it reads "Monsters: 200% hit points,
  240% damage, armour class -6. They drop 63% gold and give 54% experience."

### 7.5 The Wild feature mask

Twelve feature bits, one checkbox each, shown in bit order:

| Bit value | Checkbox label |
|---|---|
| 1 | "Dungeon Radio: chiptune music and sound effects" |
| 2 | "Monster trading cards drop from kills" |
| 4 | "Vending machines, junk food, the rubber duck and the Roomba" |
| 8 | "Hats" |
| 16 | "LarnCoin at the bank (and the Tax Auditor)" |
| 32 | "The Glitch Realm (scroll of segfault)" |
| 64 | "Fishing at fountains" |
| 128 | "Mysterious eggs hatch into pets" |
| 256 | "Choose a perk on every level-up" |
| 512 | "Weather and day/night on the surface" |
| 1024 | "Polaroid photo mode" |
| 2048 | "Town bounty board: WANTED posters with gold rewards" |

All twelve default to on (mask 4095). The mask is handed to each newly launched game
process in `ULARN_WILD`; the game stores it with a new character and a restored game
keeps the mask it was created with (Part 12). The app-side features (radio, cards, photo
menu entries) follow the current Settings mask.

Forward-compatibility rule for newly added features: alongside the mask the app stores
which bits existed when the mask was last written (`wildKnown`; if absent it is taken to
be the first eleven bits, 2047). When reading, any bit that is valid now but was not
"known" then is forced on, so a feature added in a later version starts ticked rather than
appearing as unticked. Effective mask = (stored mask, default 4095, OR-ed with the valid
bits not in the known set), limited to the twelve valid bits. Every write stores the mask
limited to the valid bits and sets the known set to all twelve.

## 8. Save slots

### 8.1 Model

- Up to **10** slots, numbered 1 to 10, each a folder (section 2).
- A slot is **empty** when it has neither a save file nor a checkpoint file (metadata or
  an options file alone do not make it non-empty).
- **Slot title**: the metadata's custom label if non-blank; otherwise the character name;
  otherwise "Unknown character".
- **Slots listed** on the start screen: slots 1 to N where N = max(3, highest non-empty
  slot number + 1), capped at 10. So there are always at least three cards, and one empty
  slot after the last used one until all ten are used.
- **First empty slot**: the lowest-numbered empty slot among 1..10.

**Metadata file `slot.json`** (pretty-printed JSON, 2-space indent, written to a temporary
sibling file, synced, then renamed over the target so a crash never leaves half a file):

| Field | Type | How it is obtained |
|---|---|---|
| `label` | text or null | Set by Rename (trimmed, at most 40 characters, blank becomes null). A new game clears it. |
| `character` | text or null | The cleaned name from the new-game dialog; for a migrated legacy save, the Settings name. |
| `female` | boolean | The new-game dialog's checkbox; for a migrated save, the Settings value. |
| `level` | integer, −1 = unknown | The character's experience level read from the shared state when progress is recorded (8.4). |
| `levelTitle` | text or null | The level title string from the shared state (trimmed; null if empty). Stored but not shown anywhere in the slot UI. |
| `depth` | text or null | The game's level name from the shared state: "H" for the surface, "1".."15" for the dungeon, "V1".."V5" for the volcano (and "?" while the game hides the level number). |
| `lastPlayed` | integer, milliseconds since the epoch, 0 = never | Set to now when a new game is created, whenever the slot is started, and whenever progress is recorded; for a migrated save, the newest file's modification time. |

When reading, text fields that are null or empty become "absent", text is truncated to 80
characters (depth to 8), and a missing or unparsable file means "no metadata".

Depth description used in the UI: absent or blank gives nothing; "H" gives "Home"; a value
starting with "V" gives "Volcano level " plus the rest; anything else gives
"Dungeon level " plus the value.

### 8.2 Slot cards

Each card is a clickable, focusable vertical block: padding 12 dp horizontal, 8 dp
vertical; minimum height 48 dp; rounded background with 12 dp corners and a 1 dp outline:

| State | Fill (ARGB) | Outline |
|---|---|---|
| Pressed | (240, 80, 70, 40) | RGB (255, 190, 60) |
| Focused | (230, 45, 47, 55) | RGB (255, 190, 60) |
| Normal | (220, 45, 47, 55) | ARGB (70, 255, 255, 255) |

Text lines:

| Line | Empty slot | Slot with a game |
|---|---|---|
| 1 (15 sp) | "Slot N · Empty", light grey, regular weight | "Slot N · <title>", white, bold |
| 2 (13 sp, RGB 210,210,210) | "Importing…" while an import into it runs, else "Tap to start a new game or import one" | Parts joined by " · ": the character name (only if a label is set, a character name exists and they differ); "Level L" or "Level ?" when unknown; the depth description if any |
| 3 (12 sp, RGB 190,190,190) | none | State, then optionally " · last played <when>". State is "Exporting…" while busy, else "Saved game" if a save file exists, else "Checkpoint". <when> is the last-played time formatted by the platform in the device locale with date, time and abbreviated month; omitted when the time is 0. |

For screen readers the card is one item whose description is its lines joined by ". ".

### 8.3 Tapping a slot: the action dialog

Tapping re-reads the slot. If a game is running the tap is refused with the toast "Leave
the current game first"; if an export or import is running, with "Slot N is busy — please
wait" (for that slot) or "Please wait for the current export or import" (for others).
These slot toasts are long-duration.

The dialog is a list with a "Cancel" button. Title "Slot N (empty)" or "Slot N: <title>".

| Slot state | Item | Action |
|---|---|---|
| Empty | "New game" | The new-game dialog (6.1) |
| Empty | "Import a saved game into this slot" | Import into this slot (8.7) |
| Has a game | "Continue saved game" (if a save file exists) or "Resume from checkpoint" (otherwise) | Play the slot |
| Has a game | "New game in this slot…" | Confirmation: message "Start a new game in slot N? <title>'s game will be deleted.", buttons "Delete & start" and "Cancel". Confirming opens the new-game dialog; the old game is only deleted when "Start" is pressed there. |
| Has a game | "Rename…" | 8.5 |
| Has a game | "Export to a file…" | 8.6 |
| Has a game | "Delete…" | Confirmation: message "Delete slot N (<title>)? Its saved game and checkpoint will be permanently deleted.", buttons "Delete" and "Cancel". Confirming deletes the save, checkpoint, metadata and options file, then the folder if it is now empty; toast "Slot N deleted"; the start panel is rebuilt. |

**Playing a slot** (also used by the quick-start button): re-check the files. If a save
file exists, start the slot with no arguments, even if a checkpoint also exists (the save
wins, as in the game itself; restoring the checkpoint would leave a stale save that would
win next time). Otherwise, if a checkpoint exists, start with `-r`. If neither exists any
more: toast "Slot N has no game any more" and rebuild the panel.

### 8.4 Recording progress

Done when the app goes to the background with a game running and whenever a game process
exits, for the active slot only (never for slot-less runs):

1. Take a fresh snapshot of the shared state (give up if it is not initialised).
2. Read the slot's metadata (or assume Settings name and gender).
3. If the state shows a real character (class number ≥ 0 and maximum HP > 0): set level,
   level title, depth and last played = now. Otherwise only set last played = now.
4. Write the metadata, but only if the slot folder still exists.

### 8.5 Rename

Dialog title "Rename slot N", message "Leave empty to show the character's name.", a
single-line field (hint and spoken name "Slot name") pre-filled with the current label, or
the character name, with the cursor at the end; buttons "Rename" and "Cancel". On Rename
the trimmed text becomes the label, except that text equal to the character name clears
the label. Failure toasts "Couldn't rename: <error text>". The panel is rebuilt either
way.

### 8.6 Export

1. Refused under the same conditions and toasts as 8.3.
2. The slot number is remembered in the `saves` preferences (key `pendingExport`), and
   the system "create document" picker is opened for an openable document of type
   `application/zip` with the suggested name "ularn-slot<N>-<who>.zip". <who> is the slot
   title with every run of characters other than ASCII letters, digits, underscore and
   hyphen replaced by one underscore, leading and trailing underscores removed, cut to 24
   characters, or "game" if nothing is left. If no picker exists: toast "No file picker
   available".
3. When the picker returns a location: if the remembered slot is not 1..10 or is now
   empty, toast "Nothing to export". Otherwise mark the slot busy, rebuild the panel (the
   card shows "Exporting…"), and write the archive on a background thread.
4. On success: toast "Slot N exported". On failure: the half-written document is deleted
   if possible and the toast is "Export failed: <error text>". Either way the busy mark
   is cleared and the panel rebuilt. A cancelled picker does nothing.

**Archive format**: a standard zip containing, in this order:

1. `ularn-slot.json`, the manifest: pretty-printed JSON with `format` = the text
   "ularn-android-slot"; `version` = 1; `exported` = current time in milliseconds;
   `abi` = the device's primary CPU ABI name (or empty); `is64` = whether the app process
   is 64-bit; `meta` = the slot's metadata object (8.1; defaults if the slot has none).
2. `ularn_player.sav`, if the slot has one, with the entry time set to the file's
   modification time.
3. `ularn_player.ckp`, likewise.

The `is64` flag exists because the game writes native-width integers into its save, so
32-bit and 64-bit builds cannot read each other's saves.

### 8.7 Import

Entry points: the start-screen button (target "first empty slot") and an empty slot's
action item (target that slot).

1. Refused with "Leave the current game first" if a game is running, with "Please wait
   for the current export or import" if one is in progress, and (for the start-screen
   button) with "No empty slot: delete a slot first" if all ten are used.
2. The requested target (0 = first empty) is remembered in the `saves` preferences (key
   `pendingImport`), and the system "open document" picker is opened for any openable
   file, hinting the types `application/zip`, `application/x-zip-compressed` and
   `application/octet-stream`. No picker: "No file picker available".
3. When a file is chosen: re-check step 1's first two conditions. The actual target is
   the requested slot if it is 1..10 and still empty, otherwise the first empty slot; if
   there is none, "No empty slot: delete a slot first". Mark the target busy (its card
   shows "Importing…") and read the file on a background thread.
4. On success: toast "Imported into slot T". On failure: a dialog titled "Import failed"
   with the reason as its message and an "OK" button. The busy mark is cleared and the
   panel rebuilt.

Validation and unpacking, in order (each failure leaves the target untouched; reasons are
the exact messages):

1. Target not empty: "Slot T isn't empty".
2. Create a staging folder under `slots/`. Read the zip entry by entry, skipping
   directory entries:
   - a name other than exactly `ularn-slot.json`, `ularn_player.sav` or
     `ularn_player.ckp`: "Unexpected file in the archive: <first 40 characters of the
     name>";
   - a name seen twice: "Duplicate entry: <name>";
   - the manifest is read with a 64 KiB cap and parsed as JSON; unparsable: "The manifest
     is damaged";
   - game files are copied into staging with an 8 MiB cap each, and given the entry's
     timestamp as modification time;
   - exceeding either cap: "A file in the archive is too large to be a saved game";
   - any other read error: "Not a readable zip file".
3. No manifest: "This isn't an Ularn save export (no ularn-slot.json)". (A file that is
   not a zip at all normally yields no entries and so ends here.)
4. Manifest `format` is not "ularn-android-slot": "This isn't an Ularn save export".
5. Manifest `version` greater than 1: "This export is from a newer version of the app".
6. Manifest has `is64` and it differs from this process: "This save comes from a 64-bit
   device and can't be loaded on this one" (or "32-bit").
7. Neither game file present: "The export contains no saved game".
8. Either present game file has zero length: "The saved game in the export is empty".
9. Write `slot.json` into staging from the manifest's `meta` (defaults if missing or
   bad). The exported "last played" time is kept, so an imported old game does not jump
   to the top as most recent.
10. Commit: check again that the target is empty ("Slot T isn't empty"). If the target
    folder exists, delete its `slot.json` and `ularn.opt` and then the folder; if the
    folder still exists (something unexpected inside), move the staged save, checkpoint
    and metadata into it one by one. Otherwise rename the staging folder to the slot
    folder.
11. Any other failure (including being unable to write the slot) is reported as
    "couldn't read the file (<internal error class name>)".

### 8.8 Migrating a pre-slot save

Older versions kept one game directly in the working directory. At every start-up, before
the slots are read:

1. Look for `ularn_player.sav` and `ularn_player.ckp` in `ularn163/`, and for a leftover
   staging folder `slots/.legacy/`. If none of these exist, there is nothing to do.
2. Move each legacy file into the staging folder (skip one whose name is already staged;
   leave a file in place if it cannot be moved). "Move" always means: try a rename; if
   that fails, copy to a temporary name, sync, verify the copy is byte-for-byte identical,
   rename it into place and only then delete the original. Nothing is ever overwritten.
3. If the staging folder holds no game file, remove it and stop.
4. Pick the first empty slot (slot 1 on a normal upgrade). If there is none, leave the
   game in the staging folder and stop (it is retried at the next launch).
5. If the staging folder has no metadata file, write one: character = the Settings name,
   female = the Settings gender, last played = the newest staged file's modification
   time.
6. If the slot folder does not exist, rename the staging folder to it (one atomic step).
   Otherwise move the save, checkpoint and metadata in one by one, never overwriting an
   existing game file (existing metadata is replaced), then remove the staging folder.

Migration failures are never fatal; the old files stay where they were.

### 8.9 Old-version saves

The app shell does **not** inspect save files: the slot list judges a slot only by the
presence of the two files, so a save written by an incompatible version looks like any
other ("Saved game" / "Checkpoint"). (The project's planning notes mention showing such a
slot as "(old version)"; that was not implemented.) Recognition happens in the game when
it loads the file: a save without the Wild Edition header or with a different save
version is deleted and the game prints "That saved game is from a different version of
Ularn and can't be loaded." and "It has been removed. Please start a new game.", then
ends that run through its death path (Part 03). The app then returns to the start screen
as after any game end, and the slot shows as empty. The only version check the app shell
itself makes is on import archives (8.7 steps 5 and 6).

## 9. The spell picker

Triggered by an on-screen control with the "Cast" action (Part 11).

1. If no game is running, nothing happens.
2. If the game is not at its command prompt (or the state cannot be read), the button
   simply sends the key `c`, like an ordinary key button.
3. Otherwise the picker dialog opens.

**List contents.** The shared state exposes, per spell index in game order, a "known"
flag and the three-letter code (Part 09). The picker keeps the known ones, in game order,
and looks each code up in its own built-in table of 39 entries (code, display name,
one-line description; below). A code missing from the table is shown with the code as its
name and no description.

**Dialog layout.** A custom header (padding 20 dp sides, 16 dp top, 8 dp bottom):

- "Cast a spell", 20 sp bold.
- "Spell points: P / M" at 80% opacity (current and maximum spell points).
- If P is 0: "You have no spell points left. They regenerate over time." in the accent
  colour, 6 dp top padding.
- If no spells are known: "You haven't learned any spells yet. Reading books teaches
  them.", 6 dp top padding.

Then the list. Each row (padding 20 dp sides, 10 dp top/bottom): a top line with the
spell name (16 sp bold, taking the free width) and, at the right, the code (15 sp
monospace, accent colour); below it the description (13 sp, 75% opacity) if there is one.

**Disabled state.** When the character has 0 spell points, every row is drawn at 40%
opacity and cannot be selected.

Buttons: neutral "Type code…" and negative "Cancel".

**"Type code…"** opens a dialog titled "Spell code" with a single-line monospace field
(hint "e.g. pro", at most 3 characters, no suggestions) which takes focus; buttons "Cast"
and "Cancel". On Cast the text is trimmed and lower-cased; if it is exactly three letters
a to z it is cast, otherwise the toast "A spell code is three letters, e.g. pro" appears.
This path works for any code, known or not, and regardless of spell points.

**Key sequence sent.** Casting code XYZ:

1. Re-check that a game is running and at the command prompt; otherwise do nothing.
2. Send `c`.
3. Wait (polling every 60 ms, also on each state change, for at most 3 seconds) until the
   game publishes a new state. If it is then waiting in "prompt" input mode with an
   accepted-answers string that begins with the 26 lower-case letters (the game's
   spell-code prompt), send the three letters. If the game is instead back at the command
   prompt, at some other prompt, or the time runs out, the letters are **not** sent, so
   they can never be mistaken for commands (for example when the game answers `c` with a
   "no spells" message).
4. A following "In what direction?" question is answered by the user with the normal
   direction UI (Part 11).

**Built-in table** (game order):

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

These are the app's own summaries; the authoritative spell rules are in Part 07.

## 10. Statistics and achievements

### 10.1 Concepts

A **run** is one character, from a new game to its death, win or quit. The native front
end gives each character a random non-zero id that is stored inside its save and
checkpoint, and counts its kills there too (Part 09), so continuing or resuming a game
continues the same run and nothing is counted twice.

The book of statistics holds:

- the **current** run (the one last seen on screen), or none;
- **parked** runs: unfinished runs other than the current one (games waiting in other
  slots), by run id;
- **lifetime** totals;
- **recent**: finished runs, newest first, at most 30;
- **unlocked**: achievement id mapped to unlock time.

Per-run record: id; started-at time; ended-at time; class number (−1 unknown); difficulty;
deepest dungeon level (1..15, 0 = never left the surface); deepest volcano level (1..5,
0 = never entered); highest character level; most gold carried; turns; kills; play time in
milliseconds; outcome (in progress, died, won, quit); cause text; score; "was near death"
flag; "recovered from the brink" flag.

Lifetime record: games played, wins, deaths, quits; best dungeon level, best volcano
level, best character level, most gold, best score; total kills, total turns and total
play time over **finished** runs; fastest win in turns (0 = none); a map from normalised
cause of death to count, in first-seen order.

Class names by class number 0..7: Ogre, Wizard, Klingon, Elf, Rogue, Adventurer, Dwarf,
Rambo; anything else displays as "?".

### 10.2 What is observed each frame

While a game is running, every drawn frame of the game view feeds one observation built
from the shared state snapshot:

| Observation field | Source in the shared state |
|---|---|
| run id | the character's run id (0 until the first command prompt, and for scoreboard runs) |
| HP, max HP | current and maximum hit points |
| level index | 0 surface, 1..15 dungeon, 16..20 volcano V1..V5 |
| level known | false when the game's level name is "?" |
| character level, gold | as shown |
| turns | the game's elapsed-time counter |
| kills | the front end's kill counter for this character |
| class number, difficulty | as stored by the game |
| end state, end score, end reason | set by the front end when the final score page appears: 0 none, 1 died, 2 won, 3 quit; the score; and the reason text from that page, e.g. "killed by a giant ant on 3" (Part 09 describes how the page is recognised: a reason beginning "a winner" means won, beginning "quit" means quit, anything else died) |

### 10.3 Bookkeeping rules

For each observation (with the current wall-clock time):

1. Run id 0: ignore entirely.
2. If there is no current run or its id differs:
   1. If a run with this id is already in the recent (finished) list, ignore the
      observation: a finished run never comes back.
   2. If the current run is unfinished, park it.
   3. If this id is parked, un-park it and make it current. Otherwise create a new run
      (started now), make it current and add 1 to lifetime "games played".
   4. While more than 10 runs are parked (10 matches the number of slots), take the
      parked run with the earliest start time, remove it and finish it as "quit" with
      cause "abandoned" and score 0.
   5. Flag the book for immediate saving.
3. If the current run is already finished, stop.
4. If max HP > 0 (a real character is on screen):
   - class number is taken if ≥ 0; difficulty is taken;
   - if the level is known: index 1..15 raises the deepest dungeon level; index above 15
     raises the deepest volcano level to (index − 15);
   - highest character level and most gold are raised to at least the observed values;
   - turns and kills are **set** to the observed values (they are the game's own
     counters, so after resuming an older checkpoint they go back and the replayed part is
     not double-counted);
   - if HP × 10 < max HP, set "was near death"; otherwise, if that flag is set and
     HP ≥ max HP, set "recovered from the brink";
   - raise the four lifetime bests (dungeon, volcano, character level, gold);
   - mark the book dirty.
5. If the end state is died, won or quit, finish the run with that outcome, the end
   reason as cause and the end score.
6. Check achievements (10.5) against the run and the lifetime record and report any newly
   unlocked.

**Finishing a run**: set outcome, cause, score and ended-at. Died: lifetime deaths + 1 and
the normalised cause's count + 1. Won: wins + 1, and the fastest win becomes this run's
turns if there was none or this is lower. Quit: quits + 1. In every case: best score is
raised to the score; total kills, turns and play time grow by the run's values; the run is
inserted at the front of the recent list, which is trimmed to 30; flag for immediate
saving.

**Normalised cause of death**: remove any " (wizard)" or " (cheater)" tag, trim, remove a
trailing " on H", " on <digits>" or " on V<digit>", trim; an empty result becomes
"unknown". Example: "killed by a bat on H (wizard)" becomes "killed by a bat".

**Play time**: the tracker measures the real time between consecutive drawn frames, capped
at 30 seconds per gap (a longer gap means the app was away; the game view redraws at
least about twice a second for its cursor blink). The gap is added to the current run only
when the observed run id is non-zero and equals the current run's id and that run is
unfinished. The clock restarts (next gap counts as 0) after a flush and after a game exit.

**On game-process exit**: first one more observation is taken from the final shared state
(to catch a final score shown while the app was not drawing). Then, if the current run is
unfinished and the game is **not** resumable (no save and no checkpoint in the game's home
directory), the current run is finished as "quit" with cause "quit" and score 0. Then the
book is saved.

Quirk: this last rule looks at "the current run in the book", not at the run of the
process that just exited. A process that never showed a character (the scoreboard, or a
new game abandoned on the class-selection screen before any checkpoint exists) therefore
closes the previously current unfinished run as a quit, although that run's save may
still exist in a slot; because a finished run is never reopened, later play of that
character is no longer tracked.

**Saving**: after an observation the book is written at once if flagged for immediate
saving (new run, run finished, achievement unlocked), or if it is merely dirty and more
than 15 seconds have passed since the last write. It is also written on game exit and,
if dirty, when the app goes to the background.

### 10.4 Storage format

`stats.json`: one compact JSON object, written to `stats.json.tmp` and renamed into place.

| Key | Content |
|---|---|
| `current` | a run object (omitted when there is none) |
| `parked` | array of run objects |
| `recent` | array of run objects, newest first |
| `lifetime` | object with `gamesPlayed`, `wins`, `deaths`, `quits`, `bestDungeon`, `bestVolcano`, `bestPlevel`, `bestGold`, `bestScore`, `totalKills`, `totalTurns`, `totalPlayMs`, `fastestWinTurns`, and `deathCauses` (object: normalised cause to count) |
| `achievements` | object: achievement id to unlock time in milliseconds |

Run object keys: `id`, `startedAt`, `endedAt`, `class`, `difficulty`, `maxDungeon`,
`maxVolcano`, `maxPlevel`, `maxGold`, `turns`, `kills`, `playMs`, `outcome` (one of the
texts IN_PROGRESS, DIED, WON, QUIT; anything unrecognised reads as in progress), `cause`,
`score`, `nearDeath`, `brink`. Missing numeric fields read as 0 (class as −1). An
unreadable file yields an empty book.

### 10.5 Achievements

Seventeen, checked in this order after every accepted observation; each unlocks once and
records the time. "Run" conditions refer to the current run's record.

| # | Id | Name | Description shown | Unlock condition |
|---|---|---|---|---|
| 1 | first_blood | First Blood | Kill a monster. | run kills ≥ 1 |
| 2 | slayer | Slayer | Kill 100 monsters in one game. | run kills ≥ 100 |
| 3 | delver | Delver | Reach dungeon level 5. | run deepest dungeon ≥ 5 |
| 4 | deep_delver | Deep Delver | Reach dungeon level 10. | run deepest dungeon ≥ 10 |
| 5 | rock_bottom | Rock Bottom | Reach the bottom of the dungeon (level 15). | run deepest dungeon ≥ 15 |
| 6 | volcano | Into the Fire | Enter the volcano. | run deepest volcano ≥ 1 |
| 7 | volcano_deep | Heart of the Volcano | Reach volcano level V5. | run deepest volcano ≥ 5 |
| 8 | seasoned | Seasoned | Reach character level 10. | run highest character level ≥ 10 |
| 9 | veteran | Veteran | Reach character level 20. | run highest character level ≥ 20 |
| 10 | hoarder | Hoarder | Carry 10,000 gold. | run most gold ≥ 10,000 |
| 11 | dragon_hoard | Dragon's Hoard | Carry 100,000 gold. | run most gold ≥ 100,000 |
| 12 | brink | Back from the Brink | Fall below 10% of your HP, then recover to full health. | run "recovered from the brink" flag |
| 13 | survivor | Survivor | Last 10,000 turns in one game. | run turns ≥ 10,000 |
| 14 | persistent | Persistent | Start 10 games. | lifetime games played ≥ 10 |
| 15 | saviour | Saviour | Win the game. | run outcome is won |
| 16 | against_clock | Against the Clock | Win in under 20,000 turns. | run outcome is won and run turns < 20,000 |
| 17 | glutton | Glutton for Punishment | Win at difficulty 1 or higher. | run outcome is won and run difficulty ≥ 1 |

(The number in description 16 is formatted with the device locale's digit grouping.)

When one or more unlock and the setting "Show a message when an achievement unlocks" is
on, a long toast shows one line per achievement: "Achievement unlocked: <name>".

### 10.6 The "Statistics & achievements" dialog

Title "Statistics & achievements"; one "OK" button; scrolling body (padding 20 dp sides,
4 dp top, 8 dp bottom). Building blocks: section headers are 16 sp bold in the accent
colour (14 dp top, 4 dp bottom padding); "rows" are a label at 80% opacity on the left and
a bold right-aligned value; "notes" are 13 sp lines with 2 dp vertical padding, optionally
dimmed to 75%.

Numbers are formatted with digit grouping. Durations are whole minutes: under an hour
"Nm", otherwise "Hh Mm".

1. **"Lifetime"** rows, in order: "Games played"; "Wins"; "Deaths"; "Quit / abandoned";
   "Deepest dungeon level" (number, or "-" if 0); "Deepest volcano level" ("V" + number,
   or "-"); "Highest character level" (or "-"); "Most gold carried"; "Best score";
   "Fastest win" ("N turns"; row present only once a win exists); "Monsters killed";
   "Turns played"; "Time played". The last three add the unfinished current run's kills,
   turns and play time to the finished-run totals.
2. **"Current game"** (only when the current run is unfinished): one undimmed note with
   the run summary (below).
3. **"Last photo"** (the photo hook; only when `photos/last.png` exists and decodes): the
   picture, full dialog width with its aspect ratio kept, spoken as "Your last photo". It
   is loaded down-sampled by the largest power of two that keeps its width at least the
   screen width. Photo mode writes this file (Part 13).
4. **"Achievements (U/17)"**, U = number unlocked. One block per achievement in table
   order, 4 dp vertical padding. Unlocked: bold "✓ <name>" in green RGB (110, 220, 120),
   then 13 sp "<description>  Unlocked <date>." (two spaces before "Unlocked"; the date in
   the locale's medium date style). Locked: the whole block at 40% opacity, bold
   "<name>  (locked)" (two spaces), then the description.
5. **"Recent games"**: the dimmed note "No finished games yet." if there are none;
   otherwise an undimmed summary note for each of the 15 newest finished runs.
6. **"Causes of death"** (only if any): up to 10 rows, most frequent first, the cause
   with its first letter capitalised and the count.

**Run summary text** (two lines):

- Line 1: class name; then " (difficulty D)" if D > 0; then " - "; then "in progress",
  "won!", the raw cause text (or "died" if empty) for a death, or for a quit "abandoned"
  if that is the cause and otherwise "quit".
- Line 2 (indented three spaces): "Deepest level N" or "Deepest Home" if the dungeon was
  never entered; ", VN" if the volcano was; then " · char lv L · T turns · K kills ·
  G gold max · <duration>"; and " · score S" if the score is above 0.

Example: "Elf (difficulty 2) - killed by a giant ant on 3" / "   Deepest level 3 · char
lv 4 · 1,234 turns · 12 kills · 500 gold max · 12m · score 1,234".

## 11. Danger-cue settings

The settings dialog section "Danger cues (low HP)" controls the low-health warnings that
the game view renders (the rendering, timing and vibration patterns are Part 11). They
are pushed to the game view when Settings is confirmed with "OK" and at start-up.

| Control | Key | Type | Default | What it controls |
|---|---|---|---|---|
| Text "Warn when HP is below" followed by a drop-down with the entries "10% of max HP", "15% of max HP", "20% of max HP", "25% of max HP", "30% of max HP", "35% of max HP", "40% of max HP", "50% of max HP" | `dangerPct` | integer percent | 25 | The threshold below which the low-HP cues are active. A stored value not in the list is shown, and immediately re-stored, as 25. |
| "Pulse the HP bar red" | `dangerPulse` | boolean | on | The HP bar pulses red while below the threshold |
| "Red vignette around the map" | `dangerVignette` | boolean | on | A red glow around the map edge while below the threshold |
| "Vibrate when HP falls below it" | `dangerVibrate` | boolean | on | One vibration at the moment HP crosses below the threshold |
| "Flash red on a big hit (20% of max HP or more)" | `bigHitCue` | boolean | on | A red flash (and a short buzz) when a single hit removes 20% of max HP or more |
| Note: "Vibration also needs "Vibrate on press" (Edit controls → Look) to be on." | | | | Both vibrations are additionally gated by the control layout's haptics switch (3.9) |

## 12. Accessibility

### 12.1 Settings

The "Accessibility" header is marked as a heading for screen readers. Every control here
applies its change immediately, without waiting for "OK".

| Control | Key | Type | Default |
|---|---|---|---|
| Label "Game text size: N%" above a slider with 14 positions (0..13); value = 0.85 + position/20, i.e. 85% to 150% in 5% steps. The slider is spoken as "Game text size" with the percentage as its state. Note below: "HUD, messages, text pages and button labels." | `a11yTextScale` | decimal | 1.0 (clamped to 0.85..1.5 on read and write) |
| "Also follow the system font size" | `a11yFollowFont` | boolean | on |
| "High contrast (solid outlined HUD and buttons, white text, underlined highlights)" | `a11yHighContrast` | boolean | off |
| "Announce new game messages (TalkBack)" | `a11yAnnounce` | boolean | off |

### 12.2 Text scale

Effective scale = (app text scale) × (system factor), clamped to 0.75..1.9, where the
system factor is the system font scale clamped to 0.85..1.3 if "Also follow the system
font size" is on, and 1 otherwise. (The clamps exist because the HUD and the 80-column
text pages are fixed layouts and the largest system sizes would leave no room for the
map.) The effective scale is given to the game view (HUD, messages, text pages, answer
buttons) and to the controls overlay (button and D-pad labels). It is recomputed whenever
the accessibility settings change, when Settings is confirmed, at start-up and on every
configuration change (so a change of the system font size takes effect at once).

### 12.3 High contrast

A single on/off flag given to the game view and the controls overlay. What changes (the
normal colours are in Part 11):

- Game view: the background and all panels become pure black; normal and dimmed text
  become pure white; text the game marks as highlighted (its three "standout" styles,
  normally red, green and blue) is drawn white and **underlined**, so no information is
  carried by colour alone; the gold amount and the coin ticker are white; the HP bar fill
  is white inside a 2 dp white outline (the danger pulse still tints it); answer buttons
  are black with a 2 dp white outline and white labels.
- Controls overlay: overall opacity is raised to at least 92%; buttons and D-pads are
  filled black with a white outline 2 dp wide (4 dp for the run-modifier button while it
  is armed or locked) and white labels; a pressed control, or the locked run button,
  inverts to a black outline and black label on its lit (accent) fill; D-pad segment marks
  use white and dark grey.

### 12.4 Announcing game messages

When "Announce new game messages" is on and an accessibility service is running, new text
in the game's message area is spoken through the platform's announcement mechanism, as
follows.

1. Every display-change notification (re)starts a 350 ms timer; the check below runs only
   when 350 ms pass without a further change, so a burst of updates within one turn is
   read once.
2. The check does nothing unless the game is showing the map (text pages are not
   announced) and the state snapshot is valid.
3. Read the message area: each of its rows, with unused cells treated as spaces, trimmed;
   empty rows are dropped. Call this list "current"; "previous" is the list from the last
   check.
4. Work out the new lines:
   - if previous is empty, all current lines are new;
   - otherwise, for k from min(size of previous, size of current) down to 1: test whether
     the last k lines of previous line up with the first k lines of current, where the
     first k−1 must be identical and the k-th current line must **begin with** the last
     previous line (the message area scrolls upward, and the bottom line may have grown,
     e.g. "You hit the bat." becoming "You hit the bat.  The bat dies."). On the first k
     that fits, the new text is the grown part of that line (if any, trimmed) followed by
     all current lines after it;
   - if no k fits, all current lines are new.
5. Current becomes previous. Join the new lines with spaces, collapse runs of whitespace
   and trim. If nothing remains, stop.
6. De-duplication: if the text equals the last spoken text and less than 1.5 seconds have
   passed since it was spoken, skip it (a redraw, not a new message). Otherwise speak it
   and remember it and the time.

The history is cleared when a new game starts, and whenever the enabled flag is
(re)applied, which happens on every accessibility-settings change, every Settings "OK" and
every configuration change; after a clear, the next check reads all visible message lines
(subject to rule 6).

### 12.5 TalkBack labels for the on-screen controls

The controls overlay draws its buttons itself, so it exposes them to accessibility
services as virtual items. The overlay as a whole is described as "Game controls". In
control-edit mode no items are exposed.

**Items.** For each control, in layout order (index 0 first, later ones on top):

- A button is one item with the button's rectangle. Its item id is index × 16 + 15.
- A D-pad yields one item per direction zone that has keys assigned. Zones are numbered
  0..8 row by row from the north-west (0 NW, 1 N, 2 NE, 3 W, 4 centre, 5 E, 6 SW, 7 S,
  8 SE); the four diagonal zones are omitted for a 4-way pad. Item id = index × 16 + zone.
  With r = half the smaller side of the pad's rectangle, the centre zone's bounds are the
  square of half-size 0.3 r at the pad centre; each other zone's bounds are a square of
  half-size 0.3 r centred 0.62 r from the pad centre in that zone's compass direction.

Every item is presented as an enabled, clickable, focusable button with its label as the
description.

**Labels.**

| Control | Spoken label |
|---|---|
| Keyboard-action button | "Show or hide keyboard" |
| Menu-action button | "Menu" |
| Edit-action button | "Edit controls" |
| Run-modifier button | "Run modifier", with the state "on" or "off" (as a separate state description on API 30+, otherwise appended: "Run modifier, on") |
| Cast-action button | "Cast a spell" |
| Photo-action button | "Take a photo" |
| Context-action button | the live context label (Part 09), else the button's own label, else "Action"; followed by ", unavailable" when the action is currently disabled |
| Key-sending button | (1) if it sends exactly one key with a known meaning (tables below), that meaning; (2) otherwise the button's visible label if it has one, with "⌨" spoken as "Keyboard", "☰" as "Menu" and "?" as "Help"; (3) otherwise a spelled-out key list (each key separated by a space; special keys as "Esc", "Enter", "Space", "Tab", "Bksp", "Ctrl+X"); (4) "Button" if it sends nothing |
| D-pad zone | the meaning of the zone's single key if known, else the spelled-out key list. While the run modifier is armed or locked, every non-centre zone is described by its run variant (a single lower-case letter upper-cased). |

Movement keys (either case): k north, j south, h west, l east, y north-west, u north-east,
b south-west, n south-east. A lower-case letter is spoken "Move <direction>"; an
upper-case letter, or any of them while the run modifier is on, "Run <direction>".

Other single keys:

| Key | Label | Key | Label |
|---|---|---|---|
| `.` | Rest | `^` | Identify trap |
| `i` | Inventory | `g` | Pack weight |
| `c` | Cast spell | `P` | Show taxes |
| `q` | Quaff potion | `S` | Save game |
| `r` | Read scroll | `?` | Help |
| `w` | Wield weapon | `\` , `,` and `D` | List known spells and items |
| `W` | Wear armor | `v` | Version |
| `T` | Take off armor | `Q` | Quit game |
| `d` | Drop | Escape | Escape |
| `e` | Eat cookie | Enter | Enter |
| `o` | Open door | Space | Space |
| `C` | Close door | Delete (code 127) | Backspace |
| `O` | Open chest | `Z` | Teleport |

(The movement table is consulted first, so `H`, `J`, `K`, `L`, `Y`, `U`, `B`, `N` are
"Run …".)

**Explore by touch.** Only when touch exploration is active, hover events over the
overlay are mapped to items with the same hit rules as touches: controls are tested from
the topmost down; a button matches if the point is inside its rectangle; a D-pad matches
if the point is inside its circle, and the zone is chosen as for a touch (within 0.28 r of
the centre is the centre zone; a 4-way pad picks the dominant axis; an 8-way pad uses 45°
sectors). If the chosen zone has no item, nothing is reported. Hovers that hit no control
pass through to the game view beneath. Hover-enter and hover-exit events are sent as the
finger moves between items.

**Activation.** A double-tap (the accessibility click action) presses the item: a D-pad
zone sends its keys once (applying and, if merely armed, consuming the run modifier); a
key button sends its keys; the run button toggles (if armed or locked it turns off,
otherwise it becomes armed); any other action button performs its action. A haptic tick
accompanies key sends and the run toggle. Afterwards the item list is refreshed without
dropping accessibility focus, so the changed run state and directions are re-read. The
list is refreshed with focus cleared when the layout changes or edit mode toggles.

### 12.6 Other accessibility affordances

- The game view is described as "Game screen".
- The start-screen title is spoken "Ularn" and is a heading; "Save slots" is a heading;
  each slot card is a single item reading all its lines.
- The new-game dialog's name field and difficulty slider are linked to their labels; the
  slider speaks "Level k".
- The rename field is spoken "Slot name"; the "⇅" toolbar button "Move toolbar to the
  other edge".

## 13. Index of dialogs

| Dialog | Title | Content | Buttons | Section |
|---|---|---|---|---|
| In-game menu | none | list of 8 to 11 entries | none | 5 |
| Leave confirmation | none | message | "Leave", "Cancel" | 5 |
| Settings | "Settings" | scrolling form | "OK" | 7 |
| New game | "New game in slot N" | name, gender, difficulty | "Start", "Cancel" | 6.1 |
| Slot actions | "Slot N (empty)" / "Slot N: <title>" | list of 2 or 5 actions | "Cancel" | 8.3 |
| Replace-game confirmation | none | message | "Delete & start", "Cancel" | 8.3 |
| Delete-slot confirmation | none | message | "Delete", "Cancel" | 8.3 |
| Rename | "Rename slot N" | message and field | "Rename", "Cancel" | 8.5 |
| Import failed | "Import failed" | reason | "OK" | 8.7 |
| Spell picker | custom header "Cast a spell" | list of known spells | "Type code…" (neutral), "Cancel" | 9 |
| Spell code | "Spell code" | 3-letter field | "Cast", "Cancel" | 9 |
| Statistics | "Statistics & achievements" | scrolling report | "OK" | 10.6 |
| Controls appearance | "Controls appearance" | opacity slider, vibrate checkbox | "OK" | 3.9 |
| Layout profiles | "Layout profiles" | list | none | 3.9 |
| Save profile | "Save profile" | name field | "Save", "Cancel" | 3.9 |
| Delete profile | "Delete profile" | list of names | none | 3.9 |
| Reset layout | none | message | "Reset", "Cancel" | 3.9 |
| Control properties | | | | Part 11 |
| Card album, Radio, photo dialogs | | | | Part 13 |

## 14. Index of toasts

Short unless noted.

| Text | When |
|---|---|
| "Couldn't start the game" | the game process could not be launched |
| "Zoom reset" | Settings "Reset map zoom" |
| "Drag to move · corner handle to resize · tap a selected control to edit it" (long) | entering control-edit mode |
| `Saved "<name>"`, "Layout copied", "Clipboard doesn't contain a layout", "Deleted", "Loaded — tap Done to keep it", "Pasted — tap Done to keep it" | layout profiles |
| "Achievement unlocked: <name>" (one line each; long) | achievements unlocking |
| "A spell code is three letters, e.g. pro" | bad typed spell code |
| "No empty slot" (long) | quick-start "New game" with no empty slot |
| "Leave the current game first" (long) | slot action or import while a game runs |
| "Slot N is busy — please wait" / "Please wait for the current export or import" (long) | slot action during an export/import |
| "Slot N has no game any more" (long) | playing a slot whose files vanished |
| "Couldn't prepare slot N: <error>" (long) | new game could not clear/write the slot |
| "Slot N deleted" (long) | slot deleted |
| "Couldn't rename: <error>" (long) | rename failed |
| "No file picker available" (long) | no document picker on the device |
| "Nothing to export" (long) | export target invalid or empty |
| "Slot N exported" / "Export failed: <error>" (long) | export finished |
| "No empty slot: delete a slot first" (long) | import with all slots used |
| "Imported into slot N" (long) | import succeeded |

Toasts raised by the Wild features (card drops, photos, radio) are listed in Part 13.

## 15. Wild Edition hooks in the app shell

For orientation only; the features themselves are Part 13.

- The activity owns an event bus fed from the game's event ring on every display change;
  card drops, the radio and photo mode listen on it, and a weather "lightning" event
  flashes the game view.
- Start screen: "Card album (C/69)" when cards are enabled.
- In-game menu: "Card album", "Radio", "Take a photo", each when its feature is enabled
  in Settings.
- A control with the Photo action takes a photo while a game is running; photo mode's
  storage-permission result is routed back to it by the activity.
- Photo mode learns the character's name by reading the "name:" entry of the options
  text the app last prepared for the slot.
- Settings: the "Wild features" section (7.2, 7.5); the mask is passed to new game
  processes in `ULARN_WILD`.
- Statistics dialog: the "Last photo" section (10.6).
- The radio is paused and resumed with the activity.
