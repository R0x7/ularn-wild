/* =============================================================================
 * FILENAME: wild_perks.c
 *
 * Ularn: Wild Edition feature "perks". See docs/WILD_EDITION_PLAN.md for the
 * design and wild.h for when each hook is called. Everything in this file
 * runs only when the feature's bit is on (wild.c checks before calling; the
 * helpers in wild_perks.h check themselves).
 *
 * Each level-up adds one to c[WILD_PERK_PICKS]. At the end of the next turn
 * (wild_turn(), after the player's command, before the monsters move) the
 * player is offered three random perks they don't have yet; the choice is
 * a bit (1 << PERK_X) in c[WILD_PERKS]. Escape puts the choice off for a few
 * turns (it comes back by itself, so a pick is never lost and the prompt can
 * never trap the player). With every perk taken, a pick is +5 max HP.
 *
 * All the state is in c[] (saved with the character) except the levels
 * Second Wind has been used on, which are wild_extra[WX_PERK_WIND_USED]
 * (save trailer), and the Escape delay, which is deliberately not saved: a restored game with a
 * pick pending asks again after the first command.
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
#include "../shared_state.h"
#include "wild.h"
#include "wild_perks.h"

/* c[WILD_PERKS] bit 15: Night Owl's +2 is on the stats right now. Kept in
 * c[] (not a static) so a save made at night restores consistently. Above
 * every perk bit, so Kotlin (which only names bits 0..9) ignores it. */
#define PERK_OWL_ON        (1L << 15)

#define PERK_SKIN_AC        2   /* Thick Skin */
#define PERK_CANNON_WC      4   /* Glass Cannon */
#define PERK_NIMBLE_DODGE  10   /* Nimble: % of melee blows that miss */
#define PERK_BOOKWORM_FREE 25   /* Bookworm: % of casts that cost nothing */
#define PERK_OWL_STAT       2   /* Night Owl: + to every stat at night */
#define PERK_EXHAUSTED_HP   5   /* every perk taken: a pick is +5 max HP */
#define PERK_DEFER_TURNS   10   /* Escape at the prompt: ask again after this */

static const char *perk_name[PERK_COUNT] =
{
  "Magnet Hands", "Thick Skin", "Vampiric", "Bookworm", "Lucky",
  "Nimble", "Haggler", "Glass Cannon", "Second Wind", "Night Owl"
};

static const char *perk_desc[PERK_COUNT] =
{
  "gold next to you slides over to you",
  "+2 armor class",
  "heal 1 HP per kill (2 when below 1/4 HP)",
  "1 cast in 4 costs no spell point",
  "monsters drop their loot twice",
  "10% of blows aimed at you miss",
  "10% off at the DND store, +10% at the trading post",
  "+4 weapon class, but -10% max HP",
  "once per dungeon level, a deadly blow leaves you at 1 HP",
  "+2 to all stats at night in town"
};

/* Night Owl when there is no day/night (the weather feature is off). */
static const char *owl_desc_no_weather = "+1 intelligence, permanently";

/* Turn before which an Escaped choice is not offered again. */
static long defer_until = 0;

/* ---- helpers for the shared insertion points (wild_perks.h) ---- */

int wild_perk(int p)
{
  return wild_on(WILDF_PERKS) && p >= 0 && p < PERK_COUNT && (c[WILD_PERKS] & (1L << p)) != 0;
}

const char *wild_perk_owned_name(int p)
{
  return wild_perk(p) ? perk_name[p] : NULL;
}

int wild_perks_ac(void)
{
  return wild_perk(PERK_SKIN) ? PERK_SKIN_AC : 0;
}

int wild_perks_wc(void)
{
  return wild_perk(PERK_CANNON) ? PERK_CANNON_WC : 0;
}

/* Every spell costs exactly one point, so "1 less, min 1" would do nothing:
 * Bookworm makes a quarter of the casts free instead. */
int wild_perks_free_spell(void)
{
  if (!wild_perk(PERK_BOOKWORM) || rnd(100) > PERK_BOOKWORM_FREE) return 0;
  Print(" (free!)");
  return 1;
}

int wild_perks_lucky(void)
{
  return wild_perk(PERK_LUCKY);
}

long wild_perks_price(long price)
{
  return wild_perk(PERK_HAGGLER) ? price - price / 10 : price;
}

