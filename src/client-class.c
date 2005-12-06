/* Client class
 *
 */

#include "common.h"
#include "client-class.h"
#include "socket.h"

Htree < Client > clients;

void Client::_init(const char *nick)
{
  _u = NULL;
  hops = -1;
  _h_family = 0;
  _i_family = 0;
  _tried_getuser = 0;
//  chans = NULL;
  _channels = 0;

  _fuhost[0] = 0;
  _fuip[0] = 0;

  _userhost[0] = 0;
  _userip[0] = 0;
  _user[0] = 0;
  strlcpy(this->nick, nick, sizeof(this->nick));
}

Client::Client(const char *nick)
{
  _init(nick);
  clients.add(this);
}

//Client::Client(const char *nick, const char *chname)
Client::Client(const char *nick, struct chanset_t *chan)
{
  _init(nick);
  AddChan(chan);
  clients.add(this);
}

Client::~Client()
{

}

// Client::RemoveChan(const char *chname)
int
 Client::RemoveChan(struct chanset_t *chan)
{
/*
  for (int i = 0; i < _channels; i++) {
    if (!strcasecmp(chans[i], chname)) {
      free(chans[i]);
      _channels--;
      if (i < _channels)
        memcpy(&chans[i], &chans[_channels], sizeof(char *));
      chans = (char **) realloc(chans, sizeof(char *) * _channels);

      if (_channels == 0) {
        free(chans);
        clients.remove(this);
        delete
          this;

        return 1;
      }
      break;
    }
  }
*/
  _channels--;
//  list_delete((struct list_type **) &chans, (struct list_type *) chan);
  chans.remove(chan);
  
  // All channels removed for this client
  
//  if (chans == NULL) {
  if (_channels == 0) {
//    free(chans);
    clients.remove(this);
    delete this;
    return 1;
  }
  
  return 0;
}

// Client::AddChan(const char *chname)
int
 Client::AddChan(struct chanset_t *chan)
{
  _channels++;
//  chans = (char **) realloc(chans, sizeof(char *) * _channels);
//  chans[_channels - 1] = strdup(chname);

  chans.add(chan);
//  list_append((struct list_type **) &chans, (struct list_type *) chan);
  return _channels;
}


void
 Client::dump_idx(int idx)
{
  char buf[1024] = "";
  ptrlist<struct chanset_t>::iterator chan;

//  struct chanset_t *chan = NULL;

  simple_snprintf(buf, sizeof(buf), "nick: %s username: %s uhost: %s chans: ", nick, _u ? _u->handle : "-", _userhost);
//  for (int i = 0; i < _channels; i++)
//    simple_snprintf(buf, sizeof(buf), "%s%s,", buf, chans[i]);
//  for (chan = chans; chan; chan = chan->next);
//    simple_snprintf(buf, sizeof(buf), "%s%s,", buf, chan->dname);

  for (chan = chans.begin(); chan; chan++)
    simple_snprintf(buf, sizeof(buf), "%s%s,", buf, chan->dname);

  dprintf(idx, "%s\n", buf);
}

void Client::NewNick(const char *newnick)
{
  ClearUser();

  clients.rename(nick, newnick);
  strlcpy(nick, newnick, NICKLEN);


  simple_snprintf(_fuhost, sizeof(_fuhost), "%s!%s", nick, _userhost);
  UpdateUser();

  if (_userip[0])
    simple_snprintf(_fuip, sizeof(_fuip), "%s!%s", nick, _userip);
}

Client *Client::Find(const char *nick)
{
  Client *client = NULL;

  client = clients.find(nick);

  return client;
}

void Client::SetUHost(const char *uhost, const char *user)
{
  if (user) {
    simple_snprintf(_userhost, sizeof(_userhost), "%s@%s", user, uhost);
    strlcpy(_user, user, sizeof(_user));
    _h_family = is_dotted_ip(uhost);
  } else {
    strlcpy(_userhost, uhost, sizeof(_userhost));

    char *host = NULL;

    if ((host = strchr(uhost, '@'))) {
      strlcpy(_user, uhost, host - uhost + 1);
      _h_family = is_dotted_ip(++host);
    }
  }
  simple_snprintf(_fuhost, sizeof(_fuhost), "%s!%s", nick, _userhost);
  UpdateUser();
}

void Client::SetUIP(const char *uip, const char *user)
{
  if (user) {
    simple_snprintf(_userip, sizeof(_userip), "%s@%s", user, uip);
    _i_family = is_dotted_ip(uip);
    strlcpy(_user, user, sizeof(_user));
  } else {
    strlcpy(_userip, uip, sizeof(_userip));

    char *host = NULL;

    if ((host = strchr(uip, '@'))) {
      strlcpy(_user, uip, host - uip + 1);
      _i_family = is_dotted_ip(++host);
    }
  }

  simple_snprintf(_fuip, sizeof(_fuip), "%s!%s", nick, _userip);
  _tried_getuser = 0;
  UpdateUser();
}

char *Client::GetUHost()
{
  return _userhost;
}

char *Client::GetUIP()
{
  return _userip;
}

void Client::ClearUser()
{
  _u = NULL;
  _tried_getuser = 0;
}

void Client::UpdateUser(bool ip)
{
  if (_u || _tried_getuser)
    return;

  if (ip && _userip[0]) {
    _u = get_user_by_host(_fuip);
//    if (!_u)
//      _u = get_user_by_host(_fuhost);
  } else
    _u = get_user_by_host(_fuhost);

  _tried_getuser = 1;
}

struct userrec *Client::GetUser(bool update)
{
  if (update)
    UpdateUser();
  return _u;
}

struct userrec *Client::GetUserByIP()
{
  _tried_getuser = 0;
  UpdateUser(1);
  return _u;
}

void Client::SetUser(struct userrec *u)
{
  _u = u;
}
