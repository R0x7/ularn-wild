/* =============================================================================
 * FILENAME: wild_coin.c
 *
 * Ularn: Wild Edition feature "coin" (WILDF_COIN). See
 * docs/WILD_EDITION_PLAN.md section 3.5 and docs/wild/coin.md.
 *
 *   - LarnCoin: a volatile parody cryptocurrency. c[WILD_COIN_PRICE] is the
 *     price in gold, c[WILD_COINS] the player's holdings (both saved with c[]).
 *     Every COIN_TICK turns the price takes a random walk step with momentum;
 *     now and then there is news (a big jump), and above COIN_RUG_PRICE the
 *     founder may rug-pull it down to 1.
 *   - Trading: the Bank of Larn menu gets (b)uy / (c)ash out (store.c calls
 *     the wild_coin_bank_* helpers), and an ATM (OCOINATM) outside the bank on
 *     the home level buys and sells for cash with a 5% fee.
 *   - Coins count towards the final score at their current price (scores.c).
 *   - The tax auditor (TAXAUDITOR): while the player owes taxes and is on the
 *     home level, one may turn up by the dungeon entrance. Its special attack
 *     (monster.c spattack() case 17) garnishes 10% of the carried gold towards
 *     the taxes instead of a blow (only a failed garnish roll lets an
 *     ordinary hit through); it leaves once the taxes are paid. The bank is
 *     frozen while taxes are owed (upstream), but the ATM still works.
 *
 * Momentum and the "price moved this turn" flag are not saved: losing them
 * on a restore only costs one step of momentum.
 *
 * Everything in this file runs only when the feature's bit is on (wild.c
 * checks before calling the hooks; the helpers in wild_coin.h check
 * themselves).
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
#include "scores.h"
#include "../shared_state.h"
#include "wild.h"
#include "wild_coin.h"

extern UlarnShared *ularn_shared;

#define COIN_START_PRICE 100
#define COIN_MAX_PRICE   99999
#define COIN_MAX_HOLD    9999999L
#define COIN_TICK        10      /* turns between price steps */
#define COIN_NEWS_ODDS   400     /* 1 in N turns */
#define COIN_RUG_PRICE   1500
#define COIN_RUG_ODDS    50      /* 1 in N price steps above COIN_RUG_PRICE */
#define ATM_FEE_PCT      5
#define AUDITOR_ODDS     150     /* 1 in N turns on the home level */
#define GOLD_CAP         2000000000L   /* keeps sums inside a 32-bit long */

/* US_EV_WILD_COIN: a = what happened */
#define COIN_EV_TICK    0   /* b = price */
#define COIN_EV_NEWS    1   /* b = price, c = story */
#define COIN_EV_RUG     2
#define COIN_EV_AUDITOR 3   /* b = 0 arrives, 1 garnish (c = gold), 2 leaves */

static int ticks;      /* turns since the last price step */
static int momentum;   /* +1 / -1: the way the last step went */
static int price_moved;  /* the price changed during the latest turn */

/* ---- the price ---- */

static long price(void)
{
  if (c[WILD_COIN_PRICE] < 1) c[WILD_COIN_PRICE] = COIN_START_PRICE;
  return c[WILD_COIN_PRICE];
}

static void set_price(long p)
{
  if (p < 1) p = 1;
  if (p > COIN_MAX_PRICE) p = COIN_MAX_PRICE;
  if (p != c[WILD_COIN_PRICE]) price_moved = 1;
  c[WILD_COIN_PRICE] = p;
}

/* p +/- pct%, moving at least 1 gold */
static long nudge(long p, int pct, int up)
{
  long step = p * pct / 100;
  if (step < 1) step = 1;
  return up ? p + step : p - step;
}

static void price_step(void)
{
  int up;
  /* momentum: two times in three the price keeps going the same way */
  if (momentum == 0)
    up = rnd(2) == 1;
  else
    up = (rnd(3) <= 2) ? (momentum > 0) : (momentum < 0);
  momentum = up ? 1 : -1;
  set_price(nudge(price(), rnd(7), up));
  wild_event(US_EV_WILD_COIN, COIN_EV_TICK, (int) price(), 0);
}

static const struct { const char *text; int up; } news[] = {
  { "A famous gnome posts about LarnCoin.", 1 },
  { "The LRS announces a LarnCoin tax.",    0 },
  { "LarnCoin halving!",                    1 },
  { "A whale dumps.",                       0 },
};

