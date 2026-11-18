/* =============================================================================
 * FILENAME: ularn_winandroid.c
 *
 * Android front end for Ularn 1.6.3a: implements ularn_win.h.
 *
 * Ported from Julian Olds' X11 front end (ularn_winx11.c). Instead of drawing,
 * every display call updates a UlarnShared struct in shared memory that the
 * app renders (tiles from ularn_gfx, status, effects, messages, text pages).
 * Input arrives as ASCII bytes on a pipe and is mapped to game actions with
 * the same default key bindings as the X11 / TTY versions.
 * =============================================================================
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "header.h"
#include "ularn_game.h"
#include "dungeon.h"
#include "player.h"
#include "ularn_win.h"
#include "monster.h"
#include "itm.h"

#include "savegame.h"
#include "../shared_state.h"
#include "ularn_android_panel.h"
#include "wild.h"
#include "wild_glitch.h"

/* Wiring, set up by spawn.c in the child before ularn_main() runs. */
UlarnShared *ularn_shared = NULL;
int ularn_input_fd = -1;
int ularn_notify_fd = -1;

#define S ularn_shared

/* ---- exported variables required by ularn_win.h ---- */
int nonap = 0;
int nosignal = 0;
char enable_scroll = 0;
int yrepcount = 0;

DisplayModeType CurrentDisplayMode = DISPLAY_TEXT;

/* ---- key bindings (ASCII only; the app maps arrows etc. to letters) ---- */

#define NUM_DIRS 8
static ActionType DirActions[NUM_DIRS] =
{
  ACTION_MOVE_WEST, ACTION_MOVE_EAST, ACTION_MOVE_SOUTH, ACTION_MOVE_NORTH,
  ACTION_MOVE_NORTHEAST, ACTION_MOVE_NORTHWEST, ACTION_MOVE_SOUTHEAST,
  ACTION_MOVE_SOUTHWEST
};

static ActionType KeyAction[128];

static void init_keymap(void)
{
  int i;
  for (i = 0; i < 128; i++) KeyAction[i] = ACTION_NULL;
  KeyAction['~'] = ACTION_DIAG;
  KeyAction['h'] = ACTION_MOVE_WEST;      KeyAction['H'] = ACTION_RUN_WEST;
  KeyAction['l'] = ACTION_MOVE_EAST;      KeyAction['L'] = ACTION_RUN_EAST;
  KeyAction['j'] = ACTION_MOVE_SOUTH;     KeyAction['J'] = ACTION_RUN_SOUTH;
  KeyAction['k'] = ACTION_MOVE_NORTH;     KeyAction['K'] = ACTION_RUN_NORTH;
  KeyAction['u'] = ACTION_MOVE_NORTHEAST; KeyAction['U'] = ACTION_RUN_NORTHEAST;
  KeyAction['y'] = ACTION_MOVE_NORTHWEST; KeyAction['Y'] = ACTION_RUN_NORTHWEST;
  KeyAction['n'] = ACTION_MOVE_SOUTHEAST; KeyAction['N'] = ACTION_RUN_SOUTHEAST;
  KeyAction['b'] = ACTION_MOVE_SOUTHWEST; KeyAction['B'] = ACTION_RUN_SOUTHWEST;
  KeyAction['.'] = ACTION_WAIT;
  KeyAction[' '] = ACTION_NONE;
  KeyAction['w'] = ACTION_WIELD;
  KeyAction['W'] = ACTION_WEAR;
  KeyAction['r'] = ACTION_READ;
  KeyAction['q'] = ACTION_QUAFF;
  KeyAction['d'] = ACTION_DROP;
  KeyAction['c'] = ACTION_CAST_SPELL;
  KeyAction['o'] = ACTION_OPEN_DOOR;
  KeyAction['C'] = ACTION_CLOSE_DOOR;
  KeyAction['O'] = ACTION_OPEN_CHEST;
  KeyAction['i'] = ACTION_INVENTORY;
  KeyAction['e'] = ACTION_EAT_COOKIE;
  KeyAction['\\'] = ACTION_LIST_SPELLS;
  KeyAction['?'] = ACTION_HELP;
  KeyAction['S'] = ACTION_SAVE;
  KeyAction['Z'] = ACTION_TELEPORT;
  KeyAction['^'] = ACTION_IDENTIFY_TRAPS;
  KeyAction['_'] = ACTION_BECOME_CREATOR;
  KeyAction['+'] = ACTION_CREATE_ITEM;
  KeyAction['-'] = ACTION_TOGGLE_WIZARD;
  KeyAction['`'] = ACTION_DEBUG_MODE;
  KeyAction['T'] = ACTION_REMOVE_ARMOUR;
  KeyAction['g'] = ACTION_PACK_WEIGHT;
  KeyAction['v'] = ACTION_VERSION;
  KeyAction['Q'] = ACTION_QUIT;
  KeyAction[0x12] = ACTION_REDRAW_SCREEN;   /* Ctrl+R */
  KeyAction['P'] = ACTION_SHOW_TAX;
  /* Conveniences: 'D' (discoveries in older Ularn) and ',' also list spells */
  KeyAction['D'] = ACTION_LIST_SPELLS;
}

