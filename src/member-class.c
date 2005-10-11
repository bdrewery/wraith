/*
 */


#include "common.h"
#include "member-class.h"
#include "chan.h"
#include "main.h"
#include "socket.h"

Member::Member(struct chanset_t *chan, const char *nick) 
{
  if (!chan) {
    delete this;
    return;
  }

  strlcpy(this->nick, nick, NICKLEN);
  
  removed = 0;
  my_chan = chan;
  user = NULL;
  userhost[0] = 0;
  userip[0] = 0;
  split = 0;
  last = now;
  joined = 0;
  delay = 0;
  hops = -1;
  tried_getuser = 0;

  chan->channel.hmember->add(this);
  my_chan->channel.members++;
}

Member::~Member()
{
  //This happens if the object is delted directly
  if (!removed)
    Remove(0);
//  my_chan->channel.members--;
}

void Member::SetUHost(const char *uhost)
{
  h_family = is_dotted_ip(uhost);
  strlcpy(userhost, uhost, sizeof(userhost));
}
void Member::SetUIP(const char *uip)
{
  i_family = is_dotted_ip(uip);
  strlcpy(userip, uip, sizeof(userhost));
}

void Member::NewNick(const char *newnick)
{
//  char tmp[NICKLEN] = "";

//  strlcpy(tmp, nick, NICKLEN);
  my_chan->channel.hmember->rename(nick, newnick);
  strlcpy(nick, newnick, NICKLEN);
}

void Member::Remove(struct chanset_t *chan, const char *nick)
{
  Member *m = Find(chan, nick);

  if (m)
    m->Remove();
}

void Member::Remove(bool do_delete)
{
  my_chan->channel.hmember->remove(this);
  removed = 1;

  my_chan->channel.members--;

  if (do_delete)
    delete this;
}

void Member::UpdateIdle()
{
  last = now;
}

void Member::UpdateIdle(struct chanset_t *chan, const char *nick)
{
  Member *m = Find(chan, nick);

  if (m) 
    m->UpdateIdle();
}

Member *Member::Find(struct chanset_t *chan, const char *nick) 
{
  if (!channel_active(chan) || !chan->channel.hmember)
    return NULL;

//  sdprintf("FIND chan - %s", chan->dname);
//  Member m = Member(chan, nick);
//  Member *ret = chan->channel.hmember->find(m.nick);
  Member *ret = chan->channel.hmember->find(nick);

//  if (!ret)
//    sdprintf("%s not found", nick);
//  else
//    sdprintf("%s FOUND", nick);
//    dprintf(DP_MODE, "PRIVMSG #skynet :%s not found.\n", nick);
  return ret;
}

int Member::cmp (const Member *m1, const Member *m2)
{
  return strcmp(m1->nick, m2->nick);
}

int Member::operator==(Member &rhs) 
{
  return !strcmp(nick, rhs.nick);
}

int Member::operator<(Member &rhs)
{
  return strcmp(nick, rhs.nick) < 0 ? 1 : 0;
}

int Member::operator>(Member &rhs)
{
  return strcmp(nick, rhs.nick) > 0 ? 0 : 1;
}

struct userrec *Member::GetUser()
{
  char s[UHOSTLEN + NICKLEN] = "";

  if (userhost[0]) {
    simple_snprintf(s, sizeof(s), "%s!%s", nick, userhost);
    user = get_user_by_host(s);
    tried_getuser = 1;
  }
  return user;
}

void Member::dump_idx(int idx)
{
  dprintf(idx, "nick: %s\n", nick);
  dprintf(idx, "  user: %s\n", user ? user->handle : "");
  dprintf(idx, "  userhost: %s\n", userhost);
  dprintf(idx, "  userip: %s\n", userip);
  dprintf(idx, "  split: %d\n", split);
  dprintf(idx, "  joined: %d\n", joined);
  dprintf(idx, "  last: %d\n", last);
  dprintf(idx, "  delay: %d\n", delay);
}
