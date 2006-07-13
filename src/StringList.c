/* StringList.c
 * 
 */

StringList::StringList() :
    list(),
    delimeter(0)
{
}

StringList::~StringList() {
  for (unsigned int i = 0; i < size(); i++)
    delete list[i];
//  delete list;
}

StringList::StringList(const StringList &Slist) :
    list(Slist.list),
    delimeter(0)
{
}
#ifdef broken
StringList& StringList::operator = (const StringList& Slist) {
  //delete list;
  list = Slist.list;

  return *this;
}
#endif

void StringList::append(const String& string) {
  append(string.begin(), string.end());
}

void StringList::append(const char *begin, const char *end) {
  Node *node = new Node;
  node->begin = begin;
  node->end = end;
  list.push_back(node);

//  for (const char *p = begin; p < end; p++)
//    cout << *p;
//  cout << endl;
}

StringList& StringList::operator += (const String& string) {
  append(string);
  return *this;
}

//StringList StringList::operator + (const String&, const String&) {
//}
ostream& operator << (ostream& os, const StringList& Slist) {
  StringList::Node *node = NULL;
  const char *p = NULL;

  for (unsigned int i=0; i < Slist.size(); i++) {
    node = Slist.list[i];
    for (p = node->begin; p < node->end; p++)
      os << *p;
    os << Slist.delimeter;
  }
  return os;
}

