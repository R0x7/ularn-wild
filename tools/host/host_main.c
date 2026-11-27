/* host_main.c - drives the real game on a Linux host for testing.
 *
 * Links the Android build's game library (upstream core + the Android front
 * end + spawn.c) into a command-line program. Keys come from stdin, one
 * "step" per line; after each step the driver waits until the game is idle
 * (waiting for input) and prints what changed: input mode, messages, status,
 * text page, and Wild Edition events. Used by tools/host/play.sh.
 *
 *   ularn_host <workdir> [--quiet] [--timeout ms] -- [game args]
 *
 * Step syntax (one line): raw characters are keys. Escapes: \n enter,
 * \e escape, \s space, \t tab, \\ backslash, \xHH byte. A line starting with
 * '#' is a comment and is echoed. An empty line just waits and dumps state.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "spawn.h"
#include "shared_state.h"

static int quiet = 0;
static int timeout_ms = 4000;
static UlarnSpawn sp;
static UlarnShared *S;
static int last_ev_seq = 0;
static char last_msg[US_MSG_LINES][US_LINE_LEN + 1];

static const char *mode_name(int m)
{
  switch (m)
  {
    case US_INPUT_BUSY: return "BUSY";
    case US_INPUT_COMMAND: return "COMMAND";
    case US_INPUT_PROMPT: return "PROMPT";
    case US_INPUT_DIR: return "DIR";
    case US_INPUT_NUMBER: return "NUMBER";
    case US_INPUT_PASSWORD: return "PASSWORD";
  }
  return "?";
}

static const char *ev_name(int t)
{
  switch (t)
  {
    case US_EV_KILL: return "KILL";
    case US_EV_LEVEL_UP: return "LEVEL_UP";
    case US_EV_NEW_LEVEL: return "NEW_LEVEL";
    case US_EV_TAKE: return "TAKE";
    case US_EV_PLAYER_HIT: return "PLAYER_HIT";
    case US_EV_GOLD: return "GOLD";
    case US_EV_SPELL: return "SPELL";
    case US_EV_DIED: return "DIED";
    case US_EV_WON: return "WON";
    case US_EV_WILD_RADIO: return "WILD_RADIO";
    case US_EV_WILD_CARD: return "WILD_CARD";
    case US_EV_WILD_VENDING: return "WILD_VENDING";
    case US_EV_WILD_HAT: return "WILD_HAT";
    case US_EV_WILD_COIN: return "WILD_COIN";
    case US_EV_WILD_GLITCH: return "WILD_GLITCH";
    case US_EV_WILD_FISHING: return "WILD_FISHING";
    case US_EV_WILD_PET: return "WILD_PET";
    case US_EV_WILD_PERK: return "WILD_PERK";
    case US_EV_WILD_WEATHER: return "WILD_WEATHER";
  }
  return "EV";
}

static int child_alive(void)
{
  int st;
  pid_t r = waitpid(sp.pid, &st, WNOHANG);
  if (r == sp.pid)
  {
    if (WIFEXITED(st)) printf("GAME EXITED code=%d\n", WEXITSTATUS(st));
    else printf("GAME KILLED signal=%d\n", WTERMSIG(st));
    return 0;
  }
  return 1;
}

/* Wait until the game asks for input and has been quiet for a moment. */
static int wait_idle(void)
{
  int waited = 0, quiet_ms = 0, seq = S->seq;
  char buf[64];
  while (waited < timeout_ms)
  {
    struct pollfd p = { sp.notify_fd, POLLIN, 0 };
    int r = poll(&p, 1, 20);
    if (r > 0 && (p.revents & POLLIN)) { if (read(sp.notify_fd, buf, sizeof buf) <= 0) return child_alive(); }
    if (!child_alive()) return 0;
    if (S->seq != seq) { seq = S->seq; quiet_ms = 0; }
    else quiet_ms += 20;
    if (S->input_mode != US_INPUT_BUSY && quiet_ms >= 60) return 1;
    waited += 20;
  }
  printf("TIMEOUT: game still busy (input_mode=%s)\n", mode_name(S->input_mode));
  return 1;
}

