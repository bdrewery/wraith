/* I hereby release this into the Public Domain - Bryan Drewery */
/*
 * binary.c -- handles:
 *   misc update functions
 *   md5 hash verifying
 *
 */


#include "common.h"
#include "binary.h"
#include "settings.h"
#include "crypt.h"
#include "shell.h"
#include "misc.h"
#include "main.h"
#include "misc_file.h"
#include "tandem.h"
#include "botnet.h"
#include "userrec.h"

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

settings_t settings = {
  "\200\200\200\200\200\200\200\200\200\200\200\200\200\200\200",
  /* -- STATIC -- */
  "", "", "", "", "", "", "", "", "", "",
  /* -- DYNAMIC -- */
  "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  /* -- PADDING */
  ""
};

static void edpack(settings_t *, const char *, int);
#ifdef DEBUG
static void tellconfig(settings_t *);
#endif

#define PACK_ENC 1
#define PACK_DEC 2

int checked_bin_buf = 0;

#define MMAP_LOOP(_offset, _block_len, _total, _len)	\
  for ((_offset) = 0; 					\
       (_offset) < (_total); 				\
       (_offset) += (_block_len),			\
       (_len) = ((_total) - (_offset)) < (_block_len) ? \
              ((_total) - (_offset)) : 			\
              (_block_len)				\
      )

#define MMAP_READ(_map, _dest, _offset, _len)	\
  memcpy((_dest), &(_map)[(_offset)], (_len));	\
  (_offset) += (_len);

