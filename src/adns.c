/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* async dns
 *
 */


#include "common.h"
#include "adns.h"
#include "egg_timer.h"
#include "main.h"
#include "net.h"
#include "misc.h"
#include "socket.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

typedef struct {
	char **list;
	time_t ttl;
	int len;
} dns_answer_t;

typedef struct dns_query {
	struct dns_query *next;
	dns_answer_t answer;
	dns_callback_t callback;
	void *client_data;
	time_t expiretime;
	char *query;
	char *ip;
	int id;
//	int timer_id;
	int answers;
	int remaining;
} dns_query_t;

/* RFC1035
                                    1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
      1  1  1  1  1  0  9  8  7  6  5  4  3  2  1  0
      5  4  3  2  1  1
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      ID                       |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    QDCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ANCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    NSCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ARCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

typedef struct {
	unsigned short id;
	unsigned short flags;
	unsigned short question_count;
	unsigned short answer_count;
	unsigned short ns_count;
	unsigned short ar_count;
} dns_header_t;

#define GET_QR(x)     (((x) >> 15) & BIT0)
#define GET_OPCODE(x) (((x) >> 11) & BIT3|BIT2|BIT1|BIT0)
#define GET_AA(x)     (((x) >> 10) & BIT0)
#define GET_TC(x)     (((x) >> 9)  & BIT0)
#define GET_RD(x)     (((x) >> 8)  & BIT0)
#define GET_RA(x)     (((x) >> 7)  & BIT0)
#define GET_RCODE(x)  ((x)         & BIT3|BIT2|BIT1|BIT0)

#define SET_RD(x) (x) |= ((x) | (1 << 8))

#define HEAD_SIZE 12

typedef struct {
	/* char name[]; */
	unsigned short type;
	unsigned short dclass;
	int ttl;
	unsigned short rdlength;
	/* char rdata[]; */
} dns_rr_t;

#define RR_SIZE 10

/* Entries from resolv.conf */
typedef struct dns_server {
	char *ip;
        struct sockaddr_in to;
} dns_server_t;

/* Entries from hosts */
typedef struct {
	char *host;
        char *ip;
} dns_host_t;

typedef struct {
	dns_answer_t answer;
	char *query;
	time_t expiretime;
} dns_cache_t;


static int query_id = 1;
static dns_header_t _dns_header = {0, 0, 0, 0, 0, 0};
static dns_query_t *query_head = NULL;
static dns_host_t *hosts = NULL;
static int nhosts = 0;
static dns_cache_t *cache = NULL;
static int ncache = 0;
static dns_server_t *servers = NULL;
static int nservers = 0;

static char separators[] = " ,\t\r\n";

int dns_idx = -1;
int dns_sock = -1;
const char *dns_ip = NULL;

static int make_header(char *buf, int id);
static int cut_host(const char *host, char *query);
static int reverse_ip(const char *host, char *reverse);
static void read_resolv(char *fname);
static void read_hosts(char *fname);
static int get_dns_idx();
//static void dns_resend_queries();
static int cache_find(const char *);
//static int dns_on_read(void *client_data, int idx, char *buf, int len);
//static int dns_on_eof(void *client_data, int idx, int err, const char *errmsg);
static void dns_on_read(int idx, char *buf, int atr);
static void dns_on_eof(int idx);
static int parse_reply(char *response, size_t nbytes, const char *);

time_t async_lookup_timeout = 30;
//time_t async_server_timeout = 40;
//int resend_on_read = 0;

static void 
dns_display(int idx, char *buf)
{
  sprintf(buf, "named   waited %lis", now - dcc[idx].timeval);
}

static void
dns_reinit(int idx)
{
        sdprintf("Re-opening dns socket...");
        killsock(dcc[idx].sock);
        lostdcc(idx);
	dns_idx = -1;
        dns_sock = -1;
	dns_ip = NULL;

        if (!get_dns_idx())
          sdprintf("Successfully reopened dns socket");
        else
          sdprintf("Failed to reopen dns socket");
}

