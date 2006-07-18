/*
 * crypt.c -- handles:
 * psybnc crypt() 
 * File encryption
 *
 */


#include "common.h"
#include "crypt.h"
#include "settings.h"
#include "misc.h"
#include "base64.h"
#include "src/crypto/crypto.h"
#include <stdarg.h>
char *encrypt_string(const char *keydata, char *in)
{
  size_t len = 0;
  unsigned char *bdata = NULL;
  char *res = NULL;

  len = strlen(in);
  bdata = encrypt_binary(keydata, (unsigned char *) in, &len);
  if (keydata && *keydata) {
    res = b64enc(bdata, &len);
    free(bdata);
    return res;
  } else {
    return (char *) bdata;
  }
}

char *decrypt_string(const char *keydata, char *in)
{
  size_t len = strlen(in);
  char *buf = NULL, *res = NULL;

  if (keydata && *keydata) {
    buf = b64dec((const unsigned char *) in, &len);
    res = (char *) decrypt_binary(keydata, (unsigned char *) buf, &len);
    free(buf);
    return res;
  } else {
    res = (char *) my_calloc(1, len + 1);
    strcpy(res, in);
    return res;
  }
}

void encrypt_cmd_pass(char *in, char *out)
{
  char *tmp = NULL;

  if (strlen(in) > MAXPASSLEN)
    in[MAXPASSLEN] = 0;
  tmp = encrypt_string(in, in);
  strcpy(out, "+");
  strlcat(out, tmp, MAXPASSLEN + 1);
  out[MAXPASSLEN] = 0;
  free(tmp);
}

static char *user_key(struct userrec *u)
{
  /* FIXME: fix after 1.2.3 */
  return u->handle;
}

char *encrypt_pass(struct userrec *u, char *in)
{
  char *tmp = NULL, buf[101] = "", *ret = NULL;
  size_t ret_size = 0;

  if (strlen(in) > MAXPASSLEN)
    in[MAXPASSLEN] = 0;

  simple_snprintf(buf, sizeof(buf), "%s-%s", settings.salt2, in);

  tmp = encrypt_string(user_key(u), buf);
  ret_size = strlen(tmp) + 1 + 1;
  ret = (char *) my_calloc(1, ret_size);

  simple_snprintf(ret, ret_size, "+%s", tmp);
  free(tmp);

  return ret;
}

char *decrypt_pass(struct userrec *u)
{
  char *tmp = NULL, *p = NULL, *ret = NULL, *pass = NULL;
  
  pass = (char *) get_user(&USERENTRY_PASS, u);
  if (pass && pass[0] == '+') {
    tmp = decrypt_string(user_key(u), &pass[1]);
    if ((p = strchr(tmp, '-')))
      ret = strdup(++p); 
    free(tmp);
  }
  if (!ret)
    ret = (char *) my_calloc(1, 1);

  return ret;
}

/*
static char *passkey()
{
  static char key[SHA1_HASH_LENGTH + 1] = "";

  if (key[0])
    return key;

  char *tmp = my_calloc(1, 512);

  simple_snprintf(tmp, sizeof(tmp), "%s-%s.%s!%s", settings.salt1, settings.salt2, settings.packname, settings.bdhash);
  key = SHA1(tmp);
  free(tmp);
  egg_bzero(tmp, 512);

  return key;
}

void encrypt_pass_new(char *s1, char *s2)
{
  char *tmp = NULL;

  if (strlen(s1) > MAXPASSLEN)
    s1[MAXPASSLEN] = 0;
  tmp = encrypt_string(s1, passkey);
  strcpy(s2, "+");
  strlcat(s2, tmp, MAXPASSLEN + 1);
  s2[MAXPASSLEN] = 0;
  free(tmp);
}
*/
int lfprintf (FILE *stream, const char *format, ...)
{
  va_list va;
  char buf[2048] = "", *ln = NULL, *nln = NULL, *tmp = NULL;
  int res;

  va_start(va, format);
  egg_vsnprintf(buf, sizeof buf, format, va);
  va_end(va);

  ln = buf;
  while (ln && *ln) {
    if ((nln = strchr(ln, '\n')))
      *nln++ = 0;

    tmp = encrypt_string(settings.salt1, ln);
    res = fprintf(stream, "%s\n", tmp);
    free(tmp);
    if (res == EOF)
      return EOF;
    ln = nln;
  }
  return 0;
}

