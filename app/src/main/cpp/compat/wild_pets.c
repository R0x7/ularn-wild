/* =============================================================================
 * FILENAME: wild_pets.c
 *
 * Ularn: Wild Edition feature "pets". See docs/WILD_EDITION_PLAN.md for the
 * design and wild.h for when each hook is called. Everything in this file
 * runs only when the feature's bit is on (wild.c checks before calling).
 *
 * A mysterious egg (OEGG) carried for EGG_TURNS turns hatches into a pet: an
 * ordinary monster on the level that this file marks as ours. The marking is
 * nothing but its position, so nothing in upstream's bookkeeping changes:
 *
 *   c[WILD_PET_MON]          species, 0 = no pet
 *   c[WILD_PET_HP]           last known hit points (hitp[][] is the truth)
 *   wild_extra[WX_PET_LEVEL] dungeon level + 1 it lives on, 0 = none,
 *                            PET_TRAVELLING while it follows the player
 *                            through a level change
 *   wild_extra[WX_PET_X/Y]   its square; X = -1 while not placed yet
 *   wild_extra[WX_PET_KILLS] monsters it has killed with its own bite; at
 *                            5, 15 and 30 it grows (pet_stage()): more hit
 *                            points and a worse bite
 *
 * Every turn the monster at that square is checked again (species, hit
 * points that only go down), so a pet that died, was teleported, polymorphed
 * or genocided by any path upstream knows nothing about is simply forgotten
 * ("Your pet is gone.") and never leaves a stranger marked as ours.
 *
 * Upstream never moves the pet (monster.c: movemt() returns early for
 * wild_is_pet()), so it never attacks the player; it moves and fights here,
 * once per turn in wild_turn(). Walking into it swaps places (player.c:
 * moveplayer() -> wild_pet_swap()).
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
#include "wild_pets.h"

extern UlarnShared *ularn_shared;

#define EGG_TURNS        300   /* carried turns until the egg hatches */
#define EGG_WOBBLE       100
#define EGG_TAPPING       30
#define EGG_MINLEVEL       2   /* one egg on each of levels 2..5 */
#define EGG_MAXLEVEL       5
#define PET_REGEN_TURNS   15   /* the pet heals 1 HP this often */
#define PET_FISH_HP       10   /* a fish fed to the pet heals this much */
#define PET_HIT_CHANCE    50   /* % chance an adjacent awake hostile bites it */
#define PET_TRAVELLING    (-1) /* wild_extra[WX_PET_LEVEL] between two levels */

#define TILE_CRACK1      398   /* hatching overlay frames */
#define TILE_CRACK2      399

#define PEV_HATCH  0           /* US_EV_WILD_PET a = ... */
#define PEV_ATTACK 1
#define PEV_DIED   2
#define PEV_FOLLOW 3
#define PEV_GROW   4           /* b = species, c = new stage */

/* A pet grows up by fighting: each stage reached adds PET_GROW_HP to its
 * maximum hit points and PET_GROW_BITE to its bite. */
#define PET_STAGES       3
#define PET_GROW_HP      8
#define PET_GROW_BITE    2
static const int grow_at[PET_STAGES] = { 5, 15, 30 };   /* kills */
static const char *const stage_name[PET_STAGES + 1] = { "baby", "young", "grown", "fearsome" };

/* ---- helpers ---- */

static int pet_species(void)
{
  int m = (int) c[WILD_PET_MON];
  return (m > 0 && m < MONST_COUNT) ? m : 0;
}

static const char *pet_name(void)
{
  return monster[pet_species()].name;
}

/* How grown the pet is: 0 (as hatched) .. PET_STAGES. */
static int pet_stage(void)
{
  int i, st = 0;
  for (i = 0; i < PET_STAGES; i++)
    if (wild_extra[WX_PET_KILLS] >= grow_at[i]) st = i + 1;
  return st;
}

/* A pet gets more hit points than its wild cousins: a 1 HP jackal would
 * not survive its first kobold. */
static int pet_maxhp(int m)
{
  return 10 + 3 * monster[m].hitpoints + PET_GROW_HP * pet_stage();
}

