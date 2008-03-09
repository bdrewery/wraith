#ifndef _USERREC_H
#define _USERREC_H

struct userrec *adduser(struct userrec *, char *, char *, char *, flag_t, int);
void addhost_by_handle(char *, char *);
void clear_masks(struct maskrec *);
void clear_userlist(struct userrec *);
int u_pass_match(struct userrec *, char *);
int delhost_by_handle(char *, char *);
int count_users(struct userrec *);
int deluser(char *);
int change_handle(struct userrec *, char *);
void correct_handle(char *);
void stream_writeuserfile(Stream&, const struct userrec *, int);
int write_userfile(int);
void touch_laston(struct userrec *, char *, time_t);
void user_del_chan(char *);
struct userrec *host_conflicts(char *);
bool clearhosts(struct userrec *);

extern struct userrec  		*userlist, *lastuser;
extern int			cache_hit, cache_miss, userfile_perm;
extern bool			noshare;
#endif /* !_USERREC_H */
