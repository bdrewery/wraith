/* base64.c: base64 encoding/decoding
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include "base64.h"
#include "src/compat/compat.h"

static const char base64[65] = ".\\0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const char base64r[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0, 0,
  0, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 0, 1, 0, 0, 0,
  0, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
  53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char base64to[256] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 62, 0, 63, 0, 0, 0, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
  49, 50, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


int base64_to_int(char *buf)
{
  int i = 0;

  while (*buf) {
    i = i << 6;
    i += base64to[(int) *buf];
    buf++;
  }
  return i;
}


/* Thank you ircu :) */
static char tobase64array[64] =
{
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '[', ']'
};

char *int_to_base64(unsigned int val)
{
  static char buf_base64[12] = "";

  buf_base64[11] = 0;
  if (!val) {
    buf_base64[10] = 'A';
    return buf_base64 + 10;
  }

  int i = 11;

  while (val) {
    i--;
    buf_base64[i] = tobase64array[val & 0x3f];
    val = val >> 6;
  }

  return buf_base64 + i;
}

#define NUM_ASCII_BYTES 3
#define NUM_ENCODED_BYTES 4

char *
b64enc(const unsigned char *data, size_t *len)
{
  size_t dlen = (((*len + (NUM_ASCII_BYTES - 1)) / NUM_ASCII_BYTES) * NUM_ENCODED_BYTES);
  char *dest = (char *) my_calloc(1, dlen + 1);
  b64enc_buf(data, *len, dest);
  *len = dlen;
  return (dest);
}

/* Encode 3 8-bit bytes to 4 6-bit characters */
void
b64enc_buf(const unsigned char *data, size_t len, char *dest)
{
#define DB(x) ((unsigned char) ((x + i) < len ? data[x + i] : 0))
  register size_t t, i;

  /* 4-byte blocks */
  for (i = 0, t = 0; i < len; i += NUM_ASCII_BYTES, t += NUM_ENCODED_BYTES) {
    dest[t] = base64[DB(0) >> 2];
    dest[t + 1] = base64[((DB(0) & 3) << 4) | (DB(1) >> 4)];
    dest[t + 2] = base64[((DB(1) & 0x0F) << 2) | (DB(2) >> 6)];
    dest[t + 3] = base64[(DB(2) & 0x3F)];
  }
#undef DB
  dest[t] = 0;
}

char *
b64dec(const unsigned char *data, size_t *len)
{
//  size_t dlen = (((*len >> 2) * 3) - 2);
  char *dest = (char *) my_calloc(1, ((*len * 3) >> 2) + 6 + 1);
//  char *dest = (char *) my_calloc(1, dlen + 1);
  b64dec_buf(data, len, dest);
//*len = dlen;
  return dest;
}

/* Decode 4 6-bit characters to 3 8-bit bytes */
void
b64dec_buf(const unsigned char *data, size_t *len, char *dest)
{
#define DB(x) ((unsigned char) (x + i < *len ? base64r[(unsigned char) data[x + i]] : 0))
//#define XB(x) ((unsigned char) (x + i < *len ? (unsigned char) data[x + i] : 0))
  register size_t t, i;
//  const size_t wholeBlocks = (*len / NUM_ENCODED_BYTES);
//  const size_t remainingBytes (*len % NUM_ENCODED_BYTES);
//  size_t maxTotal = (wholeBlocks + (0 != remainingBytes)) * NUM_ASCII_BYTES;

#ifdef NO
  register int pads = 0;
#endif

  for (i = 0, t = 0; i < *len; i += NUM_ENCODED_BYTES, t += NUM_ASCII_BYTES) {

    dest[t] = (DB(0) << 2) + (DB(1) >> 4);
    dest[t + 1] = ((DB(1) & 0x0F) << 4) + (DB(2) >> 2);
    dest[t + 2] = ((DB(2) & 3) << 6) + DB(3);
  }

#ifdef no
i -= NUM_ENCODED_BYTES;
    /* Check for nulls (padding) - the >= check is because binary data might contain VALID NULLS */
    if ((i + NUM_ENCODED_BYTES) >= *len) {
//      if (dest[t] == 0) ++pads;
      if (data[i+2] == '.' && data[i+3] == '.') ++pads;
      if (data[i+3] == '.') ++pads;
//      if (dest[t+1] == 0 && data[t+1] == '.') ++pads;
//      if (dest[t+2] == 0 && data[t+2] == '.') ++pads;
//printf("pads: %d data: %s\n", pads, data);
    }
#endif

#undef DB

/*
printf("i: %d *len: %d\n", i, *len);
if (dest[t] == 0) printf("t:%d %d+%d '%c'\n", t, (XB(0) << 2), (XB(1) >> 4), base64r[(XB(0) << 2)] + base64r[(XB(1) >> 4)]);
else printf("t/%d\n", t);
if (dest[t+1] == 0) printf("t+1:%d %d+%d '%c'\n", t+1, ((XB(1) & 0x0F) << 4), (XB(2) >> 2), base64r[((XB(1) & 0x0F) << 4)] + base64r[(XB(2) >> 2)]);
else printf("t+1/%d\n", t+1);
if (dest[t+2] == 0) printf("t+2:%d %d+%d '%c'\n", t+2, ((XB(2) & 3) << 6), XB(3), base64r[((XB(2) & 3) << 6)] + base64r[XB(3)]);
else printf("t+2/%d\n", t+2);
*/

  t += 3;
  t -= (t % 4);
*len = t;
dest[t] = 0;

//  *len = t - pads;
//  dest[*len] = 0;


//printf("pads: %d\n", pads);
//*len = t;
//if (pads && t == ((t + 3) - ((t+3)%4)))
//printf("*************************************\n****************************************\n****************************************\n****************\a\a\a\a\n");
//printf("t: %d pads: %d told: %d\n", t, pads, (t + 3) - ((t+3)%4));
}