/* ---- tiles (from ularn_winx11.c) ---- */

static int PlayerTiles[8][2] =
{
  { 165, 181 }, { 166, 182 }, { 167, 183 }, { 168, 184 },
  { 169, 185 }, { 170, 186 }, { 171, 187 }, { 172, 188 }
};

/* The monster a mimic is currently pretending to be. */
static int mimicmonst = MIMIC;

#define TILE_CURSOR1 174
#define WALL_TILES   352

static int EffectTile[EFFECT_COUNT][9] =
{
  { 191, 198, 196, 194, 192, 195, 193, 197, 199 },
  { 191, 206, 204, 202, 200, 203, 201, 205, 207 },
  { 191, 214, 212, 210, 208, 211, 209, 213, 215 },
  { 191, 222, 220, 218, 216, 219, 217, 221, 223 },
  { 191, 230, 228, 226, 224, 227, 225, 229, 231 }
};

#define MAX_MAGICFX_FRAME 8
struct MagicEffectDataType
{
  int Overlay;
  int Frames;
  int Tile1[MAX_MAGICFX_FRAME];
};

static struct MagicEffectDataType magicfx_tile[MAGIC_COUNT] =
{
  { 1, 8, { 240, 241, 242, 243, 244, 245, 246, 247 } }, /* Sparkle */
  { 0, 6, { 256, 272, 288, 304, 320, 336, 0, 0 } },     /* Sleep */
  { 0, 6, { 257, 273, 289, 305, 321, 337, 0, 0 } },     /* Web */
  { 0, 6, { 258, 274, 290, 306, 322, 338, 0, 0 } },     /* Phantasmal */
  { 0, 6, { 259, 275, 291, 307, 323, 339, 0, 0 } },     /* Cloud kill */
  { 0, 6, { 260, 276, 292, 308, 324, 340, 0, 0 } },     /* Vaporize rock */
  { 0, 6, { 261, 277, 293, 309, 325, 341, 0, 0 } },     /* Dehydrate */
  { 0, 6, { 262, 278, 294, 310, 326, 342, 0, 0 } },     /* Drain life */
  { 0, 6, { 263, 279, 295, 311, 327, 343, 0, 0 } },     /* Flood */
  { 0, 6, { 264, 280, 296, 312, 328, 344, 0, 0 } },     /* Finger of death */
  { 0, 6, { 265, 281, 297, 313, 329, 345, 0, 0 } },     /* Teleport away */
  { 0, 6, { 266, 282, 298, 314, 330, 346, 0, 0 } },     /* Magic fire */
  { 0, 6, { 267, 283, 299, 315, 331, 347, 0, 0 } },     /* Make wall */
  { 0, 6, { 268, 284, 300, 316, 332, 348, 0, 0 } },     /* Summon demon */
  { 0, 6, { 269, 285, 301, 317, 333, 349, 0, 0 } }      /* Annihilate */
};

/* Effects shown in the effects panel, in order (bit i of S->effects). */
static int EffectIds[US_EFFECTS] =
{
  STEALTH, UNDEADPRO, SPIRITPRO, CHARMCOUNT, TIMESTOP, HOLDMONST, GIANTSTR,
  FIRERESISTANCE, DEXCOUNT, STRCOUNT, SCAREMONST, HASTESELF, CANCELLATION,
  INVISIBILITY, ALTPRO, PROTECTIONTIME, WTW
};

/* ---- text buffers (point into shared memory) ---- */

typedef char TextLine[US_LINE_LEN + 1];
typedef uint8_t FormatLine[US_LINE_LEN];

static TextLine *Text;
static FormatLine *Format;
static int MaxLine;
static FormatType CurrentFormat = FORMAT_NORMAL;
static int CursorX = 1, CursorY = 1;

static FormatType CurrentMsgFormat = FORMAT_NORMAL, CurrentTextFormat = FORMAT_NORMAL;
static int MsgCursorX = 1, MsgCursorY = 1, TextCursorX = 1, TextCursorY = 1;

/* ---- change notification ---- */

