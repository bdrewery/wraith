/* EncryptedStream.c
 *
 */
#include <bdlib/src/String.h>
#include "EncryptedStream.h"
#include <stdarg.h>
#include "compat/compat.h"

int EncryptedStream::gets (char *data, size_t maxSize) {
  size_t size = Stream::gets(data, maxSize);
  if (key.length()) {
    bd::String tmp(data, size);
    if (tmp[tmp.length() - 1] == '\n')
      --tmp;
    tmp.base64Decode();
    bd::String decrypted(decrypt_string(key, tmp));
    decrypted += '\n';

    strlcpy(data, decrypted.c_str(), maxSize);
    return decrypted.length();
  }
  return size;
}

void EncryptedStream::printf (const char* format, ...)
{
  char va_out[1024] = "";
  va_list va;
  size_t len = 0;

  va_start(va, format);
  len = vsnprintf(va_out, sizeof(va_out), format, va);
  va_end(va);

  bd::String string(va_out, len);
  if (key.length()) {
    if (string[string.length() - 1] == '\n')
      --string;
    bd::String encrypted(encrypt_string(key, string));
    encrypted.base64Encode();
    encrypted += '\n';
    Stream::puts(encrypted);
    return;
  }
  Stream::puts(string);
}

