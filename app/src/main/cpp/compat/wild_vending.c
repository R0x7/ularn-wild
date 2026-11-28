/* =============================================================================
 * FILENAME: wild_vending.c
 *
 * Ularn: Wild Edition feature "vending" (WILDF_VENDING). See
 * docs/WILD_EDITION_PLAN.md section 3.3 and docs/wild/vending.md.
 *
 *   - Vending machines (OVENDING) on ~40% of levels 1-10 and V1-V3, plus one
 *     next to the DnD store on the home level. Buy junk food for 50 gold, or
 *     kick the machine and hope it doesn't fall on you.
 *   - Junk food (OFIZZ, OPIZZA, OENERGYDRINK) is eaten with 'e' or when found.
 *   - The rubber duck (ORUBBERDUCK) is "read" with 'r' (or rubbed when found):
 *     it identifies every potion and scroll in the pack, 3 times per game
 *     (c[WILD_DUCK] counts the uses).
 *   - The roomba (ROOMBA) wanders some deeper levels eating floor items into
 *     its stolen-item slots; hitm() drops them back when it dies.
 *
 * A machine that wobbles keeps that state in its own iarg[][] (saved with the
 * level), so several machines can each be wobbly and save/restore keeps it.
 *
 * Everything in this file runs only when the feature's bit is on (wild.c
 * checks before calling).
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
#include "scroll.h"
#include "scores.h"
#include "../shared_state.h"
#include "wild.h"

#define VEND_PRICE      50
#define VEND_CHANCE     40    /* % of eligible levels with a machine */
#define ROOMBA_CHANCE   20    /* % of levels >= ROOMBA_MINLEVEL with a roomba */
#define ROOMBA_MINLEVEL 3
#define DUCK_CHARGES    3

/* iarg[][] of an OVENDING square */
#define VEND_STEADY  0
#define VEND_WOBBLY  1

/* Levels that may get a machine: 1-10 and V1-V3. */
static int vending_level(int lev)
{
  return (lev >= 1 && lev <= 10) || (lev >= MAXLEVEL && lev < MAXLEVEL + 3);
}

/* Stairs, elevators and shafts: keep machines from blocking them. */
static int is_exit(int obj)
{
  switch (obj)
  {
    case OSTAIRSUP: case OSTAIRSDOWN: case OELEVATORUP: case OELEVATORDOWN:
    case OVOLUP: case OVOLDOWN: case OENTRANCE:
      return 1;
    default:
      return 0;
  }
}

static int next_to_exit(int x, int y)
{
  int dx, dy;
  for (dx = -1; dx <= 1; dx++)
    for (dy = -1; dy <= 1; dy++)
      if (checkxy(x + dx, y + dy) && is_exit(item[x + dx][y + dy])) return 1;
  return 0;
}

static void place_machine(int x, int y)
{
  item[x][y] = OVENDING;
  iarg[x][y] = VEND_STEADY;
  /* the home level (and wizard mode) know the whole map already */
  if (wizard || level == 0) know[x][y] = OVENDING;
}

/* Home level: a machine on a free square close to the DnD store. */
static void place_home_machine(void)
{
  int sx, sy, r, x, y;
  for (sx = 0; sx < MAXX; sx++)
    for (sy = 0; sy < MAXY; sy++)
    {
      if (item[sx][sy] != ODNDSTORE) continue;
      for (r = 1; r <= 3; r++)
        for (y = sy - r; y <= sy + r; y++)
          for (x = sx - r; x <= sx + r; x++)
          {
            if (!checkxy(x, y) || item[x][y] != ONOTHING) continue;
            if (mitem[x][y].mon != MONST_NONE) continue;
            if (x == playerx && y == playery) continue;
            if (next_to_exit(x, y)) continue;
            place_machine(x, y);
            return;
          }
      return;
    }
}

/* ---- the roomba ---- */

/* Loose things a roomba can swallow: gold, gems, weapons, armour, potions,
 * scrolls, junk food... but never features, chests, traps or other features'
 * fixtures (the egg stays put so the pets feature keeps it). */
static int roomba_edible(int obj)
{
  if (obj >= OGOLDPILE && obj <= OSAPPHIRE) return obj != OCHEST;
  switch (obj)
  {
    case OCOOKIE: case OSPEED: case OACID: case OHASH: case OSHROOMS: case OCOKE:
    case OFIZZ: case OPIZZA: case OENERGYDRINK: case ORUBBERDUCK:
    case OHATCONE: case OHATPROPELLER: case OHATWIZARD: case OHATPARTY: case OHATTINFOIL:
    case OFISHINGROD: case OFISH: case OOLDBOOT: case OBOTTLE: case OCASSETTE:
      return 1;
    default:
      return 0;
  }
}

static int near_player(int x, int y)
{
  int dx = x - playerx, dy = y - playery;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  return !c[BLINDCOUNT] && dx <= 6 && dy <= 6;
}