static void publish(void)
{
  char b = 1;
  if (!S) return;
  S->cursor_x = CursorX;
  S->cursor_y = CursorY;
  android_panel_inventory(S);
  __sync_synchronize();
  S->seq++;
  __sync_synchronize();
  if (ularn_notify_fd >= 0)
  {
    /* Non-blocking: if the pipe is full the app is already going to redraw. */
    if (write(ularn_notify_fd, &b, 1) < 0) { /* ignore */ }
  }
}

/* Read one key from the app; 0 on end of input (app went away). */
static int read_key(void)
{
  unsigned char ch;
  ssize_t n;

  for (;;)
  {
    n = read(ularn_input_fd, &ch, 1);
    if (n == 1) break;
    if (n < 0 && errno == EINTR) continue;
    /* App closed the pipe: leave like a hang-up. */
    ularn_exit(1);
  }

  /* Normalise what touch / soft keyboards send to what the game expects. */
  if (ch == '\n') ch = '\r';
  if (ch == 0x7f) ch = '\010';
  return ch;
}

static void set_input_mode(int mode, const char *answers, int caret)
{
  S->input_mode = mode;
  S->caret = caret;
  if (answers)
  {
    strncpy(S->answers, answers, sizeof(S->answers) - 1);
    S->answers[sizeof(S->answers) - 1] = 0;
  }
  else S->answers[0] = 0;
  publish();
}

static void clear_input_mode(void)
{
  S->input_mode = US_INPUT_BUSY;
  S->caret = 0;
  S->answers[0] = 0;
}

/* ---- run statistics (danger cues / stats) ----
 *
 * The app keeps per-run statistics. Two unused slots of the attribute array
 * c[] (saved with the game, checkpoint included) hold a random id for the
 * character and the number of monsters it has killed, so a restored game
 * keeps both and nothing is counted twice. The greedy-cheater check only
 * looks at the ability scores, so these slots are left alone by the game.
 */
#define C_RUN_ID UNUSED_99
#define C_KILLS  UNUSED_98

/* Called at each command prompt: by then the character exists. */
static void note_run(void)
{
  if (c[C_RUN_ID] <= 0)
    c[C_RUN_ID] = (long) ((arc4random() & 0x3fffffff) | 1);
  S->run_id = (int32_t) c[C_RUN_ID];
  S->kills = (int32_t) c[C_KILLS];
}

/* hitm() reports each monster the player kills with "\nThe %s died!". */
static void note_kill(const char *str)
{
  size_t n = strlen(str);
  if (n > 11 && strncmp(str, "\nThe ", 5) == 0 && strcmp(str + n - 6, " died!") == 0)
  {
    c[C_KILLS]++;
    S->kills = (int32_t) c[C_KILLS];
  }
}

/*
 * died() shows the final score (show_player_score) and then waits with
 * "Press return to continue". Read the score and the cause of death from that
 * page. (Quitting with Q and saving skip this page; the app tells those apart
 * by which save files are left.)
 */
static void note_game_end(const char *prompt, const char *answers)
{
  char joined[US_TEXT_LINES * US_LINE_LEN + 1];
  char reason[US_LINE_LEN + 1];
  char *p;
  long score = 0;
  int y, n = 0, r = 0, space = 0;
  size_t len;

  if (S->end_state != US_END_NONE || S->run_id == 0) return;
  if (CurrentDisplayMode != DISPLAY_TEXT) return;
  if (strcmp(answers, "\015") != 0 || strstr(prompt, "Press return") == NULL) return;

  for (y = 0; y < US_TEXT_LINES; y++)
    if (strstr(S->text_chr[y], "U L A R N    S C O R E S") != NULL) break;
  for (; y < US_TEXT_LINES; y++)
    if (sscanf(S->text_chr[y], "Score: %ld", &score) == 1) break;
  if (y >= US_TEXT_LINES) return;

  /* The "<name> <reason>" line, possibly wrapped, up to the closing rule. */
  for (y++; y < US_TEXT_LINES && strncmp(S->text_chr[y], "-----", 5) != 0; y++)
  {
    memcpy(joined + n, S->text_chr[y], US_LINE_LEN);
    n += US_LINE_LEN;
  }
  joined[n] = 0;

  /* Collapse whitespace. */
  for (p = joined; *p && r < US_LINE_LEN; p++)
  {
    if (*p == ' ' || *p == '\t') { space = (r > 0); continue; }
    if (space && r < US_LINE_LEN - 1) reason[r++] = ' ';
    space = 0;
    reason[r++] = *p;
  }
  reason[r] = 0;

  p = reason;
  len = strlen(logname);
  if (len > 0 && strncmp(p, logname, len) == 0) p += len;
  while (*p == ' ') p++;

  strncpy(S->end_reason, p, US_LINE_LEN);
  S->end_reason[US_LINE_LEN] = 0;
  S->end_score = (int32_t) score;
  S->kills = (int32_t) c[C_KILLS];
  if (strncmp(p, "a winner", 8) == 0) S->end_state = US_END_WON;
  else if (strncmp(p, "quit", 4) == 0) S->end_state = US_END_QUIT;
  else S->end_state = US_END_DIED;
}

