/* =============================================================================
 * FILENAME: wild_bounty.c
 *
 * Ularn: Wild Edition feature "bounty board". See docs/wild/bounty.md and
 * wild.h for when each hook is called. Everything in this file runs only
 * when the feature's bit is on (wild.c checks before calling; the helper in
 * wild_bounty.h checks itself).
 *
 * The town crier keeps one WANTED poster up at a time: a monster species
 * from about one level deeper than the character has been, and a gold
 * reward that grows with that depth. Killing one of them claims the bounty;
 * the gold is paid the next time the character is in town, where a new
 * poster goes up. A poster nobody answers is replaced after a while (or at
 * once if the species has been genocided), so a bounty can never get stuck.
 *
 * All the state is in the save trailer (wild_extra[WX_BOUNTY_*]); the
 * "already handled this town visit" flag is deliberately not saved, so a
 * restored game in town repeats the notice once.
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
#include "wild_bounty.h"

/* Turns an unclaimed poster stays up before the crier replaces it. */
#define BOUNTY_EXPIRE 2500

extern UlarnShared *ularn_shared;

/* The side panel's BOUNTY section (shared_state.h: bounty_info). Runs on
 * every map repaint (wild_paint() clears the fields first, so with the
 * feature off nothing is shown) and whenever the poster changes, since the
 * map is not always repainted after a turn. */
static void bounty_publish(void)
{
  UlarnShared *s = ularn_shared;
  int m = wild_extra[WX_BOUNTY_MON];
  if (s == NULL) return;
  memset(s->bounty_info, 0, sizeof s->bounty_info);
  if (m <= 0 || m >= MONST_COUNT) return;
  s->bounty_info[0] = m;
  s->bounty_info[1] = wild_extra[WX_BOUNTY_GOLD];
  s->bounty_info[2] = wild_extra[WX_BOUNTY_CLAIMED] ? 1 : 0;
  /* taken down at the first town visit after this turn (the app counts
   * down with game_time); a genocided species' poster at the next one */
  if (!wild_extra[WX_BOUNTY_CLAIMED])
    s->bounty_info[3] = (monster[m].flags & FL_GENOCIDED) != 0 ? 0 :
                        wild_extra[WX_BOUNTY_POSTED] + BOUNTY_EXPIRE;
}

/* Has this stay in town been dealt with yet? (reset on every level change) */
static int town_handled;

static int depth_for_bounty(void)
{
  int i, deepest = 0;
  for (i = 1; i <= DBOTTOM; i++)
    if (beenhere[i]) deepest = i;
  for (i = MAXLEVEL; i <= VBOTTOM; i++)
    if (beenhere[i]) deepest = 12;   /* the volcano: as deep as makemonst() goes */
  deepest++;
  if (deepest > 12) deepest = 12;
  return deepest;
}

static void clear_bounty(void)
{
  wild_extra[WX_BOUNTY_MON] = 0;
  wild_extra[WX_BOUNTY_GOLD] = 0;
  wild_extra[WX_BOUNTY_CLAIMED] = 0;
  wild_extra[WX_BOUNTY_POSTED] = 0;
}

/* Put up a new poster. Lemmings are too easy to be worth a poster, so
 * makemonst() is asked again if it offers one. */
static void post_bounty(void)
{
  int lev = depth_for_bounty();
  int tries, m = 0;
  for (tries = 0; tries < 20; tries++)
  {
    m = makemonst(lev);
    if (m > 0 && m < MAXMONST && m != LEMMING && (monster[m].flags & FL_GENOCIDED) == 0)
      break;
    m = 0;
  }
  if (m == 0) return;   /* nothing suitable: try again next visit */
  wild_extra[WX_BOUNTY_MON] = m;
  wild_extra[WX_BOUNTY_GOLD] = 100 + 25 * lev * lev;
  wild_extra[WX_BOUNTY_CLAIMED] = 0;
  wild_extra[WX_BOUNTY_POSTED] = (int) gtime;
  Printf("\nThe town crier nails up a poster: WANTED, one %s. Reward: %d gold.",
         monster[m].name, wild_extra[WX_BOUNTY_GOLD]);
}

static void town_visit(void)
{
  int m = wild_extra[WX_BOUNTY_MON];

  if (m != 0 && wild_extra[WX_BOUNTY_CLAIMED])
  {
    c[GOLD] += wild_extra[WX_BOUNTY_GOLD];
    Printf("\nThe town crier counts out your bounty: %d gold for the %s.",
           wild_extra[WX_BOUNTY_GOLD], monster[m].name);
    clear_bounty();
  }
  else if (m != 0 && ((monster[m].flags & FL_GENOCIDED) != 0 ||
                      gtime - wild_extra[WX_BOUNTY_POSTED] > BOUNTY_EXPIRE))
  {
    Printf("\nThe faded poster for the %s has been taken down.", monster[m].name);
    clear_bounty();
  }
  else if (m != 0)
  {
    Printf("\nStill WANTED: one %s. Reward: %d gold.", monster[m].name,
           wild_extra[WX_BOUNTY_GOLD]);
    return;
  }

  post_bounty();
}

void wild_bounty_init(void) { town_handled = 0; }
void wild_bounty_new_character(void) { town_handled = 0; }

void wild_bounty_turn(void)
{
  if (level != 0 || town_handled) return;
  town_handled = 1;
  town_visit();
  bounty_publish();
}

void wild_bounty_new_level(int lev) { (void) lev; town_handled = 0; }
void wild_bounty_leaving_level(int lev) { (void) lev; }
void wild_bounty_make_level(int lev) { (void) lev; }

void wild_bounty_kill(int monst, int x, int y)
{
  (void) x; (void) y;
  if (monst == 0 || monst != wild_extra[WX_BOUNTY_MON] || wild_extra[WX_BOUNTY_CLAIMED]) return;
  wild_extra[WX_BOUNTY_CLAIMED] = 1;
  Printf("\nThat %s was WANTED! Collect %d gold from the town crier.",
         monster[monst].name, wild_extra[WX_BOUNTY_GOLD]);
  if (level == 0) town_handled = 0;   /* killed in town: pay out next turn */
  bounty_publish();
}

void wild_bounty_level_up(int newlevel) { (void) newlevel; }
void wild_bounty_take(int obj, int arg) { (void) obj; (void) arg; }
int  wild_bounty_object(int obj) { (void) obj; return 0; }
int  wild_bounty_wear(int slot) { (void) slot; return 0; }
int  wild_bounty_takeoff(void) { return 0; }
int  wild_bounty_read(int slot) { (void) slot; return 0; }
int  wild_bounty_eat(int slot) { (void) slot; return 0; }
int  wild_bounty_dodge(void) { return 0; }
void wild_bounty_player_hit(int dmg) { (void) dmg; }
void wild_bounty_paint(void) { bounty_publish(); }

const char *wild_bounty_text(void)
{
  static char buf[120];
  int m;
  if (!wild_on(WILDF_BOUNTY)) return NULL;
  m = wild_extra[WX_BOUNTY_MON];
  if (m <= 0 || m >= MONST_COUNT) return NULL;
  if (wild_extra[WX_BOUNTY_CLAIMED])
    snprintf(buf, sizeof buf, "%s: done! Collect %d gold in town.",
             monster[m].name, wild_extra[WX_BOUNTY_GOLD]);
  else
    snprintf(buf, sizeof buf, "WANTED: one %s. Reward: %d gold.",
             monster[m].name, wild_extra[WX_BOUNTY_GOLD]);
  return buf;
}
