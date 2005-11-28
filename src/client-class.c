/* Client class
 *
 */

#include "common.h"
#include "client-class.h"
#include "socket.h"

Htree < Client > clients;

//Client::Client(const char *nick, const char *chname)
Client::Client(const char *nick, struct chanset_t *chan)
{
  _u = NULL;
  hops = -1;
  _h_family = 0;
  _i_family = 0;
  _tried_getuser = 0;
//  _chans = NULL;
  _channels = 0;

  _userhost[0] = 0;
  _userip[0] = 0;
  _user[0] = 0;
  strlcpy(_nick, nick, sizeof(_nick));

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
    if (!strcasecmp(_chans[i], chname)) {
      free(_chans[i]);
      _channels--;
      if (i < _channels)
        memcpy(&_chans[i], &_chans[_channels], sizeof(char *));
      _chans = (char **) realloc(_chans, sizeof(char *) * _channels);

      if (_channels == 0) {
        free(_chans);
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
//  list_delete((struct list_type **) &_chans, (struct list_type *) chan);
  _chans.remove(chan);
  
  // All channels removed for this client
  
//  if (_chans == NULL) {
  if (_channels == 0) {
//    free(_chans);
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
//  _chans = (char **) realloc(_chans, sizeof(char *) * _channels);
//  _chans[_channels - 1] = strdup(chname);

  _chans.add(chan);
//  list_append((struct list_type **) &_chans, (struct list_type *) chan);
  return _channels;
}


void
 Client::dump_idx(int idx)
{
  char buf[1024] = "";
  ptrlist<struct chanset_t>::iterator chan;

//  struct chanset_t *chan = NULL;

  simple_snprintf(buf, sizeof(buf), "nick: %s user: %s uhost: %s chans: ", _nick, _user, _userhost);
//  for (int i = 0; i < _channels; i++)
//    simple_snprintf(buf, sizeof(buf), "%s%s,", buf, _chans[i]);
//  for (chan = _chans; chan; chan = chan->next);
//    simple_snprintf(buf, sizeof(buf), "%s%s,", buf, chan->dname);

  for (chan = _chans.begin(); chan; chan++)
    simple_snprintf(buf, sizeof(buf), "%s%s,", buf, chan->dname);

  dprintf(idx, "%s\n", buf);
}

void Client::NewNick(const char *newnick)
{
  clients.rename(_nick, newnick);
  strlcpy(_nick, newnick, NICKLEN);
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
}

char *Client::GetUHost()
{
  return _userhost;
}

char *Client::GetUIP()
{
  return _userip;
}