void Encrypt_File(char *infile, char *outfile)
{
  FILE *f = NULL, *f2 = NULL;
  bool std = 0;

  if (!strcmp(outfile, "STDOUT"))
    std = 1;
  f  = fopen(infile, "r");
  if(!f)
    return;
  if (!std) {
    f2 = fopen(outfile, "w");
    if (!f2)
      return;
  } else {
    printf("----------------------------------START----------------------------------\n");
  }

  char *buf = (char *) my_calloc(1, 1024);
  while (fgets(buf, 1024, f) != NULL) {
    remove_crlf(buf);

    if (std)
      printf("%s\n", encrypt_string(settings.salt1, buf));
    else
      lfprintf(f2, "%s\n", buf);
    buf[0] = 0;
  }
  free(buf);
  if (std)
    printf("-----------------------------------END-----------------------------------\n");

  fclose(f);
  if (f2)
    fclose(f2);
}

void Decrypt_File(char *infile, char *outfile)
{
  FILE *f = NULL, *f2 = NULL;
  bool std = 0;

  if (!strcmp(outfile, "STDOUT"))
    std = 1;
  f  = fopen(infile, "r");
  if (!f)
    return;
  if (!std) {
    f2 = fopen(outfile, "w");
    if (!f2)
      return;
  } else {
    printf("----------------------------------START----------------------------------\n");
  }

  char *buf = (char *) my_calloc(1, 2048);
  while (fgets(buf, 2048, f) != NULL) {
    char *temps = NULL;

    remove_crlf(buf);
    temps = (char *) decrypt_string(settings.salt1, buf);
    if (!std)
      fprintf(f2, "%s\n",temps);
    else
      printf("%s\n", temps);
    free(temps);
    buf[0] = 0;
  }
  free(buf);
  if (std)
    printf("-----------------------------------END-----------------------------------\n");

  fclose(f);
  if (f2)
    fclose(f2);
}


char *MD5(const char *string) 
{
  static char	  md5string[MD5_HASH_LENGTH + 1] = "";
  unsigned char   md5out[MD5_HASH_LENGTH + 1] = "";
  MD5_CTX ctx;

  MD5_Init(&ctx);
  MD5_Update(&ctx, string, strlen(string));
  MD5_Final(md5out, &ctx);
  strlcpy(md5string, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(md5string));
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return md5string;
}

char *
MD5FILE(const char *bin)
{
  FILE *f = NULL;

  if (!(f = fopen(bin, "rb")))
    return "";

  static char     md5string[MD5_HASH_LENGTH + 1] = "";
  unsigned char   md5out[MD5_HASH_LENGTH + 1] = "", buffer[1024] = "";
  MD5_CTX ctx;
  size_t binsize = 0, len = 0;

  MD5_Init(&ctx);
  while ((len = fread(buffer, 1, sizeof buffer, f))) {
    binsize += len;
    MD5_Update(&ctx, buffer, len);
  }
  MD5_Final(md5out, &ctx);
  strlcpy(md5string, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(md5string));
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  return md5string;
}

char *SHA1(const char *string)
{
  static char	  sha1string[SHA_HASH_LENGTH + 1] = "";
  unsigned char   sha1out[SHA_HASH_LENGTH + 1] = "";
  SHA_CTX ctx;

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, string, strlen(string));
  SHA1_Final(sha1out, &ctx);
  strlcpy(sha1string, btoh(sha1out, SHA_DIGEST_LENGTH), sizeof(sha1string));
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return sha1string;
}

/* convert binary hashes to hex */
char *btoh(const unsigned char *md, size_t len)
{
  char buf[100] = "", *ret = NULL;

  for (size_t i = 0; i < len; i+=4) {
    sprintf(&(buf[i << 1]), "%02x", md[i]);
    sprintf(&(buf[(i + 1) << 1]), "%02x", md[i + 1]);
    sprintf(&(buf[(i + 2) << 1]), "%02x", md[i + 2]);
    sprintf(&(buf[(i + 3) << 1]), "%02x", md[i + 3]);
  }

  ret = buf;
  return ret;
}
#ifdef k
void do_crypt_console()
{
  char inbuf[1024] = "";
  int which = 5;
  char *p = NULL;

  printf("Crypt menu:\n");
  printf("-----------\n");
  printf("1) String\n");
  printf("2) File\n");

  printf("1) MD5\n");
  printf("2) SHA1\n");
  printf("3) AES256 (binary)\n");
  printf("4) AES256+base64\n");
  printf("5) exit\n");
  printf("\n");
  printf("[5]: ");

  fgets(inbuf, sizeof(inbuf), stdin);
  if ((p = strchr(inbuf, '\n')))
    *p = 0;

  which = atoi(inbuf);

  switch (which) {
    case 
}
#endif
