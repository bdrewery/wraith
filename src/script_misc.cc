/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
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

/*
 * script_misc.c -- misc scripting commands
 *
 */

#include "script.h"
#include "main.h"
#include "net.h"
#include <bdlib/src/String.h>

void script_putlog(const bd::String text) {
  putlog(LOG_MISC, "*", "%s", text.c_str());
}

void script_putcmdlog(const bd::String text) {
  putlog(LOG_CMDS, "*", "%s", text.c_str());
}

void script_putloglev(const bd::String levels, const bd::String channel, const bd::String text) {
  int lev = 0;

  lev = logmodes(levels.c_str());
  if (!lev)
    throw bd::String("No valid log-level given");

  putlog(lev, channel.c_str(), text.c_str());
}

bd::String script_duration(int seconds) {
  char s[70];
  long tmp;

  if (seconds <= 0)
    return "0 seconds";

  s[0] = 0;
  if (seconds >= 31536000) {
    tmp = (seconds / 31536000);
    sprintf(s, "%lu year%s ", tmp, (tmp == 1) ? "" : "s");
    seconds -= (tmp * 31536000);
  }
  if (seconds >= 604800) {
    tmp = (seconds / 604800);
    sprintf(&s[strlen(s)], "%lu week%s ", tmp, (tmp == 1) ? "" : "s");
    seconds -= (tmp * 604800);
  }
  if (seconds >= 86400) {
    tmp = (seconds / 86400);
    sprintf(&s[strlen(s)], "%lu day%s ", tmp, (tmp == 1) ? "" : "s");
    seconds -= (tmp * 86400);
  }
  if (seconds >= 3600) {
    tmp = (seconds / 3600);
    sprintf(&s[strlen(s)], "%lu hour%s ", tmp, (tmp == 1) ? "" : "s");
    seconds -= (tmp * 3600);
  }
  if (seconds >= 60) {
    tmp = (seconds / 60);
    sprintf(&s[strlen(s)], "%lu minute%s ", tmp, (tmp == 1) ? "" : "s");
    seconds -= (tmp * 60);
  }
  if (seconds > 0) {
    tmp = (seconds);
    sprintf(&s[strlen(s)], "%lu second%s", tmp, (tmp == 1) ? "" : "s");
  }
  if (strlen(s) > 0 && s[strlen(s) - 1] == ' ')
    s[strlen(s) - 1] = 0;

  return s;
}

bd::String script_unixtime() {
  return bd::String::printf("%li", time(NULL));
}

bd::String script_ctime(long unixtime) {
  time_t tt;

  tt = (time_t) unixtime;
  return ctime(&tt);
}

bd::String script_strftime(const bd::String format, long time) {
  char buf[512];
  struct tm *tm1;
  time_t t;

  if (time)
    t = (time_t) time;
  else
    t = now;
  tm1 = localtime(&t);
  if (strftime(buf, sizeof(buf) - 1, format.c_str(), tm1))
    return buf;
  else
    throw bd::String("error with strftime");
}

long script_myip() {
  return getmyip();
}

unsigned long script_rand(long limit) {
  if (limit <= 0)
    throw bd::String("random limit must be greater than zero");
  else if (limit > RANDOM_MAX)
    throw bd::String::printf("random limit must be less than %li", (long)RANDOM_MAX);

  return randint(limit);
}


void init_script_misc() {
  script_add_command("ctime",		script_ctime,		"unixtime");
  script_add_command("duration",	script_duration,	"seconds");
  script_add_command("putlog",		script_putlog,		"text");
  script_add_command("putcmdlog",	script_putcmdlog,	"text");
  script_add_command("putloglev",	script_putloglev,	"level(s) channel text");
  script_add_command("myip",		script_myip);
  script_add_command("rand",		script_rand,		"limit");
  script_add_command("strftime",	script_strftime,	"formatstring ?time?",		1);
  script_add_command("unixtime",	script_unixtime);
}

/* vim: set sts=2 sw=2 ts=8 et: */