static char *
bin_checksum(const char *fname, int todo)
{
  MD5_CTX ctx;
  static char hash[MD5_HASH_LENGTH + 1] = "";
  unsigned char md5out[MD5_HASH_LENGTH + 1] = "", buf[PREFIXLEN + 1] = "";
  int fd = -1;
  size_t len = 0, offset = 0, size = 0, newpos = 0;
  unsigned char *map = NULL, *outmap = NULL;
  char *fname_bak = NULL;
  Tempfile *newbin = NULL;

  MD5_Init(&ctx);

  ++checked_bin_buf;
 
  hash[0] = 0;

  fixmod(fname);

  if (todo == GET_CHECKSUM) {
    fd = open(fname, O_RDONLY);
    if (fd == -1) werr(ERR_BINSTAT);
    size = lseek(fd, 0, SEEK_END);
    map = (unsigned char*) mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if ((void*)map == MAP_FAILED) goto fatal;
    MMAP_LOOP(offset, sizeof(buf) - 1, size, len) {
      if (!memcmp(&map[offset], &settings.prefix, PREFIXLEN))
        break;
    }
    MD5_Update(&ctx, map, offset);

    /* Hash everything after the packdata too */
    offset += sizeof(settings_t);
    MD5_Update(&ctx, &map[offset], size - offset);

    MD5_Final(md5out, &ctx);
    strlcpy(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
    OPENSSL_cleanse(&ctx, sizeof(ctx));

    munmap(map, size);
    close(fd);
  }

  if (todo == GET_CONF) {
    fd = open(fname, O_RDONLY);
    if (fd == -1) werr(ERR_BINSTAT);
    size = lseek(fd, 0, SEEK_END);
    map = (unsigned char*) mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if ((void*)map == MAP_FAILED) goto fatal;

    /* Find the packdata */
    MMAP_LOOP(offset, sizeof(buf) - 1, size, len) {
      if (!memcmp(&map[offset], &settings.prefix, PREFIXLEN))
        break;
    }
    MD5_Update(&ctx, map, offset);

    /* Hash everything after the packdata too */
    MD5_Update(&ctx, &map[offset + sizeof(settings_t)], size - (offset + sizeof(settings_t)));

    MD5_Final(md5out, &ctx);
    strlcpy(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
    OPENSSL_cleanse(&ctx, sizeof(ctx));

    settings_t newsettings;

    /* Read the settings struct into newsettings */
    MMAP_READ(map, &newsettings, offset, sizeof(settings));

    /* Decrypt the new data */
    edpack(&newsettings, hash, PACK_DEC);
    OPENSSL_cleanse(hash, sizeof(hash));

    /* Copy over only the dynamic data, leaving the pack config static */
    memcpy(&settings.bots, &newsettings.bots, SIZE_CONF);

    munmap(map, size);
    close(fd);

    return ".";
  }

  if (todo & WRITE_CHECKSUM) {
    newbin = new Tempfile("bin", 0);

    size = strlen(fname) + 2;
    fname_bak = (char *) my_calloc(1, size);
    simple_snprintf(fname_bak, size, "%s~", fname);

    fd = open(fname, O_RDONLY);
    if (fd == -1) werr(ERR_BINSTAT);
    size = lseek(fd, 0, SEEK_END);

    map = (unsigned char*) mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if ((void*)map == MAP_FAILED) goto fatal;

    /* Find settings struct in original binary */
    MMAP_LOOP(offset, sizeof(buf) - 1, size, len) {
      if (!memcmp(&map[offset], &settings.prefix, PREFIXLEN))
        break;
    }
    MD5_Update(&ctx, map, offset);
    /* Hash everything after the packdata too */
    MD5_Update(&ctx, &map[offset + sizeof(settings_t)], size - (offset + sizeof(settings_t)));
    MD5_Final(md5out, &ctx);
    strlcpy(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
    OPENSSL_cleanse(&ctx, sizeof(ctx));

    strlcpy(settings.hash, hash, sizeof(settings.hash));

    /* encrypt the entire struct with the hash (including hash) */
    edpack(&settings, hash, PACK_ENC);
    OPENSSL_cleanse(hash, sizeof(hash));

    /* Copy everything up to this point into the new binary (including the settings header/prefix) */
    outmap = (unsigned char*) mmap(0, size, PROT_WRITE, MAP_SHARED, newbin->fd, 0);
    if ((void*)outmap == MAP_FAILED) goto fatal;

    if (lseek(newbin->fd, size - 1, SEEK_SET) == -1) goto fatal;
    if (write(newbin->fd, "", 1) != 1) goto fatal;

    offset += PREFIXLEN;
    memcpy(outmap, map, offset);

    newpos = offset;

    if (todo & WRITE_PACK) {
      /* Now copy in our encrypted settings struct */
      memcpy(&outmap[newpos], &settings.hash, SIZE_PACK);
#ifdef DEBUG
      sdprintf(STR("writing pack: %d\n"), SIZE_PACK);
#endif
    } else {
      /* Just copy the original pack data to the new binary */
      memcpy(&outmap[newpos], &map[offset], SIZE_PACK);
    }
    offset += SIZE_PACK;
    newpos += SIZE_PACK;

    if (todo & WRITE_CONF) {
      /* Copy in the encrypted conf data */
      memcpy(&outmap[newpos], &settings.bots, SIZE_CONF);
#ifdef DEBUG
      sdprintf(STR("writing conf: %d\n"), SIZE_CONF);
#endif
    } else {
      /* Just copy the original conf data to the new binary */
      memcpy(&outmap[newpos], &map[offset], SIZE_CONF);
    }

    newpos += SIZE_CONF;
    offset += SIZE_CONF;

    /* Write the rest of the binary */
    memcpy(&outmap[newpos], &map[offset], size - offset);
    newpos += size - offset;
    offset += size - offset;

    munmap(map, size);
    close(fd);

    munmap(outmap, size);
    newbin->my_close();

    if (size != newpos) {
      delete newbin;
      fatal(STR("Binary corrupted"), 0);
    }

    if (movefile(fname, fname_bak)) {
      printf(STR("Failed to move file (%s -> %s): %s\n"), fname, fname_bak, strerror(errno));
      delete newbin;
      fatal("", 0);
    }

    if (movefile(newbin->file, fname)) {
      printf(STR("Failed to move file (%s -> %s): %s\n"), newbin->file, fname, strerror(errno));
      delete newbin;
      fatal("", 0);
    }

    fixmod(fname);
    unlink(fname_bak);
    delete newbin;
    
    return hash;
  fatal:
    if ((void*)map != MAP_FAILED)
      munmap(map, size);
    if (fd != -1)
      close(fd);

    if ((void*)outmap != MAP_FAILED)
      munmap(outmap, size);
    delete newbin;
    werr(ERR_BINSTAT);
  }

  return hash;
}

static int
features_find(const char *buffer)
{
  if (!egg_strcasecmp(buffer, STR("no_take")))
    return FEATURE_NO_TAKE;
  else if (!egg_strcasecmp(buffer, STR("no_mdop")))
    return FEATURE_NO_MDOP;
  return 0;
}

static int
readcfg(const char *cfgfile)
{
  FILE *f = NULL;

  if ((f = fopen(cfgfile, "r")) == NULL) {
    printf(STR("Error: Can't open '%s' for reading\n"), cfgfile);
    exit(1);
  }

  char *buffer = NULL, *p = NULL;
  int skip = 0, line = 0, feature = 0;

  printf(STR("Reading '%s' "), cfgfile);
  while ((!feof(f)) && ((buffer = step_thru_file(f)) != NULL)) {
    ++line;
    if ((*buffer)) {
      if (strchr(buffer, '\n'))
        *(char *) strchr(buffer, '\n') = 0;
      if ((skipline(buffer, &skip)))
        continue;
      if ((strchr(buffer, '<') || strchr(buffer, '>')) && !strstr(buffer, "SALT")) {
        printf(STR(" Failed\n"));
        printf(STR("%s:%d: error: Look at your configuration file again...\n"), cfgfile, line);
//        exit(1);
      }
      p = strchr(buffer, ' ');
      while (p && (strchr(LISTSEPERATORS, p[0])))
        *p++ = 0;
      if (p) {
        if (!egg_strcasecmp(buffer, STR("packname"))) {
          strlcpy(settings.packname, trim(p), sizeof settings.packname);
          printf(".");
        } else if (!egg_strcasecmp(buffer, STR("shellhash"))) {
          strlcpy(settings.shellhash, trim(p), sizeof settings.shellhash);
          printf(".");
        } else if (!egg_strcasecmp(buffer, STR("dccprefix"))) {
          strlcpy(settings.dcc_prefix, trim(p), sizeof settings.dcc_prefix);
          printf(".");
        } else if (!egg_strcasecmp(buffer, STR("owner"))) {
          strlcat(settings.owners, trim(p), sizeof(settings.owners));
          strlcat(settings.owners, ",", sizeof(settings.owners));
          printf(".");
        } else if (!egg_strcasecmp(buffer, STR("owneremail"))) {
          strlcat(settings.owneremail, trim(p), sizeof(settings.owneremail));
          strlcat(settings.owneremail, ",", sizeof(settings.owneremail));
          printf(".");
        } else if (!egg_strcasecmp(buffer, STR("hub"))) {
          strlcat(settings.hubs, trim(p), sizeof(settings.hubs));
          strlcat(settings.hubs, ",", sizeof(settings.hubs));
          printf(".");
        } else if (!egg_strcasecmp(buffer, STR("salt1"))) {
          strlcat(settings.salt1, trim(p), sizeof(settings.salt1));
          printf(".");
        } else if (!egg_strcasecmp(buffer, STR("salt2"))) {
          strlcat(settings.salt2, trim(p), sizeof(settings.salt2));
          printf(".");
        } else {
          printf("%s %s\n", buffer, p);
          printf(",");
        }
      } else { /* SINGLE DIRECTIVES */
        if ((feature = features_find(buffer))) {
          int features = atol(settings.features);
          features |= feature;
          simple_snprintf(settings.features, sizeof(settings.features), "%d", features);
          printf(".");
        }
      }
    }
    buffer = NULL;
  }
  if (f)
    fclose(f);
  if (!settings.salt1[0] || !settings.salt2[0]) {
    /* Write salts back to the cfgfile */
    char salt1[SALT1LEN + 1] = "", salt2[SALT2LEN + 1] = "";

    printf(STR("Creating Salts"));
    if ((f = fopen(cfgfile, "a")) == NULL) {
      printf(STR("Cannot open cfgfile for appending.. aborting\n"));
      exit(1);
    }
    make_rand_str(salt1, SALT1LEN);
    make_rand_str(salt2, SALT2LEN);
    salt1[sizeof salt1 - 1] = salt2[sizeof salt2 - 1] = 0;
    strlcpy(settings.salt1, salt1, sizeof(settings.salt1));
    strlcpy(settings.salt2, salt2, sizeof(settings.salt2));
    fprintf(f, STR("SALT1 %s\n"), salt1);
    fprintf(f, STR("SALT2 %s\n"), salt2);
    fflush(f);
    fclose(f);
  }
  printf(STR(" Success\n"));
  return 1;
}

static void edpack(settings_t *incfg, const char *in_hash, int what)
{
  char *tmp = NULL, *hash = (char *) in_hash, nhash[51] = "";
  unsigned char *(*enc_dec_string)(const char *, unsigned char *, size_t *);
  size_t len = 0;

  if (what == PACK_ENC)
    enc_dec_string = encrypt_binary;
  else
    enc_dec_string = decrypt_binary;

#define dofield(_field) 		do {							\
	if (_field) {										\
		len = sizeof(_field) - 1;							\
		tmp = (char *) enc_dec_string(hash, (unsigned char *) _field, &len);		\
		if (what == PACK_ENC) 								\
		  egg_memcpy(_field, tmp, len);							\
		else 										\
		  simple_snprintf(_field, sizeof(_field), "%s", tmp);				\
		free(tmp);									\
	}											\
} while (0)

//FIXME: Maybe this should be done for EACH dofield(), ie, each entry changes the encryption for next line?
//makes it harder to fuck with, then again, maybe current is fine?
#define dohash(_field)		do {								\
	if (what == PACK_ENC)									\
	  strlcat(nhash, _field, sizeof(nhash));						\
	dofield(_field);									\
	if (what == PACK_DEC)									\
	  strlcat(nhash, _field, sizeof(nhash));						\
} while (0)

#define update_hash()		do {				\
	hash = MD5(nhash);					\
	OPENSSL_cleanse(nhash, sizeof(nhash));			\
	nhash[0] = 0;						\
} while (0)

  /* -- STATIC -- */

  dohash(incfg->hash);
  dohash(incfg->packname);
  update_hash();

  dohash(incfg->shellhash);
  update_hash();

  dofield(incfg->dcc_prefix);
  dofield(incfg->features);
  dofield(incfg->owners);
  dofield(incfg->owneremail);
  dofield(incfg->hubs);

  dohash(incfg->salt1);
  dohash(incfg->salt2);
  update_hash();

  /* -- DYNAMIC -- */
  dofield(incfg->bots);
  dofield(incfg->uid);
  dofield(incfg->autouname);
  dofield(incfg->pscloak);
  dofield(incfg->autocron);
  dofield(incfg->watcher);
  dofield(incfg->uname);
  dofield(incfg->username);
  dofield(incfg->datadir);
  dofield(incfg->homedir);
  dofield(incfg->binpath);
  dofield(incfg->binname);
  dofield(incfg->portmin);
  dofield(incfg->portmax);


  OPENSSL_cleanse(nhash, sizeof(nhash));
#undef dofield
#undef dohash
#undef update_hash
}

