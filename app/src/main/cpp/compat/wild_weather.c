/* =============================================================================
 * FILENAME: wild_weather.c
 *
 * Ularn: Wild Edition feature "weather" (weather and day/night on the
 * surface). See docs/WILD_EDITION_PLAN.md (3.10) for the design and wild.h
 * for when each hook is called. Everything in this file runs only when the
 * feature's bit is on (wild.c checks before calling).
 *
 * State (all saved with the game):
 *   c[WILD_WEATHER]            hour (0-23) | weather state << 8, as the app
 *                              reads it from S->weather. The hour is always
 *                              (gtime / 50 + 8) % 24, so a day is 1200 turns
 *                              and a new game starts at 08:00.
 *   wild_extra[WX_WEATHER_NEXT] gtime of the next change of weather.
 *
 * On the surface (level 0):
 *   - the weather changes every 200-600 turns, with a message;
 *   - at night (21:00-05:59) random monsters come twice as often (an extra
 *     tick of the upstream rmst countdown, which runs right after
 *     wild_turn() in the main loop), and once in a while a migration of
 *     lemmings enters at the east edge and marches west across the map;
 *   - in a storm, lightning now and then strikes a monster near the player;
 *   - when rain or a storm clears in daylight, a rainbow comes out and a pot
 *     of gold waits at its end: an ordinary pile of gold on a free cell,
 *     marked on the player's map (nothing new is saved).
 * The app draws the rest (game/WeatherFx.kt): tint, rain, snow, stars, the
 * sun / moon in the HUD, lightning flashes.
 * =============================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "header.h"
#include "ularn_game.h"
#include "ularn_win.h"
#include "dungeon.h"
#include "player.h"
#include "monster.h"
#include "itm.h"
#include "../shared_state.h"
#include "wild.h"

extern UlarnShared *ularn_shared;

/* Weather states (S->weather >> 8; mirrored in Kotlin: WeatherMath). */
#define WX_CLEAR 0
#define WX_RAIN  1
#define WX_SNOW  2
#define WX_STORM 3

#define TURNS_PER_HOUR   50
#define CHANGE_MIN      200   /* the weather lasts 200..600 turns */
#define CHANGE_RAND     400
#define NIGHT_START      21   /* night is 21:00 .. 05:59 */
#define NIGHT_END         6
#define MIGRATION_ODDS  300   /* 1 in 300 night turns on the surface */
#define MIGRATION_MIN     8   /* 8..12 lemmings */
#define MIGRATION_RAND    5
#define LIGHTNING_ODDS  200   /* 1 in 200 storm turns */
#define LIGHTNING_DMG    20
#define LIGHTNING_RANGE   7   /* "visible": this close to the player */
#define RAINBOW_BASE     25   /* pot of gold: 25d + rnd(50d), d = deepest level */
#define RAINBOW_RAND     50

/* A new game starts at 08:00, so the first night is a few hundred turns away. */
#define DAY_START_HOUR 8

static int hour_now(void)
{
  return (int) ((gtime / TURNS_PER_HOUR + DAY_START_HOUR) % 24);
}

static int state_now(void)
{
  return (int) ((c[WILD_WEATHER] >> 8) & 0xff);
}

static int is_night(int hour)
{
  return hour >= NIGHT_START || hour < NIGHT_END;
}

/* The app reads S->weather every frame, but the status (and so
 * wild_paint()) is only repainted when something on it changes: publish a
 * new hour or weather straight away. */
static void publish(int hour, int state)
{
  if (ularn_shared != NULL)
    ularn_shared->weather = (int32_t) ((hour & 0xff) | ((state & 0xff) << 8));
}

static void store(int hour, int state)
{
  c[WILD_WEATHER] = (long) ((hour & 0xff) | ((state & 0xff) << 8));
  publish(hour, state);
}

static const char *describe(int state)
{
  switch (state)
  {
    case WX_RAIN:  return "It is raining.";
    case WX_SNOW:  return "It is snowing. In Larn!";
    case WX_STORM: return "A storm rages over the town.";
    default:       return NULL;
  }
}

/* ---- the rainbow ---- */

/* How deep the character has been: 1 (never below the town) .. 12 (the
 * volcano counts as the bottom), so a pot of gold keeps up with the game. */
static int deepest_level(void)
{
  int i, deepest = 1;
  for (i = 1; i <= DBOTTOM; i++)
    if (beenhere[i]) deepest = i;
  for (i = MAXLEVEL; i <= VBOTTOM; i++)
    if (beenhere[i]) deepest = 12;
  return deepest > 12 ? 12 : deepest;
}

/* Rain or a storm has just cleared over the town in daylight with the player
 * there to see it: a pot of gold appears at the end of the rainbow. It is an
 * ordinary pile of gold on a free cell, already on the player's map. Returns
 * 0 (and nothing happens) if there is no free cell for it. */
