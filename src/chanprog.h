#ifndef _CHANPROG_H
#define _CHANPROG_H

#include "src/chan.h"
#include "RfcString.h"

#define DO_LOCAL	1
#define DO_NET		2
#define CMD		4

extern bd::HashTable<RfcString, struct chanset_t *> chanset_by_dname;

int do_chanset(char *, struct chanset_t *, const char *, int);
void checkchans(int);
void tell_verbose_uptime(int);
void tell_verbose_status(int);
void tell_settings(int);
int isowner(char *);
void reaffirm_owners();
void reload();
void chanprog();
void rehash_ip();
void check_timers();
void check_utimers();
void rmspace(char *s);
void set_chanlist(const char *host, struct userrec *rec);
void clear_chanlist_member(const char *nick);
#define clear_chanlist() clear_chanlist_member(NULL)
bool bot_shouldjoin(struct userrec* , struct flag_record *, const struct chanset_t *, bool = 0);
bool shouldjoin(const struct chanset_t *);
char *samechans(const char *, const char *);
void add_myself_to_userlist();
void add_child_bots();
bool is_hub(const char*);
void load_internal_users();
void setup_HQ(int);
void privmsg(bd::String target, bd::String msg, int idx);
void notice(bd::String target, bd::String msg, int idx);
void keyx(const bd::String& target, const char *);
void set_fish_key(char *, bd::String);
struct userrec *check_chanlist(const char *);
struct userrec *check_chanlist_hand(const char *);
/*
 * Returns memberfields if the nick is in the member list.
 */
static inline memberlist *
ismember(const struct chanset_t *chan, const RfcString nick) {
  if (!chan || !nick || !nick[0])
    return NULL;
  return chan->channel.hashed_members[nick];
}
static inline memberlist *
ismember(const struct chanset_t *chan, const char *nick) {
  return ismember(chan, RfcString(nick));
}
struct chanset_t *findchan(const char *name);
/*
 * Find a chanset by display name (ie !channel)
 */
static inline struct chanset_t *
findchan_by_dname(const char *name) {
  return chanset_by_dname[name];
}

extern struct chanset_t		*chanset, *chanset_default;
extern char			admin[], origbotnick[HANDLEN + 1], origbotname[NICKLEN], jupenick[NICKLEN], botname[NICKLEN], *def_chanset;
extern in_port_t			my_port;
extern int			reset_chans;
extern bool			cookies_disabled;

#endif /* !_CHANPROG_H */
