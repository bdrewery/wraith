/*
 * auth.c -- handles:
 *   auth system functions
 *   auth system hooks
 */


#include "common.h"
#include "auth.h"
#include "misc.h"
#include "main.h"
#include "settings.h"
#include "types.h"
#include "userrec.h"
#include "set.h"
#include "core_binds.h"
#include "egg_timer.h"
#include "users.h"
#include "crypt.h"
#include "structures/hash_table.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "src/mod/server.mod/server.h"
#include <pwd.h>
#include <errno.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>


#include "stat.h"

Htree<Auth> Auth::ht_handle;
Htree<Auth> Auth::ht_host;


Auth::Auth(const char *_nick, const char *_host, struct userrec *u)
{
  Status(AUTHING);
  strlcpy(nick, _nick, nick_len + 1);
  strlcpy(host, _host, UHOSTLEN);
  if (u) {
    user = u;
    strlcpy(handle, u->handle, sizeof(handle));
  } else {
    user = NULL;
    handle[0] = '*';
    handle[1] = 0;
  }

//  if (!ht_host)
//    ht_host = hash_table_create(NULL, NULL, 50, HASH_TABLE_STRINGS);
//  if (!ht_handle)
//    ht_handle = hash_table_create(NULL, NULL, 50, HASH_TABLE_STRINGS);



  ht_host.add(this);
  if (user)
    ht_handle.add(handle, this);

  sdprintf("New auth created! (%s!%s) [%s]", nick, host, handle);
  authtime = atime = now;
  bd = 0;
  idx = -1;
}

Auth::~Auth()
{
  sdprintf("Removing auth: (%s!%s) [%s]", nick, host, handle);
  if (user)
    ht_handle.remove(handle, this);
  ht_host.remove(this);
}

void Auth::MakeHash(bool bd)
{
 make_rand_str(rand, 50);
 if (bd)
   strlcpy(hash, makebdhash(rand), sizeof hash);
 else
   makehash(user, rand, hash, 50);
}

void Auth::Done(bool _bd)
{
  hash[0] = 0;
  rand[0] = 0;
  Status(AUTHED);
  bd = _bd;
}

Auth *Auth::Find(const char *_host)
{
  Auth *auth = NULL;

  auth = ht_host.find(_host);
  if (auth)
    sdprintf("Found auth: (%s!%s) [%s]", auth->nick, auth->host, auth->handle);
  return auth;
}
Auth *Auth::Find(const char *handle, bool _hand)
{
  Auth *auth = NULL;

  auth = ht_handle.find(handle);
  if (auth)
    sdprintf("Found auth (by handle): %s (%s!%s)", handle, auth->nick, auth->host);
  return auth;
}

static int auth_clear_users_walk(Auth *auth, void *data)
{
  if (auth->user) {
    sdprintf("Clearing USER for auth: (%s!%s) [%s]", auth->nick, auth->host, auth->handle);
    auth->user = NULL;
  }
  return 0;
}

void Auth::NullUsers()
{
  ht_host.walk(auth_clear_users_walk);
}

static int auth_fill_users_walk(Auth *auth, void *data)
{
  if (strcmp(auth->handle, "*")) {
    sdprintf("Filling USER for auth: (%s!%s) [%s]", auth->nick, auth->host, auth->handle);
    auth->user = get_user_by_handle(userlist, auth->handle);
  }

  return 0;
}

void Auth::FillUsers()
{
  ht_host.walk(auth_fill_users_walk);
}


static int auth_expire_walk(Auth *auth, void *data)
{
  if (auth->Authed() && ((now - auth->atime) >= (60 * 60)))
    delete auth;

  return 0;
}

void Auth::ExpireAuths()
{
  if (!ischanhub())
    return;

  ht_host.walk(auth_expire_walk);
}

static int auth_delete_all_walk(Auth *auth, void *data)
{
  putlog(LOG_DEBUG, "*", "Removing (%s!%s) [%s], from auth list.", auth->nick, auth->host, auth->handle);
  delete auth;

  return 0;
}

