#ifndef _MEMBER_H
#define _MEMBER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "structures/hash_table.h"
#include "client-class.h"

class Member {
  public:
    Member(struct chanset_t *chan, const char *nick);
    ~Member();


    static Member *Find(struct chanset_t *chan, const char *nick);
    static void Remove(struct chanset_t *chan, const char *nick);

    void Remove(bool = 1);
    void NewNick(const char *);

//mFIXME

    char *GetKey() { return nick; };
    struct userrec *GetUser();
    void SetUser(struct userrec *u, bool getuser = 0) { user = u; tried_getuser = getuser; };

    void UpdateIdle();
    static void UpdateIdle(struct chanset_t *, const char *);

    void dump_idx(int idx);

    int operator==(Member &rhs);
    int operator<(Member &rhs);
    int operator>(Member &rhs);

    static int cmp(const Member *, const Member *);

//    struct userrec *user  __attribute__((deprecated));
    struct userrec *user;

    time_t joined;
    time_t split;
    time_t last;
    time_t delay;
    unsigned short flags;

    char *nick;
//    char nick[NICKLEN];
//    char userhost[UHOSTLEN];
//    char userip[UHOSTLEN];
//    char fullhost[NICKLEN + UHOSTLEN];
//    char fullip[NICKLEN + UHOSTLEN];
    bool tried_getuser;
    Client *client;

  private:
    struct chanset_t *my_chan;
    bool removed;
};

#endif /* !_MEMBER_H */