/* ---- map ---- */

static void GetTile(int x, int y, int *TileId)
{
  MonsterIdType k;

  if ((x == playerx) && (y == playery) && (c[BLINDCOUNT] == 0))
  {
    *TileId = PlayerTiles[class_num][(int) sex];
    return;
  }

  if (know[x][y] == OUNKNOWN)
  {
    *TileId = objtilelist[OUNKNOWN];
  }
  else
  {
    k = mitem[x][y].mon;
    if (k != 0)
    {
      if ((c[BLINDCOUNT] == 0) &&
          (((stealth[x][y] & STEALTH_SEEN) != 0) ||
           ((stealth[x][y] & STEALTH_AWAKE) != 0)))
      {
        if (k == MIMIC)
        {
          if ((gtime % 10) == 0)
          {
            /* Wild Edition: never disguised as one of its monsters */
            do mimicmonst = rnd(MAXMONST);
            while (mimicmonst == INVISIBLESTALKER || is_wild_monst(mimicmonst));
          }
          *TileId = monsttilelist[mimicmonst];
        }
        else if ((k == INVISIBLESTALKER) && (c[SEEINVISIBLE] == 0))
        {
          *TileId = objtilelist[(int) know[x][y]];
        }
        else if ((k >= DEMONLORD) && (k <= LUCIFER) && (c[EYEOFLARN] == 0))
        {
          *TileId = objtilelist[(int) know[x][y]];
        }
        else
        {
          *TileId = monsttilelist[k];
        }
      }
      else
      {
        *TileId = objtilelist[(int) know[x][y]];
      }
    }
    else
    {
      k = know[x][y];
      *TileId = objtilelist[k];
    }
    /* Wild Edition glitch: items (only) are drawn as other items */
    if (*TileId == objtilelist[(int) know[x][y]])
      *TileId = wild_glitch_object_tile(know[x][y], *TileId);
  }

  if (*TileId == objtilelist[OWALL])
  {
    *TileId = WALL_TILES + iarg[x][y];
  }
}

static void set_tile(int x, int y)
{
  int t;
  GetTile(x, y, &t);
  S->tiles[y][x] = (int16_t) t;
  S->overlay[y][x] = -1;
  /* A monster is "displayed" when GetTile() chose its tile (Wild Edition
   * monsters have tiles >= 384, so the panel can't go by tile range). */
  {
    int m = mitem[x][y].mon;
    int shown = m != MONST_NONE && !(x == playerx && y == playery) &&
                (t == monsttilelist[m] || (m == MIMIC && t == monsttilelist[mimicmonst]));
    android_panel_cell(S, x, y, shown);
  }
}

/* Recompute every tile from game state (cheap: 67x17). */
static void PaintMap(void)
{
  int x, y;
  for (y = 0; y < MAXY; y++)
    for (x = 0; x < MAXX; x++)
      set_tile(x, y);
  S->player_x = playerx;
  S->player_y = playery;
  S->player_cursor = (c[BLINDCOUNT] == 0);
  wild_paint();   /* Wild Edition: hat, glitch, weather, pet, events... */
}