#ifdef no
static void
dns_timeout(int idx)
{
  sdprintf("DNS socket timed out");
  /*egg_dns_cancel(dcc[idx].u.dns_id, 1);*/
//  resend_on_read = 1;
  dns_reinit(idx);  
//  sleep(2);
//  dns_resend_queries();
}
#endif

static struct dcc_table dns_handler = {
  "adns",
  DCT_VALIDIDX,
  dns_on_eof,
  dns_on_read,
  NULL,
  /*dns_timeout*/
  NULL,
  dns_display,
  NULL,
  NULL,
  NULL
};

static void dcc_dnswait(int idx, char *buf, int len)
{
  /* Ignore anything now. */
}

static void eof_dcc_dnswait(int idx)
{
  putlog(LOG_MISC, "*", "Lost connection while resolving hostname [%s/%d]",
         iptostr(htonl(dcc[idx].addr)), dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void display_dcc_dnswait(int idx, char *buf)
{
  sprintf(buf, "dns   waited %lis", now - dcc[idx].timeval);
}

static void kill_dcc_dnswait(int idx, void *x)
{
  struct dns_info *p = (struct dns_info *) x;

  if (p) {
    if (p->cbuf)
      free(p->cbuf);
  }
}

struct dcc_table DCC_DNSWAIT = {
 "DNSWAIT",
  DCT_VALIDIDX,
  eof_dcc_dnswait,
  dcc_dnswait,
  NULL,
  NULL,
  display_dcc_dnswait,
  kill_dcc_dnswait,
  NULL,
  NULL
};



/*
static void async_timeout(void *client_data)
{
  int id = (int) client_data;
  sdprintf("%d timed out", id);
  egg_dns_cancel(id, 1);
}
*/

static void answer_init(dns_answer_t *answer)
{
	memset(answer, 0, sizeof(*answer));
}

static void answer_add(dns_answer_t *answer, const char *what)
{
	answer->list = (char **) my_realloc(answer->list, sizeof(*answer->list) * (answer->len+2));
	answer->list[answer->len] = strdup(what);
	answer->len++;
	answer->list[answer->len] = NULL;
}

static void answer_free(dns_answer_t *answer)
{
	int i;
	for (i = 0; i < answer->len; i++) free(answer->list[i]);
	if (answer->list) free(answer->list);
}

static dns_query_t *alloc_query(const char* query, dns_callback_t callback, void* client_data)
{
	dns_query_t *q = (dns_query_t *) my_calloc(1, sizeof(*q));

	q->id = query_id++;
	q->query = strdup(query);
	q->answers = 0;
	q->callback = callback;
	q->client_data = client_data;
	q->expiretime = now + async_lookup_timeout;
	q->next = query_head;
	query_head = q;

	return q;
}

static int get_dns_idx()
{
	int sock;
       
	sock = -1;
	sock = socket_create(NULL, 0, NULL, 0, SOCKET_NONBLOCK | SOCKET_UDP | SOCKET_BROADCAST);
	if (sock < 0) {
		putlog(LOG_ERROR, "*", "Unable to open DNS socket. (%d)", sock);
		return 1;
	}

//	dns_idx = sockbuf_new();
//	sockbuf_set_handler(dns_idx, &dns_handler, NULL);
//	sockbuf_set_sock(dns_idx, sock, 0);
//        allocsock(sock, SOCK_CONNECT);

        if (sock >= 0) {
          dns_idx = new_dcc(&dns_handler, 0);

          if (dns_idx < 0) {
           putlog(LOG_SERV, "*", "NO MORE DCC CONNECTIONS -- Can't create dns connection.");
           killsock(sock);
           return 1;
          }
          sdprintf("dns_idx: %d", dns_idx);
          dcc[dns_idx].sock = sock;
          dns_sock = sock;
          sdprintf("dns_sock: %d", dcc[dns_idx].sock);
//          strcpy(dcc[dns_idx].host, dns_ip);
          strcpy(dcc[dns_idx].nick, "(adns)");
//          sdprintf("dns_ip: %s", dns_ip);
          dcc[dns_idx].timeval = now;
          dns_handler.timeout_val = 0;
          return 0;
        }
	return 1;
}

void egg_dns_send(char *query, int len)
{
	sdprintf("egg_dns_send()");
        if (dns_idx >= 0 && dcc[dns_idx].sock == -1) {
          lostdcc(dns_idx);
          dns_idx = -1;
        }
	if (dns_idx < 0) {
		if (get_dns_idx()) {
                  sdprintf("get_dns_idx() failed in egg_dns_send");
                  return;
                }
	}
/*
        if (!dns_handler.timeout_val) {
          dns_handler.timeout_val = &async_server_timeout;
          sdprintf("SETTING TIMEOUT to %li", async_server_timeout);
          dcc[dns_idx].timeval = now;
        }
*/
	/* Send request to all name servers */
	for (int i = 0; i < nservers; ++i) {
		sendto(dcc[dns_idx].sock, query, len, 0, 
                   (struct sockaddr*) &(servers[i].to), sizeof(struct sockaddr));
	}
	
//        write(dcc[dns_idx].sock, query, len);
//	sockbuf_write(dns_idx, query, len);
}

dns_query_t *find_query(const char *host)
{
  dns_query_t *q = NULL;

  for (q = query_head; q; q = q->next)
    if (!egg_strcasecmp(q->query, host))
      return q;
  return NULL;
}

void dns_send_query(dns_query_t *q)
{
  char buf[512] = "";
  int len;

  /* Host lookup */
  if (!q->ip) {
    /* Send the ipv4 query. */
    q->remaining = 1;
    len = make_header(buf, q->id);
    len += cut_host(q->query, buf + len);
    buf[len] = 0; ++len; buf[len] = DNS_A; ++len;
    buf[len] = 0; ++len; buf[len] = 1; ++len;

    egg_dns_send(buf, len);

#ifdef USE_IPV6
    /* Now send the ipv6 query. */
    q->remaining++;
    len = make_header(buf, q->id);
    len += cut_host(q->query, buf + len);
    buf[len] = 0; ++len; buf[len] = DNS_AAAA; ++len;
    buf[len] = 0; ++len; buf[len] = 1; ++len;

    egg_dns_send(buf, len);
#endif
  /* Ip lookup */
  } else if (q->ip) {
    q->remaining = 1;
    len = make_header(buf, q->id);
    len += cut_host(q->ip, buf + len);
    buf[len] = 0; ++len; buf[len] = DNS_PTR; ++len;
    buf[len] = 0; ++len; buf[len] = 1; ++len;

    egg_dns_send(buf, len);
  }
}

/*
void dns_resend_queries()
{
  dns_query_t *q = NULL;

  for (q = query_head; q; q = q->next) {
    if (now >= q->expiretime) {
sdprintf("RESENDING: %s", q->query);
      dns_send_query(q);
    }
  }
}
*/

/*
void dns_create_timeout_timer(dns_query_t **qm, const char *query, int timeout)
{
	dns_query_t *q = *qm;
	egg_timeval_t howlong;

	howlong.sec = timeout;
	howlong.usec = 0;

	q->timer_id = timer_create_complex(&howlong, query, (Function) async_timeout, (void *) q->id, 0);
}
*/

/* Perform an async dns lookup. This is host -> ip. For ip -> host, use
 * egg_dns_reverse(). We return a dns id that you can use to cancel the
 * lookup. */
int egg_dns_lookup(const char *host, int timeout, dns_callback_t callback, void *client_data)
{
	dns_query_t *q = NULL;
	int i, cache_id;

	if (is_dotted_ip(host)) {
		/* If it's already an ip, we're done. */
		dns_answer_t answer;

                sdprintf("egg_dns_lookup(%s, %d) -> already an ip", host, timeout);
		answer_init(&answer);
		answer_add(&answer, host);
		callback(-1, client_data, host, answer.list);
		answer_free(&answer);
		return(-1);
	}

	/* Ok, now see if it's in our static host list (.hosts) */
	for (i = 0; i < nhosts; i++) {
		if (!egg_strcasecmp(host, hosts[i].host)) {
			dns_answer_t answer;
  
                        sdprintf("egg_dns_lookup(%s, %d) -> [.hosts] -> %s", host, timeout, hosts[i].ip);
			answer_init(&answer);
			answer_add(&answer, hosts[i].ip);
			callback(-1, client_data, host, answer.list);
			answer_free(&answer);
			return(-1);
		}
	}

        /* Now check cached list */
	cache_id = cache_find(host);
	if (cache_id >= 0) {
		shuffleArray(cache[cache_id].answer.list, cache[cache_id].answer.len);
                sdprintf("egg_dns_lookup(%s, %d) -> [cached (%ds)] -> %s", host, timeout, cache[cache_id].expiretime - now, cache[cache_id].answer.list[0]);
		callback(-1, client_data, host, cache[cache_id].answer.list);
		return(-1);
	}

	/* check if the query was already made */
        if (find_query(host)) {
          sdprintf("egg_dns_lookup(%s, %d) -> already requested", host, timeout);
          return(-2);
        }

	/* Allocate our query struct. */
        q = alloc_query(host, callback, client_data);

        sdprintf("egg_dns_lookup(%s, %d) -> querying", host, timeout);
        dns_send_query(q);

//        /* setup a timer to detect dead ns */
//	dns_create_timeout_timer(&q, host, timeout);

	return(q->id);
}

/* Perform an async dns reverse lookup. This does ip -> host. For host -> ip
 * use egg_dns_lookup(). We return a dns id that you can use to cancel the
 * lookup. */
int egg_dns_reverse(const char *ip, int timeout, dns_callback_t callback, void *client_data)
{
	dns_query_t *q;
	int i, cache_id;

	sdprintf("egg_dns_reverse(%s, %d)", ip, timeout);

	if (!is_dotted_ip(ip)) {
		/* If it's not a valid ip, don't even make the request. */
		callback(-1, client_data, ip, NULL);
		return(-1);
	}

	/* Ok, see if we have it in the static host list (.hosts) */
	for (i = 0; i < nhosts; i++) {
		if (!egg_strcasecmp(hosts[i].ip, ip)) {
			dns_answer_t answer;

			answer_init(&answer);
			answer_add(&answer, hosts[i].host);
			callback(-1, client_data, ip, answer.list);
			answer_free(&answer);
			return(-1);
		}
	}

        /* Then check the cached dns list */
	cache_id = cache_find(ip);
        if (cache_id >= 0) {
		shuffleArray(cache[cache_id].answer.list, cache[cache_id].answer.len);
		callback(-1, client_data, ip, cache[cache_id].answer.list);
		return(-1);
	}

	/* check if the query was already made */
        if (find_query(ip))
          return(-1);

	q = alloc_query(ip, callback, client_data);

	/* We need to transform the ip address into the proper form
	 * for reverse lookup. */
	if (strchr(ip, ':')) {
		char temp[128] = "";

		socket_ipv6_to_dots(ip, temp);
sdprintf("dots: %s", temp);
		q->ip = (char *) my_calloc(1, strlen(temp) + 8 + 1);
//		reverse_ip(temp, q->ip);
		strcat(q->ip, temp);
		strcat(q->ip, "ip6.arpa");
sdprintf("reversed ipv6 ip: %s", q->ip);
	} else {
		q->ip = (char *) my_calloc(1, strlen(ip) + 13 + 1);
		reverse_ip(ip, q->ip);
		strcat(q->ip, ".in-addr.arpa");
	}

        dns_send_query(q);

//	/* setup timer to detect dead ns */
//	dns_create_timeout_timer(&q, ip, timeout);

	return(q->id);
}

//static int dns_on_read(void *client_data, int idx, char *buf, int len)
static void dns_on_read(int idx, char *buf, int atr)
{
        dcc[idx].timeval = now;

//	if (resend_on_read) {
//		resend_on_read = 0;
//		dns_resend_queries();
//		return;
//	}

        struct sockaddr_in from;
        socklen_t socklen = sizeof(struct sockaddr_in);

        atr = recvfrom(dcc[idx].sock, buf, 512, 0, (struct sockaddr *) &from, &socklen);

        if (atr == -1) {
          if (errno == EAGAIN)
            atr = recvfrom(dcc[idx].sock, buf, 512, 0, (struct sockaddr *) &from, &socklen);
          if (atr == -1) {
            dns_on_eof(idx);
            return;
          }
        }

        const char *server_ip = iptostr(from.sin_addr.s_addr);

        /* Make sure this was a server that was queried */
        bool found = 0;
        for (int i = 0; i < nservers; ++i) {
                if (servers[i].to.sin_addr.s_addr == from.sin_addr.s_addr) {
                        found = 1;
                        break;
                }
        }
        if (found == 0) {
                sdprintf("IGNORING DNS REPLY FROM UNQUERIED IP: %s\n", server_ip);
                return;
        }

/*
        sdprintf("SETTING TIMEOUT to 0");
        dns_handler.timeout_val = 0;
*/
//        sdprintf("DNS reply from: %s", server_ip);
	if (parse_reply(buf, atr, server_ip))
	/* FIXME: The udp socket is broadcast now, should just remove this server is parse_reply returns 1 */
          dns_on_eof(idx);
	return;
}

static void dns_on_eof(int idx)
{
        sdprintf("EOF on dns idx: %d sock: %d (%s)", idx, dcc[idx].sock, dcc[idx].host);
        dns_reinit(idx);

	return;
}

/* for .restart
int egg_dns_shutdown(void)
{
	int i;

	if (nservers > 0) {
		for (i = 0; i < nservers; i++) {
			if (servers[i].ip) free(servers[i].ip);
		}
		free(servers); servers = NULL;
		nservers = 0;
	}
	
	if (nhosts > 0) {
		for (i = 0; i < nhosts; i++) {
			if (hosts[i].host) free(hosts[i].host);
			if (hosts[i].ip) free(hosts[i].ip);
		}
		free(hosts); hosts = NULL;
		nhosts = 0;
	}

	return (0);
}
*/

static void add_dns_server(char *ip)
{
	servers = (dns_server_t *) my_realloc(servers, (nservers+1)*sizeof(*servers));
	servers[nservers].ip = strdup(ip);

	struct sockaddr_in *to = &(servers[nservers].to);
	to->sin_family = AF_INET;
	to->sin_port = htons(DNS_PORT);
	to->sin_addr.s_addr = inet_addr(ip);
	egg_memset(to->sin_zero, 0, sizeof(to->sin_zero));

        ++nservers;
        sdprintf("Added NS: %s", ip);
}

static void add_host(char *host, char *ip)
{
	hosts = (dns_host_t *) my_realloc(hosts, (nhosts+1)*sizeof(*hosts));
	hosts[nhosts].host = strdup(host);
	hosts[nhosts].ip = strdup(ip);
	nhosts++;
}

static int cache_expired(int id)
{
	if (cache[id].expiretime && (now >= cache[id].expiretime))  return(1);
	return (0);
}

static void cache_del(int id)
{
	answer_free(&cache[id].answer);
	free(cache[id].query);
	cache[id].expiretime = 0;

	ncache--;

	if (id < ncache) egg_memcpy(&cache[id], &cache[ncache], sizeof(dns_cache_t));
	else egg_bzero(&cache[id], sizeof(dns_cache_t));

	cache = (dns_cache_t *) my_realloc(cache, (ncache+1)*sizeof(*cache));
}

static void cache_add(const char *query, dns_answer_t *answer)
{
	int i;

	cache = (dns_cache_t *) my_realloc(cache, (ncache+1)*sizeof(*cache));
	egg_bzero(&cache[ncache], sizeof(cache[ncache]));
	cache[ncache].query = strdup(query);
	answer_init(&cache[ncache].answer);
        for (i = 0; i < answer->len; i++)
		answer_add(&cache[ncache].answer, answer->list[i]);
	cache[ncache].expiretime = now + answer->ttl;
	ncache++;
}

static int cache_find(const char *query)
{
	int i;

	for (i = 0; i < ncache; i++)
		if (!egg_strcasecmp(cache[i].query, query)) return (i);

	return (-1);
}

void dns_cache_flush()
{
  int i = 0;

  for (i = 0; i < ncache; i++) {
    cache_del(i);
    if (i == ncache) break;
    i--;
  }
}

static int read_thing(char *buf, char *ip)
{
	int skip, len;

	skip = strspn(buf, separators);
	buf += skip;
	len = strcspn(buf, separators);
	egg_memcpy(ip, buf, len);
	ip[len] = 0;
	return(skip + len);
}

static void read_resolv(char *fname)
{
	FILE *fp;
	char buf[512], ip[512];

	fp = fopen(fname, "r");
	if (!fp) return;
	while (fgets(buf, sizeof(buf), fp)) {
		if (!strncasecmp(buf, "nameserver", 10)) {
			read_thing(buf+10, ip);
			if (strlen(ip)) add_dns_server(ip);
		}
	}
	fclose(fp);
}

static void read_hosts(char *fname)
{
	FILE *fp;
	char buf[512], ip[512], host[512];
	int skip, n;

	fp = fopen(fname, "r");
	if (!fp) return;
	while (fgets(buf, sizeof(buf), fp)) {
		if (strchr(buf, '#')) continue;
		skip = read_thing(buf, ip);
		if (!strlen(ip)) continue;
		while ((n = read_thing(buf+skip, host))) {
			skip += n;
			if (strlen(host)) add_host(host, ip);
		}
	}
	fclose(fp);
}


static int make_header(char *buf, int id)
{
	_dns_header.question_count = htons(1);
//	_dns_header.id = htons(id);
	_dns_header.id = id;
	egg_memcpy(buf, &_dns_header, HEAD_SIZE);
	return(HEAD_SIZE);
}

static int cut_host(const char *host, char *query)
{
	char *period, *orig;
	int len;

	orig = query;
	while ((period = strchr(host, '.'))) {
		len = period - host;
		if (len > 63) return(-1);
		*query++ = len;
		egg_memcpy(query, host, len);
		query += len;
		host = period+1;
	}
	len = strlen(host);
	if (len) {
		*query++ = len;
		egg_memcpy(query, host, len);
		query += len;
	}
	*query++ = 0;
	return(query-orig);
}

static int reverse_ip(const char *host, char *reverse)
{
	char *period;
	int offset, len;

	period = strchr(host, '.');
	if (!period) {
		len = strlen(host);
		egg_memcpy(reverse, host, len);
		return(len);
	}
	else {
		len = period - host;
		offset = reverse_ip(host+len+1, reverse);
		reverse[offset++] = '.';
		egg_memcpy(reverse+offset, host, len);
		reverse[offset+len] = 0;
		return(offset+len);
	}
}

int egg_dns_cancel(int id, int issue_callback)
{
	dns_query_t *q, *prev = NULL;

	for (q = query_head; q; q = q->next) {
		if (q->id == id) break;
		prev = q;
	}
	if (!q) return(-1);
	if (prev) prev->next = q->next;
	else query_head = q->next;
	sdprintf("Cancelling query: %s", q->query);
	if (issue_callback) q->callback(q->id, q->client_data, q->query, NULL);
	if (q->ip)
		free(q->ip);
	free(q->query);
	free(q);
	return(0);
}

static int skip_name(unsigned char *ptr)
{
	int len;
	unsigned char *start = ptr;

	while ((len = *ptr++) > 0) {
		if (len > 63) {
			ptr++;
			break;
		}
		else {
			ptr += len;
		}
	}
	return(ptr - start);
}

/*
void print_header(dns_header_t &header)
{
#define dofield(_field)         sdprintf("%s: %d\n", #_field, _field)
	dofield(header.id);
	dofield(header.question_count);
	dofield(header.answer_count);
	dofield(header.ar_count);
	dofield(header.ns_count);
#undef dofield
}

void print_reply(dns_rr_t &reply)
{
#define dofield(_field)         sdprintf("%s: %d\n", #_field, _field)
	dofield(reply.type);
	dofield(reply.dclass);
	dofield(reply.ttl);
	dofield(reply.rdlength);
#undef dofield
}
*/

static int parse_reply(char *response, size_t nbytes, const char *server_ip)
{
	dns_header_t header;
	dns_query_t *q = NULL, *prev = NULL;
	dns_rr_t reply;
	char result[512] = "";
	short rr;
	int r = -1;
	unsigned const char *eop = (unsigned char *) response + nbytes;
	unsigned char *ptr = (unsigned char *) response;

	egg_memcpy(&header, ptr, HEAD_SIZE);
	ptr += HEAD_SIZE;

	/* header.id is already in our order, echoed by the server */
	header.flags = ntohs(header.flags);
	header.question_count = ntohs(header.question_count);
	header.answer_count = ntohs(header.answer_count);
	header.ar_count = ntohs(header.ar_count);
	header.ns_count = ntohs(header.ns_count);

//	print_header(header);

	/* Find our copy of the query before proceeding. */
	for (q = query_head; q; q = q->next) {
		if (q->id == header.id) break;
		prev = q;
	}
	if (!q) { 
//                sdprintf("Ignoring duplicate or invalid reply(%d) from: %s", header.id, server_ip);
                return 0;
        }

        /* Did this server give us recursion? */
        if (!GET_RA(header.flags)) {
                sdprintf("Ignoring reply(%d) from %s: no recusion available.", header.id, server_ip);
                /* FIXME: Remove dns server from list */
                return 0;
        }

        sdprintf("Reply (%d) questions: %d answers: %d ar: %d ns: %d flags: %d from: %s", header.id, header.question_count, header.answer_count, header.ar_count, header.ns_count, header.flags, server_ip);
        
//        /* destroy our async timeout */
//        timer_destroy(q->timer_id);

	/* Pass over the questions. */
	for (rr = 0; rr < header.question_count; rr++) {
		ptr += skip_name(ptr);
		ptr += 4;
	}
	/* End of questions. */

//	for (rr = 0; rr < header.answer_count + header.ar_count + header.ns_count; rr++) {


	q->answers += header.answer_count;

	for (rr = 0; rr < header.answer_count; rr++) {
		result[0] = 0;
		/* Read in the answer. */
		ptr += skip_name(ptr);

		egg_memcpy(&reply, ptr, RR_SIZE);
		ptr += RR_SIZE;

		reply.type = ntohs(reply.type);
		reply.dclass = ntohs(reply.dclass);
		reply.rdlength = ntohs(reply.rdlength);
		reply.ttl = ntohl(reply.ttl);
		/* Save the lowest ttl */
		if (reply.ttl && ((!q->answer.ttl) || (q->answer.ttl > reply.ttl))) q->answer.ttl = reply.ttl;

//		print_reply(reply);

		switch (reply.type) {
		case DNS_A:
			inet_ntop(AF_INET, ptr, result, 512);
                        sdprintf("Reply (%d): A %s", header.id, result);
			answer_add(&q->answer, result);
			break;
		case DNS_AAAA:
#ifdef USE_IPV6
			inet_ntop(AF_INET6, ptr, result, 512);
                        sdprintf("Reply (%d): AAAA %s", header.id, result);
			answer_add(&q->answer, result);
#endif /* USE_IPV6 */
			break;
		case DNS_PTR:
			r = my_dn_expand((const unsigned char *) response, eop, ptr, result, sizeof(result));

			if (r != -1 && result[0]) {
                                sdprintf("Reply (%d): PTR %s", header.id, result);
				answer_add(&q->answer, result);
                        }
			break;
		default:
			sdprintf("Unhandled DNS reply (%d) type: %d", header.id, reply.type);
			break;
		}

		ptr += reply.rdlength;
                if ((size_t) (ptr - (unsigned char*) response) > nbytes) {
                  sdprintf("MALFORMED/TRUNCATED DNS PACKET detected (need TCP).");
                  q->remaining = 0;
                  break;
                }
	}
	/* FIXME: This assumes 1 dns query, but the new format
                  sends out 4 queries, each with multiple questions.
                  I guess this doesn't matter though as if the reply
                  has gotten this far, it's probably a good server
                  and will return the other answer
        */
	/* Don't continue if we haven't gotten all expected replies. */
	if (--q->remaining > 0) return 0;

        if (q->answer.len == 0) {
/* FIXME: might just be a broken nameserver, no reason to fail. */
          sdprintf("Failed to get any answers for query");

          if (prev) prev->next = q->next;
          else query_head = q->next;

          q->callback(q->id, q->client_data, q->query, NULL);

          free(q->query);
          if (q->ip)
            free(q->ip);
          free(q);
          return 1;		/* get a new server */
        }

	/* Ok, we have, so now issue the callback with the answers. */
	if (prev) prev->next = q->next;
	else query_head = q->next;

	cache_add(q->query, &q->answer);

	q->callback(q->id, q->client_data, q->query, q->answer.list);
	answer_free(&q->answer);
	free(q->query);
        if (q->ip)
          free(q->ip);
	free(q);

	return(0);
}


void tell_dnsdebug(int idx)
{
	dns_query_t *q = NULL;
	int i, n = 0;

	dprintf(idx, "NS: %s\n", dns_ip);


	for (q = query_head; q; q = q->next)
		dprintf(idx, "DNS (%d) (%lis): %s\n", q->id, q->expiretime - now, q->query);

//	for (i = 0; i < nhosts; i++)
//           dprintf(idx, "HOST #%d: %s/%s\n", i, hosts[i].host, hosts[i].ip);

	for (i = 0; i < ncache; i++) {
		dprintf(idx, "cache(%d) %s expires in %lis\n", i, cache[i].query, cache[i].expiretime - now);
    		for (n = 0; n < cache[i].answer.len; n++)
			dprintf(idx, "%d: %s\n", n, cache[i].answer.list[n]);
	}
}

static void expire_queries()
{
  dns_query_t *q = NULL, *next = NULL;
  int i = 0;

  /* need to check for expired queries and either:
    a) recheck/change ns
    b) expire due to ttl
    */

  if (query_head) {
    for (q = query_head; q; q = q->next) {
      if (q->expiretime <= now) {		/* set in alloc_query */
        if (q->next)
          next = q->next;
        egg_dns_cancel(q->id, 1);
        if (!next) break;
        q = next;
      }
    }
  }

  for (i = 0; i < ncache; i++) {
    if (cache_expired(i)) {
      cache_del(i);
      if (i == ncache) break;
      i--;
    }
  }

}


/* Read in .hosts and /etc/hosts and .resolv.conf and /etc/resolv.conf */
int egg_dns_init()
{
        SET_RD(_dns_header.flags);
        _dns_header.flags = htons(_dns_header.flags);
	read_resolv(".resolv.conf");
	read_resolv("/etc/resolv.conf");
//	read_hosts("/etc/hosts");
	read_hosts(".hosts");
    
        /* some backup servers, probably will never be used. */
        add_dns_server("4.2.2.2");
        add_dns_server("68.2.16.30"); //some cox ns
        add_dns_server("68.6.16.25"); //another cox

/* root servers for future development (tracing down)
	add_dns_server("198.41.0.4");
	add_dns_server("192.228.79.201");
	add_dns_server("192.33.4.12");
	add_dns_server("128.8.10.90");
	add_dns_server("192.203.230.10");
	add_dns_server("192.5.5.241");
	add_dns_server("192.112.36.4");
	add_dns_server("128.63.2.53");
	add_dns_server("192.36.148.17");
	add_dns_server("192.58.128.30");
	add_dns_server("193.0.14.129");
	add_dns_server("198.32.64.12");
	add_dns_server("202.12.27.33");
*/


	timer_create_secs(3, "adns_check_expires", (Function) expire_queries);

	return(0);
}


bool valid_dns_id(int idx, int id)
{
  if (id == -1)
    return 1;
  if (valid_idx(idx) && dcc[idx].dns_id && dcc[idx].dns_id == id)
    return 1;
  sdprintf("dns_id: %d is not associated with dead idx: %d", id, idx);
  return 0;
}
