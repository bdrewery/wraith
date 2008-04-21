#ifndef _STRINGLIST_H
#define _STRINGLIST_H 1

#include <vector>
#include <bdlib/src/String.h>

/**
 * @class StringList Container for a list of String objects (to be easily compared/printed, etc)
 */
class StringList {
  private:
    struct Node {
      const char *begin;
      const char *end;
    };
    std::vector<Node*> list;
    char delimeter;

    StringList& operator = (const StringList&);
  public:
    StringList();
    StringList(const StringList &);
    ~StringList();

    const size_t size() const { return list.size(); };
    void append(const bd::String&);
    void append(const char *, const char *);
    void delim(const char __delim) { delimeter = __delim; };
    char delim() const { return delimeter; };
//    const String& operator [] (int k) const { return list.at(k); };

    StringList& operator += (const String&);

    friend std::ostream& operator << (std::ostream&, const StringList &);
};
//StringList operator + (const String&, const String&);
std::ostream& operator << (std::ostream&, const StringList &);

#endif