static void PaintStatus(void)
{
  char Line[96];
  char Buf[96];
  int i;

  Line[0] = 0;
  if (c[SPELLMAX] > 99)
    sprintf(Buf, "Spells:%3ld(%3ld)", (long) c[SPELLS], (long) c[SPELLMAX]);
  else
    sprintf(Buf, "Spells:%3ld(%2ld) ", (long) c[SPELLS], (long) c[SPELLMAX]);
  strcat(Line, Buf);
  sprintf(Buf, " AC: %-3ld  WC: %-3ld  Level", (long) c[AC], (long) c[WCLASS]);
  strcat(Line, Buf);
  if (c[LEVEL] > 99) sprintf(Buf, "%3ld", (long) c[LEVEL]);
  else sprintf(Buf, " %-2ld", (long) c[LEVEL]);
  strcat(Line, Buf);
  sprintf(Buf, " Exp: %-9ld %s", (long) c[EXPERIENCE], class[c[LEVEL] - 1]);
  strcat(Line, Buf);
  strncpy(S->status1, Line, sizeof(S->status1) - 1);

  sprintf(Buf, "%ld (%ld)", (long) c[HP], (long) c[HPMAX]);
  snprintf(Line, sizeof(Line),
           "HP: %11s STR=%-2ld INT=%-2ld WIS=%-2ld CON=%-2ld DEX=%-2ld CHA=%-2ld LV:",
           Buf, (long) (c[STRENGTH] + c[STREXTRA]), (long) c[INTELLIGENCE],
           (long) c[WISDOM], (long) c[CONSTITUTION], (long) c[DEXTERITY],
           (long) c[CHARISMA]);

  if ((level == 0) || (wizard)) c[TELEFLAG] = 0;

  if (c[TELEFLAG]) strcat(Line, " ?");
  else strcat(Line, levelname[level]);

  sprintf(Buf, "  Gold: %-8ld", (long) c[GOLD]);
  strncat(Line, Buf, sizeof(Line) - strlen(Line) - 1);
  strncpy(S->status2, Line, sizeof(S->status2) - 1);

  S->hp = c[HP];
  S->hpmax = c[HPMAX];
  S->spells = c[SPELLS];
  S->spellmax = c[SPELLMAX];
  S->gold = c[GOLD];
  S->plevel = c[LEVEL];
  S->ac = c[AC];
  S->wc = c[WCLASS];
  S->exp = c[EXPERIENCE];
  S->dlevel = level;
  if (c[TELEFLAG]) strcpy(S->level_name, "?");
  else
  {
    strncpy(S->level_name, levelname[level], sizeof(S->level_name) - 1);
    S->level_name[sizeof(S->level_name) - 1] = 0;
  }
  S->class_num = class_num;
  S->sex = sex;
  S->stats[0] = c[STRENGTH] + c[STREXTRA];
  S->stats[1] = c[INTELLIGENCE];
  S->stats[2] = c[WISDOM];
  S->stats[3] = c[CONSTITUTION];
  S->stats[4] = c[DEXTERITY];
  S->stats[5] = c[CHARISMA];
  strncpy(S->level_title, class[c[LEVEL] - 1], sizeof(S->level_title) - 1);
  S->level_title[sizeof(S->level_title) - 1] = 0;
  S->game_time = (int32_t) gtime;
  S->hardness = c[HARDGAME];

  c[TMP] = c[STRENGTH] + c[STREXTRA];
  for (i = 0; i < 100; i++) cbak[i] = c[i];
}

static void PaintEffects(void)
{
  int i, bits = 0;
  for (i = 0; i < US_EFFECTS; i++)
  {
    if (c[EffectIds[i]] != 0) bits |= (1 << i);
    cbak[EffectIds[i]] = c[EffectIds[i]];
  }
  S->effects = bits;
}

static void PaintWindow(void)
{
  if (CurrentDisplayMode == DISPLAY_MAP)
  {
    PaintEffects();
    PaintStatus();
    PaintMap();
  }
  publish();
}

/* ---- ularn_win.h implementation ---- */

int init_app(void)
{
  int y;

  init_keymap();

  memset(S->msg_chr, ' ', sizeof(S->msg_chr));
  memset(S->text_chr, ' ', sizeof(S->text_chr));
  for (y = 0; y < US_MSG_LINES; y++) S->msg_chr[y][US_LINE_LEN] = 0;
  for (y = 0; y < US_TEXT_LINES; y++) S->text_chr[y][US_LINE_LEN] = 0;
  memset(S->msg_fmt, 0, sizeof(S->msg_fmt));
  memset(S->text_fmt, 0, sizeof(S->text_fmt));
  memset(S->overlay, 0xff, sizeof(S->overlay));
  S->class_num = -1;

  Text = S->text_chr;
  Format = S->text_fmt;
  MaxLine = US_TEXT_LINES;
  CurrentDisplayMode = DISPLAY_TEXT;
  S->display_mode = US_DISPLAY_TEXT;
  clear_input_mode();
  publish();
  return 1;
}

void close_app(void)
{
  publish();
}

/*
 * Checkpointing. 1.6.3a restores checkpoints (-r) and deletes them when a
 * game ends, but nothing writes them; Android may kill a backgrounded app at
 * any time, so write one here, between commands, where the game state is
 * always consistent: every CKP_INTERVAL commands, and whenever the app asks
 * (it sends KEY_CHECKPOINT when it goes to the background).
 */
#define CKP_INTERVAL   25
#define KEY_CHECKPOINT 0x1d   /* Ctrl+] - not bound to anything in the game */

