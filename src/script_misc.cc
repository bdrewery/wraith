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
#include "egg_timer.h"
#include "main.h"
#include "misc.h"
#include "net.h"
#include "userent.h"
#include <bdlib/src/base64.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/String.h>

bd::String script_decrypt(const bd::String key, const bd::String enc) {
  return decrypt_string(key, bd::base64Decode(enc));
}

bd::String script_encrypt(const bd::String key, const bd::String string) {
  return bd::base64Encode(encrypt_string(key, string));
}

bd::String script_encpass(const bd::String password) {
 char *encrypted_pass;
 bd::String ret;

 encrypted_pass = encpass(password.mdata());
 ret = encrypted_pass;
 free(encrypted_pass);
 return ret;
}

void script_exit(bd::String reason) {
  bd::String msg;

  msg = bd::String::printf("BOT SHUTDOWN (%s)", reason ? reason.c_str() : "No reason");
  if (reason)
    strlcpy(quit_msg, msg.c_str(), sizeof(quit_msg));
  else
    quit_msg[0] = 0;

  kill_bot(msg.c_str(), quit_msg[0] ? quit_msg : "EXIT");
}

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

static bd::HashTable<bd::ScriptCallbacker*, bd::ScriptCallbackerPtr> _timer_callbacks;

void script_timer_callback(bd::ScriptCallbacker* callback_command) {
  // Forward to the Script callback
  ContextNote("timer callback", callback_command->cmd.c_str());
  callback_command->call();
}

void script_timer_destroy_callback(bd::ScriptCallbacker* callback_command) {
  /* This may free the ScriptCallbackerPtr. */
  _timer_callbacks.remove(callback_command);
}

static bd::String _script_timer(int seconds, bd::ScriptCallbackerPtr cmd, int count, int type) {
  bd::String timer_name;
  egg_timeval_t howlong;
  int timer_id;

  if (seconds < 0)
    throw bd::String("time value must be positive");

  if (count < 0)
    throw bd::String("count value must be >= 0");
  else if (!count)
    count = 1;

  howlong.sec = seconds;
  howlong.usec = 0;

  timer_name = bd::String("script:") + cmd->cmd;

  if (count)
    type |= TIMER_REPEAT;
  else
    type |= TIMER_ONCE;

  /* Store the shared_ptr for later and remove in a destructor callback. */
  _timer_callbacks[cmd.get()] = cmd;
  timer_id = timer_create_complex(&howlong, timer_name.c_str(),
      (Function) script_timer_callback,
      (Function) script_timer_destroy_callback, cmd.get(), TIMER_SCRIPT|type, count);

  return bd::String::printf("timer%lu", (unsigned long) timer_id);
}

bd::String script_timer(int minutes, bd::ScriptCallbackerPtr cmd, int count) {
  return _script_timer(minutes * 60, cmd, count, TIMER_SCRIPT_MINUTELY);
}

bd::String script_utimer(int seconds, bd::ScriptCallbackerPtr cmd, int count) {
  return _script_timer(seconds, cmd, count, TIMER_SCRIPT_SECONDLY);
}

static void _script_killtimer(const bd::String timerID) {
  if (timerID(0, 5) != "timer")
    throw bd::String("argument is not a timerID");

  if (timer_destroy(atoi(timerID(5).c_str())))
    throw bd::String("invalid timerID");
}

void script_killtimer(const bd::String timerID) {
  _script_killtimer(timerID);
}

void script_killutimer(const bd::String timerID) {
  _script_killtimer(timerID);
}

bd::Array<bd::Array<bd::String>> _script_timers(int type) {
  bd::Array<bd::Array<bd::String>> ret;
  int *ids = 0, n = 0, called = 0, remaining = 0;
  egg_timeval_t howlong, trigger_time, mynow, diff;

  if ((n = timer_list(&ids, type))) {
    int i = 0;
    char *name = NULL;

    timer_get_now(&mynow);

    for (i = 0; i < n; i++) {
      bd::Array<bd::String> timer;

      timer_info(ids[i], &name, &howlong, &trigger_time, &called, &remaining);
      timer_diff(&mynow, &trigger_time, &diff);

      timer << bd::String::printf("%li", diff.sec);
      timer << name;
      timer << bd::String::printf("timer%d", ids[i]);
      timer << bd::String::printf("%d", remaining);

      ret << timer;
    }
    free(ids);
  }

  return ret;
}

bd::Array<bd::Array<bd::String>> script_timers() {
  return _script_timers(TIMER_SCRIPT_MINUTELY);
}

bd::Array<bd::Array<bd::String>> script_utimers() {
  return _script_timers(TIMER_SCRIPT_SECONDLY);
}

bd::String script_duration(int seconds) {
  char s[70];
  long tmp;

  if (seconds < 0)
    throw bd::String("seconds must be positive");
  else if (seconds == 0)
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

bd::String script_md5(const bd::String string) {
  return MD5(string.c_str());
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
  script_add_command("decrypt", 	script_decrypt, 	"key encrypted-base64-string");
  script_add_command("die",        	script_exit,     	"?reason?",			0);
  script_add_command("encpass", 	script_encpass, 	"password");
  script_add_command("encrypt", 	script_encrypt, 	"key string");
  script_add_command("exit",    	script_exit,     	"?reason?",			0);
  script_add_command("putlog",		script_putlog,		"text");
  script_add_command("putcmdlog",	script_putcmdlog,	"text");
  script_add_command("putloglev",	script_putloglev,	"level(s) channel text");
  script_add_command("killtimer",	script_killtimer,	"timerID");
  script_add_command("killutimer",	script_killutimer,	"timerID");
  script_add_command("md5",		script_md5,		"string");
  script_add_command("myip",		script_myip);
  script_add_command("rand",		script_rand,		"limit");
  script_add_command("strftime",	script_strftime,	"formatstring ?time?",		1);
  script_add_command("timer",		script_timer,		"minutes command ?count?",	2);
  script_add_command("timers",		script_timers);
  script_add_command("utimers",		script_utimers);
  script_add_command("utimer",		script_utimer,		"seconds command ?count?",	2);
  script_add_command("unixtime",	script_unixtime);
}

/* vim: set sts=2 sw=2 ts=8 et: */