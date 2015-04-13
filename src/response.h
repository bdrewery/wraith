#ifndef _RESPONSE_H
#define _RESPONSE_H

enum response_t {
	RES_BANNED = 0,
	RES_KICKBAN,
	RES_MASSDEOP,
	RES_BADOP,
	RES_BADOPPED,
	RES_BITCHOP,
	RES_BITCHOPPED,
	RES_MANUALOP,
	RES_MANUALOPPED,
	RES_CLOSED,
	RES_FLOOD,
	RES_NICKFLOOD,
	RES_KICKFLOOD,
	RES_REVENGE,
	RES_USERNAME,
	RES_PASSWORD,
	RES_BADUSERPASS,
	RES_MIRCVER,
	RES_MIRCSCRIPT,
	RES_OTHERSCRIPT,
};

const char *response(response_t);

inline const char *
r_banned(struct chanset_t *chan)
{
  return response(RES_BANNED);
}

#endif /* !_RESPONSE_H */