long wild_perks_sell(long value)
{
  return (wild_perk(PERK_HAGGLER) && value > 0) ? value + value / 10 : value;
}

/* ---- the perks themselves ---- */

static int is_gold(int obj)
{
  return obj == OGOLDPILE || obj == ODGOLD || obj == OKGOLD || obj == OMAXGOLD;
}

/* Magnet Hands: one neighbouring gold pile moves under the player, where
 * the main loop's lookforobject() picks it up before the monsters move. */
static void magnet(void)
{
  int dx, dy, x, y;
  if (!wild_perk(PERK_MAGNET) || item[playerx][playery] != ONOTHING) return;
  for (dy = -1; dy <= 1; dy++)
    for (dx = -1; dx <= 1; dx++)
    {
      x = playerx + dx; y = playery + dy;
      if ((dx == 0 && dy == 0) || x < 0 || x >= MAXX || y < 0 || y >= MAXY) continue;
      if (!is_gold((unsigned char) item[x][y])) continue;
      item[playerx][playery] = item[x][y];
      iarg[playerx][playery] = iarg[x][y];
      item[x][y] = ONOTHING;
      iarg[x][y] = 0;
      if (know[x][y] != OUNKNOWN)
      {
        know[x][y] = ONOTHING;
        show1cell(x, y);
      }
      Print("\nThe gold slides over to you.");
      return;
    }
}

/* The weather feature keeps the hour of the day in the low byte. */
static int is_night(void)
{
  int hour = (int) (c[WILD_WEATHER] & 0xff);
  return level == 0 && (hour >= 21 || hour <= 5);
}

/* Night Owl with weather on: the +2 is on the stats exactly while the
 * player is in town at night. */
static void owl(void)
{
  int want, have, i;
  if (!wild_perk(PERK_OWL) || !wild_on(WILDF_WEATHER)) return;
  want = is_night();
  have = (c[WILD_PERKS] & PERK_OWL_ON) != 0;
  if (want == have) return;
  for (i = ABILITY_FIRST; i <= ABILITY_LAST; i++)
    adjust_ability((AttributeType) i, want ? PERK_OWL_STAT : -PERK_OWL_STAT);
  if (want)
  {
    c[WILD_PERKS] |= PERK_OWL_ON;
    Print("\nThe night air sharpens your senses.");
  }
  else
  {
    c[WILD_PERKS] &= ~PERK_OWL_ON;
    Print("\nThe night owl in you settles down.");
  }
  UpdateStatus();
}

/* What taking perk `p` does at once (the rest works through the helpers). */
static void grant(int p)
{
  c[WILD_PERKS] |= 1L << p;
  wild_event(US_EV_WILD_PERK, p, 0, 0);
  Printf("\nYou gain the %s perk!", perk_name[p]);
  switch (p)
  {
    case PERK_CANNON:
      c[HPMAX] -= c[HPMAX] / 10;
      if (c[HPMAX] < 1) c[HPMAX] = 1;
      if (c[HP] > c[HPMAX]) c[HP] = c[HPMAX];
      break;
    case PERK_OWL:
      if (!wild_on(WILDF_WEATHER))
      {
        adjust_ability(INTELLIGENCE, 1);
        Print(" You feel wiser in the dark.");
      }
      else
        owl();
      break;
    default:
      break;
  }
  recalc();
  UpdateStatus();
}

