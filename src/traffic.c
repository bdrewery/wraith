/*
 */


#include "common.h"
#include "traffic.h"

/* Traffic stats
 */
egg_traffic_t traffic;


void event_resettraffic()
{
  traffic.out_total.irc += traffic.out_today.irc;
  traffic.out_total.bn += traffic.out_today.bn;
  traffic.out_total.dcc += traffic.out_today.dcc;
  traffic.out_total.filesys += traffic.out_today.filesys;
  traffic.out_total.trans += traffic.out_today.trans;
  traffic.out_total.unknown += traffic.out_today.unknown;

  traffic.in_total.irc += traffic.in_today.irc;
  traffic.in_total.bn += traffic.in_today.bn;
  traffic.in_total.dcc += traffic.in_today.dcc;
  traffic.in_total.filesys += traffic.in_today.filesys;
  traffic.in_total.trans += traffic.in_today.trans;
  traffic.in_total.unknown += traffic.in_today.unknown;

  egg_memset(&traffic.out_today, 0, sizeof(traffic.out_today));
  egg_memset(&traffic.in_today, 0, sizeof(traffic.in_today));
}

void traffic_update(int idx, size_t len) {
  if (dcc[idx].type->name) {
    if (!strncmp(dcc[idx].type->name, "BOT", 3))
      traffic.in_today.bn += len;
    else if (!strcmp(dcc[idx].type->name, "SERVER"))
      traffic.in_today.irc += len;
    else if (!strncmp(dcc[idx].type->name, "CHAT", 4))
      traffic.in_today.dcc += len;
    else if (!strncmp(dcc[idx].type->name, "FILES", 5))
      traffic.in_today.dcc += len;
    else if (!strcmp(dcc[idx].type->name, "SEND"))
      traffic.in_today.trans += len;
    else if (!strncmp(dcc[idx].type->name, "GET", 3))
      traffic.in_today.trans += len;
    else
      traffic.in_today.unknown += len;
  }
}