#ifdef DEBUG 
static void
tellconfig(settings_t *incfg)
{
#define dofield(_field)		printf("%s: %s\n", #_field, _field);
  // -- STATIC --
  dofield(incfg->hash);
  dofield(incfg->packname);
  dofield(incfg->shellhash);
  dofield(incfg->dcc_prefix);
  dofield(incfg->features);
  dofield(incfg->owners);
  dofield(incfg->owneremail);
  dofield(incfg->hubs);
  dofield(incfg->salt1);
  dofield(incfg->salt2);
  // -- DYNAMIC --
  dofield(incfg->bots);
  dofield(incfg->uid);
  dofield(incfg->autouname);
  dofield(incfg->pscloak);
  dofield(incfg->autocron);
  dofield(incfg->watcher);
  dofield(incfg->uname);
  dofield(incfg->username);
  dofield(incfg->datadir);
  dofield(incfg->homedir);
  dofield(incfg->binpath);
  dofield(incfg->binname);
  dofield(incfg->portmin);
  dofield(incfg->portmax);
#undef dofield
}
#endif

void
check_sum(const char *fname, const char *cfgfile)
{
  if (!settings.hash[0]) {
    if (!cfgfile)
      fatal(STR("Binary not initialized."), 0);

    readcfg(cfgfile);

// tellconfig(&settings); 
    if (bin_checksum(fname, WRITE_CHECKSUM|WRITE_CONF|WRITE_PACK))
      printf(STR("* Wrote settings to binary.\n")); 
    exit(0);
  } else {
    char *hash = bin_checksum(fname, GET_CHECKSUM);

// tellconfig(&settings); 
    edpack(&settings, hash, PACK_DEC);
#ifdef DEBUG
 tellconfig(&settings); 
#endif
    int n = strcmp(settings.hash, hash);
    OPENSSL_cleanse(settings.hash, sizeof(settings.hash));
    OPENSSL_cleanse(hash, strlen(hash));

    if (n) {
      unlink(fname);
      fatal(STR("!! Invalid binary"), 0);
    }
  }
}

