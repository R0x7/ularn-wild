/* =============================================================================
 * FILENAME: wild_fishing.c
 *
 * Ularn: Wild Edition feature "fishing" (WILDF_FISHING). See
 * docs/WILD_EDITION_PLAN.md section 3.7 and docs/wild/fishing.md.
 *
 *   - The fishing rod (OFISHINGROD) is sold by the DnD store and one lies on
 *     dungeon level 1 or 2. With a rod in the pack the fountain prompt gains
 *     (f)ish (dungeon_obj.c: ofountain() -> wild_fishing_fish()).
 *   - Fishing takes 1-5 turns, monsters keep moving, and a hit makes you drop
 *     the line. The catch: a fish, nothing, an old boot, a message in a
 *     bottle, a ring, a potion or the fountain kraken. c[WILD_FISH_XP] counts
 *     catches and shifts the odds away from boots and empty hooks.
 *   - A fish (OFISH) is eaten: heals 10 + character level. A bottle (OBOTTLE)
 *     is read: a fortune, then it's gone. Below ground, one note in four is
 *     a treasure map instead: a stash of gold appears somewhere on the level
 *     and is marked on the map (see treasure_map()). An old boot (OOLDBOOT) sells for
 *     1 gp; the first time it's picked up, 1 in 5 has a small gem inside
 *     (its iarg becomes 1 once searched, so dropping it doesn't re-roll).
 *   - The fountain kraken (FOUNTAINKRAKEN) takes the fountain's place and is
 *     tied to it: wild_fishing_turn() pulls it back whenever it strays more
 *     than 2 cells. Only one is tracked at a time, in wild_extra[]:
 *     WX_KRAKEN_X/Y = the old fountain, WX_KRAKEN_LEVEL = its level + 1.
 *
 * Everything in this file runs only when the feature's bit is on (wild.c
 * checks before calling; the wild_fishing.h helpers check themselves).
 * =============================================================================
 */
#include <stdio.h>
#include <string.h>

#include "header.h"
#include "ularn_game.h"
#include "ularn_win.h"
#include "dungeon.h"
#include "player.h"
#include "monster.h"
#include "itm.h"
#include "potion.h"
#include "fortune.h"
#include "../shared_state.h"
#include "wild.h"
#include "wild_fishing.h"

#define FISH_MAX_WAIT    5    /* a cast takes 1..FISH_MAX_WAIT turns */
#define FISH_DRY_CHANCE  15   /* % chance the fountain dries up after a catch */
#define BOOT_GEM_CHANCE  20   /* % of old boots with a gem inside */
#define KRAKEN_TETHER    2    /* cells the kraken may stray from its fountain */
#define BOTTLE_MAP_CHANCE 25  /* % of bottles read below ground that hold a treasure map */

/* iarg of an old boot: has it been shaken out yet? */
#define BOOT_UNSEARCHED  0
#define BOOT_SEARCHED    1

/* What comes out of the fountain */
enum
{
  CATCH_NOTHING, CATCH_FISH, CATCH_BOOT, CATCH_BOTTLE, CATCH_RING,
  CATCH_POTION, CATCH_KRAKEN, CATCH_KINDS
};

/* Rings a fountain can cough up (ORING is ring mail, not a ring). */
static const int fish_rings[] =
{
  OREGENRING, OPROTRING, OENERGYRING, ODEXRING, OSTRRING, OCLEVERRING,
  ODAMRING, ORINGOFEXTRA
};

static int have_rod(void)
{
  int i;
  for (i = 0; i < IVENSIZE; i++)
    if (iven[i] == OFISHINGROD) return 1;
  return 0;
}

static int dist(int x1, int y1, int x2, int y2)
{
  int dx = x1 - x2, dy = y1 - y2;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  return dx > dy ? dx : dy;
}

/* ---- the catch ---- */

/* Base odds (out of 100): fish 45, nothing 20, boot 15, bottle 8, ring 5,
 * potion 4, kraken 3. Every 3 catches moves a point from the empty hook
 * (and half a point from the boot) to the bottle, ring and potion, up to 10. */
