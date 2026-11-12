/* =============================================================================
 * FILENAME: wild.h
 *
 * Ularn: Wild Edition. The hub every experimental feature hangs off.
 * See docs/WILD_EDITION_PLAN.md.
 *
 * Upstream game code calls the wild_* hooks below (one line each); wild.c
 * fans each hook out to the per-feature files compat/wild_<feature>.c. A
 * feature does nothing unless its bit is set in the feature mask, which the
 * app passes in the environment variable ULARN_WILD and which is stored with
 * the character in c[WILD_FLAGS].
 * =============================================================================
 */
#ifndef ULARN_WILD_H
#define ULARN_WILD_H

#include <stdio.h>
#include <stdint.h>
#include "../shared_state.h"   /* US_EV_* event ids, UlarnShared */

/* ---- feature bits WILDF_* (mirrored in Kotlin: Settings.WILDF_*) ----
 * (WILDF_, not WILD_: the WILD_* names are c[] attribute indices in player.h) */
#define WILDF_RADIO    (1 << 0)
#define WILDF_CARDS    (1 << 1)
#define WILDF_VENDING  (1 << 2)
#define WILDF_HATS     (1 << 3)
#define WILDF_COIN     (1 << 4)
#define WILDF_GLITCH   (1 << 5)
#define WILDF_FISHING  (1 << 6)
#define WILDF_PETS     (1 << 7)
#define WILDF_PERKS    (1 << 8)
#define WILDF_WEATHER  (1 << 9)
#define WILDF_PHOTO    (1 << 10)   /* Kotlin only; listed for completeness */
#define WILDF_BOUNTY   (1 << 11)
#define WILDF_ALL      0xfff

/* Non-zero when feature `bit` is enabled for this character. */
int wild_on(int bit);

/* ---- save file header (savegame.c) ---- */
#define WILD_SAVE_MAGIC     "ULRNWILD"
#define WILD_SAVE_MAGIC_LEN 8
#define WILD_SAVE_VERSION   2

/* How a Wild object is used from the pack (action.c: whatitem()). The table
 * lives in wild.c; a feature that adds a use for its object edits it there. */
#define WILD_USE_NONE 0
#define WILD_USE_READ 1
#define WILD_USE_WEAR 2
#define WILD_USE_EAT  3
int wild_item_use(int obj);

/* ---- extra save slots (int32, in the save trailer) ---- */
#define WILD_EXTRA_COUNT 64
#define WX_PET_X      0
#define WX_PET_Y      1
#define WX_PET_LEVEL  2
#define WX_VENDING_WOBBLE 3
#define WX_KRAKEN_X   4
#define WX_KRAKEN_Y   5
#define WX_GLITCH_SEED 6
#define WX_WEATHER_NEXT 7
#define WX_PERK_WIND_LEVEL 8  /* perks: old saves only; see WX_PERK_WIND_USED */
#define WX_RADIO_TRACKS 9
#define WX_KRAKEN_LEVEL 24    /* fishing: level+1 of the kraken's fountain, 0 = none */
#define WX_FISH_ROD_LEVEL 25  /* fishing: dungeon level that gets the free rod */
#define WX_BOUNTY_MON     10  /* bounty: wanted species, 0 = no poster */
#define WX_BOUNTY_GOLD    11  /* bounty: reward */
#define WX_BOUNTY_CLAIMED 12  /* bounty: 1 = killed, collect in town */
#define WX_BOUNTY_POSTED  13  /* bounty: gtime when the poster went up */
#define WX_PERK_WIND_USED 14  /* perks: levels Second Wind was used on (bit n = level n) */
/* 15..23 and 26..63 free: claim one by adding a define here */
extern int wild_extra[WILD_EXTRA_COUNT];

/* ---- hooks called by upstream code (see wild.c) ---- */
void wild_init(void);                 /* after options, before restore */
void wild_new_character(void);        /* a new game's character exists */
void wild_turn(void);                 /* each turn, after regen() */
void wild_new_level(int lev);         /* newcavelevel(): level in memory */
void wild_leaving_level(int lev);     /* newcavelevel(): before savelevel() */
void wild_make_level(int lev);        /* makemaze()+makeobject() done, first visit */
void wild_kill(int monst, int x, int y);   /* player killed a monster */
void wild_level_up(int newlevel);     /* character level went up */
void wild_take(int obj, int arg);     /* item went into the pack */
void wild_object(int obj);            /* stepped on a Wild object (>= OWILD_FIRST) */
int  wild_wear(int slot);             /* 'W' on a Wild object; 1 = handled */
int  wild_takeoff(void);              /* 'T' when a Wild thing is worn; 1 = handled */
int  wild_read(int slot);             /* 'r' on a Wild object; 1 = handled */
int  wild_eat(int slot);              /* 'e' on a Wild object; 1 = handled */
int  wild_dodge(void);                /* 1 = this melee attack on the player misses */
void wild_player_hit(int dmg);        /* player took damage */
void wild_spell(int spell);           /* player cast a spell */
void wild_gold(int amount);           /* player picked up gold */
void wild_died(int won);              /* game over */
void wild_save(FILE *fp);             /* trailer, after write_spheres() */
int  wild_restore(FILE *fp);          /* trailer; 0 = ok */
void wild_paint(void);                /* front end: copy state into UlarnShared */

/* Wizard-mode debug menu (key '='): places objects under the player,
 * creates monsters, raises levels... Returns the action to perform next
 * (ACTION_WAIT after placing something so lookforobject() sees it). */
int wild_debug(void);

/* ---- events to the app ---- */
void wild_event(int type, int a, int b, int c);

/* ---- helpers for features ---- */
/* The game's own "You have found X. Take it?" prompt (object.c). */
void wild_finditem(int itm);
/* Is there a monster at (x,y) that is this character's pet? (wild_pets.c) */
int  wild_is_pet(int x, int y);
/* Random floor cell with nothing on it and no monster; 0 if none found. */
int  wild_free_cell(int *x, int *y);
/* Kills of species `monst` by this character, including the one being
 * reported to wild_<feature>_kill() (the hub counts it first). */
int  wild_kill_count(int monst);

/* ---- per-feature entry points (compat/wild_<feature>.c) ---- */
#define WILD_FEATURE_HOOKS(F) \
  void wild_##F##_init(void); \
  void wild_##F##_new_character(void); \
  void wild_##F##_turn(void); \
  void wild_##F##_new_level(int lev); \
  void wild_##F##_leaving_level(int lev); \
  void wild_##F##_make_level(int lev); \
  void wild_##F##_kill(int monst, int x, int y); \
  void wild_##F##_level_up(int newlevel); \
  void wild_##F##_take(int obj, int arg); \
  int  wild_##F##_object(int obj); \
  int  wild_##F##_wear(int slot); \
  int  wild_##F##_takeoff(void); \
  int  wild_##F##_read(int slot); \
  int  wild_##F##_eat(int slot); \
  int  wild_##F##_dodge(void); \
  void wild_##F##_player_hit(int dmg); \
  void wild_##F##_paint(void);

WILD_FEATURE_HOOKS(radio)
WILD_FEATURE_HOOKS(cards)
WILD_FEATURE_HOOKS(vending)
WILD_FEATURE_HOOKS(hats)
WILD_FEATURE_HOOKS(coin)
WILD_FEATURE_HOOKS(glitch)
WILD_FEATURE_HOOKS(fishing)
WILD_FEATURE_HOOKS(pets)
WILD_FEATURE_HOOKS(perks)
WILD_FEATURE_HOOKS(weather)
WILD_FEATURE_HOOKS(bounty)

#endif