static int max2(int a, int b) { return a > b ? a : b; }
static int dist(int x1, int y1, int x2, int y2)
{
  return max2(x1 > x2 ? x1 - x2 : x2 - x1, y1 > y2 ? y1 - y2 : y2 - y1);
}

/* The pet is placed on the level the player is on. */
static int pet_here(void)
{
  int x = wild_extra[WX_PET_X], y = wild_extra[WX_PET_Y];
  return pet_species() != 0 && wild_extra[WX_PET_LEVEL] == level + 1 &&
         x >= 0 && x < MAXX && y >= 0 && y < MAXY;
}

static void pet_publish(void)
{
  UlarnShared *s = ularn_shared;
  if (s == NULL) return;
  if (pet_here() && !c[BLINDCOUNT] &&
      know[wild_extra[WX_PET_X]][wild_extra[WX_PET_Y]] != OUNKNOWN)
  {
    s->pet_x = wild_extra[WX_PET_X];
    s->pet_y = wild_extra[WX_PET_Y];
  }
  else
  {
    s->pet_x = -1;
    s->pet_y = -1;
  }

  /* the side panel's PET section */
  {
    int m = pet_species(), st = pet_stage(), lev = wild_extra[WX_PET_LEVEL] - 1;
    memset(s->pet_info, 0, sizeof s->pet_info);
    memset(s->pet_level, 0, sizeof s->pet_level);
    if (m == 0) return;
    s->pet_info[0] = m;
    s->pet_info[1] = (int32_t) c[WILD_PET_HP];
    s->pet_info[2] = pet_maxhp(m);
    s->pet_info[3] = st;
    s->pet_info[4] = wild_extra[WX_PET_KILLS];
    s->pet_info[5] = st < PET_STAGES ? grow_at[st] : 0;
    /* left behind on another level (not PET_TRAVELLING, which is -1) */
    if (lev >= 0 && lev < NLEVELS && lev != level)
    {
      const char *name = levelname[lev];
      while (*name == ' ') name++;   /* " H", " 3": padded for the status line */
      strncpy(s->pet_level, name, sizeof s->pet_level - 1);
    }
  }
}

static void pet_clear(void)
{
  c[WILD_PET_MON] = 0;
  c[WILD_PET_HP] = 0;
  wild_extra[WX_PET_X] = -1;
  wild_extra[WX_PET_Y] = -1;
  wild_extra[WX_PET_LEVEL] = 0;
  wild_extra[WX_PET_KILLS] = 0;   /* the next pet starts as a baby again */
  pet_publish();
}

static void redraw(int x, int y)
{
  if (know[x][y] != OUNKNOWN) show1cell(x, y);
}

/* Takes the monster at (x,y) off the map; anything it carried drops there. */
static void remove_monster(int x, int y)
{
  int i, n = mitem[x][y].n;
  mitem[x][y].mon = MONST_NONE;
  mitem[x][y].n = 0;
  hitp[x][y] = 0;
  stealth[x][y] = 0;
  for (i = 0; i < n; i++)
    createitem(x, y, mitem[x][y].it[i].item, mitem[x][y].it[i].itemarg);
  redraw(x, y);
}

/* Moves the pet's monster (with all its bookkeeping) to an empty square. */
static void pet_move_to(int nx, int ny)
{
  int ox = wild_extra[WX_PET_X], oy = wild_extra[WX_PET_Y];
  mitem[nx][ny] = mitem[ox][oy];
  mitem[ox][oy].mon = MONST_NONE;
  mitem[ox][oy].n = 0;
  hitp[nx][ny] = hitp[ox][oy];
  hitp[ox][oy] = 0;
  stealth[nx][ny] = (char) (stealth[ox][oy] | STEALTH_AWAKE);
  stealth[ox][oy] = 0;
  wild_extra[WX_PET_X] = nx;
  wild_extra[WX_PET_Y] = ny;
  redraw(ox, oy);
  redraw(nx, ny);
}

/* A square the pet may stand on: no wall, closed door or monster, not the
 * player. */
static int pet_can_stand(int x, int y)
{
  return cgood(x, y, 0, 1) && !(x == playerx && y == playery);
}

