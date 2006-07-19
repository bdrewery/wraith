#ifndef _STREAM_H
#define _STREAM_H 1

#include <iostream>
#include "String.h"

#define STREAM_BLOCKSIZE	1024
/* The possibilities for the third argument to `fseek'.
   These values should not be changed.  */
#define SEEK_SET        0       /* Seek from beginning of file.  */
#define SEEK_CUR        1       /* Seek from current position.  */
#define SEEK_END        2       /* Seek from end of file.  */

class Stream : public String {
  protected:
        unsigned int pos;

  public:
        Stream() : String(), pos(0) {};
        Stream(Stream &stream) : String(stream), pos(stream.pos) {};
        Stream(const char* string) : String(string), pos(0) {};
        Stream(const char* string, size_t length) : String(string, length), pos(0) {};
        Stream(const char ch) : String(ch), pos(0) {};
        Stream(const int newSize) : String(), pos(0) { if (newSize > 0) Reserve(newSize); };

        virtual void printf(const char*, ...);
        virtual void Reserve(size_t);

        /**
         * @brief Returns the position of the Stream.
         * @return Position of the Stream.
        */
        const size_t tell() const { return pos; };

        /**
         * @brief Truncates the stream at the current position.
        */
        void truncate() { Ref->len = pos; };

        int seek(int, int);
        void puts(String);
        virtual int gets(char *, size_t);
};
#endif