static void trim(char *s)
{
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\n')) s[--n] = 0;
}

static void dump(void)
{
  int y, i, changed = 0;
  char line[US_LINE_LEN + 2];

  printf("[mode=%s", mode_name(S->input_mode));
  if (S->input_mode == US_INPUT_PROMPT) printf(" answers=\"%s\"", S->answers);
  printf(" display=%s]\n", S->display_mode == US_DISPLAY_MAP ? "MAP" : "TEXT");
  if (!quiet || S->display_mode == US_DISPLAY_TEXT)
  {
    if (S->display_mode == US_DISPLAY_TEXT)
    {
      for (y = 0; y < US_TEXT_LINES; y++)
      {
        memcpy(line, S->text_chr[y], US_LINE_LEN); line[US_LINE_LEN] = 0; trim(line);
        if (line[0]) printf("TEXT: %s\n", line);
      }
    }
  }
  for (y = 0; y < US_MSG_LINES; y++)
  {
    memcpy(line, S->msg_chr[y], US_LINE_LEN); line[US_LINE_LEN] = 0; trim(line);
    if (strcmp(line, last_msg[y]) != 0) changed = 1;
    strcpy(last_msg[y], line);
  }
  if (changed || !quiet)
    for (y = 0; y < US_MSG_LINES; y++) if (last_msg[y][0]) printf("MSG: %s\n", last_msg[y]);
  if (!quiet)
  {
    printf("STATUS: %s | %s\n", S->status1, S->status2);
    printf("POS: (%d,%d) level=%s hp=%d/%d gold=%d plevel=%d time=%d", S->player_x, S->player_y,
           S->level_name, S->hp, S->hpmax, S->gold, S->plevel, S->game_time);
    printf(" wild=0x%x hat=%d glitch=%d weather=0x%x coin=%d pet=(%d,%d) ctx=%d/%d", S->wild_flags,
           S->hat_tile, S->glitch, S->weather, S->coin_price, S->pet_x, S->pet_y, S->ctx_kind, S->ctx_object);
    if (S->pet_info[0])
      printf(" petinfo=%d:%d/%d:stage%d:kills%d:next%d:at[%s]", S->pet_info[0], S->pet_info[1], S->pet_info[2],
             S->pet_info[3], S->pet_info[4], S->pet_info[5], S->pet_level);
    if (S->bounty_info[0])
      printf(" bounty=%d:%dg:claimed%d:downat%d", S->bounty_info[0], S->bounty_info[1],
             S->bounty_info[2], S->bounty_info[3]);
    if (S->coin_held)
      printf(" coinheld=%d", S->coin_held);
    printf(" tile=%d\n", S->tiles[S->player_y][S->player_x]);
  }
  for (i = last_ev_seq; i < S->ev_seq; i++)
  {
    int k = i & (US_EVENTS - 1);
    printf("EVENT: %s(%d,%d,%d)\n", ev_name(S->ev[k][0]), S->ev[k][1], S->ev[k][2], S->ev[k][3]);
  }
  last_ev_seq = S->ev_seq;
  if (S->end_state) printf("END: state=%d score=%d reason=\"%s\"\n", S->end_state, S->end_score, S->end_reason);
  fflush(stdout);
}

static int send_key(unsigned char ch)
{
  if (write(sp.input_fd, &ch, 1) != 1) { printf("write failed: %s\n", strerror(errno)); return 0; }
  return 1;
}