static void price_news(void)
{
  int i = rund((int) (sizeof news / sizeof news[0]));
  momentum = news[i].up ? 1 : -1;
  set_price(nudge(price(), 29 + rnd(31), news[i].up));   /* 30..60% */
  Printf("\nLarnCoin news: %s", news[i].text);
  Printf(" LarnCoin is %s to %ld gp.", news[i].up ? "up" : "down", price());
  wild_event(US_EV_WILD_COIN, COIN_EV_NEWS, (int) price(), i);
}

static void rug_pull(void)
{
  momentum = 0;
  set_price(1);
  Print("\nLarnCoin's founder has moved to a volcano. The price collapses.");
  UlarnBeep();
  wild_event(US_EV_WILD_COIN, COIN_EV_RUG, 1, 0);
}

/* ---- trading ---- */

static long coin_value(long coins)
{
  long long v = (long long) coins * price();
  return v > GOLD_CAP ? GOLD_CAP : (long) v;
}

static long fee(long gp)
{
  return (long) (((long long) gp * ATM_FEE_PCT + 99) / 100);   /* rounded up: the house wins */
}

static long add_capped(long a, long b)
{
  long long s = (long long) a + b;
  return s > GOLD_CAP ? GOLD_CAP : (long) s;
}

/* Buys coins with the bank account and then the carried gold (bank), or with
 * the carried gold only plus a fee (ATM). Prints after "buy". */
static void buy(int atm)
{
  long p = price(), funds, maxn, n, cost;

  funds = atm ? c[GOLD] : add_capped(c[BANKACCOUNT], c[GOLD]);
  if (funds < 0) funds = 0;
  /* the most coins the funds cover, fee included (the estimate is at most a
   * coin or two over because of the fee's rounding) */
  if (atm)
    maxn = (long) ((long long) funds * 100 / ((long long) p * (100 + ATM_FEE_PCT)));
  else
    maxn = funds / p;
  if (maxn > COIN_MAX_HOLD - c[WILD_COINS]) maxn = COIN_MAX_HOLD - c[WILD_COINS];
  while (maxn > 0 && maxn * p + (atm ? fee(maxn * p) : 0) > funds) maxn--;
  if (maxn < 0) maxn = 0;

  Printf("\nHow many LarnCoin (%ld gp each, up to %ld)? ", p, maxn);
  n = get_num_input(maxn);
  if (n <= 0) return;
  if (n > maxn)
  {
    Print("\n  You can't afford that many.");
    nap(2000);
    return;
  }
  cost = n * p + (atm ? fee(n * p) : 0);
  if (atm)
    c[GOLD] -= cost;
  else if (cost <= c[BANKACCOUNT])
    c[BANKACCOUNT] -= cost;
  else
  {
    c[GOLD] -= cost - c[BANKACCOUNT];
    c[BANKACCOUNT] = 0;
  }
  c[WILD_COINS] += n;
  wild_coin_paint();   /* no map repaint follows a trade */
  Printf("\n  You buy %ld LarnCoin for %ld gp. Number go up.", n, cost);
  nap(1000);
}

/* Sells coins into the bank account (bank) or for cash minus a fee (ATM). */
static void sell(int atm)
{
  long n, gp;

  if (c[WILD_COINS] <= 0)
  {
    Print("\n  You don't have any LarnCoin.");
    nap(2000);
    return;
  }
  Printf("\nHow many LarnCoin (%ld gp each, you have %ld)? ", price(), c[WILD_COINS]);
  n = get_num_input(c[WILD_COINS]);
  if (n <= 0) return;
  if (n > c[WILD_COINS])
  {
    Print("\n  You don't have that many.");
    nap(2000);
    return;
  }
  gp = coin_value(n);
  if (atm) gp -= fee(gp);
  if (gp < 0) gp = 0;
  c[WILD_COINS] -= n;
  wild_coin_paint();   /* no map repaint follows a trade */
  if (atm)
    c[GOLD] = add_capped(c[GOLD], gp);
  else
    c[BANKACCOUNT] = add_capped(c[BANKACCOUNT], gp);
  Printf("\n  You sell %ld LarnCoin for %ld gp%s.", n, gp, atm ? " in cash" : ", paid into your account");
  nap(1000);
}

