#ifndef _ENCRYPTEDSTREAM_H
#define _ENCRYPTEDSTREAM_H 1

#include <iostream>
#include "Stream.h"

class EncryptedStream : public Stream {
  private:
        String key;

  protected:
        virtual void _puts(String);

  public:
        EncryptedStream(const char* keyStr) : Stream(), key(String(keyStr)) {};
        EncryptedStream(String& keyStr) : Stream(), key(keyStr) {};
        EncryptedStream(EncryptedStream& stream) : Stream(stream), key(stream.key) {};

        virtual int gets(char *, size_t);
};
#endif