static void roomba_eat(int x, int y)
{
  int obj = item[x][y];
  int n = mitem[x][y].n;

  if (n >= 6) return;   /* full: it just drives over things */
  mitem[x][y].it[n].item = (char) obj;
  mitem[x][y].it[n].itemarg = iarg[x][y];
  mitem[x][y].n++;
  item[x][y] = ONOTHING;
  iarg[x][y] = 0;
  if (know[x][y] != OUNKNOWN)
  {
    know[x][y] = ONOTHING;
    show1cell(x, y);
  }
  wild_event(US_EV_WILD_VENDING, 3, obj, 0);
  if (near_player(x, y))
    Printf("\nThe roomba hums contentedly as it eats %s.", objectname[obj]);
}

/* ---- buying and kicking ---- */

/* Something drops out of the machine: into the pack, or next to the player
 * if the pack is full. */
static void dispense(int obj, int arg)
{
  Printf("\nThe machine drops %s into the tray.", objectname[obj]);
  if (take(obj, arg) != 0)
  {
    createitem(playerx, playery, obj, arg);
    Print("\nIt rolls onto the floor.");
  }
}

static void vend_random(void)
{
  int r = rnd(100);
  if (r <= 35)      dispense(OFIZZ, 0);
  else if (r <= 65) dispense(OPIZZA, 0);
  else if (r <= 90) dispense(OENERGYDRINK, 0);
  else if (r <= 97) dispense(ORUBBERDUCK, 0);
  else
  {
    Print("\nHuh, that's not what you pressed.");
    dispense(OPOTION, newpotion());
  }
}

static void vend_buy(void)
{
  Print("buy.");
  if (c[GOLD] < VEND_PRICE)
  {
    Printf("\nYou don't have %d gold.", (long) VEND_PRICE);
    return;
  }
  c[GOLD] -= VEND_PRICE;
  wild_event(US_EV_WILD_VENDING, 0, 0, 0);
  Print("\nThe machine whirrs and clunks.");
  vend_random();
  UpdateStatus();
}

static void vend_kick(void)
{
  int r = rnd(100);
  int dmg;

  Print("kick.");
  wild_event(US_EV_WILD_VENDING, 1, 0, 0);
  if (iarg[playerx][playery] != VEND_WOBBLY && r <= 95)
  {
    if (r <= 50)
      Print("\nTHUNK.");
    else if (r <= 80)
    {
      Print("\nSomething falls out!");
      vend_random();
    }
    else
    {
      Print("\nThe machine wobbles ominously.");
      iarg[playerx][playery] = VEND_WOBBLY;
    }
    return;
  }

  /* topple: gone for good, and it lands on you */
  forget();
  iarg[playerx][playery] = 0;
  wild_event(US_EV_WILD_VENDING, 2, 0, 0);
  Print("\nThe vending machine topples onto you!");
  UlarnBeep();
  dmg = 9 + rnd(16);   /* 10..25 */
  losehp(DIED_WILD_VENDING, dmg);
  UpdateStatus();
}

static void vending_machine(void)
{
  int i;
  Print("\n\nThere is a humming vending machine here.");
  i = get_prompt_input("\nThe machine hums. (b)uy for 50 gold, (k)ick it, or (i)gnore? ",
                       "bki\033", 1);
  switch (i)
  {
    case 'b': vend_buy(); break;
    case 'k': vend_kick(); break;
    default:  Print("ignore."); break;
  }
}

/* ---- food and the duck ---- */

static int is_food(int obj)
{
  return obj == OFIZZ || obj == OPIZZA || obj == OENERGYDRINK;
}

static void eat_food(int obj)
{
  long heal;
  switch (obj)
  {
    case OFIZZ:
      Print("\nYou crack open the Fizz and chug it. Everything speeds up!");
      c[HASTESELF] += 30;
      Print("\nYou burp.");
      break;
    case OPIZZA:
      Print("\nYou eat the slice of pizza.");
      heal = c[HPMAX] / 4;
      if (heal < 1) heal = 1;
      c[HP] += heal;
      if (c[HP] > c[HPMAX]) c[HP] = c[HPMAX];
      if (rnd(100) <= 10)
      {
        Print("\nThat was a *good* slice.");
        c[CONSTITUTION]++;
      }
      break;
    case OENERGYDRINK:
      Print("\nYou down the energy drink. Your mind races.");
      c[SPELLS] += 8;
      if (c[SPELLS] > c[SPELLMAX]) c[SPELLS] = c[SPELLMAX];
      c[CONFUSE] += 5;
      Print("\nYour hands are shaking.");
      break;
    default:
      break;
  }
  recalc();
  UpdateStatus();
}

/* Talk the problem through with the duck: identifies the pack's potions and
 * scrolls, while the duck still has patience (c[WILD_DUCK] uses). */
