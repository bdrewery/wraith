#ifdef LEAF
/*
 * server.h -- part of server.mod
 *
 */

#ifndef _EGG_MOD_SERVER_SERVER_H
#define _EGG_MOD_SERVER_SERVER_H

#define check_tcl_ctcp(a,b,c,d,e,f) check_tcl_ctcpr(a,b,c,d,e,f,H_ctcp)
#define check_tcl_ctcr(a,b,c,d,e,f) check_tcl_ctcpr(a,b,c,d,e,f,H_ctcr)

#ifndef MAKING_SERVER
/* 4 - 7 */
#define botuserhost ((char *)(server_funcs[5]))
#define quiet_reject (*(int *)(server_funcs[6]))
#define serv (*(int *)(server_funcs[7]))
#define servi (*(int *)(server_funcs[7]))
/* 8 - 11 */
#define flud_thr (*(int*)(server_funcs[8]))
#define flud_time (*(int*)(server_funcs[9]))
#define flud_ctcp_thr (*(int*)(server_funcs[10]))
#define flud_ctcp_time (*(int*)(server_funcs[11]))
/* 12 - 15 */
#define match_my_nick ((int(*)(char *))server_funcs[12])
/* #define fixfrom ((void (*)(char *))server_funcs[14]) -- moved to core */
#define answer_ctcp (*(int *)(server_funcs[15]))
/* 16 - 19 */
#define trigger_on_ignore (*(int *)(server_funcs[16]))
#define check_tcl_ctcpr ((int(*)(char*,char*,struct userrec*,char*,char*,char*,p_tcl_bind_list))server_funcs[17])
#define detect_avalanche ((int(*)(char *))server_funcs[18])
#define nuke_server ((void(*)(char *))server_funcs[19])
/* 20 - 22 */
#define newserver ((char *)(server_funcs[20]))
#define newserverport (*(int *)(server_funcs[21]))
#define newserverpass ((char *)(server_funcs[22]))
/* 23 - 26 */
#define cycle_time (*(int *)(server_funcs[23]))
#define default_port (*(int *)(server_funcs[24]))
#define server_online (*(int *)(server_funcs[25]))
#define min_servs (*(int *)(server_funcs[26]))
/* 27 - 30 */
#define H_raw (*(p_tcl_bind_list *)(server_funcs[27]))
#define H_msg (*(p_tcl_bind_list *)(server_funcs[29]))
#define H_msgm (*(p_tcl_bind_list *)(server_funcs[30]))
/* 31 - 34 */
#define H_ctcp (*(p_tcl_bind_list *)(server_funcs[33]))
#define H_ctcr (*(p_tcl_bind_list *)(server_funcs[34]))
/* 35 - 38 */
#define ctcp_reply ((char *)(server_funcs[35]))
#define get_altbotnick ((char *(*)(void))(server_funcs[36]))
#define nick_len (*(int *)(server_funcs[37]))
#define server_lag (*(int *)(server_funcs[39]))
#define curserv (*(int *)(server_funcs[40))
#define cursrvname ((char *)(server_funcs[41]))
#define botrealname ((char *)(server_funcs[42]))
#ifdef S_AUTH
#define H_msgc (*(p_tcl_bind_list *)(server_funcs[43]))
#endif /* S_AUTH */

#else		/* MAKING_SERVER */

/* Macros for commonly used commands.
 */

#define free_null(ptr)	do {				\
	nfree(ptr);					\
	ptr = NULL;					\
} while (0)

#endif		/* MAKING_SERVER */

struct server_list {
  struct server_list	*next;

  char			*name;
  int			 port;
  char			*pass;
  char			*realname;
};

/* Available net types.  */
enum {
	NETT_EFNET		= 0,	/* EfNet except new +e/+I hybrid. */
	NETT_IRCNET		= 1,	/* Ircnet.			  */
	NETT_UNDERNET		= 2,	/* Undernet.			  */
	NETT_DALNET		= 3,	/* Dalnet.			  */
	NETT_HYBRID_EFNET	= 4	/* new +e/+I Efnet hybrid.	  */
} nett_t;

#define IRC_CANTCHANGENICK "Can't change nickname on %s.  Is my nickname banned?"
#endif		/* _EGG_MOD_SERVER_SERVER_H */

#endif /*leaf*/