static int pick_catch(void)
{
  int w[CATCH_KINDS];
  int s, i, r, total = 0;

  s = (int) (c[WILD_FISH_XP] / 3);
  if (s > 10) s = 10;
  w[CATCH_FISH]    = 45;
  w[CATCH_NOTHING] = 20 - s;
  w[CATCH_BOOT]    = 15 - s / 2;
  w[CATCH_BOTTLE]  = 8 + s / 2;
  w[CATCH_RING]    = 5 + s / 2;
  w[CATCH_POTION]  = 4 + s / 2;
  w[CATCH_KRAKEN]  = 3;
  for (i = 0; i < CATCH_KINDS; i++) total += w[i];

  r = rnd(total);   /* 1..total */
  for (i = 0; i < CATCH_KINDS; i++)
  {
    if (r <= w[i]) return i;
    r -= w[i];
  }
  return CATCH_NOTHING;
}

/* Into the pack, or onto the floor next to the player if it's full. */
static void land(int obj, int arg)
{
  c[WILD_FISH_XP]++;
  wild_event(US_EV_WILD_FISHING, 1, obj, arg);
  if (take(obj, arg) != 0)
  {
    createitem(playerx, playery, obj, arg);
    Print("\nIt flops onto the floor next to you.");
  }
}

/* ---- the kraken ---- */

static void kraken_forget(void)
{
  wild_extra[WX_KRAKEN_X] = 0;
  wild_extra[WX_KRAKEN_Y] = 0;
  wild_extra[WX_KRAKEN_LEVEL] = 0;
}

static void put_kraken(int x, int y)
{
  mitem[x][y].mon = FOUNTAINKRAKEN;
  mitem[x][y].n = 0;
  hitp[x][y] = monster[FOUNTAINKRAKEN].hitpoints;
  stealth[x][y] = STEALTH_AWAKE | STEALTH_SEEN;
}

/* Where the surge throws the player: two cells away in direction k if that
 * is free and the cell between isn't a wall, else one cell. 0 if neither. */
static int knockback(int fx, int fy, int k, int *x, int *y)
{
  int x1 = fx + diroffx[k], y1 = fy + diroffy[k];
  int x2 = x1 + diroffx[k], y2 = y1 + diroffy[k];
  if (cgood(x1, y1, 0, 1) && cgood(x2, y2, 1, 1)) { *x = x2; *y = y2; return 1; }
  if (cgood(x1, y1, 1, 1)) { *x = x1; *y = y1; return 1; }
  return 0;
}

/* The fountain under the player turns into a kraken. It wants the fountain's
 * cell, so the surge throws the player clear (see knockback()); if there is
 * no room, the kraken comes up next to the player instead (and is still tied
 * to the fountain's cell). The monsters then skip this turn (hitflag), so a
 * low-level angler gets one move to run: the tether keeps the kraken from
 * reaching anyone 4 or more cells from its fountain. Returns 0 if nothing
 * surfaced. */
static int kraken_surfaces(void)
{
  int fx = playerx, fy = playery;
  int i, k, x, y;

  if (monster[FOUNTAINKRAKEN].flags & FL_GENOCIDED) return 0;
  /* one kraken at a time: the tether only tracks one */
  if (wild_extra[WX_KRAKEN_LEVEL] != 0) return 0;

  for (k = rnd(8), i = -8; i < 0; i++, k++)
  {
    if (k > 8) k = 1;
    if (knockback(fx, fy, k, &x, &y))
    {
      Print("\nSomething enormous surfaces. The fountain was deeper than it looked.");
      Print("\nThe surge throws you aside!");
      forget();
      lastpx = playerx;
      lastpy = playery;
      playerx = (char) x;
      playery = (char) y;
      put_kraken(fx, fy);
      show1cell(fx, fy);
      showplayer();
      break;
    }
  }

  if (i == 0)
  {
    /* boxed in: it squeezes out next to the player instead */
    for (k = rnd(8), i = -8; i < 0; i++, k++)
    {
      if (k > 8) k = 1;
      x = fx + diroffx[k];
      y = fy + diroffy[k];
      if (cgood(x, y, 0, 1)) break;
    }
    if (i == 0) return 0;
    Print("\nSomething enormous surfaces. The fountain was deeper than it looked.");
    forget();
    put_kraken(x, y);
    show1cell(x, y);
  }

  wild_extra[WX_KRAKEN_X] = fx;
  wild_extra[WX_KRAKEN_Y] = fy;
  wild_extra[WX_KRAKEN_LEVEL] = level + 1;
  wild_event(US_EV_WILD_FISHING, 2, fx, fy);
  UlarnBeep();
  hitflag = 1;   /* main loop: no monster move before the player's next action */
  return 1;
}