/* An empty square near the player (adjacent first, then 2 away). */
static int near_player(int *px, int *py)
{
  int r, k, i, x, y;
  for (r = 1; r <= 2; r++)
  {
    k = rund(8);
    for (i = 0; i < 8 * r; i++)
    {
      /* walk the ring of radius r starting at a random point */
      int n = (k + i) % (8 * r), side = n / (2 * r), off = n % (2 * r);
      switch (side)
      {
        case 0:  x = playerx - r + off; y = playery - r; break;
        case 1:  x = playerx + r; y = playery - r + off; break;
        case 2:  x = playerx + r - off; y = playery + r; break;
        default: x = playerx - r; y = playery + r - off; break;
      }
      if (pet_can_stand(x, y)) { *px = x; *py = y; return 1; }
    }
  }
  return 0;
}

/* Puts a new pet monster of species m (with hp) on (x,y). */
static void pet_place(int m, int hp, int x, int y)
{
  mitem[x][y].mon = (char) m;
  mitem[x][y].n = 0;
  hitp[x][y] = (short) hp;
  stealth[x][y] = STEALTH_AWAKE | STEALTH_SEEN;
  c[WILD_PET_MON] = m;
  c[WILD_PET_HP] = hp;
  wild_extra[WX_PET_X] = x;
  wild_extra[WX_PET_Y] = y;
  wild_extra[WX_PET_LEVEL] = level + 1;
  show1cell(x, y);
}

/* Re-checks that the monster at the stored square is still our pet. 1 = it
 * is (c[WILD_PET_HP] is refreshed), 0 = no pet on this level. */
static int pet_verify(void)
{
  int x, y, m, h;
  if (!pet_here()) return 0;
  x = wild_extra[WX_PET_X];
  y = wild_extra[WX_PET_Y];
  m = mitem[x][y].mon;
  h = hitp[x][y];
  /* Monsters don't regenerate, so a pet with more hit points than we last
   * saw is some other monster of the same species that took its place. */
  if (m != pet_species() || h <= 0 || h > c[WILD_PET_HP])
  {
    Printf("\nYour %s is gone.", pet_name());
    wild_event(US_EV_WILD_PET, PEV_DIED, pet_species(), 0);
    pet_clear();
    return 0;
  }
  c[WILD_PET_HP] = h;
  return 1;
}

static void pet_dies(void)
{
  int x = wild_extra[WX_PET_X], y = wild_extra[WX_PET_Y];
  Printf("\nYour %s dies. You feel a lump in your throat.", pet_name());
  wild_event(US_EV_WILD_PET, PEV_DIED, pet_species(), 1);
  remove_monster(x, y);
  pet_clear();
}

/* ---- hatching ---- */

static int egg_slot(void)
{
  int i;
  for (i = 0; i < IVENSIZE; i++)
    if ((unsigned char) iven[i] == OEGG) return i;
  return -1;
}

static int random_species(void)
{
  static const int common[4] = { JACKAL, SNAKE, ANT, EYE };
  if (rnd(100) <= 6) return HELLHOUND;
  return common[rund(4)];
}

/* The egg on (x,y) cracks open: two quick overlay frames, the same way
 * spells animate (nap() publishes the frame). */
static void hatch_animation(int x, int y)
{
  UlarnShared *s = ularn_shared;
  if (s == NULL || c[BLINDCOUNT]) return;
  s->overlay[y][x] = TILE_CRACK1;
  nap(250);
  s->overlay[y][x] = TILE_CRACK2;
  nap(250);
  show1cell(x, y);   /* clears the overlay */
}

static void hatch(void)
{
  int x, y, m, slot = egg_slot();
  if (slot < 0) return;
  if (!near_player(&x, &y))
  {
    c[WILD_EGG] = 1;   /* no room: try again next turn */
    return;
  }
  wild_use_up(slot);
  m = random_species();
  c[WILD_EGG] = 0;
  wild_extra[WX_PET_KILLS] = 0;
  pet_place(m, pet_maxhp(m), x, y);
  Printf("\nThe egg hatches! A baby %s blinks up at you.", monster[m].name);
  Print("\nIt seems to think you are its mother.");
  wild_event(US_EV_WILD_PET, PEV_HATCH, m, 0);
  hatch_animation(x, y);
  recalc();
  UpdateStatus();
}