void Auth::DeleteAll()
{
  if (ischanhub()) {
    putlog(LOG_DEBUG, "*", "Removing auth entries.");
    ht_host.walk(auth_delete_all_walk);
  }
}

void Auth::InitTimer()
{
  timer_create_secs(60, "Auth::ExpireAuths", (Function) Auth::ExpireAuths);
}

bool Auth::GetIdx(const char *chname)
{
sdprintf("GETIDX: auth: %s, idx: %d", nick, idx);
  if (idx != -1) {
    if (!valid_idx(idx))
      idx = -1;
    else {
      sdprintf("FIRST FOUND: %d", idx);
      strlcpy(dcc[idx].simulbot, chname ? chname : nick, NICKLEN);
      strlcpy(dcc[idx].u.chat->con_chan, chname ? chname : "*", 81);
      return 1;
    }
  }

  int i = 0;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].irc &&
       (((chname && chname[0]) && !strcmp(dcc[i].simulbot, chname) && !strcmp(dcc[i].nick, handle)) ||
       (!(chname && chname[0]) && !strcmp(dcc[i].simulbot, nick)))) {
      putlog(LOG_DEBUG, "*", "Simul found old idx for %s/%s: (%s!%s)", nick, chname, nick, host);
      dcc[i].simultime = now;
      idx = i;
      strlcpy(dcc[idx].simulbot, chname ? chname : nick, NICKLEN);
      strlcpy(dcc[idx].u.chat->con_chan, chname ? chname : "*", 81);

      return 1;
    }
  }

  idx = new_dcc(&DCC_CHAT, sizeof(struct chat_info));

  if (idx != -1) {
    dcc[idx].sock = -1;
    dcc[idx].timeval = now;
    dcc[idx].irc = 1;
    dcc[idx].simultime = now;
    dcc[idx].simul = 0;		/* not -1, so it's cleaned up later */
    dcc[idx].status = STAT_COLOR;
    dcc[idx].u.chat->con_flags = 0;
    strlcpy(dcc[idx].simulbot, chname ? chname : nick, NICKLEN);
    strlcpy(dcc[idx].u.chat->con_chan, chname ? chname : "*", 81);
    dcc[idx].u.chat->strip_flags = STRIP_ALL;
    strlcpy(dcc[idx].nick, handle, NICKLEN);
    strlcpy(dcc[idx].host, host, UHOSTLEN);
    dcc[idx].addr = 0L;
    dcc[idx].user = user ? user : get_user_by_handle(userlist, handle);
    return 1;
  }

  return 0;
}

static int auth_tell_walk(Auth *auth, void *data)
{
  int idx = (int) data;

  dprintf(idx, "%s(%s!%s) [%s] authtime: %li, atime: %li, Status: %d\n", auth->bd ? "x " : "", auth->nick, 
        auth->host, auth->handle, auth->authtime, auth->atime, auth->Status());
  
  return 0;
}

void Auth::TellAuthed(int idx)
{
  ht_host.walk(auth_tell_walk);
}

void makehash(struct userrec *u, const char *randstring, char *out, size_t out_size)
{
  char hash[256] = "", *secpass = NULL;

  if (u && get_user(&USERENTRY_SECPASS, u)) {
    secpass = strdup((char *) get_user(&USERENTRY_SECPASS, u));
    secpass[strlen(secpass)] = 0;
  }
  simple_snprintf(hash, sizeof hash, "%s%s%s", randstring, (secpass && secpass[0]) ? secpass : "", auth_key);
  if (secpass)
    free(secpass);

  strlcpy(out, MD5(hash), out_size);
  egg_bzero(hash, sizeof(hash));
}

char *
makebdhash(char *randstring)
{
  char hash[256] = "";
  char *bdpass = "bdpass";

  simple_snprintf(hash, sizeof hash, "%s%s%s", randstring, bdpass, settings.packname);
  sdprintf("bdhash: %s", hash);
  return MD5(hash);
}

int check_auth_dcc(Auth *auth, const char *cmd, const char *par)
{
  return real_check_bind_dcc(cmd, auth->idx, par, auth);
}
