#ifndef _BINARY_TREE_H
#define _BINARY_TREE 1

#include <cstdlib>
#include <iostream>

template <class Key, class Value>
class binary_tree {
  private:
    class Node {
      public:
      Node *left;
      Node *right;
      Key key;
      Value value;

      Node(Key k, Value v) {
        key = k;
        value = v;
        left = right = NULL;
      }
     };

     void insertNode(Node *&search, Node *node) {
       if (search == NULL)
         search = node;
       else if (node->key < search->key)
         insertNode(search->left, node);
       else if (node->key > search->key)
         insertNode(search->right, node);
     }
     Node *head;
  public:
    binary_tree() : head(NULL) {};
    virtual ~binary_tree() {};

    void insert(Key key, Value value) {
        Node *node = new Node(key, value);
        insertNode(head, node);
    }

    void print() {
      print(0, head);
    }

    void print(int i, Node *current) {
      if (current == NULL) {
        std::cout << "NULL";
        return;
      }

      std::cout << "Current: " << "key: " << current->key << " value: " << current->value << std::endl;

      for (int n = 0; n <= i; ++n)
        std::cout << " ";
      std::cout << "Left : ";
      print(i+1, current->left);
      std::cout << std::endl;

      for (int n = 0; n <= i; ++n)
        std::cout << " ";
      std::cout << "Right: ";
      print(i+1, current->right);
      std::cout << std::endl;
    }
};

#endif /* !_BINARY_TREE_H */