char *wild_coin_bank_keys(char *keys)
{
  static char buf[32];
  char *esc;
  size_t n;

  if (!wild_on(WILDF_COIN)) return keys;
  esc = strchr(keys, '\033');
  n = esc ? (size_t) (esc - keys) : strlen(keys);
  if (n > sizeof buf - 4) return keys;
  memcpy(buf, keys, n);
  strcpy(buf + n, "bc");
  if (esc) strcat(buf, "\033");
  return buf;
}

void wild_coin_bank_line(void)
{
  if (!wild_on(WILDF_COIN)) return;
  Printf("LarnCoin @ %ld gp. You hold %ld (%ld gp): (", price(), c[WILD_COINS],
         coin_value(c[WILD_COINS]));
  Standout("b");
  Print(")uy, (");
  Standout("c");
  Print(")ash out");
}

int wild_coin_bank_key(int key)
{
  if (!wild_on(WILDF_COIN)) return 0;
  switch (key)
  {
    case 'b': Print("buy"); buy(0); return 1;
    case 'c': Print("cash out"); sell(0); return 1;
    default:  return 0;
  }
}

long wild_coin_value(void)
{
  if (!wild_on(WILDF_COIN) || c[WILD_COINS] <= 0) return 0;
  return coin_value(c[WILD_COINS]);
}

/* ---- the ATM ---- */

static void coin_atm(void)
{
  int i;
  Print("\n\nThere is a LarnCoin ATM here.");
  Printf("\nLarnCoin is %ld gp. You hold %ld coin%s.", price(), c[WILD_COINS],
         c[WILD_COINS] == 1 ? "" : "s");
  i = get_prompt_input("\nThe ATM blinks (5% fee). (b)uy, (s)ell, or (i)gnore? ", "bsi\033", 1);
  switch (i)
  {
    case 'b': Print("buy."); buy(1); break;
    case 's': Print("sell."); sell(1); break;
    default:  Print("ignore."); return;
  }
  UpdateStatus();
}

/* Home level: the ATM on a free square next to the bank. */
static void place_atm(void)
{
  int sx, sy, r, x, y;
  for (sx = 0; sx < MAXX; sx++)
    for (sy = 0; sy < MAXY; sy++)
    {
      if (item[sx][sy] != OBANK) continue;
      for (r = 1; r <= 3; r++)
        for (y = sy - r; y <= sy + r; y++)
          for (x = sx - r; x <= sx + r; x++)
          {
            if (!checkxy(x, y) || item[x][y] != ONOTHING) continue;
            if (mitem[x][y].mon != MONST_NONE) continue;
            if (x == playerx && y == playery) continue;
            item[x][y] = OCOINATM;
            iarg[x][y] = 0;
            know[x][y] = OCOINATM;   /* the home level is always known */
            return;
          }
      return;
    }
}

/* ---- the tax auditor ---- */

static int auditor_here(void)
{
  int x, y;
  for (x = 0; x < MAXX; x++)
    for (y = 0; y < MAXY; y++)
      if (mitem[x][y].mon == TAXAUDITOR) return 1;
  return 0;
}

/* An awake auditor on a free square near the dungeon entrance. */
static void spawn_auditor(void)
{
  int sx, sy, r, x, y;
  for (sx = 0; sx < MAXX; sx++)
    for (sy = 0; sy < MAXY; sy++)
    {
      if (item[sx][sy] != OENTRANCE) continue;
      for (r = 1; r <= 4; r++)
        for (y = sy - r; y <= sy + r; y++)
          for (x = sx - r; x <= sx + r; x++)
          {
            if (!checkxy(x, y) || item[x][y] != ONOTHING) continue;
            if (mitem[x][y].mon != MONST_NONE) continue;
            if (x == playerx && y == playery) continue;
            mitem[x][y].mon = TAXAUDITOR;
            mitem[x][y].n = 0;
            hitp[x][y] = monster[TAXAUDITOR].hitpoints;
            stealth[x][y] = STEALTH_AWAKE | STEALTH_SEEN;
            show1cell(x, y);
            Print("\nA tax auditor from the LRS arrives, clipboard in hand.");
            UlarnBeep();
            wild_event(US_EV_WILD_COIN, COIN_EV_AUDITOR, 0, 0);
            return;
          }
      return;
    }
}

