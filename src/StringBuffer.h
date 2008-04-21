/* StringBuffer.h
 *
 */
#ifndef _STRINGBUFFER_H
#define _STRINGBUFFER_H 1

#include <bdlib/src/String.h>
/* Doesn't shrink internal buffer ever */
/**
 * @class StringBuffer
 * @brief A String that doesn't shrink.
 */
class StringBuffer : public bd::String {
  public:
    StringBuffer() : String() {};
    StringBuffer(const String& str) : String(str) {};
    StringBuffer(const StringBuffer& str) : String(str) {};
    StringBuffer(const int size) : String(size) {};

    virtual const StringBuffer& operator = (const char ch) {
      AboutToModify(capacity());
      Ref->len = 0;
      append(ch);
      return *this;
    }

    virtual const StringBuffer& operator = (const char *string) {
      AboutToModify(capacity());
      Ref->len = 0;
      append(string);
      return *this;
    }
  //const StringBuffer& operator = (const StringBuffer& string) {
  //  return *this = String::operator=(string);
 // }
    //friend bool operator == (const String&, const String&);
};
#endif
