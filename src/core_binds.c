/*
 * core_binds.c -- handles:
 *
 *   binds for the CORE
 *
 */


#include "common.h"
#include "dccutil.h"
#include "auth.h"
#include "core_binds.h"
#include "userrec.h"
#include "main.h"
#include "settings.h"
#include "set.h"
#include "users.h"
#include "misc.h"
#include "tclhash.h"
#include "dcc.h"
#include "src/mod/notes.mod/notes.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/console.mod/console.h"

extern cmd_t 		C_dcc[];

static bind_table_t *BT_dcc = NULL;
static bind_table_t *BT_bot = NULL;

void core_binds_init()
{
        BT_bot = bind_table_add("bot", 3, "sss", MATCH_EXACT, 0);
        BT_dcc = bind_table_add("dcc", 2, "is", MATCH_PARTIAL | MATCH_FLAGS, 0);
	egg_bzero(&cmdlist, 500);
        add_builtins("dcc", C_dcc);
}

bool check_aliases(int idx, const char *cmd, const char *args)
{
  char *a = NULL, *p = NULL, *aliasp = NULL, *aliasdup = NULL, *argsp = NULL, *argsdup = NULL;
  bool found = 0;

  aliasp = aliasdup = strdup(alias);
  argsp = argsdup = strdup(args);

  while ((a = strsep(&aliasdup, ","))) { //a = entire alias "alias cmd params"
    p = newsplit(&a);   //p = alias cmd //a = rcmd params
    if (!egg_strcasecmp(p, cmd)) { //a match on the cmd we were given!
      p = newsplit(&a); //p = rcmd //a = params

      if (!egg_strcasecmp(cmd, p)) {
        putlog(LOG_WARN, "*", "Loop detected in alias '%s'", p);
        free(argsp);
        return 0;
      }

      char *myargs = NULL, *pass = NULL;
      size_t size = 0;

      found = 1;

      size = strlen(a) + 1 + strlen(argsdup) + 1 + 2;
      myargs = (char *) calloc(1, size);

      if (has_cmd_pass(p)) {
        pass = newsplit(&argsdup);
        simple_snprintf(myargs, size, "%s %s %s", pass, a, argsdup);
      } else
        simple_snprintf(myargs, size, "%s %s", a, argsdup);
      putlog(LOG_CMDS, "*", "@ #%s# [%s -> %s %s] ...", dcc[idx].nick, cmd, p, a);
      check_bind_dcc(p, idx, myargs);

      if (myargs)
        free(myargs);
      break;
    }
  }

  free(aliasp);
  free(argsp);

  return found;
}

int check_bind_dcc(const char *cmd, int idx, const char *text)
{
  return real_check_bind_dcc(cmd, idx, text, NULL);
}

int real_check_bind_dcc(const char *cmd, int idx, const char *text, Auth *auth)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;
  char *args = strdup(text);

  get_user_flagrec(dcc[idx].user, &fr, dcc[idx].u.chat->con_chan);

  table = bind_table_lookup("dcc");

  for (entry = table->entries; entry && entry->next; entry = entry->next) {
    if (!egg_strcasecmp(cmd, entry->mask)) {
      if (has_cmd_pass(cmd)) {
        if (flagrec_ok(&entry->user_flags, &fr)) {
          char *p = NULL, work[1024] = "", pass[128] = "";

          p = strchr(args, ' ');
          if (p)
            *p = 0;
          strlcpy(pass, args, sizeof(pass));

          if (check_cmd_pass(cmd, pass)) {
            if (p)
              *p = ' ';
            strlcpy(work, args, sizeof(work));
            p = work;
            newsplit(&p);
            strcpy(args, p);
          } else {
            dprintf(idx, "Invalid command password.\nUse: $b%scommand <password> [arguments]$b\n", settings.dcc_prefix);
            putlog(LOG_CMDS, "*", "$ #%s# %s %s", dcc[idx].nick, cmd, args);
            putlog(LOG_MISC, "*", "%s attempted %s%s with missing or incorrect command password", dcc[idx].nick, settings.dcc_prefix, cmd);
            free(args);
            return 0;
          }
        } else {
          putlog(LOG_CMDS, "*", "! #%s# %s %s", dcc[idx].nick, cmd, args);
          dprintf(idx, "What?  You need '%shelp'\n", settings.dcc_prefix);
          free(args);
          return 0;
        }
      }
      break;
    }
  }

  if (entry && auth) {
    if (!(entry->cflags & AUTH))
      return 0;
  }

  int hits = 0, ret = 0;
  bool log_bad = 0;

  ret = check_bind_hits(BT_dcc, cmd, &fr, &hits, idx, args);

  if (hits != 1)
    log_bad = 1;

  if (hits == 0) {
    if (!check_aliases(idx, cmd, args)) 
      dprintf(idx, "What?  You need '%shelp'\n", settings.dcc_prefix);
    else
      log_bad = 0;
  } else if (hits > 1)
    dprintf(idx, "Ambiguous command.\n");

  if (log_bad)
    putlog(LOG_CMDS, "*", "! #%s# %s %s", dcc[idx].nick, cmd, args);

  free(args);

  return ret;
}

void check_bind_bot(const char *nick, const char *code, const char *param)
{
  char *mynick = NULL, *myparam = NULL, *p1 = NULL, *p2 = NULL;

  mynick = p1 = strdup(nick);
  myparam = p2 = strdup(param);

  check_bind(BT_bot, code, NULL, mynick, code, myparam);
  free(p1);
  free(p2);
}

void check_chon(char *hand, int idx)
{
  struct userrec        *u = NULL;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);

  console_chon(hand, idx);
  channels_chon(hand, idx);
}

void check_chof(char *hand, int idx)
{
  struct userrec        *u = NULL;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);
}

void check_nkch(const char *ohand, const char *nhand)
{
  notes_change(ohand, nhand);
}

void check_away(const char *bot, int idx, const char *msg)
{
  away_notes(bot, idx, msg);
}