/* Keep the tracked kraken within KRAKEN_TETHER cells of its fountain: it is
 * attached to the plumbing. Also notices (e.g. after a restore, or a
 * genocide) that the kraken is gone, and stops tracking it. */
static void kraken_tether(void)
{
  int ax, ay, x, y, kx = -1, ky = -1, bx = -1, by = -1, d, best = 1000, i;

  if (wild_extra[WX_KRAKEN_LEVEL] == 0) return;
  if (monster[FOUNTAINKRAKEN].flags & FL_GENOCIDED) { kraken_forget(); return; }
  if (wild_extra[WX_KRAKEN_LEVEL] != level + 1) return;   /* it's on another level */

  ax = wild_extra[WX_KRAKEN_X];
  ay = wild_extra[WX_KRAKEN_Y];
  if (!checkxy(ax, ay)) { kraken_forget(); return; }

  /* the kraken closest to its fountain */
  for (x = 0; x < MAXX; x++)
    for (y = 0; y < MAXY; y++)
      if (mitem[x][y].mon == FOUNTAINKRAKEN && (d = dist(x, y, ax, ay)) < best)
      {
        best = d;
        kx = x;
        ky = y;
      }
  if (kx < 0) { kraken_forget(); return; }
  if (best <= KRAKEN_TETHER) return;

  /* pull it back to the free cell in range nearest to where it got to */
  best = 1000;
  for (x = ax - KRAKEN_TETHER; x <= ax + KRAKEN_TETHER; x++)
    for (y = ay - KRAKEN_TETHER; y <= ay + KRAKEN_TETHER; y++)
    {
      if (!cgood(x, y, 0, 1) || (x == playerx && y == playery)) continue;
      d = dist(x, y, kx, ky);
      if (d < best) { best = d; bx = x; by = y; }
    }
  if (bx < 0) return;   /* nowhere to go; try again next turn */

  for (i = 0; i < mitem[kx][ky].n; i++)
  {
    mitem[bx][by].it[i] = mitem[kx][ky].it[i];
    mitem[kx][ky].it[i].item = ONOTHING;
    mitem[kx][ky].it[i].itemarg = 0;
  }
  mitem[bx][by].n = mitem[kx][ky].n;
  mitem[bx][by].mon = FOUNTAINKRAKEN;
  hitp[bx][by] = hitp[kx][ky];
  stealth[bx][by] = stealth[kx][ky];
  mitem[kx][ky].mon = MONST_NONE;
  mitem[kx][ky].n = 0;
  hitp[kx][ky] = 0;
  stealth[kx][ky] = 0;
  if (know[kx][ky] != OUNKNOWN) show1cell(kx, ky);
  if (know[bx][by] != OUNKNOWN) show1cell(bx, by);
  if (!c[BLINDCOUNT] && dist(bx, by, playerx, playery) <= 6)
    Print("\nThe fountain kraken is yanked back by its plumbing.");
}

/* ---- fishing ---- */

int wild_fishing_can_fish(void)
{
  return wild_on(WILDF_FISHING) && have_rod();
}