static void egg_turn(void)
{
  if (egg_slot() < 0) return;      /* only a carried egg incubates */
  if (pet_species() != 0) return;  /* one pet at a time */
  if (c[WILD_EGG] <= 0) c[WILD_EGG] = EGG_TURNS;
  c[WILD_EGG]--;
  if (c[WILD_EGG] == EGG_WOBBLE) Print("\nThe egg wobbles.");
  else if (c[WILD_EGG] == EGG_TAPPING) Print("\nYou hear tapping from inside the egg.");
  else if (c[WILD_EGG] <= 0) hatch();
}

/* ---- the pet's turn ---- */

/* Distance (in steps) from every square to (sx,sy), walls, closed doors
 * and the player blocking, other monsters not (they move). -1 = unreachable. */
static short path[MAXX][MAXY];

static void flood_from(int sx, int sy)
{
  static short qx[MAXX * MAXY], qy[MAXX * MAXY];
  int head = 0, tail = 0, x, y, k;
  for (x = 0; x < MAXX; x++)
    for (y = 0; y < MAXY; y++)
      path[x][y] = -1;
  path[sx][sy] = 0;
  qx[tail] = (short) sx; qy[tail] = (short) sy; tail++;
  while (head < tail)
  {
    int cx = qx[head], cy = qy[head];
    head++;
    for (k = 1; k <= 8; k++)
    {
      x = cx + diroffx[k];
      y = cy + diroffy[k];
      if (x < 0 || x >= MAXX || y < 0 || y >= MAXY || path[x][y] >= 0) continue;
      if (!cgood(x, y, 0, 0) || (x == playerx && y == playery)) continue;
      path[x][y] = (short) (path[cx][cy] + 1);
      qx[tail] = (short) x; qy[tail] = (short) y; tail++;
    }
  }
}

/* How far (x,y) is from the player for the pet: path length, or, if the
 * flood didn't reach it (shouldn't happen), plain distance after that. */
static int pet_score(int x, int y)
{
  return path[x][y] >= 0 ? path[x][y] : 10000 + dist(x, y, playerx, playery);
}

/* One step toward the player, along the shortest path. */
static void pet_follow(void)
{
  int px = wild_extra[WX_PET_X], py = wild_extra[WX_PET_Y];
  int k, x, y, bx = -1, by = -1, best;
  if (dist(px, py, playerx, playery) <= 1) return;
  flood_from(playerx, playery);
  best = pet_score(px, py);
  for (k = 1; k <= 8; k++)
  {
    x = px + diroffx[k];
    y = py + diroffy[k];
    if (!pet_can_stand(x, y)) continue;
    if (pet_score(x, y) < best) { best = pet_score(x, y); bx = x; by = y; }
  }
  if (bx >= 0) pet_move_to(bx, by);
}

/* Set while the pet's bite is inside hitm(): wild_pets_kill() counts a
 * death reported in that window as the pet's own kill. */
static int biting = 0;
static int bite_killed = 0;

/* The pet killed something by itself: count it, and at grow_at[] kills it
 * grows a stage. The hit points the stage adds are given at once, so the
 * growth is felt in the fight it happened in. */
static void pet_made_kill(void)
{
  int x, y, before = pet_stage(), st;
  if (wild_extra[WX_PET_KILLS] < 1000000) wild_extra[WX_PET_KILLS]++;
  st = pet_stage();
  if (st == before) return;
  x = wild_extra[WX_PET_X];
  y = wild_extra[WX_PET_Y];
  hitp[x][y] = (short) (hitp[x][y] + PET_GROW_HP * (st - before));
  c[WILD_PET_HP] = hitp[x][y];
  Printf("\nYour %s has grown! It is a %s %s now: tougher, with a nastier bite.",
         pet_name(), stage_name[st], pet_name());
  wild_event(US_EV_WILD_PET, PEV_GROW, pet_species(), st);
}

