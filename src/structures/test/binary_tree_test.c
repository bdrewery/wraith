
#define TESTING 1
#include "binary_tree.h"
#include <string>
using namespace std;

int main(int argc, char *argv[])
{
  binary_tree<string, string> tree;
  string key1 = "Key01";
  string value1 = "Value01";

  string key2 = "Key02";
  string value2 = "Value02";

  string key3 = "Key03";
  string value3 = "Value03";

  string key5 = "Key05";
  string value5 = "Value05";

  string key4 = "Key04";
  string value4 = "Value04";

  string key10 = "Key10";
  string value10 = "Value10";


  tree.insert(key1, value1);
  tree.insert(key2, value2);
  tree.insert(key5, value5);
  tree.insert(key4, value4);
  tree.insert(key10, value10);
  tree.insert(key3, value3);


  string val = tree.find("Key05");
  cout << val << endl;
//  cout << tree << endl;

  tree.print_pre_order();
//  tree.print_in_order();

  tree.remove(key5);

//  tree.print_in_order();

  tree.print_pre_order();
}