static void rub_duck(void)
{
  int i, n = 0;

  if (c[WILD_DUCK] >= DUCK_CHARGES)
  {
    Print("\nThe duck has heard enough.");
    return;
  }
  c[WILD_DUCK]++;
  Print("\nYou explain your problem to the duck. The duck says nothing.");
  Print("\nYou suddenly understand everything.");
  for (i = 0; i < IVENSIZE; i++)
  {
    if (iven[i] == OPOTION && ivenarg[i] >= 0 && ivenarg[i] < MAXPOTION &&
        !potionknown[ivenarg[i]])
    {
      potionknown[ivenarg[i]] = 1;
      Printf("\n  %c) a magic potion of%s", i + 'a', potionname[ivenarg[i]]);
      n++;
    }
    else if (iven[i] == OSCROLL && ivenarg[i] >= 0 && ivenarg[i] < MAXSCROLL &&
             !scrollknown[ivenarg[i]])
    {
      scrollknown[ivenarg[i]] = 1;
      Printf("\n  %c) a magic scroll of%s", i + 'a', scrollname[ivenarg[i]]);
      n++;
    }
  }
  if (n == 0) Print("\n...about everything you're carrying, which you already knew.");
  if (c[WILD_DUCK] >= DUCK_CHARGES) Print("\nThe duck looks tired.");
}

/* Found junk food on the floor: like a fortune cookie. */
static void found_food(int obj)
{
  int i;
  Printf("\n\nYou find %s.", objectname[obj]);
  i = get_prompt_input("\nDo you (e) eat it, (t) take it, or (i) ignore it? ", "eit\033", 1);
  if (i == 'e')
  {
    Print("eat.");
    forget();
    eat_food(obj);
  }
  else if (i == 't')
  {
    Print("take.");
    if (take(obj, iarg[playerx][playery]) == 0) forget();
  }
  else
    Print("ignore.");
}

/* Found the duck: like the brass lamp. */
static void found_duck(void)
{
  int i;
  Print("\n\nYou find a rubber duck.");
  i = get_prompt_input("\nDo you want to (r) rub it, (t) take it, or (i) ignore it? ", "rit\033", 1);
  if (i == 'r')
  {
    Print("rub.");
    rub_duck();
  }
  else if (i == 't')
  {
    Print("take.");
    if (take(ORUBBERDUCK, 0) == 0) forget();
  }
  else
    Print("ignore.");
}

/* ---- hooks ---- */

void wild_vending_init(void) {}
void wild_vending_new_character(void) {}

void wild_vending_turn(void)
{
  int x, y;
  if (c[TIMESTOP]) return;   /* frozen like every other monster */
  for (x = 0; x < MAXX; x++)
    for (y = 0; y < MAXY; y++)
      if (mitem[x][y].mon == ROOMBA && roomba_edible(item[x][y]))
        roomba_eat(x, y);
}

void wild_vending_new_level(int lev) { (void) lev; }
void wild_vending_leaving_level(int lev) { (void) lev; }

void wild_vending_make_level(int lev)
{
  int x, y, tries;

  if (lev == 0)
    place_home_machine();
  else if (vending_level(lev) && rnd(100) <= VEND_CHANCE)
  {
    for (tries = 0; tries < 20; tries++)
    {
      if (!wild_free_cell(&x, &y)) break;
      if (next_to_exit(x, y)) continue;
      place_machine(x, y);
      break;
    }
  }

  if (lev >= ROOMBA_MINLEVEL && rnd(100) <= ROOMBA_CHANCE && wild_free_cell(&x, &y))
  {
    mitem[x][y].mon = ROOMBA;
    mitem[x][y].n = 0;
    hitp[x][y] = monster[ROOMBA].hitpoints;
    stealth[x][y] = STEALTH_AWAKE;
  }
}

void wild_vending_kill(int monst, int x, int y)
{
  /* hitm() calls this before it drops what the roomba ate */
  if (monst == ROOMBA && mitem[x][y].n > 0)
    Print("\nThe roomba beeps sadly and coughs up its contents.");
}

void wild_vending_level_up(int newlevel) { (void) newlevel; }
void wild_vending_take(int obj, int arg) { (void) obj; (void) arg; }

int wild_vending_object(int obj)
{
  if (obj == OVENDING) { vending_machine(); return 1; }
  if (is_food(obj)) { found_food(obj); return 1; }
  if (obj == ORUBBERDUCK) { found_duck(); return 1; }
  return 0;
}

int wild_vending_wear(int slot) { (void) slot; return 0; }
int wild_vending_takeoff(void) { return 0; }

int wild_vending_read(int slot)
{
  if (iven[slot] != ORUBBERDUCK) return 0;
  rub_duck();
  return 1;
}

int wild_vending_eat(int slot)
{
  int obj = iven[slot];
  if (!is_food(obj)) return 0;
  wild_use_up(slot);
  eat_food(obj);
  return 1;
}

int  wild_vending_dodge(void) { return 0; }
void wild_vending_player_hit(int dmg) { (void) dmg; }
void wild_vending_paint(void) {}