/* Bites one adjacent monster. Returns 1 if it attacked. */
static int pet_attack(void)
{
  int px = wild_extra[WX_PET_X], py = wild_extra[WX_PET_Y];
  int k, i, x, y, dmg, m = pet_species();
  for (k = rund(8), i = 0; i < 8; i++)
  {
    x = px + diroffx[1 + (k + i) % 8];
    y = py + diroffy[1 + (k + i) % 8];
    if (x < 0 || x >= MAXX || y < 0 || y >= MAXY) continue;
    if (mitem[x][y].mon == MONST_NONE || (x == playerx && y == playery)) continue;
    ifblind(x, y);   /* sets lastmonst, which hitm() prints */
    Printf("\nYour %s %s the %s!", pet_name(), m == EYE ? "glares at" : "bites", lastmonst);
    wild_event(US_EV_WILD_PET, PEV_ATTACK, mitem[x][y].mon, 0);
    dmg = rnd(monster[m].damage + 2 + PET_GROW_BITE * pet_stage());
    biting = 1;
    bite_killed = 0;
    wild_hit_monster(x, y, dmg);   /* not the player's blow (wild.c) */
    biting = 0;
    /* hitm() may have done anything: only a pet that is still there grows */
    if (bite_killed && pet_verify()) pet_made_kill();
    return 1;
  }
  return 0;
}

/* Nothing to bite: step toward the nearest monster that is close to the
 * player (or to the pet), without straying more than 2 squares from the
 * player. Returns 1 if it moved. */
static int pet_hunt(void)
{
  int px = wild_extra[WX_PET_X], py = wild_extra[WX_PET_Y];
  int k, x, y, tx = -1, ty = -1, best = 3, d;
  for (y = playery - 3; y <= playery + 3; y++)
    for (x = playerx - 3; x <= playerx + 3; x++)
    {
      if (x < 0 || x >= MAXX || y < 0 || y >= MAXY) continue;
      if (mitem[x][y].mon == MONST_NONE || (x == px && y == py)) continue;
      if (dist(x, y, playerx, playery) > 2 && dist(x, y, px, py) > 2) continue;
      d = dist(x, y, px, py);
      if (d < best) { best = d; tx = x; ty = y; }
    }
  if (tx < 0) return 0;
  flood_from(tx, ty);   /* finds the way around the player too */
  best = path[px][py] >= 0 ? path[px][py] : 10000;
  for (k = 1; k <= 8; k++)
  {
    x = px + diroffx[k];
    y = py + diroffy[k];
    if (!pet_can_stand(x, y) || dist(x, y, playerx, playery) > 2) continue;
    if (path[x][y] >= 0 && path[x][y] < best)
    {
      pet_move_to(x, y);
      return 1;
    }
  }
  return 0;
}

/* Adjacent awake hostiles may bite back. */
static void pet_defend(void)
{
  int px = wild_extra[WX_PET_X], py = wild_extra[WX_PET_Y];
  int k, x, y, m, dmg;
  if (c[TIMESTOP] || c[HOLDMONST]) return;
  for (k = 1; k <= 8; k++)
  {
    x = px + diroffx[k];
    y = py + diroffy[k];
    if (x < 0 || x >= MAXX || y < 0 || y >= MAXY) continue;
    m = mitem[x][y].mon;
    if (m == MONST_NONE || (stealth[x][y] & STEALTH_AWAKE) == 0) continue;
    if (monster[m].damage <= 0 || rnd(100) > PET_HIT_CHANCE) continue;
    dmg = rnd(monster[m].damage + 2);
    if (c[BLINDCOUNT]) Printf("\nSomething hits your %s.", pet_name());
    else Printf("\nThe %s hits your %s.", monster[m].name, pet_name());
    hitp[px][py] = (short) (hitp[px][py] - dmg);
    if (hitp[px][py] <= 0)
    {
      pet_dies();
      return;
    }
    c[WILD_PET_HP] = hitp[px][py];
  }
}

