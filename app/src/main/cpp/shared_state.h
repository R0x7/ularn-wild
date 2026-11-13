/* shared_state.h - display state shared between the game process and the app.
 *
 * The spawner maps this struct with MAP_SHARED|MAP_ANONYMOUS before fork(),
 * so the game child writes into it and the app reads the same pages through
 * a direct ByteBuffer. Field offsets are reported to Kotlin at runtime via
 * NativeBridge.layout(), so nothing has to be mirrored by hand.
 *
 * Concurrency: the game is the only writer. It bumps `seq` after each batch
 * of changes and pokes the notify pipe; the app redraws from whatever is in
 * the struct. A torn read just shows a half-updated frame for one redraw.
 */
#ifndef ULARN_SHARED_STATE_H
#define ULARN_SHARED_STATE_H

#include <stdint.h>

#define US_MAGIC    0x554C524E   /* 'ULRN' */
#define US_VERSION  2            /* 2 = Wild Edition fields */

#define US_MAP_W    67           /* MAXX */
#define US_MAP_H    17           /* MAXY */
#define US_LINE_LEN 80
#define US_MSG_LINES  5
#define US_TEXT_LINES 25
#define US_EFFECTS  17

/* display_mode */
#define US_DISPLAY_MAP  0
#define US_DISPLAY_TEXT 1

/* input_mode: what the game is currently waiting for */
#define US_INPUT_BUSY     0  /* not waiting (thinking / animating) */
#define US_INPUT_COMMAND  1  /* get_normal_input */
#define US_INPUT_PROMPT   2  /* get_prompt_input: one of `answers` */
#define US_INPUT_DIR      3  /* get_dir_input */
#define US_INPUT_NUMBER   4  /* get_num_input */
#define US_INPUT_PASSWORD 5  /* get_password_input */

/* text format codes (FormatType) */
#define US_FMT_NORMAL    0
#define US_FMT_STANDOUT  1
#define US_FMT_STANDOUT2 2
#define US_FMT_STANDOUT3 3
#define US_FMT_BOLD      4
#define US_FMT_INVERSE   5

/* context action / spells (fields at the end of UlarnShared) */
#define US_SPELLS   40           /* >= SPELL_COUNT (39 in 1.6.3a) */
#define US_CTX_NAME 40

/* ctx_kind: what the player could do with the square they stand on, or
 * with a closed door next to them (see compat/android_context.c). */
#define US_CTX_NONE        0   /* nothing to act on */
#define US_CTX_INSPECT     1   /* a feature with nothing to do (statue, open door...) */
#define US_CTX_TAKE        2   /* an item: '.' re-looks, then 't' takes */
#define US_CTX_GOLD        3   /* gold: picked up by '.' */
#define US_CTX_STAIRS_UP   4   /* '.' then 'u' */
#define US_CTX_STAIRS_DOWN 5   /* '.' then 'd' */
#define US_CTX_SHAFT_DOWN  6   /* volcanic shaft: '.' then 'c' */
#define US_CTX_SHAFT_UP    7   /* '.' then 'c' */
#define US_CTX_ENTER       8   /* building / dungeon entrance: '.' then 'g' */
#define US_CTX_PAD         9   /* Dealer McDope's: '.' then 'c' */
#define US_CTX_CHEST       10  /* 'O' (enhanced interface) or '.' then 'o' */
#define US_CTX_FOUNTAIN    11  /* '.' (prompt: drink / wash) */
#define US_CTX_ALTAR       12  /* '.' (prompt: pray / desecrate) */
#define US_CTX_THRONE      13  /* '.' (prompt: pry / sit) */
#define US_CTX_LAMP        14  /* brass lamp: '.' (prompt: rub / take) */
#define US_CTX_VENDING     15  /* Wild Edition vending machine: '.' (prompt: buy / kick) */
#define US_CTX_COINATM     16  /* Wild Edition LarnCoin ATM: '.' (prompt: buy / sell) */