void android_update_context(void);   /* android_context.c */

static void checkpoint(void)
{
  int saved_nosignal = nosignal;
  if (!ckpflag) return;
  savegame(ckpfile);
  nosignal = saved_nosignal;
}

ActionType get_normal_input(void)
{
  static int commands = 0;
  ActionType Event = ACTION_NULL;
  int ch;

  if (++commands % CKP_INTERVAL == 0) checkpoint();
  note_run();

  android_update_context();   /* context action / spells (android_context.c) */
  set_input_mode(US_INPUT_COMMAND, NULL, 0);
  while (Event == ACTION_NULL)
  {
    ch = read_key();
    if (ch == KEY_CHECKPOINT) { checkpoint(); continue; }
    /* Wild Edition test aid: in wizard mode '=' opens the debug menu. */
    if (ch == '=' && wizard) { clear_input_mode(); Event = wild_debug(); if (Event == ACTION_NULL) set_input_mode(US_INPUT_COMMAND, NULL, 0); continue; }
    if (ch < 128) Event = KeyAction[ch];

    if (!enhance_interface &&
        ((Event == ACTION_OPEN_DOOR) || (Event == ACTION_OPEN_CHEST)))
      Event = ACTION_NULL;
  }
  clear_input_mode();
  return Event;
}

char get_prompt_input(char *prompt, char *answers, int ShowCursor)
{
  int ch;

  note_game_end(prompt, answers);
  Print(prompt);
  set_input_mode(US_INPUT_PROMPT, answers, ShowCursor);
  for (;;)
  {
    ch = read_key();
    if (ch != 0 && strchr(answers, ch) != NULL) break;
  }
  clear_input_mode();
  publish();
  return (char) ch;
}

void get_password_input(char *password, int Len)
{
  int ch;
  int Pos = 0;

  set_input_mode(US_INPUT_PASSWORD, NULL, 1);
  do
  {
    ch = read_key();
    if (isprint(ch) && (Pos < Len))
    {
      password[Pos++] = (char) ch;
      Printc('*');
      publish();
    }
    else if (ch == '\010' && Pos > 0)
    {
      CursorX--; Printc(' '); CursorX--; Pos--;
      publish();
    }
  } while (ch != '\015' && ch != 27);
  password[Pos] = 0;
  clear_input_mode();
  publish();
}

int get_num_input(int defval)
{
  int ch;
  int Pos = 0;
  int value = 0;
  int neg = 0;

  set_input_mode(US_INPUT_NUMBER, "-*0123456789\010\015", 1);
  do
  {
    ch = read_key();
    if (ch == 27) { clear_input_mode(); publish(); return defval; }
    if ((ch == '-') && (Pos == 0))
    {
      neg = 1; Printc((char) ch); Pos++;
    }
    else if (ch == '*')
    {
      clear_input_mode(); publish();
      return defval;
    }
    else if (ch == '\010')
    {
      if (Pos > 0)
      {
        if ((Pos == 1) && neg) neg = 0;
        else value = value / 10;
        CursorX--; Printc(' '); CursorX--; Pos--;
      }
    }
    else if ((ch >= '0') && (ch <= '9'))
    {
      value = value * 10 + (ch - '0');
      Printc((char) ch);
      Pos++;
    }
    publish();
  } while (ch != '\015');

  clear_input_mode();
  publish();
  if (Pos == 0) return defval;
  return neg ? -value : value;
}

ActionType get_dir_input(char *prompt, int ShowCursor)
{
  ActionType Event;
  int idx, got_dir = 0;
  int ch;

  Print(prompt);
  set_input_mode(US_INPUT_DIR, NULL, ShowCursor);
  for (;;)
  {
    ch = read_key();
    Event = (ch < 128) ? KeyAction[ch] : ACTION_NULL;
    for (idx = 0; idx < NUM_DIRS && !got_dir; idx++)
      if (DirActions[idx] == Event) got_dir = 1;
    if (got_dir) break;
  }
  clear_input_mode();
  publish();
  return Event;
}

void UpdateStatus(void)
{
  if (CurrentDisplayMode == DISPLAY_TEXT) return;
  PaintStatus();
  publish();
}

void UpdateEffects(void)
{
  if (CurrentDisplayMode == DISPLAY_TEXT) return;
  PaintEffects();
  publish();
}

void UpdateStatusAndEffects(void)
{
  if (CurrentDisplayMode == DISPLAY_TEXT) return;
  PaintEffects();
  PaintStatus();
  publish();
}

