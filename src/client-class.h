#ifndef _CLIENT_CLASS_H
#define _CLIENT_CLASS_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "chan.h"
#include "hash_table.h"

class Client {
  private:
    struct userrec *_user;
    int _hops;
    int _uhost_family;

    bool _tried_getuser;

    char _nick[NICKLEN];
    char _userhost[UHOSTLEN];
    char _userip[UHOSTLEN];

//    char **_chans;
    int _channels;
    ptrlist<struct chanset_t> _chans;
//    struct chanset_t *_chans;
  public:
//    Client(const char *nick, const char *chname);
    Client(const char *nick, struct chanset_t *chan);
    ~Client();

    char *GetKey() { return _nick; };

    void NewNick(const char *newnick);
    static Client *Find(const char *nick);


//    int RemoveChan(const char *chname);
//    int AddChan(const char *chname);
    int RemoveChan(struct chanset_t *chan);
    int AddChan(struct chanset_t *chan);

    void dump_idx(int idx);
};


extern Htree<Client> clients;
#endif