static bool check_bin_initialized(const char *fname)
{
  int i = 0;
  size_t len = strlen(shell_escape(fname)) + 3 + 1;
  char *path = (char *) my_calloc(1, len);

  simple_snprintf(path, len, STR("%s -p"), shell_escape(fname));

  i = system(path);
  free(path);
  if (i != -1 && WEXITSTATUS(i) == 4)
    return 1;

  return 0;
}

void write_settings(const char *fname, int die, bool doconf)
{
  char *hash = NULL;
  int bits = WRITE_CHECKSUM;
  /* see if the binary is already initialized or not */
  bool initialized = check_bin_initialized(fname);

  /* only write pack data if the binary is uninitialized
   * otherwise, assume it has similar/correct/updated pack data
   */
  if (!initialized)
    bits |= WRITE_PACK;
  if (doconf)
    bits |= WRITE_CONF;

  /* only bother writing anything if we have pack or doconf, checksum is worthless to write out */
  if (bits & (WRITE_PACK|WRITE_CONF)) {
    if ((hash = bin_checksum(fname, bits))) {
      printf(STR("* Wrote %ssettings to: %s.\n"), ((bits & WRITE_PACK) && !(bits & WRITE_CONF)) ? "pack " :
                                             ((bits & WRITE_CONF) && !(bits & WRITE_PACK)) ? "conf " :
                                             ((bits & WRITE_PACK) && (bits & WRITE_CONF))  ? "pack/conf "  :
                                             "",
                                             fname);
      if (die == -1)			/* only bother decrypting if we aren't about to exit */
        edpack(&settings, hash, PACK_DEC);
    }
  }

  if (die >= 0)
    exit(die);
}

