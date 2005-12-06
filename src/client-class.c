/* Client class
 *
 */

#include "common.h"
#include "client-class.h"
#include "socket.h"

Htree < Client > clients;

void Client::_init(const char *nick)
{
  u = NULL;
  hops = -1;
  h_family = 0;
  i_family = 0;
  tried_getuser = 0;
//  chans = NULL;
  channels = 0;

  fuhost[0] = 0;
  fuip[0] = 0;

  userhost[0] = 0;
  userip[0] = 0;
  user[0] = 0;
  strlcpy(this->nick, nick, sizeof(this->nick));
}

Client::Client(const char *nick)
{
  _init(nick);
  clients.add(this);
}

Client::Client(const char *nick, struct chanset_t *chan)
{
  _init(nick);
  AddChan(chan);
  clients.add(this);
}

Client::~Client()
{

}

int
 Client::RemoveChan(struct chanset_t *chan)
{
  channels--;
  chans.remove(chan);
  
  // All channels removed for this client
  if (channels == 0) {
    clients.remove(this);
    delete this;
    return 1;
  }
  
  return 0;
}

int
 Client::AddChan(struct chanset_t *chan)
{
  channels++;
  chans.add(chan);

  return channels;
}


void
 Client::dump_idx(int idx)
{
  char buf[1024] = "";
  ptrlist<struct chanset_t>::iterator chan;


  simple_snprintf(buf, sizeof(buf), "nick: %s username: %s uhost: %s chans: ", nick, u ? u->handle : "-", userhost);

  for (chan = chans.begin(); chan; chan++)
    simple_snprintf(buf, sizeof(buf), "%s%s,", buf, chan->dname);

  dprintf(idx, "%s\n", buf);
}

void Client::NewNick(const char *newnick)
{
  ClearUser();

  clients.rename(nick, newnick);
  strlcpy(nick, newnick, NICKLEN);


  simple_snprintf(_fuhost, sizeof(_fuhost), "%s!%s", nick, userhost);
  UpdateUser();

  if (userip[0])
    simple_snprintf(_fuip, sizeof(_fuip), "%s!%s", nick, userip);
}

Client *Client::Find(const char *nick)
{
  Client *client = NULL;

  client = clients.find(nick);

  return client;
}

void Client::SetUHost(const char *uhost, const char *usr)
{
  if (user) {
    simple_snprintf(userhost, sizeof(userhost), "%s@%s", usr, uhost);
    strlcpy(user, usr, sizeof(user));
    h_family = is_dotted_ip(uhost);
  } else {
    strlcpy(userhost, uhost, sizeof(userhost));

    char *host = NULL;

    if ((host = strchr(uhost, '@'))) {
      strlcpy(user, uhost, host - uhost + 1);
      h_family = is_dotted_ip(++host);
    }
  }
  simple_snprintf(fuhost, sizeof(fuhost), "%s!%s", nick, userhost);
  ClearUser();
  UpdateUser();
}

void Client::SetUIP(const char *uip, const char *usr)
{
  if (user) {
    simple_snprintf(userip, sizeof(userip), "%s@%s", usr, uip);
    i_family = is_dotted_ip(uip);
    strlcpy(user, usr, sizeof(user));
  } else {
    strlcpy(userip, uip, sizeof(userip));

    char *host = NULL;

    if ((host = strchr(uip, '@'))) {
      strlcpy(user, uip, host - uip + 1);
      i_family = is_dotted_ip(++host);
    }
  }

  simple_snprintf(fuip, sizeof(fuip), "%s!%s", nick, userip);
  ClearUser();
  UpdateUser(1);
}

char *Client::GetUHost()
{
  return userhost;
}

char *Client::GetUIP()
{
  return userip;
}

void Client::ClearUser()
{
  u = NULL;
  tried_getuser = 0;
}

void Client::UpdateUser(bool ip)
{
  if (u || tried_getuser)
    return;

  if (ip && _userip[0]) {
    u = get_user_by_host(fuip);
//    if (!_u)
//      _u = get_user_by_host(_fuhost);
  } else
    u = get_user_by_host(fuhost);

  tried_getuser = 1;
}

struct userrec *Client::GetUser(bool update)
{
  if (update)
    UpdateUser();
  return u;
}

struct userrec *Client::GetUserByIP()
{
  tried_getuser = 0;
  UpdateUser(1);
  return u;
}

void Client::SetUser(struct userrec *us)
{
  u = us;
}