static int rainbow(int hour, int old)
{
  int x, y, d = deepest_level();
  if (!wild_free_cell(&x, &y)) return 0;
  item[x][y] = OGOLDPILE;
  iarg[x][y] = (short) (RAINBOW_BASE * d + rnd(RAINBOW_RAND * d));
  know[x][y] = OGOLDPILE;
  show1cell(x, y);
  wild_event(US_EV_WILD_WEATHER, WX_CLEAR, hour, 2);
  Print(old == WX_STORM ? "\nThe storm passes, and a rainbow arches over the town!" :
                         "\nThe rain stops, and a rainbow arches over the town!");
  Printf("\nIts end touches the ground to the %s. Is that a pot of gold?",
         wild_compass(x, y));
  return 1;
}

/* ---- weather changes ---- */

static void schedule_next(void)
{
  wild_extra[WX_WEATHER_NEXT] = (int) (gtime + CHANGE_MIN + rnd(CHANGE_RAND));
}

/* A different state; clear skies are the most common, storms the rarest. */
static int pick_state(int old)
{
  int s, r;
  do
  {
    r = rnd(100);
    s = r <= 40 ? WX_CLEAR : r <= 70 ? WX_RAIN : r <= 85 ? WX_SNOW : WX_STORM;
  } while (s == old);
  return s;
}

static void change_weather(int hour)
{
  int old = state_now();
  int s = pick_state(old);
  store(hour, s);
  schedule_next();
  wild_event(US_EV_WILD_WEATHER, s, hour, 0);
  if (level != 0) return;   /* it changed while you were underground */
  switch (s)
  {
    case WX_RAIN:  Print("\nIt starts to rain."); break;
    case WX_SNOW:  Print("\nSnow! In Larn!"); break;
    case WX_STORM: Print("\nThunder rolls over the mountains."); break;
    default:
      if ((old == WX_RAIN || old == WX_STORM) && !is_night(hour) && rainbow(hour, old))
        break;
      Print(old == WX_SNOW ? "\nThe snow stops and the sky clears." :
                             "\nThe clouds part. The sky clears.");
      break;
  }
}

/* ---- the lemming migration ---- */

static int lemmings_on_level(void)
{
  int x, y, n = 0;
  for (x = 0; x < MAXX; x++)
    for (y = 0; y < MAXY; y++)
      if (mitem[x][y].mon == LEMMING) n++;
  return n;
}

static int free_for_lemming(int x, int y)
{
  return cgood(x, y, 0, 1) && !(x == playerx && y == playery);
}

/* A column of awake lemmings at the east edge of the map. */
static void lemming_migration(void)
{
  int n, y0, i, x, y, placed = 0;
  if (monster[LEMMING].flags & FL_GENOCIDED) return;
  if (lemmings_on_level() >= 4) return;   /* one migration at a time */
  n = MIGRATION_MIN - 1 + rnd(MIGRATION_RAND);
  y0 = rnd(MAXY - n) - 1;   /* 0 .. MAXY - n - 1 */
  for (i = 0; i < n; i++)
  {
    y = y0 + i;
    for (x = MAXX - 1; x >= MAXX - 4; x--)
    {
      if (!free_for_lemming(x, y)) continue;
      mitem[x][y].mon = LEMMING;
      mitem[x][y].n = 0;
      hitp[x][y] = monster[LEMMING].hitpoints;
      stealth[x][y] = STEALTH_AWAKE;
      if (know[x][y] != OUNKNOWN) show1cell(x, y);
      placed++;
      break;
    }
  }
  if (placed > 0)
    Print("\nYou hear the pitter-patter of a thousand tiny feet.");
}

/* Is (x,y) in the window upstream movemonst() moves monsters in? Those
 * lemmings are left to the game (they come for you instead of marching). */
static int in_move_window(int x, int y)
{
  int wx = c[AGGRAVATE] ? 10 : 5, wy = c[AGGRAVATE] ? 5 : 3;
  return x >= playerx - wx && x < playerx + wx + 1 &&
         y >= playery - wy && y < playery + wy + 1;
}

static void move_lemming(int x, int y, int nx, int ny)
{
  mitem[nx][ny] = mitem[x][y];
  hitp[nx][ny] = hitp[x][y];
  stealth[nx][ny] = stealth[x][y] | STEALTH_AWAKE;
  mitem[x][y].mon = MONST_NONE;
  mitem[x][y].n = 0;
  hitp[x][y] = 0;
  stealth[x][y] = 0;
  if (know[x][y] != OUNKNOWN) show1cell(x, y);
  if (know[nx][ny] != OUNKNOWN) show1cell(nx, ny);
}

/* Every other turn, the surface's lemmings that the game itself is not
 * moving take one step west; at the west edge they leave the map. The scan
 * goes west to east, so a lemming never moves twice in one march. */