void set_display(DisplayModeType Mode)
{
  if (CurrentDisplayMode == DISPLAY_MAP)
  {
    MsgCursorX = CursorX; MsgCursorY = CursorY; CurrentMsgFormat = CurrentFormat;
  }
  else
  {
    TextCursorX = CursorX; TextCursorY = CursorY; CurrentTextFormat = CurrentFormat;
  }

  CurrentDisplayMode = Mode;

  if (CurrentDisplayMode == DISPLAY_MAP)
  {
    CursorX = MsgCursorX; CursorY = MsgCursorY; CurrentFormat = CurrentMsgFormat;
    Text = S->msg_chr; Format = S->msg_fmt; MaxLine = US_MSG_LINES;
    S->display_mode = US_DISPLAY_MAP;
  }
  else
  {
    CursorX = TextCursorX; CursorY = TextCursorY; CurrentFormat = CurrentTextFormat;
    Text = S->text_chr; Format = S->text_fmt; MaxLine = US_TEXT_LINES;
    S->display_mode = US_DISPLAY_TEXT;
  }

  PaintWindow();
}

static void IncCursorY(int Count)
{
  int Line;

  while (Count-- > 0)
  {
    CursorY++;
    if (CursorY > MaxLine)
    {
      for (Line = 0; Line < MaxLine - 1; Line++)
      {
        memcpy(Text[Line], Text[Line + 1], US_LINE_LEN);
        memcpy(Format[Line], Format[Line + 1], US_LINE_LEN);
      }
      CursorY--;
      memset(Text[MaxLine - 1], ' ', US_LINE_LEN);
      memset(Format[MaxLine - 1], FORMAT_NORMAL, US_LINE_LEN);
    }
  }
}

static void IncCursorX(int Count)
{
  CursorX += Count;
  if (CursorX > US_LINE_LEN)
  {
    CursorX = 1;
    IncCursorY(1);
  }
}

void ClearText(void)
{
  int y;
  for (y = 0; y < MaxLine; y++)
  {
    memset(Text[y], ' ', US_LINE_LEN);
    Text[y][US_LINE_LEN] = 0;
    memset(Format[y], FORMAT_NORMAL, US_LINE_LEN);
  }
  CursorX = 1;
  CursorY = 1;
  publish();
}

void UlarnBeep(void)
{
  if (!nobeep)
  {
    S->beep_count++;
    publish();
  }
}

void MoveCursor(int x, int y)
{
  if (x < 1) x = 1;
  if (x > US_LINE_LEN) x = US_LINE_LEN;
  if (y < 1) y = 1;
  if (y > MaxLine) y = MaxLine;
  CursorX = x;
  CursorY = y;
}

void Printc(char ch)
{
  int incx;

  switch (ch)
  {
    case '\t':
      incx = ((((CursorX - 1) / 8) + 1) * 8 + 1) - CursorX;
      IncCursorX(incx);
      break;

    case '\n':
      CursorX = 1;
      IncCursorY(1);
      break;

    case '\015':
      break;

    default:
      if (CursorY < 1 || CursorY > MaxLine || CursorX < 1 || CursorX > US_LINE_LEN)
        return;
      Text[CursorY - 1][CursorX - 1] = ch;
      Format[CursorY - 1][CursorX - 1] = (uint8_t) CurrentFormat;
      IncCursorX(1);
      break;
  }
}

void Print(char *string)
{
  if (string == NULL || *string == 0) return;
  note_kill(string);
  while (*string) Printc(*string++);
  publish();
}

void Printf(char *fmt, ...)
{
  char buf[2048];
  va_list argptr;

  va_start(argptr, fmt);
  vsnprintf(buf, sizeof(buf), fmt, argptr);
  va_end(argptr);

  Print(buf);
}

void Standout(char *String)
{
  CurrentFormat = FORMAT_STANDOUT;
  Print(String);
  CurrentFormat = FORMAT_NORMAL;
}

void SetFormat(FormatType format)
{
  CurrentFormat = format;
}

void ClearToEOL(void)
{
  int x;
  if (CursorY < 1 || CursorY > MaxLine) return;
  for (x = CursorX; x <= US_LINE_LEN; x++)
  {
    Text[CursorY - 1][x - 1] = ' ';
    Format[CursorY - 1][x - 1] = FORMAT_NORMAL;
  }
}

void ClearToEOPage(int x, int y)
{
  int tx, ty;

  if (y < 1) y = 1;
  if (y > MaxLine) return;
  for (tx = x; tx <= US_LINE_LEN; tx++)
  {
    Text[y - 1][tx - 1] = ' ';
    Format[y - 1][tx - 1] = FORMAT_NORMAL;
  }
  for (ty = y + 1; ty <= MaxLine; ty++)
  {
    memset(Text[ty - 1], ' ', US_LINE_LEN);
    memset(Format[ty - 1], FORMAT_NORMAL, US_LINE_LEN);
  }
}

