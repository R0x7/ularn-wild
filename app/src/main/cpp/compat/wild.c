/* =============================================================================
 * FILENAME: wild.c
 *
 * Ularn: Wild Edition hub (see wild.h and docs/WILD_EDITION_PLAN.md).
 * Fans every upstream hook out to the features whose bit is on.
 * =============================================================================
 */
#include <stdlib.h>
#include <string.h>

#include "header.h"
#include "ularn_game.h"
#include "dungeon.h"
#include "player.h"
#include "monster.h"
#include "itm.h"
#include "saveutils.h"
#include "scores.h"
#include "ularn_win.h"
#include "../shared_state.h"
#include "wild.h"

extern UlarnShared *ularn_shared;

int wild_extra[WILD_EXTRA_COUNT];

/* Kill counts per species for this character (saved in the trailer). */
static int kill_counts[MONST_COUNT];

/* Mask from the environment, applied to a new character only. */
static int env_mask = -1;

static int read_env_mask(void)
{
  const char *e = getenv("ULARN_WILD");
  if (e == NULL || *e == 0) return WILDF_ALL;
  return (int) (strtol(e, NULL, 10) & WILDF_ALL);
}

int wild_on(int bit)
{
  return (c[WILD_FLAGS] & bit) != 0;
}

/* ---- fan-out helpers ---- */
#define EACH(F, call) \
  if (wild_on(WILDF_RADIO))   wild_radio_##F call; \
  if (wild_on(WILDF_CARDS))   wild_cards_##F call; \
  if (wild_on(WILDF_VENDING)) wild_vending_##F call; \
  if (wild_on(WILDF_HATS))    wild_hats_##F call; \
  if (wild_on(WILDF_COIN))    wild_coin_##F call; \
  if (wild_on(WILDF_GLITCH))  wild_glitch_##F call; \
  if (wild_on(WILDF_FISHING)) wild_fishing_##F call; \
  if (wild_on(WILDF_PETS))    wild_pets_##F call; \
  if (wild_on(WILDF_PERKS))   wild_perks_##F call; \
  if (wild_on(WILDF_WEATHER)) wild_weather_##F call; \
  if (wild_on(WILDF_BOUNTY))  wild_bounty_##F call;

/* First feature that returns non-zero handles it. Pets go before fishing so
 * eating a fish next to your pet feeds the pet instead. */