static void pet_turn(void)
{
  int m = pet_species();
  if (m == 0) return;

  /* Genocided wherever it is: its level would drop it on the next visit. */
  if (monster[m].flags & FL_GENOCIDED)
  {
    if (pet_here() && mitem[wild_extra[WX_PET_X]][wild_extra[WX_PET_Y]].mon == m)
      remove_monster(wild_extra[WX_PET_X], wild_extra[WX_PET_Y]);
    Printf("\nYour %s fades out of existence.", pet_name());
    wild_event(US_EV_WILD_PET, PEV_DIED, m, 2);
    pet_clear();
    return;
  }

  /* Arrived on this level with the player: place it next to them. */
  if (wild_extra[WX_PET_LEVEL] == level + 1 && wild_extra[WX_PET_X] < 0)
  {
    int x, y;
    if (c[WILD_PET_HP] <= 0) c[WILD_PET_HP] = 1;
    if (near_player(&x, &y)) pet_place(m, (int) c[WILD_PET_HP], x, y);
    return;
  }

  if (!pet_verify()) return;

  if (!pet_attack() && !pet_hunt()) pet_follow();
  if (!pet_verify()) return;   /* hitm() may have done anything */
  pet_defend();
  if (!pet_here()) return;

  if ((gtime % PET_REGEN_TURNS) == 0 && c[WILD_PET_HP] < pet_maxhp(m))
  {
    hitp[wild_extra[WX_PET_X]][wild_extra[WX_PET_Y]]++;
    c[WILD_PET_HP]++;
  }
}

/* ---- hooks ---- */

void wild_pets_init(void) {}

void wild_pets_new_character(void)
{
  pet_clear();
}

void wild_pets_turn(void)
{
  egg_turn();
  pet_turn();
  pet_publish();
}

/* Between wild_leaving_level() and wild_new_level(): what to tell the
 * player once we know the level really changed (genocide reloads the same
 * level, for one). */
#define LEFT_NOTHING 0
#define LEFT_FOLLOWS 1
#define LEFT_BEHIND  2
static int left_from = -1;
static int left_how = LEFT_NOTHING;

void wild_pets_new_level(int lev)
{
  if (lev != left_from && pet_species() != 0)
  {
    if (left_how == LEFT_FOLLOWS)
    {
      Printf("\nYour %s follows you.", pet_name());
      wild_event(US_EV_WILD_PET, PEV_FOLLOW, pet_species(), 0);
    }
    else if (left_how == LEFT_BEHIND)
      Print("\nYou hear a sad whine behind you.");
  }
  left_from = -1;
  left_how = LEFT_NOTHING;
  if (wild_extra[WX_PET_LEVEL] == PET_TRAVELLING)
  {
    /* placed by the next wild_turn(), once the player's square is final
     * (going up to the town, moveplayer() moves them after this hook) */
    wild_extra[WX_PET_LEVEL] = lev + 1;
    wild_extra[WX_PET_X] = -1;
    wild_extra[WX_PET_Y] = -1;
  }
  pet_publish();
}

/* Called before the level is stored: an adjacent pet comes along, any other
 * stays behind on it. */
void wild_pets_leaving_level(int lev)
{
  int x, y, m = pet_species();
  left_from = lev;
  left_how = LEFT_NOTHING;
  if (m == 0 || wild_extra[WX_PET_LEVEL] != lev + 1) return;
  if (monster[m].flags & FL_GENOCIDED) return;   /* wild_turn() says goodbye */
  if (wild_extra[WX_PET_X] < 0)
  {
    wild_extra[WX_PET_LEVEL] = PET_TRAVELLING;   /* never got placed here */
    left_how = LEFT_FOLLOWS;
    return;
  }
  if (!pet_verify()) return;
  x = wild_extra[WX_PET_X];
  y = wild_extra[WX_PET_Y];
  if (dist(x, y, playerx, playery) <= 1 && mitem[x][y].n == 0)
  {
    c[WILD_PET_HP] = hitp[x][y];
    remove_monster(x, y);
    wild_extra[WX_PET_LEVEL] = PET_TRAVELLING;
    wild_extra[WX_PET_X] = -1;
    wild_extra[WX_PET_Y] = -1;
    left_how = LEFT_FOLLOWS;
  }
  else
    left_how = LEFT_BEHIND;
}