/* ctx_flags */
#define US_CTXF_ENHANCED   1   /* enhanced interface on ('o' + dir opens doors) */
#define US_CTXF_BLOCKED    2   /* the game won't offer it now (monster adjacent, time stopped) */
#define US_CTXF_TIMESTOP   4   /* time is stopped: nothing can be moved or looked at */

typedef struct UlarnShared
{
  int32_t magic;
  int32_t version;
  volatile int32_t seq;

  int32_t display_mode;
  int32_t input_mode;
  char    answers[64];          /* acceptable keys for US_INPUT_PROMPT */
  int32_t caret;                /* 1 = show caret at cursor */
  int32_t cursor_x;             /* 1-based, in the active text area */
  int32_t cursor_y;
  int32_t beep_count;

  /* map */
  int32_t player_x;
  int32_t player_y;
  int32_t player_cursor;        /* 1 = draw the cursor tile over the player */
  int16_t tiles[US_MAP_H][US_MAP_W];    /* tile index into ularn_gfx */
  int16_t overlay[US_MAP_H][US_MAP_W];  /* -1 = none (spell sparkle etc.) */

  /* status */
  char    status1[96];
  char    status2[96];
  int32_t hp, hpmax, spells, spellmax, gold, plevel, ac, wc;
  int32_t exp;
  int32_t dlevel;               /* dungeon level index */
  char    level_name[8];        /* "H", "1".."10", "V1".. */
  int32_t effects;              /* bit i set = effect i active (see front end) */
  int32_t class_num;            /* character class 0..7, -1 = not chosen yet */
  int32_t sex;                  /* 0 female, 1 male */
  int32_t stats[6];             /* STR (incl. bonus), INT, WIS, CON, DEX, CHA */
  char    level_title[40];      /* "novice explorer" ... */
  int32_t game_time;            /* gtime (turns) */
  int32_t hardness;             /* difficulty */

  /* message area (map mode) and full text page (text mode) */
  char    msg_chr[US_MSG_LINES][US_LINE_LEN + 1];
  uint8_t msg_fmt[US_MSG_LINES][US_LINE_LEN];
  char    text_chr[US_TEXT_LINES][US_LINE_LEN + 1];
  uint8_t text_fmt[US_TEXT_LINES][US_LINE_LEN];

  /* minimap / side panel */
  /* mm_cat: what each map square shows, for the minimap. Written together
   * with tiles[][] (so it only ever reflects what the player can see):
   * low 5 bits = US_MM_* terrain/landmark class, US_MM_MONSTER = a monster
   * is displayed there. */
#define US_MM_UNKNOWN   0
#define US_MM_FLOOR     1
#define US_MM_WALL      2
#define US_MM_DOOR      3
#define US_MM_ITEM      4
#define US_MM_GOLD      5
#define US_MM_UP        6   /* stairs / elevator up */
#define US_MM_DOWN      7   /* stairs / elevator down */
#define US_MM_VOLCANO   8   /* volcanic shaft (down) / tube (up) */
#define US_MM_ENTRANCE  9   /* dungeon entrance */
#define US_MM_BUILDING  10  /* shops, bank, school, home, LRS... */
#define US_MM_FOUNTAIN  11
#define US_MM_ALTAR     12
#define US_MM_THRONE    13
#define US_MM_TRAP      14  /* known traps, pits, teleporters, spheres */
#define US_MM_FEATURE   15  /* statue, mirror */
#define US_MM_MONSTER   0x80
  uint8_t mm_cat[US_MAP_H][US_MAP_W];
  /* Inventory slot a..z as the game's inventory list describes it
   * ("" = empty slot), e.g. "dagger +1 (weapon in hand)". */
#define US_INV_SLOTS 26
#define US_INV_LEN   80
  char    inv_text[US_INV_SLOTS][US_INV_LEN];
  /* context action / spells */
  int32_t ctx_object;           /* item[playerx][playery]; hidden traps read as ONOTHING */
  int32_t ctx_kind;             /* US_CTX_* */
  int32_t ctx_flags;            /* US_CTXF_* */
  int32_t ctx_doors;            /* bit i: known closed door in direction "hjklyubn"[i] */
  char    ctx_name[US_CTX_NAME]; /* e.g. "a staircase going down" */
  int32_t spell_count;          /* SPELL_COUNT */
  uint8_t spell_known[US_SPELLS];        /* spelknow[] != 0 */
  char    spell_codes[US_SPELLS][4];     /* spelcode[], e.g. "pro" */
  /* danger cues / stats */
  int32_t run_id;               /* random id of the current character, kept in
                                   its save / checkpoint; 0 = no character yet */
  int32_t kills;                /* monsters the player killed this run (also
                                   kept in the save) */
  int32_t end_state;            /* US_END_*: set when the final score shows */
  int32_t end_score;
  char    end_reason[US_LINE_LEN + 1];  /* e.g. "killed by a giant ant on 3" */

  /* ---- Wild Edition (compat/wild.c: wild_paint(), wild_event()) ---- */
#define US_KILL_COUNTS 96          /* >= MONST_COUNT */
#define US_EVENTS      64          /* ring size, power of two */
  int32_t wild_flags;           /* low 16: feature mask; high 16: perk bits */
  int32_t hat_tile;             /* overlay tile drawn on the player, 0 = none */
  int32_t glitch;               /* turns of glitch left (0 = reality is fine) */
  int32_t weather;              /* hour (0-23) | weather state << 8 */
  int32_t coin_price;           /* LarnCoin price in gold; < 0: ticker hidden (wild_coin.c) */
  int32_t pet_x, pet_y;         /* the pet's cell, -1 = none on this level */
  int32_t kill_counts[US_KILL_COUNTS];
  /* Event ring: ev[seq & (US_EVENTS-1)] = {type, a, b, c}; the app keeps the
   * last seq it consumed and reads forward. US_EV_* types below. */
  volatile int32_t ev_seq;
  int32_t ev[US_EVENTS][4];
} UlarnShared;

