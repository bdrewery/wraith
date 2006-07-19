/* EncryptedStream.c
 *
 */
#include "EncryptedStream.h"
#include <stdarg.h>
#include "compat/compat.h"

int EncryptedStream::gets (char *data, size_t maxSize) {
  size_t size = Stream::gets(data, maxSize);
  if (key.length()) {
    String tmp(data, size);
    if (tmp[tmp.length() - 1] == '\n')
      --tmp;
    tmp.base64Decode();
    tmp.decrypt(key);
    tmp += '\n';

    strlcpy(data, tmp.c_str(), maxSize);
    return tmp.length();
  }
  return size;
}

void EncryptedStream::_puts (String string)
{
  if (key.length()) {
    if (string[string.length() - 1] == '\n')
      --string;
    string.encrypt(key);
    string.base64Encode();
    string += '\n';
  }
  Stream::_puts(string);
}

