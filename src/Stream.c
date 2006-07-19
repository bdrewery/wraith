/* Stream.c
 *
 */
#include "Stream.h"
#include <stdarg.h>
#include <algorithm> // min() / max()

void Stream::Reserve (size_t newSize) {
  if (newSize < capacity())
    return;
  newSize = STREAM_BLOCKSIZE * ((newSize + STREAM_BLOCKSIZE -1) / STREAM_BLOCKSIZE);
  String::Reserve(newSize);
}

int Stream::seek (int offset, int whence) {
  int newpos;

  switch (whence) {
    case SEEK_SET:
      newpos = offset;
      break;
    case SEEK_CUR:
      newpos = tell() + offset;
      break;
    case SEEK_END:
      newpos = capacity() - offset;
      break;
    default:
      newpos = tell();
  }
  if (newpos < 0)
    newpos = 0;
  else if ((unsigned) newpos > capacity())
    newpos = capacity();
  pos = newpos;

  return newpos;
}

void Stream::puts (const String string) {
  puts(string.data(), string.length());
}

void Stream::puts (const char* string, size_t len) {
  replace(tell(), string, len);
  pos += len;
  /* WTF **/
  Ref->size = (capacity() < tell()) ? tell() : capacity();
}

int Stream::gets (char *data, size_t maxSize) {
  size_t toRead, read = 0;
  char c = 0;

  toRead = (maxSize <= (capacity() - tell())) ? maxSize : (capacity() - tell());

  while ((read < toRead) && (c != '\n')) {
    c = Ref->buf[pos++];
    *data++ = c;
    ++read;
  }

  if ( (read < toRead) || (toRead < maxSize))
    *data = 0;

  return read;
}

void Stream::printf (const char* format, ...)
{
  char va_out[1024] = "";
  va_list va;

  va_start(va, format);
  size_t len = vsnprintf(va_out, sizeof(va_out), format, va);
  va_end(va);

  puts(va_out, len);
}

int Stream::loadFile(const char* file)
{
  FILE *f = NULL;
  f = fopen(file, "rb");
  if (f == NULL)
    return 0;

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);
  Reserve(size);

  size_t len = 0;
  char buf[STREAM_BLOCKSIZE + 1];

  while ((len = fread(buf, 1, sizeof(buf) - 1, f)))
    puts(buf, len);

  fclose(f);
  seek(0, SEEK_SET);
  return 1;
}