int main(int argc, char **argv)
{
  const char *workdir = NULL;
  char *gargv[32]; int gargc = 0;
  char *envp[8]; int envc = 0;
  char line[4096];
  int i, rc;

  gargv[gargc++] = "ularn";
  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--quiet") == 0) quiet = 1;
    else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) timeout_ms = atoi(argv[++i]);
    else if (strcmp(argv[i], "--") == 0) { for (i++; i < argc && gargc < 31; i++) gargv[gargc++] = argv[i]; break; }
    else if (!workdir) workdir = argv[i];
    else { fprintf(stderr, "unexpected argument %s\n", argv[i]); return 2; }
  }
  gargv[gargc] = NULL;
  if (!workdir) { fprintf(stderr, "usage: ularn_host <workdir> [--quiet] [--timeout ms] -- [game args]\n"); return 2; }
  /* HOME, USER, ULARN_WILD, ULARN_SEED are inherited from our environment. */
  envp[envc] = NULL;

  signal(SIGPIPE, SIG_IGN);
  rc = ularn_spawn(workdir, gargc, gargv, envp, &sp);
  if (rc != 0) { fprintf(stderr, "spawn failed: %s\n", strerror(-rc)); return 1; }
  S = ularn_shared_region();

  printf("# game started pid=%d\n", (int) sp.pid);
  if (wait_idle()) dump(); else return 1;

  while (fgets(line, sizeof line, stdin))
  {
    char *p = line;
    size_t n = strlen(line);
    if (n && line[n - 1] == '\n') line[--n] = 0;
    if (line[0] == '#') { printf("%s\n", line); continue; }
    if (strcmp(line, "!map") == 0 || strcmp(line, "!items") == 0)
    {
      /* The level as the minimap classes see it: # wall, . floor, + door,
       * o item, $ gold, < > stairs, V volcano, E entrance, B building,
       * F fountain, A altar, T throne, ^ trap, ~ feature, M monster, @ you.
       * "!items" leaves out monsters and you, so what lies under them shows
       * (a test counting gold piles is not fooled by a lemming on one). */
      int x, y, items = strcmp(line, "!items") == 0;
      static const char cls[16] = " .#+o$<>VEBFAT^~";
      for (y = 0; y < US_MAP_H; y++)
      {
        char row[US_MAP_W + 1];
        for (x = 0; x < US_MAP_W; x++)
        {
          int c = S->mm_cat[y][x];
          row[x] = items ? cls[c & 15] :
                   (x == S->player_x && y == S->player_y) ? '@' : (c & US_MM_MONSTER) ? 'M' : cls[c & 15];
        }
        row[US_MAP_W] = 0;
        printf("MAP: %s\n", row);
      }
      continue;
    }
    if (strcmp(line, "!inv") == 0)
    {
      int i;
      for (i = 0; i < US_INV_SLOTS; i++) if (S->inv_text[i][0]) printf("INV: %c) %s\n", 'a' + i, S->inv_text[i]);
      continue;
    }
    if (strcmp(line, "!tiles") == 0)
    {
      int x, y;
      for (y = S->player_y - 2; y <= S->player_y + 2; y++)
      {
        printf("TILES:");
        for (x = S->player_x - 4; x <= S->player_x + 4; x++)
          if (x >= 0 && x < US_MAP_W && y >= 0 && y < US_MAP_H) printf(" %4d", S->tiles[y][x]); else printf("    -");
        printf("\n");
      }
      continue;
    }
    printf(">> %s\n", line);
    while (*p)
    {
      unsigned char ch = (unsigned char) *p++;
      if (ch == '\\' && *p)
      {
        char e = *p++;
        switch (e)
        {
          case 'n': ch = '\r'; break;
          case 'e': ch = 27; break;
          case 's': ch = ' '; break;
          case 't': ch = '\t'; break;
          case '\\': ch = '\\'; break;
          case 'x': { char hex[3] = { p[0], p[1], 0 }; ch = (unsigned char) strtol(hex, NULL, 16); p += 2; break; }
          default: ch = (unsigned char) e; break;
        }
      }
      if (!send_key(ch)) goto done;
      /* Let each key land so prompts that appear mid-sequence see the next one. */
      if (!wait_idle()) goto done;
    }
    dump();
  }
done:
  /* the events of the last keys, when they ended the game (a save, a quit) */
  for (i = last_ev_seq; i < S->ev_seq; i++)
  {
    int k = i & (US_EVENTS - 1);
    printf("EVENT: %s(%d,%d,%d)\n", ev_name(S->ev[k][0]), S->ev[k][1], S->ev[k][2], S->ev[k][3]);
  }
  last_ev_seq = S->ev_seq;
  if (child_alive())
  {
    close(sp.input_fd);   /* hang-up: the game exits */
    ularn_wait(sp.pid);
    printf("# game ended (input closed)\n");
  }
  return 0;
}