/* Offers one pick. Returns 1 when a pick was used up, 0 when deferred. */
static int offer(void)
{
  int avail[PERK_COUNT], pick[3];
  char answers[5];
  int n = 0, k, j, r, ch;

  for (j = 0; j < PERK_COUNT; j++)
    if (!(c[WILD_PERKS] & (1L << j))) avail[n++] = j;

  /* Every perk taken: all pending picks become max HP in one go (a jump
   * of many levels, e.g. wizard '_', should not print a line per level). */
  if (n == 0)
  {
    k = (int) c[WILD_PERK_PICKS] * PERK_EXHAUSTED_HP;
    raisemhp(k);
    c[WILD_PERK_PICKS] = 0;
    Printf("\nLEVEL UP! You have every perk already: +%d max HP instead.", k);
    wild_event(US_EV_WILD_PERK, -1, k, 0);
    UpdateStatus();
    return 1;
  }

  k = n < 3 ? n : 3;
  for (j = 0; j < k; j++)
  {
    r = rund(n);
    pick[j] = avail[r];
    avail[r] = avail[--n];
    answers[j] = (char) ('a' + j);
  }
  answers[k] = ESC;
  answers[k + 1] = 0;

  Print("\nLEVEL UP! Choose a perk:");
  for (j = 0; j < k; j++)
    Printf("\n(%c) %s: %s", 'a' + j, perk_name[pick[j]],
           (pick[j] == PERK_OWL && !wild_on(WILDF_WEATHER)) ? owl_desc_no_weather : perk_desc[pick[j]]);
  ch = get_prompt_input("\nWhich perk? (Esc: decide later) ", answers, 1);

  if (ch < 'a' || ch >= 'a' + k)
  {
    defer_until = gtime + PERK_DEFER_TURNS;
    Print("\nYou can choose your perk in a little while.");
    return 0;
  }
  c[WILD_PERK_PICKS]--;
  grant(pick[ch - 'a']);
  return 1;
}

/* ---- hooks ---- */

void wild_perks_init(void) { defer_until = 0; }
void wild_perks_new_character(void) { defer_until = 0; }

void wild_perks_turn(void)
{
  if (c[WILD_PERK_PICKS] < 0) c[WILD_PERK_PICKS] = 0;
  if (defer_until > gtime + PERK_DEFER_TURNS) defer_until = 0;   /* clock went back */
  while (c[WILD_PERK_PICKS] > 0 && gtime >= defer_until)
    if (!offer()) break;
  magnet();
  owl();
}

void wild_perks_new_level(int lev) { (void) lev; }
void wild_perks_leaving_level(int lev) { (void) lev; }
void wild_perks_make_level(int lev) { (void) lev; }

/* Vampiric: a kill heals; more when it is needed most. */
void wild_perks_kill(int monst, int x, int y)
{
  (void) monst; (void) x; (void) y;
  if (!wild_perk(PERK_VAMPIRE)) return;
  raisehp(c[HP] < c[HPMAX] / 4 ? 2 : 1);
  UpdateStatus();
}

void wild_perks_level_up(int newlevel)
{
  (void) newlevel;
  c[WILD_PERK_PICKS]++;
}

void wild_perks_take(int obj, int arg) { (void) obj; (void) arg; }
int  wild_perks_object(int obj) { (void) obj; return 0; }
int  wild_perks_wear(int slot) { (void) slot; return 0; }
int  wild_perks_takeoff(void) { return 0; }
int  wild_perks_read(int slot) { (void) slot; return 0; }
int  wild_perks_eat(int slot) { (void) slot; return 0; }

/* Nimble. */
int wild_perks_dodge(void)
{
  return wild_perk(PERK_NIMBLE) && rnd(100) <= PERK_NIMBLE_DODGE;
}

/* The levels Second Wind has been used on: bit n = level n (0 is the town,
 * the volcano's levels follow the dungeon's, so NLEVELS bits in all).
 * Saves from before this record kept only the level of the latest use, + 1,
 * in WX_PERK_WIND_LEVEL; that one is carried over the first time through. */
static int wind_used(void)
{
  int old = wild_extra[WX_PERK_WIND_LEVEL];
  if (old > 0 && old <= NLEVELS) wild_extra[WX_PERK_WIND_USED] |= 1 << (old - 1);
  wild_extra[WX_PERK_WIND_LEVEL] = 0;
  return wild_extra[WX_PERK_WIND_USED];
}

/* Second Wind: called before losehp() takes `dmg` off c[HP]. Topping HP up
 * to dmg + 1 leaves the player at exactly 1 HP after the blow. Once per
 * level for the whole game: remembering only the latest level (as this did
 * at first) armed it again on a level as soon as it was used on another. */
void wild_perks_player_hit(int dmg)
{
  if (!wild_perk(PERK_WIND) || dmg <= 0 || c[HP] - dmg > 0) return;
  if (level < 0 || level >= NLEVELS) return;
  if (wind_used() & (1 << level)) return;
  wild_extra[WX_PERK_WIND_USED] |= 1 << level;
  c[HP] = dmg + 1;
  Print("\nSecond wind! You cling to life.");
  wild_event(US_EV_WILD_PERK, PERK_WIND, 1, 0);
}

void wild_perks_paint(void) {}