void wild_fishing_fish(void)
{
  int turns, t, obj;

  if (!wild_fishing_can_fish() || item[playerx][playery] != OFOUNTAIN) return;

  wild_event(US_EV_WILD_FISHING, 0, 0, 0);
  Print("\nYou cast your line into the fountain");
  turns = rnd(FISH_MAX_WAIT);
  hitflag = 0;
  for (t = 0; t < turns; t++)
  {
    /* a turn passes, like waiting with '.': monsters move, time goes by */
    Print(".");
    nap(300);
    if (c[HASTEMONST]) movemonst();
    movemonst();
    regen();
    wild_turn();
    if (hitflag)
    {
      Print("\nYou drop your line!");
      return;
    }
  }
  /* the tether (inside wild_turn) may have moved things around, but the
   * player is still standing on the fountain */
  if (item[playerx][playery] != OFOUNTAIN) return;

  switch (pick_catch())
  {
    case CATCH_FISH:
      Print("\nA tug! You reel in a fish.");
      land(OFISH, 0);
      break;
    case CATCH_BOOT:
      Print("\nA heavy tug! You reel in... an old boot.");
      land(OOLDBOOT, BOOT_UNSEARCHED);
      break;
    case CATCH_BOTTLE:
      Print("\nYou reel in a corked bottle with a note inside.");
      land(OBOTTLE, 0);
      break;
    case CATCH_RING:
      Print("\nSomething glints on the hook: a ring!");
      obj = fish_rings[rund(sizeof fish_rings / sizeof fish_rings[0])];
      land(obj, rnd(level / 4 + 1));
      break;
    case CATCH_POTION:
      Print("\nYou fish out a potion. Someone threw this in.");
      land(OPOTION, newpotion());
      break;
    case CATCH_KRAKEN:
      if (kraken_surfaces()) return;   /* the fountain is gone */
      Print("\nSomething big brushes the line, then lets go.");
      break;
    default:
      Print("\nNothing bites.");
      break;
  }

  if (rnd(100) <= FISH_DRY_CHANCE)
  {
    Print("\nThe fountain's bubbling slowly quietens.");
    item[playerx][playery] = ODEADFOUNTAIN;
    know[playerx][playery] = ODEADFOUNTAIN;
  }
  UpdateStatus();
}

/* ---- the catch, used ---- */

static void eat_fish(void)
{
  long heal = 10 + c[LEVEL];
  Print("\nYou eat the fish. Raw, but surprisingly good.");
  c[HP] += heal;
  if (c[HP] > c[HPMAX]) c[HP] = c[HPMAX];
  UpdateStatus();
}

/* Which way (x,y) lies from the player, in words. y grows southwards. */
static const char *compass(int x, int y)
{
  static const char *const names[3][3] =
  {
    { "north-west", "north", "north-east" },
    { "west",       "here",  "east"       },
    { "south-west", "south", "south-east" }
  };
  int dx = x - playerx, dy = y - playery;
  int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  int col = 1, row = 1;

  /* call it a straight line unless the other axis is at least half as long */
  if (ax * 2 >= ay) col = dx < 0 ? 0 : (dx > 0 ? 2 : 1);
  if (ay * 2 >= ax) row = dy < 0 ? 0 : (dy > 0 ? 2 : 1);
  return names[row][col];
}

/* The note is a treasure map of the level the reader is standing on: a stash
 * of gold (more of it the deeper the level) appears on a random empty floor
 * cell, which the player now knows about, so it shows on the map. Nothing is
 * stored: the stash is an ordinary pile of gold from here on. Returns 0 if
 * this note isn't a map (above ground, the luck of the draw, or no room). */
static int treasure_map(void)
{
  int x, y, gold;

  if (level == 0 || rnd(100) > BOTTLE_MAP_CHANCE) return 0;
  if (!wild_free_cell(&x, &y)) return 0;

  gold = 40 * level + rnd(60 * level);   /* 41..100 on level 1, about x level below */
  item[x][y] = OGOLDPILE;
  iarg[x][y] = (short) gold;
  know[x][y] = OGOLDPILE;
  show1cell(x, y);
  wild_event(US_EV_WILD_FISHING, 3, x, y);

  Print(" It is a map of this very level!");
  Printf("\nAn X marks a spot to the %s. You mark it on your own map.", compass(x, y));
  return 1;
}

static void read_bottle(void)
{
  char *p;
  Print("\nYou uncork the bottle and unroll the note.");
  if (!treasure_map())
  {
    p = fortune(fortfile);
    if (p != NULL)
    {
      Print(" It says:\n");
      Print(p);
    }
    else
      Print(" The ink has washed out.");
  }
  Print("\nThe bottle slips from your fingers and is gone.");
}

/* Found a fish: like junk food. */
static void found_fish(void)
{
  int i;
  Print("\n\nYou find a fish.");
  i = get_prompt_input("\nDo you (e) eat it, (t) take it, or (i) ignore it? ", "eit\033", 1);
  if (i == 'e')
  {
    Print("eat.");
    forget();
    eat_fish();
  }
  else if (i == 't')
  {
    Print("take.");
    if (take(OFISH, iarg[playerx][playery]) == 0) forget();
  }
  else
    Print("ignore.");
}