static void lemmings_march(void)
{
  int x, y, k, ny;
  static const int dy[3] = { 0, -1, 1 };
  if (c[TIMESTOP] || c[HOLDMONST] || (gtime & 1)) return;
  for (x = 0; x < MAXX; x++)
    for (y = 0; y < MAXY; y++)
    {
      if (mitem[x][y].mon != LEMMING || in_move_window(x, y)) continue;
      if (wild_is_pet(x, y)) continue;   /* a pet lemming stays home */
      if (x == 0)
      {
        mitem[x][y].mon = MONST_NONE;
        mitem[x][y].n = 0;
        hitp[x][y] = 0;
        stealth[x][y] = 0;
        if (know[x][y] != OUNKNOWN) show1cell(x, y);
        continue;
      }
      for (k = 0; k < 3; k++)
      {
        ny = y + dy[k];
        if (ny < 0 || ny >= MAXY || !free_for_lemming(x - 1, ny)) continue;
        move_lemming(x, y, x - 1, ny);
        break;
      }
    }
}

/* ---- storm lightning ---- */

static void lightning(int hour)
{
  int x, y, n = 0, pick, px = -1, py = -1;
  if (c[BLINDCOUNT] == 0)
    for (x = playerx - LIGHTNING_RANGE; x <= playerx + LIGHTNING_RANGE; x++)
      for (y = playery - LIGHTNING_RANGE; y <= playery + LIGHTNING_RANGE; y++)
      {
        if (x < 0 || y < 0 || x >= MAXX || y >= MAXY) continue;
        if (mitem[x][y].mon == MONST_NONE || wild_is_pet(x, y)) continue;
        if (know[x][y] == OUNKNOWN) continue;
        /* reservoir sampling: each visible monster equally likely */
        n++;
        pick = rnd(n);
        if (pick == 1) { px = x; py = y; }
      }
  wild_event(US_EV_WILD_WEATHER, WX_STORM, hour, 1);
  if (px < 0)
  {
    Print("\nLightning splits the sky. The thunder is deafening.");
    return;
  }
  ifblind(px, py);
  Printf("\nA bolt of lightning strikes the %s!", lastmonst);
  hitm(px, py, LIGHTNING_DMG, 1);
}

/* ---- hooks ---- */

void wild_weather_init(void) {}

void wild_weather_new_character(void)
{
  store(hour_now(), WX_CLEAR);
  schedule_next();
}

void wild_weather_turn(void)
{
  int hour = hour_now(), old_hour = (int) (c[WILD_WEATHER] & 0xff);
  int state = state_now();

  /* a save made before the feature had a schedule */
  if (wild_extra[WX_WEATHER_NEXT] <= 0) schedule_next();

  store(hour, state);
  if (gtime >= wild_extra[WX_WEATHER_NEXT]) change_weather(hour);
  state = state_now();

  if (level != 0) return;

  if (hour != old_hour)
  {
    if (hour == NIGHT_END)
    {
      Print("\nDawn breaks over Larn.");
      wild_event(US_EV_WILD_WEATHER, state, hour, 0);
    }
    else if (hour == NIGHT_START)
    {
      Print("\nNight falls. Things stir in the dark.");
      wild_event(US_EV_WILD_WEATHER, state, hour, 0);
    }
  }

  if (is_night(hour))
  {
    /* twice the random monsters: one extra tick of the main loop's countdown
     * (never down to 0 here, so the game's own check makes the monster) */
    if (c[TIMESTOP] == 0 && rmst > 1) rmst--;
    if (rnd(MIGRATION_ODDS) == 1) lemming_migration();
  }
  lemmings_march();

  if (state == WX_STORM && rnd(LIGHTNING_ODDS) == 1) lightning(hour);
}

void wild_weather_new_level(int lev)
{
  const char *d;
  if (lev != 0) return;
  d = describe(state_now());
  if (d != NULL) Printf("\n%s", d);
}

void wild_weather_leaving_level(int lev) { (void) lev; }
void wild_weather_make_level(int lev) { (void) lev; }
void wild_weather_kill(int monst, int x, int y) { (void) monst; (void) x; (void) y; }
void wild_weather_level_up(int newlevel) { (void) newlevel; }
void wild_weather_take(int obj, int arg) { (void) obj; (void) arg; }
int  wild_weather_object(int obj) { (void) obj; return 0; }
int  wild_weather_wear(int slot) { (void) slot; return 0; }
int  wild_weather_takeoff(void) { return 0; }
int  wild_weather_read(int slot) { (void) slot; return 0; }
int  wild_weather_eat(int slot) { (void) slot; return 0; }
int  wild_weather_dodge(void) { return 0; }
void wild_weather_player_hit(int dmg) { (void) dmg; }

/* Runs on every repaint, after wild_paint() copied c[WILD_WEATHER]: shows
 * the current hour even before the next turn (e.g. right after a restore).
 * c[] itself is left alone so wild_weather_turn() still sees the hour change
 * and announces dawn and dusk. */
void wild_weather_paint(void)
{
  publish(hour_now(), state_now());
}