static void 
clear_settings(void)
{
//  egg_memset(&settings.bots, 0, sizeof(settings_t) - SIZE_PACK - PREFIXLEN);
  egg_memset(&settings.bots, 0, SIZE_CONF);
}

void conf_to_bin(conf_t *in, bool move, int die)
{
  conf_bot *bot = NULL;
  char *newbin = NULL;

  clear_settings();
  sdprintf("converting conf to bin\n");
  simple_snprintf(settings.uid, sizeof(settings.uid), "%d", in->uid);
  simple_snprintf(settings.watcher, sizeof(settings.watcher), "%d", in->watcher);
  simple_snprintf(settings.autocron, sizeof(settings.autocron), "%d", in->autocron);
  simple_snprintf(settings.autouname, sizeof(settings.autouname), "%d", in->autouname);
  simple_snprintf(settings.portmin, sizeof(settings.portmin), "%d", in->portmin);
  simple_snprintf(settings.portmax, sizeof(settings.portmax), "%d", in->portmax);
  simple_snprintf(settings.pscloak, sizeof(settings.pscloak), "%d", in->pscloak);

  strlcpy(settings.binname, in->binname, sizeof(settings.binname));
  if (in->username)
    strlcpy(settings.username, in->username, sizeof(settings.username));
  if (in->uname)
    strlcpy(settings.uname, in->uname, sizeof(settings.uname));
  strlcpy(settings.datadir, in->datadir, sizeof(settings.datadir));
  if (in->homedir)
    strlcpy(settings.homedir, in->homedir, sizeof(settings.homedir));
  strlcpy(settings.binpath, in->binpath, sizeof(settings.binpath));
  for (bot = in->bots; bot && bot->nick; bot = bot->next) {
    simple_snprintf(settings.bots, sizeof(settings.bots), STR("%s%s%s %s %s%s %s,"), 
                           settings.bots && settings.bots[0] ? settings.bots : "",
                           bot->disabled ? "/" : "",
                           bot->nick,
                           bot->net.ip ? bot->net.ip : "*", 
                           bot->net.host6 ? "+" : "", 
                           bot->net.host ? bot->net.host : (bot->net.host6 ? bot->net.host6 : "*"),
                           bot->net.ip6 ? bot->net.ip6 : "");
    }

#ifndef CYGWIN_HACKS
  if (move)
    newbin = move_bin(in->binpath, in->binname, 0);
  else
#endif /* !CYGWIN_HACKS */
    newbin = binname;
//  tellconfig(&settings); 
  write_settings(newbin, -1, 1);

  if (die >= 0)
    exit(die);
}