/* event types (a, b, c are per type) */
#define US_EV_NONE        0
#define US_EV_KILL        1   /* a = monster id, b/c = cell */
#define US_EV_LEVEL_UP    2   /* a = new character level */
#define US_EV_NEW_LEVEL   3   /* a = dungeon level index */
#define US_EV_TAKE        4   /* a = object id, b = arg */
#define US_EV_PLAYER_HIT  5   /* a = damage */
#define US_EV_GOLD        6   /* a = amount */
#define US_EV_SPELL       7   /* a = spell id */
#define US_EV_DIED        8
#define US_EV_WON         9
/* feature-specific events: 100 + 10 * feature index */
#define US_EV_WILD_RADIO    100  /* a = track unlocked */
#define US_EV_WILD_CARD     110  /* a = monster id, b = holo */
#define US_EV_WILD_VENDING  120  /* a = 0 buy, 1 kick, 2 topple, 3 roomba ate */
#define US_EV_WILD_HAT      130  /* a = hat object id or 0 (taken off) */
#define US_EV_WILD_COIN     140  /* a = 0 tick, 1 news, 2 rug pull, 3 auditor */
#define US_EV_WILD_GLITCH   150  /* a = 1 start, 0 end */
#define US_EV_WILD_FISHING  160  /* a = 0 cast, 1 catch (b = object), 2 kraken, 3 treasure map (b,c = x,y) */
#define US_EV_WILD_PET      170  /* a = 0 hatch, 1 attack, 2 died, 3 follow */
#define US_EV_WILD_PERK     180  /* a = perk index chosen */
#define US_EV_WILD_WEATHER  190  /* a = new weather state, b = hour */

/* end_state */
#define US_END_NONE 0
#define US_END_DIED 1
#define US_END_WON  2
#define US_END_QUIT 3

#endif
