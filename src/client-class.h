#ifndef _CLIENT_CLASS_H
#define _CLIENT_CLASS_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "chan.h"
#include "hash_table.h"

class Client {
  private:
    /* These will increase memory, but decrease load. */
    char _fuhost[UHOSTLEN + NICKLEN];
    char _fuip[UHOSTLEN + NICKLEN];

    char _userhost[UHOSTLEN];
    char _userip[UHOSTLEN];
    char _user[11];

    struct userrec *_u;
    int _i_family;
    int _h_family;

    bool _tried_getuser;

//    char **_chans;
    int _channels;
//    struct chanset_t *_chans;
    void _init(const char *);
  public:
//    Client(const char *nick, const char *chname);
    Client(const char *nick, struct chanset_t *chan);
    Client(const char *nick);
    ~Client();

    char *GetKey() { return nick; };

    void NewNick(const char *newnick);
    static Client *Find(const char *nick);


//    int RemoveChan(const char *chname);
//    int AddChan(const char *chname);
    int RemoveChan(struct chanset_t *chan);
    int AddChan(struct chanset_t *chan);

    void dump_idx(int idx);
    void SetUHost(const char *, const char * = NULL);
    char *GetUHost();
    void SetUIP(const char *, const char * = NULL);
    char *GetUIP();

    void ClearUser();
    void UpdateUser(bool = 0);
    struct userrec *GetUser(bool = 1);
    struct userrec *GetUserByIP();
    void SetUser(struct userrec *u);


    ptrlist<struct chanset_t> chans;
    char nick[NICKLEN];
    int hops;
};


extern Htree<Client> clients;
#endif
