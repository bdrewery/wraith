#ifndef _CLIENT_CLASS_H
#define _CLIENT_CLASS_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "chan.h"
#include "structures/hash_table.h"

class Client {
  private:
    /* These will increase memory, but decrease load. */
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
    static void FillUsers();
    static void ClearUsers();

//    int RemoveChan(const char *chname);
//    int AddChan(const char *chname);
    int RemoveChan(struct chanset_t *chan);
    int AddChan(struct chanset_t *chan);

    void dump_idx(int idx);
    void SetUHost(const char *, const char * = NULL);
    char *GetUHost();
    void SetUIP(const char *, const char * = NULL);
    char *GetUIP();
    void SetGecos(const char*);
    char *GetGecos();

    void ClearUser();
    void UpdateUser(bool = 0);
    struct userrec *GetUser(bool = 1);
    struct userrec *GetUserByIP();
    void SetUser(struct userrec *);


    ptrlist<struct chanset_t> chans;
    struct userrec *u;
    int hops;
    int i_family;
    int h_family;
    int channels;
    char nick[NICKLEN];
    char fuhost[UHOSTLEN + NICKLEN];
    char fuip[UHOSTLEN + NICKLEN];
    char userhost[UHOSTLEN];
    char userip[UHOSTLEN];
    char user[11];
    char gecos[REALLEN + 1];
    bool tried_getuser;
};


extern Htree<Client> clients;
#endif
