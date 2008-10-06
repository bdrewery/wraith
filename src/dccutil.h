#ifndef _DCCUTIL_H
#define _DCCUTIL_H

#include "common.h"
#include "dcc.h"
#include "auth.h"

#define dprintf dprintf_eggdrop

/* Public structure for the listening port map */
struct portmap {
  port_t realport;
  port_t mappedto;
  struct portmap *next;
};

/* Fake idx's for dprintf - these should be ridiculously large +ve nums
 */
#define DP_STDOUT       0x7FF1
//#define DP_LOG          0x7FF2
#define DP_DEBUG	0x7FF2
#define DP_SERVER       0x7FF3
#define DP_HELP         0x7FF4
#define DP_STDERR       0x7FF5
#define DP_MODE         0x7FF6
#define DP_MODE_NEXT    0x7FF7
#define DP_SERVER_NEXT  0x7FF8
#define DP_HELP_NEXT    0x7FF9
#define DP_DUMP		0x8000



void init_dcc(void);
void dumplots(int, const char *, const char *);
#ifdef __GNUC__
 void dprintf(int, const char *, ...) __attribute__((format(printf, 2, 3)));
 void chatout(const char *, ...) __attribute__((format(printf, 1, 2)));
 void chanout_but(int, int, const char *, ...) __attribute__((format(printf, 3, 4)));
#else
 void dprintf(int, const char *, ...);
 void chatout(const char *, ...);
 void chanout_but(int, int, const char *, ...);
#endif
void dcc_chatter(int);
void lostdcc(int);
void makepass(char *);
void tell_dcc(int);
void not_away(int);
void set_away(int, char *);
void dcc_remove_lost(void);
void flush_lines(int, struct chat_info *);
struct dcc_t *find_idx(int);
int new_dcc(struct dcc_table *, int);
void del_dcc(int);
char *add_cr(char *);
void changeover_dcc(int, struct dcc_table *, int);
void do_boot(int, const char *, const char *);
int detect_dcc_flood(time_t *, struct chat_info *, int);
void identd_open(const char * = NULL, const char * = NULL);
void identd_close();
int listen_all(port_t, bool);
bool valid_idx(int);
int dcc_read(FILE *, bool);
void dcc_write(FILE *, int);
int check_cmd_pass(const char *, char *);
int has_cmd_pass(const char *);
void set_cmd_pass(char *, int);
void cmdpass_free(struct cmd_pass *);

extern int		max_dcc;
extern time_t		connect_timeout;

#endif /* !_DCCUTIL_H */