void reload_bin_data() {
  if (bin_checksum(binname, GET_CONF)) {
    putlog(LOG_MISC, "*", STR("Rehashed config data from binary."));

    conf_bot *oldbots = NULL, *oldbot = NULL;
    bool was_localhub = conf.bot->localhub ? 1 : 0;
    
    /* save the old bots list */
    if (conf.bots)
      oldbots = conf_bots_dup(conf.bots);

    /* Save the old conf.bot */
    oldbot = (conf_bot *) my_calloc(1, sizeof(conf_bot));
    conf_bot_dup(oldbot, conf.bot);

    /* free up our current conf struct */
    free_conf();

    /* Fill conf[] with binary data from settings[] */
    bin_to_conf();

    /* fill up conf.bot using origbotname */
    fill_conf_bot(0); /* 0 to avoid exiting if conf.bot cannot be filled */

    if (was_localhub) {
      /* add any new bots not in userfile */
      conf_add_userlist_bots();

       /* deluser removed bots from conf */
      if (oldbots)
        deluser_removed_bots(oldbots, conf.bots);
#ifdef this_is_handled_by_confedit_now
      /* no longer the localhub (or removed), need to alert the new one to rehash (or start it) */
      if (!conf.bot || !conf.bot->localhub) {
        conf_bot *localhub = conf_getlocalhub(conf.bots);

        /* then SIGHUP new localhub or spawn new localhub */
        if (localhub) {
          /* Check for pid again - may be using fork-interval */
          localhub->pid = checkpid(localhub->nick, localhub);
          if (localhub->pid)
            conf_killbot(NULL, localhub, SIGHUP);		//restart the new localhub
          /* else
               start new localhub - done below in spawnbots() */
        }
      }

      /* start/disable new bots as necesary */
      conf_checkpids(conf.bots);
      spawnbots(1);		//1 signifies to not start me!
#endif
    }

    if (conf.bot && conf.bot->disabled) {
      if (tands > 0) {
        botnet_send_chat(-1, conf.bot->nick, STR("Bot disabled in binary."));
        botnet_send_bye(STR("Bot disabled in binary."));
      }

      if (server_online)
        nuke_server(STR("bbl"));

      werr(ERR_BOTDISABLED);
    } else if (!conf.bot) {
      conf.bot = oldbot;

      if (tands > 0) {
        botnet_send_chat(-1, conf.bot->nick, STR("Bot removed from binary."));
        botnet_send_bye(STR("Bot removed from binary."));
      }

      if (server_online)
        nuke_server(STR("it's been good, cya"));

      werr(ERR_BADBOT);
    }

    /* The bot nick changed! (case) */
    if (strcmp(conf.bot->nick, oldbot->nick)) {
      change_handle(conf.bot->u, conf.bot->nick);
//      var_set_by_name(conf.bot->nick, "nick", conf.bot->nick);
//      var_set_userentry(conf.bot->nick, "nick", conf.bot->nick);
    }

    free_bot(oldbot);

    if (oldbots)
      free_conf_bots(oldbots);

    if (!conf.bot->localhub)
      free_conf_bots(conf.bots);
  }
}