/* Taxes paid: every auditor on the level packs up and goes. */
static void dismiss_auditors(void)
{
  int x, y, n = 0;
  for (x = 0; x < MAXX; x++)
    for (y = 0; y < MAXY; y++)
      if (mitem[x][y].mon == TAXAUDITOR)
      {
        mitem[x][y].mon = MONST_NONE;
        mitem[x][y].n = 0;
        hitp[x][y] = 0;
        if (know[x][y] != OUNKNOWN) show1cell(x, y);
        n++;
      }
  if (n)
  {
    Print("\nThe tax auditor stamps your file PAID and leaves.");
    wild_event(US_EV_WILD_COIN, COIN_EV_AUDITOR, 2, 0);
  }
}

static void auditor_turn(void)
{
  if (level != 0) return;
  if (outstanding_taxes <= 0)
  {
    dismiss_auditors();
    return;
  }
  if (c[TIMESTOP] || rnd(AUDITOR_ODDS) != 1 || auditor_here()) return;
  spawn_auditor();
}

char *wild_coin_garnish(void)
{
  static char msg[128];
  long amt;

  if (!wild_on(WILDF_COIN)) return 0;
  if (c[GOLD] <= 0)
    return "\nThe %s finds no gold to garnish and makes a note of it.";
  amt = c[GOLD] / 10;
  if (amt < 1) amt = 1;
  c[GOLD] -= amt;
  /* the money goes to the LRS (the scoreboard keeps the tax bill) */
  if (outstanding_taxes > 0) paytaxes(amt);
  wild_event(US_EV_WILD_COIN, COIN_EV_AUDITOR, 1, (int) amt);
  snprintf(msg, sizeof msg,
           "\nThe %%s garnishes %ld gp of your gold. The auditor issues you a receipt.", amt);
  return msg;   /* spattack() then skips the blow: an audit, not a mugging */
}

/* ---- hooks ---- */

void wild_coin_init(void)
{
  ticks = 0;
  momentum = 0;
  price_moved = 0;
}

void wild_coin_new_character(void)
{
  c[WILD_COIN_PRICE] = COIN_START_PRICE;
  c[WILD_COINS] = 0;
}

void wild_coin_turn(void)
{
  price_moved = 0;
  (void) price();   /* repairs a missing price */
  if (rnd(COIN_NEWS_ODDS) == 1)
    price_news();
  else if (++ticks >= COIN_TICK)
  {
    ticks = 0;
    if (price() > COIN_RUG_PRICE && rnd(COIN_RUG_ODDS) == 1)
      rug_pull();
    else
      price_step();
  }
  auditor_turn();
}

void wild_coin_new_level(int lev) { (void) lev; }
void wild_coin_leaving_level(int lev) { (void) lev; }

void wild_coin_make_level(int lev)
{
  if (lev == 0) place_atm();
}

void wild_coin_kill(int monst, int x, int y)
{
  (void) x; (void) y;
  if (monst == TAXAUDITOR)
    Print("\nThe auditor's clipboard clatters to the ground. The LRS will hear of this.");
}

void wild_coin_level_up(int newlevel) { (void) newlevel; }
void wild_coin_take(int obj, int arg) { (void) obj; (void) arg; }

int wild_coin_object(int obj)
{
  if (obj != OCOINATM) return 0;
  coin_atm();
  return 1;
}

int  wild_coin_wear(int slot) { (void) slot; return 0; }
int  wild_coin_takeoff(void) { return 0; }
int  wild_coin_read(int slot) { (void) slot; return 0; }
int  wild_coin_eat(int slot) { (void) slot; return 0; }
int  wild_coin_dodge(void) { return 0; }
void wild_coin_player_hit(int dmg) { (void) dmg; }

/* The HUD ticker (GameView.kt) and the side panel's LARNCOIN section: the
 * price, negative when the ticker should stay hidden (no coins held and no
 * move this turn) so the app still sees every change and can draw the
 * arrow; and the coins held. */
void wild_coin_paint(void)
{
  UlarnShared *s = ularn_shared;
  if (s == NULL) return;
  s->coin_price = (int32_t) ((c[WILD_COINS] > 0 || price_moved) ? price() : -price());
  s->coin_held = (int32_t) c[WILD_COINS];
}