/* Found a bottle: like a scroll. */
static void found_bottle(void)
{
  int i;
  Print("\n\nYou find a message in a bottle.");
  i = get_prompt_input("\nDo you (r) read it, (t) take it, or (i) ignore it? ", "rit\033", 1);
  if (i == 'r' && c[BLINDCOUNT] == 0)
  {
    Print("read.");
    forget();
    read_bottle();
  }
  else if (i == 'r')
    Print("\nYou can't read anything when you're blind!");
  else if (i == 't')
  {
    Print("take.");
    if (take(OBOTTLE, iarg[playerx][playery]) == 0) forget();
  }
  else
    Print("ignore.");
}

/* ---- store helpers ---- */

int wild_fishing_hidden(int obj)
{
  return obj == OFISHINGROD && !wild_on(WILDF_FISHING);
}

int wild_fishing_value(int obj)
{
  return (obj == OOLDBOOT && wild_on(WILDF_FISHING)) ? 1 : -1;
}

/* ---- hooks ---- */

void wild_fishing_init(void) {}

void wild_fishing_new_character(void)
{
  wild_extra[WX_FISH_ROD_LEVEL] = rnd(2);   /* level 1 or 2 */
}

void wild_fishing_turn(void)
{
  kraken_tether();
}

void wild_fishing_new_level(int lev) { (void) lev; }
void wild_fishing_leaving_level(int lev) { (void) lev; }

void wild_fishing_make_level(int lev)
{
  int x, y;
  if (lev != wild_extra[WX_FISH_ROD_LEVEL] || !wild_free_cell(&x, &y)) return;
  item[x][y] = OFISHINGROD;
  iarg[x][y] = 0;
  if (wizard) know[x][y] = OFISHINGROD;   /* newcavelevel() already revealed the rest */
}

void wild_fishing_kill(int monst, int x, int y)
{
  if (monst != FOUNTAINKRAKEN || wild_extra[WX_KRAKEN_LEVEL] != level + 1) return;
  if (dist(x, y, wild_extra[WX_KRAKEN_X], wild_extra[WX_KRAKEN_Y]) > KRAKEN_TETHER + 2) return;
  Print("\nIts tentacles slither back down the drain.");
  kraken_forget();
}

void wild_fishing_level_up(int newlevel) { (void) newlevel; }

/* The first time an old boot is picked up, shake it out. The gem drops next
 * to the player rather than into the pack: take() prints the boot after this
 * hook, and "You pick up: a ruby / You pick up: an old boot" read oddly. */
void wild_fishing_take(int obj, int arg)
{
  int i;
  if (obj != OOLDBOOT || arg != BOOT_UNSEARCHED) return;
  for (i = 0; i < IVENSIZE; i++)
    if (iven[i] == OOLDBOOT && ivenarg[i] == BOOT_UNSEARCHED)
    {
      ivenarg[i] = BOOT_SEARCHED;
      break;
    }
  if (rnd(100) > BOOT_GEM_CHANCE) return;
  Print("\nSomething rattles in the boot. A small gem falls out!");
  createitem(playerx, playery, ORUBY + rund(3), rnd(5) + 2);   /* ruby, emerald or sapphire */
}

int wild_fishing_object(int obj)
{
  if (obj == OFISH) { found_fish(); return 1; }
  if (obj == OBOTTLE) { found_bottle(); return 1; }
  return 0;
}

int wild_fishing_wear(int slot) { (void) slot; return 0; }
int wild_fishing_takeoff(void) { return 0; }

int wild_fishing_read(int slot)
{
  if (iven[slot] != OBOTTLE) return 0;
  iven[slot] = ONOTHING;
  read_bottle();
  return 1;
}

int wild_fishing_eat(int slot)
{
  if (iven[slot] != OFISH) return 0;
  iven[slot] = ONOTHING;
  eat_fish();
  return 1;
}

int  wild_fishing_dodge(void) { return 0; }
void wild_fishing_player_hit(int dmg) { (void) dmg; }
void wild_fishing_paint(void) {}
