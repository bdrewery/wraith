/*
 * channels.h -- part of channels.mod
 *
 */

#ifndef _EGG_MOD_CHANNELS_CHANNELS_H
#define _EGG_MOD_CHANNELS_CHANNELS_H

#define MASKREASON_MAX	307	/* Max length of ban/invite/exempt/etc.
				   reasons.				*/
#define MASKREASON_LEN	(MASKREASON_MAX + 1)


#ifdef MAKING_CHANNELS

#define PLSMNS(x) (x ? '+' : '-')


static void check_expired_masks(void);
static void tell_masks(const char type, int idx, bool show_inact, char *match, bool all = 0);
static void get_mode_protect(struct chanset_t *chan, char *s);
static void set_mode_protect(struct chanset_t *chan, char *set);
static int count_mask(maskrec *);

#endif				/* MAKING_CHANNELS */

void remove_channel(struct chanset_t *);
void add_chanrec_by_handle(struct userrec *, char *, char *);
void get_handle_chaninfo(char *, char *, char *);
void set_handle_chaninfo(struct userrec *, char *, char *, char *);
struct chanuserrec *get_chanrec(struct userrec *u, char *);
struct chanuserrec *add_chanrec(struct userrec *u, char *);
void del_chanrec(struct userrec *, char *);
void write_bans(Stream&, int);
void write_exempts (Stream&, int);
void write_chans (Stream&, int);
void write_invites (Stream&, int);
bool expired_mask(struct chanset_t *, char *);
void set_handle_laston(char *, struct userrec *, time_t);
int u_delmask(char type, struct chanset_t *c, char *who, int doit);
bool u_addmask(char type, struct chanset_t *, char *, char *, char *, time_t, int);
int u_sticky_mask(maskrec *, char *);
int u_setsticky_mask(struct chanset_t *, maskrec *, char *, int, const char);
int SplitList(char *, const char *, int *, const char ***);
int channel_modify(char *, struct chanset_t *, int, char **, bool);
int channel_add(char *, char *, char *);
void clear_channel(struct chanset_t *, bool);
int u_equals_mask(maskrec *, char *);
bool u_match_mask(struct maskrec *, char *);
bool ismasked(masklist *, const char *);
bool ismodeline(masklist *, const char *);
void channels_report(int, int);
void channels_writeuserfile(Stream&);
int channels_chon(const char *, int);
void rcmd_chans(char *, char *, char *);

extern char		glob_chanset[512];

/* Macro's here because their functions were replaced by something more
 * generic. <cybah>
 */
#define isbanned(chan, user)    ismasked((chan)->channel.ban, user)
#define isexempted(chan, user)  ismasked((chan)->channel.exempt, user)
#define isinvited(chan, user)   ismasked((chan)->channel.invite, user)

#define ischanban(chan, user)    ismodeline((chan)->channel.ban, user)
#define ischanexempt(chan, user) ismodeline((chan)->channel.exempt, user)
#define ischaninvite(chan, user) ismodeline((chan)->channel.invite, user)
#define ischanmask(type, chan, user) ismodeline(type == 'b' ? (chan)->channel.ban : type == 'e' ? (chan)->channel.exempt : (chan)->channel.invite, user)

#define u_setsticky_ban(chan, host, sticky)     u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->bans : global_bans, host, sticky, 'b')
#define u_setsticky_exempt(chan, host, sticky)  u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->exempts : global_exempts, host, sticky, 'e')
#define u_setsticky_invite(chan, host, sticky)  u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->invites : global_invites, host, sticky, 'I')
#endif				/* _EGG_MOD_CHANNELS_CHANNELS_H */
