#ifndef _STREAM_H
#define _STREAM_H 1

#include <iostream>
#include <bdlib/src/String.h>

#define STREAM_BLOCKSIZE	1024
/* The possibilities for the third argument to `fseek'.
   These values should not be changed.  */
#define SEEK_SET        0       /* Seek from beginning of file.  */
#define SEEK_CUR        1       /* Seek from current position.  */
#define SEEK_END        2       /* Seek from end of file.  */

class Stream {
  protected:
        bd::String str;
        unsigned int pos;

  public:
        Stream() : str(), pos(0) {};
        Stream(Stream &stream) : str(stream.str), pos(stream.pos) {};
        Stream(const char* string) : str(string), pos(0) {};
        Stream(const char* string, size_t len) : str(string, len), pos(0) {};
        Stream(const char ch) : str(ch), pos(0) {};
        Stream(const int newSize) : str(), pos(0) { if (newSize > 0) Reserve(newSize); };
        ~Stream() {};

        virtual void printf(const char*, ...);
        virtual void Reserve(const size_t) const;

        /**
         * @brief Returns the position of the Stream.
         * @return Position of the Stream.
        */
        size_t tell() const { return pos; };

        /**
         * @brief Truncates the stream at the current position.
        */
        //FIXME: This is not truncating, but the only use so far is really a clear()
        void truncate() { str.clear(); };

//        operator void*() { return tell() <= length(); };

        int seek(int, int);
        void puts(const bd::String&);
        void puts(const char*, size_t);
        virtual int gets(char *, size_t);
        int loadFile(const char*);

        inline const char* data() const { return str.data(); };
        inline size_t length() const { return str.length(); };
        inline size_t capacity() const { return str.capacity(); };
        inline bool operator ! () const { return str.isEmpty(); };
};
#endif
