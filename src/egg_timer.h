#ifndef _EGG_TIMER_H_
#define _EGG_TIMER_H_

#include "common.h"
#include "types.h"

typedef struct egg_timeval_b {
	long sec;
	long usec;
} egg_timeval_t;

#define TIMER_ONCE             BIT1
#define TIMER_REPEAT           BIT2
#define TIMER_SCRIPT           BIT3
#define TIMER_SCRIPT_MINUTELY  BIT4
#define TIMER_SCRIPT_SECONDLY  BIT5

/* Create a simple timer with no client data and no flags. */
#define timer_create(howlong,name,callback) timer_create_complex(howlong, name, callback, NULL, NULL, TIMER_ONCE, 0)

/* Create a simple timer with no client data, but it repeats. */
#define timer_create_repeater(howlong,name,callback,count) timer_create_complex(howlong, name, callback, NULL, NULL, TIMER_REPEAT, count)

void timer_get_now(egg_timeval_t *_now);
int timer_get_now_sec(int *sec);
void timer_update_now(egg_timeval_t *_now);
int timer_diff(egg_timeval_t *from_time, egg_timeval_t *to_time, egg_timeval_t *diff);
long timeval_diff(const egg_timeval_t *tv1, const egg_timeval_t *tv2);
int timer_create_secs(int, const char *, Function);
int timer_create_complex(egg_timeval_t *howlong, const char *name, Function callback, Function destroy_callback, void *client_data, int flags, int count);
int timer_destroy(int timer_id);
#ifdef not_used
int timer_destroy_all();
#endif
int timer_get_shortest(egg_timeval_t *howlong);
void timer_run();
int timer_list(int **ids, int flags);
int timer_info(int id, char **name, egg_timeval_t *initial_len, egg_timeval_t *trigger_time, int *called, int *remaining);
#endif /* _EGG_TIMER_H_ */