void show1cell(int x, int y)
{
  if (c[BLINDCOUNT]) return;

  know[x][y] = item[x][y];
  if (mitem[x][y].mon != MONST_NONE)
    stealth[x][y] |= STEALTH_SEEN;

  set_tile(x, y);
}

void showplayer(void)
{
  /* The previous player square loses the player tile. */
  if (S->player_x >= 0 && S->player_x < MAXX && S->player_y >= 0 && S->player_y < MAXY)
    set_tile(S->player_x, S->player_y);
  S->player_x = playerx;
  S->player_y = playery;
  S->player_cursor = (c[BLINDCOUNT] == 0);
  set_tile(playerx, playery);
  publish();
}

void showcell(int x, int y)
{
  int minx, maxx, miny, maxy, mx, my;

  if (c[AWARENESS])
  {
    minx = x - 3; maxx = x + 3; miny = y - 3; maxy = y + 3;
  }
  else
  {
    minx = x - 1; maxx = x + 1; miny = y - 1; maxy = y + 1;
  }

  if (c[BLINDCOUNT])
  {
    minx = maxx = x;
    miny = maxy = y;
  }

  if (minx < 0) minx = 0;
  if (maxx > MAXX - 1) maxx = MAXX - 1;
  if (miny < 0) miny = 0;
  if (maxy > MAXY - 1) maxy = MAXY - 1;

  for (my = miny; my <= maxy; my++)
  {
    for (mx = minx; mx <= maxx; mx++)
    {
      if ((mx == playerx) && (my == playery))
      {
        know[mx][my] = item[mx][my];
      }
      else if ((know[mx][my] != item[mx][my]) ||
               ((mx == lastpx) && (my == lastpy)) ||
               ((mitem[mx][my].mon != MONST_NONE) &&
                ((stealth[mx][my] & STEALTH_SEEN) == 0)))
      {
        know[mx][my] = item[mx][my];
        if (mitem[mx][my].mon != MONST_NONE)
          stealth[mx][my] |= STEALTH_SEEN;
      }
    }
  }

  if (lastpx >= 0 && lastpx < MAXX && lastpy >= 0 && lastpy < MAXY)
    set_tile(lastpx, lastpy);
  for (my = miny; my <= maxy; my++)
    for (mx = minx; mx <= maxx; mx++)
      set_tile(mx, my);

  showplayer();
}

void drawscreen(void)
{
  PaintWindow();
}

void draws(int minx, int miny, int maxx, int maxy)
{
  (void) minx; (void) miny; (void) maxx; (void) maxy;
  PaintWindow();
}

void mapeffect(int x, int y, DirEffectsType effect, int dir)
{
  if (c[BLINDCOUNT]) return;
  if (x < 0 || x >= MAXX || y < 0 || y >= MAXY) return;
  S->tiles[y][x] = (int16_t) EffectTile[effect][dir];
  S->overlay[y][x] = -1;
  publish();
}

int magic_effect_frames(MagicEffectsType fx)
{
  return magicfx_tile[fx].Frames;
}

void magic_effect(int x, int y, MagicEffectsType fx, int frame)
{
  int TileId;

  if (frame > magicfx_tile[fx].Frames) return;
  if (frame < 0 || frame >= MAX_MAGICFX_FRAME) return;
  if (c[BLINDCOUNT]) return;
  if (x < 0 || x >= MAXX || y < 0 || y >= MAXY) return;

  if (magicfx_tile[fx].Overlay)
  {
    GetTile(x, y, &TileId);
    S->tiles[y][x] = (int16_t) TileId;
    S->overlay[y][x] = (int16_t) magicfx_tile[fx].Tile1[frame];
  }
  else
  {
    S->tiles[y][x] = (int16_t) magicfx_tile[fx].Tile1[frame];
    S->overlay[y][x] = -1;
  }
  publish();
}

void nap(int delay)
{
  publish();
  if (delay > 0) usleep((useconds_t) delay * 1000);
}

void GetUser(char *username, int *uid)
{
  /* As in the X11 version, the login name comes from $USER (the app sets
   * "player"). The save file is named after it, so it must not change when
   * the player renames their character; the display name comes from the
   * options file ("name:") via logname. */
  const char *u = getenv("USER");
  *uid = (int) getuid();
  strcpy(username, (u && *u) ? u : "player");
}