void wild_pets_make_level(int lev)
{
  int x, y;
  if (lev < EGG_MINLEVEL || lev > EGG_MAXLEVEL) return;
  if (!wild_free_cell(&x, &y)) return;
  item[x][y] = OEGG;
  iarg[x][y] = 0;
  if (wizard) know[x][y] = OEGG;
}

/* The player killed it (a spell, mostly: walking into it swaps places). */
void wild_pets_kill(int monst, int x, int y)
{
  if (biting) bite_killed = 1;   /* the pet's own bite did it (pet_attack()) */
  if (monst != pet_species() || !wild_is_pet(x, y)) return;
  Printf("\nYou killed your own %s! You feel terrible.", pet_name());
  wild_event(US_EV_WILD_PET, PEV_DIED, monst, 3);
  pet_clear();
}

void wild_pets_level_up(int newlevel) { (void) newlevel; }

void wild_pets_take(int obj, int arg)
{
  (void) arg;
  if (obj != OEGG) return;
  if (c[WILD_EGG] <= 0) c[WILD_EGG] = EGG_TURNS;
  Print("\nThe egg is warm. Something inside it moves.");
}

int  wild_pets_object(int obj) { (void) obj; return 0; }
int  wild_pets_wear(int slot) { (void) slot; return 0; }
int  wild_pets_takeoff(void) { return 0; }
int  wild_pets_read(int slot) { (void) slot; return 0; }
/* "Eating" a fish with the pet next to you feeds it instead. (Only reached
 * when no feature listed before pets in wild.c handles the fish itself.) */
int wild_pets_eat(int slot)
{
  int x, y, m;
  if (slot < 0 || slot >= IVENSIZE || (unsigned char) iven[slot] != OFISH) return 0;
  if (!pet_verify()) return 0;
  x = wild_extra[WX_PET_X];
  y = wild_extra[WX_PET_Y];
  if (dist(x, y, playerx, playery) > 1) return 0;
  m = pet_species();
  wild_use_up(slot);
  hitp[x][y] = (short) (hitp[x][y] + PET_FISH_HP > pet_maxhp(m) ? pet_maxhp(m) : hitp[x][y] + PET_FISH_HP);
  c[WILD_PET_HP] = hitp[x][y];
  Printf("\nYou toss the fish to your %s. It gobbles it up happily.", pet_name());
  recalc();
  UpdateStatus();
  return 1;
}
int  wild_pets_dodge(void) { return 0; }
void wild_pets_player_hit(int dmg) { (void) dmg; }

void wild_pets_paint(void)
{
  pet_publish();
}

/* ---- called from player.c: moveplayer() ---- */
int wild_pet_swap(int x, int y)
{
  int ox = playerx, oy = playery;
  if (!wild_is_pet(x, y)) return 0;
  if (!cgood(ox, oy, 0, 1))
  {
    /* the player is somewhere it can't go (inside a wall, say) */
    Printf("\nYour %s is in the way.", pet_name());
    return 0;
  }
  /* pet_move_to() refuses nothing: the square is free once the player
   * steps off it, which moveplayer() does right after this */
  pet_move_to(ox, oy);
  c[WILD_PET_HP] = hitp[ox][oy];
  return 1;
}

/* ---- called from show.c: seemagic() ---- */
const char *wild_pet_text(void)
{
  static char buf[160];
  int m, st, kills, n;
  if (!wild_on(WILDF_PETS)) return NULL;
  m = pet_species();
  if (m == 0) return NULL;
  st = pet_stage();
  kills = wild_extra[WX_PET_KILLS];
  n = snprintf(buf, sizeof buf, "A %s %s: %ld of %d hit points, %d kill%s",
               stage_name[st], monster[m].name, (long) c[WILD_PET_HP], pet_maxhp(m),
               kills, kills == 1 ? "" : "s");
  if (n > 0 && n < (int) sizeof buf)
  {
    if (st < PET_STAGES)
      snprintf(buf + n, sizeof buf - n, " (it grows at %d).", grow_at[st]);
    else
      snprintf(buf + n, sizeof buf - n, ".");
  }
  return buf;
}