#define FIRST(F, call) \
  if (wild_on(WILDF_RADIO)   && wild_radio_##F call)   return 1; \
  if (wild_on(WILDF_CARDS)   && wild_cards_##F call)   return 1; \
  if (wild_on(WILDF_VENDING) && wild_vending_##F call) return 1; \
  if (wild_on(WILDF_HATS)    && wild_hats_##F call)    return 1; \
  if (wild_on(WILDF_COIN)    && wild_coin_##F call)    return 1; \
  if (wild_on(WILDF_GLITCH)  && wild_glitch_##F call)  return 1; \
  if (wild_on(WILDF_PETS)    && wild_pets_##F call)    return 1; \
  if (wild_on(WILDF_FISHING) && wild_fishing_##F call) return 1; \
  if (wild_on(WILDF_PERKS)   && wild_perks_##F call)   return 1; \
  if (wild_on(WILDF_WEATHER) && wild_weather_##F call) return 1; \
  if (wild_on(WILDF_BOUNTY)  && wild_bounty_##F call)  return 1; \
  return 0;

void wild_init(void)
{
  env_mask = read_env_mask();
  memset(kill_counts, 0, sizeof kill_counts);
  memset(wild_extra, 0, sizeof wild_extra);
  /* Features get their init even before the mask is known: nothing that
   * depends on c[] belongs in _init. The mask is applied in
   * wild_new_character() (new game) or wild_restore() (saved game). */
  wild_radio_init();   wild_cards_init();   wild_vending_init(); wild_hats_init();
  wild_coin_init();    wild_glitch_init();  wild_fishing_init(); wild_pets_init();
  wild_perks_init();   wild_weather_init();  wild_bounty_init();
}

void wild_new_character(void)
{
  int i;
  if (env_mask < 0) env_mask = read_env_mask();
  c[WILD_FLAGS] = env_mask;
  for (i = WILD_FLAGS + 1; i < ATTRIBUTE_COUNT; i++) c[i] = 0;
  memset(kill_counts, 0, sizeof kill_counts);
  memset(wild_extra, 0, sizeof wild_extra);
  EACH(new_character, ())
}

void wild_turn(void)
{
  if (c[WILD_FLAGS] == 0) return;
  EACH(turn, ())
}

void wild_new_level(int lev)      { EACH(new_level, (lev)) }
void wild_leaving_level(int lev)  { EACH(leaving_level, (lev)) }
void wild_make_level(int lev)     { EACH(make_level, (lev)) }

void wild_kill(int monst, int x, int y)
{
  if (monst > 0 && monst < MONST_COUNT) kill_counts[monst]++;
  wild_event(US_EV_KILL, monst, x, y);
  EACH(kill, (monst, x, y))
}

int wild_kill_count(int monst)
{
  return (monst > 0 && monst < MONST_COUNT) ? kill_counts[monst] : 0;
}

void wild_level_up(int newlevel)
{
  wild_event(US_EV_LEVEL_UP, newlevel, 0, 0);
  EACH(level_up, (newlevel))
}

void wild_take(int obj, int arg)
{
  wild_event(US_EV_TAKE, obj, arg, 0);
  EACH(take, (obj, arg))
}

void wild_object(int obj)
{
  /* An unowned Wild object behaves like any other item: offer to take it. */
  if (wild_on(WILDF_RADIO)   && wild_radio_object(obj))   return;
  if (wild_on(WILDF_CARDS)   && wild_cards_object(obj))   return;
  if (wild_on(WILDF_VENDING) && wild_vending_object(obj)) return;
  if (wild_on(WILDF_HATS)    && wild_hats_object(obj))    return;
  if (wild_on(WILDF_COIN)    && wild_coin_object(obj))    return;
  if (wild_on(WILDF_GLITCH)  && wild_glitch_object(obj))  return;
  if (wild_on(WILDF_FISHING) && wild_fishing_object(obj)) return;
  if (wild_on(WILDF_PETS)    && wild_pets_object(obj))    return;
  if (wild_on(WILDF_PERKS)   && wild_perks_object(obj))   return;
  if (wild_on(WILDF_WEATHER) && wild_weather_object(obj)) return;
  if (wild_on(WILDF_BOUNTY)  && wild_bounty_object(obj))  return;
  wild_finditem(obj);
}

int wild_wear(int slot)    { FIRST(wear, (slot)) }
int wild_takeoff(void)     { FIRST(takeoff, ()) }
int wild_read(int slot)    { FIRST(read, (slot)) }
int wild_eat(int slot)     { FIRST(eat, (slot)) }
int wild_dodge(void)       { FIRST(dodge, ()) }

void wild_player_hit(int dmg)
{
  wild_event(US_EV_PLAYER_HIT, dmg, 0, 0);
  EACH(player_hit, (dmg))
}

void wild_spell(int spell) { wild_event(US_EV_SPELL, spell, 0, 0); }
void wild_gold(int amount) { wild_event(US_EV_GOLD, amount, 0, 0); }
void wild_died(int won)    { wild_event(won ? US_EV_WON : US_EV_DIED, 0, 0, 0); }

/* ---- save trailer ---- */
#define WILD_TRAILER_MAGIC 0x444c4957   /* 'WILD' little-endian */

void wild_save(FILE *fp)
{
  int32_t magic = WILD_TRAILER_MAGIC;
  int32_t n = MONST_COUNT, m = WILD_EXTRA_COUNT;
  int32_t v;
  int i;
  /* One int per bwrite(): the save checksum (saveutils.c: sum()) depends on
   * how the bytes are split into calls, and wild_restore() reads them one
   * at a time. Writing the arrays as one block made any non-zero entry
   * (a kill, a pet...) look like an altered save on restore. */
  bwrite(fp, (char *) &magic, sizeof magic);
  bwrite(fp, (char *) &n, sizeof n);
  for (i = 0; i < n; i++) { v = kill_counts[i]; bwrite(fp, (char *) &v, sizeof v); }
  bwrite(fp, (char *) &m, sizeof m);
  for (i = 0; i < m; i++) { v = wild_extra[i]; bwrite(fp, (char *) &v, sizeof v); }
}

int wild_restore(FILE *fp)
{
  int32_t magic = 0, n = 0, m = 0, v;
  int i;
  bread(fp, (char *) &magic, sizeof magic);
  if (magic != WILD_TRAILER_MAGIC) return -1;
  bread(fp, (char *) &n, sizeof n);
  if (n < 0 || n > 4096) return -1;
  memset(kill_counts, 0, sizeof kill_counts);
  for (i = 0; i < n; i++)
  {
    bread(fp, (char *) &v, sizeof v);
    if (i < MONST_COUNT) kill_counts[i] = v;
  }
  bread(fp, (char *) &m, sizeof m);
  if (m < 0 || m > 4096) return -1;
  memset(wild_extra, 0, sizeof wild_extra);
  for (i = 0; i < m; i++)
  {
    bread(fp, (char *) &v, sizeof v);
    if (i < WILD_EXTRA_COUNT) wild_extra[i] = v;
  }
  return 0;
}

/* ---- display ---- */
void wild_paint(void)
{
  UlarnShared *s = ularn_shared;
  int i;
  if (s == NULL) return;
  s->wild_flags = (int32_t) ((c[WILD_FLAGS] & 0xffff) | ((c[WILD_PERKS] & 0xffff) << 16));
  s->hat_tile = 0;
  s->glitch = (int32_t) c[WILD_GLITCH];
  s->weather = (int32_t) c[WILD_WEATHER];
  s->coin_price = (int32_t) c[WILD_COIN_PRICE];
  s->pet_x = -1;
  s->pet_y = -1;
  for (i = 0; i < MONST_COUNT && i < US_KILL_COUNTS; i++)
    s->kill_counts[i] = kill_counts[i];
  EACH(paint, ())
}

void wild_event(int type, int a, int b, int cc)
{
  UlarnShared *s = ularn_shared;
  int i;
  if (s == NULL) return;
  i = s->ev_seq & (US_EVENTS - 1);
  s->ev[i][0] = type;
  s->ev[i][1] = a;
  s->ev[i][2] = b;
  s->ev[i][3] = cc;
  __sync_synchronize();
  s->ev_seq++;
}

/* ---- item uses (action.c lists Wild objects under read/wear/eat) ---- */
int wild_item_use(int obj)
{
  switch (obj)
  {
    case OHATCONE: case OHATPROPELLER: case OHATWIZARD: case OHATPARTY: case OHATTINFOIL:
      return WILD_USE_WEAR;
    case OFIZZ: case OPIZZA: case OENERGYDRINK: case OFISH:
      return WILD_USE_EAT;
    case ORUBBERDUCK: case OBOTTLE:
      return WILD_USE_READ;
    default:
      return WILD_USE_NONE;
  }
}

/* ---- wizard debug menu ---- */
int wild_debug(void)
{
  int t, a, x, y;
  t = get_prompt_input("\nWild debug: (o)bject here, (m)onster next to you, (g)old, (x)p level, "
                       "(t)urns, (h)eal, (d)amage, (f)lags? ", "omgxthdf\033", 1);
  switch (t)
  {
    case 'o':
      Print(" Item: ");
      t = get_num_input(0);
      Print(" Arg: ");
      a = get_num_input(0);
      if (t <= 0 || t >= OCOUNT) { Print("\nNo such object."); return ACTION_NULL; }
      item[playerx][playery] = (char) t;
      iarg[playerx][playery] = (short) a;
      know[playerx][playery] = (char) t;
      Printf("\nPlaced %s under you.", objectname[t]);
      return ACTION_WAIT;
    case 'm':
      Print(" Monster: ");
      t = get_num_input(0);
      if (t <= 0 || t >= MONST_COUNT) { Print("\nNo such monster."); return ACTION_NULL; }
      createmonster(t);
      Printf("\nCreated %s.", monster[t].name);
      return ACTION_WAIT;
    case 'g':
      c[GOLD] += 1000;
      Print("\n+1000 gold.");
      return ACTION_NULL;
    case 'x':
      raiselevel();
      Print("\nLevel raised.");
      return ACTION_NULL;
    case 't':
      Print(" Turns: ");
      t = get_num_input(0);
      for (a = 0; a < t; a++) { gtime++; wild_turn(); }
      Printf("\n%d turns passed (wild_turn only).", t);
      return ACTION_NULL;
    case 'h':
      c[HP] = c[HPMAX];
      c[SPELLS] = c[SPELLMAX];
      Print("\nHealed.");
      return ACTION_NULL;
    case 'd':
      /* a blow of N hit points through losehp(), like any monster's: the
       * way to test what happens at death's door (Second Wind) */
      Print(" Damage: ");
      t = get_num_input(0);
      if (t <= 0) return ACTION_NULL;
      Printf("\nYou take %d damage.", t);
      losehp(DIED_OWN_MAGIC, t);   /* a wizard asked for it */
      UpdateStatus();
      return ACTION_NULL;
    case 'f':
      Printf("\nwild flags 0x%lx hat %ld perks 0x%lx egg %ld pet %ld coins %ld price %ld glitch %ld duck %ld weather 0x%lx",
             (long) c[WILD_FLAGS], (long) c[WILD_HAT], (long) c[WILD_PERKS], (long) c[WILD_EGG],
             (long) c[WILD_PET_MON], (long) c[WILD_COINS], (long) c[WILD_COIN_PRICE],
             (long) c[WILD_GLITCH], (long) c[WILD_DUCK], (long) c[WILD_WEATHER]);
      return ACTION_NULL;
    default:
      (void) x; (void) y;
      return ACTION_NULL;
  }
}

/* ---- helpers ---- */
int wild_is_pet(int x, int y)
{
  return wild_on(WILDF_PETS) && wild_extra[WX_PET_LEVEL] == level + 1 &&
         c[WILD_PET_MON] != 0 && wild_extra[WX_PET_X] == x && wild_extra[WX_PET_Y] == y &&
         x >= 0 && x < MAXX && y >= 0 && y < MAXY && mitem[x][y].mon == c[WILD_PET_MON];
}

int wild_free_cell(int *x, int *y)
{
  int tries;
  for (tries = 0; tries < 400; tries++)
  {
    int cx = rnd(MAXX - 2), cy = rnd(MAXY - 2);
    if (item[cx][cy] == ONOTHING && mitem[cx][cy].mon == MONST_NONE &&
        !(cx == playerx && cy == playery))
    {
      *x = cx; *y = cy;
      return 1;
    }
  }
  return 0;
}
