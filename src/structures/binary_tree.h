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
     Node *root;

    void print_pre_order(int i, Node *current) {
      if (current == NULL) {
        std::cout << "NULL";
        return;
      }

      std::cout << "Current: " << "key: " << current->key << " value: " << current->value << std::endl;

      for (int n = 0; n <= i; ++n)
        std::cout << " ";
      std::cout << "Left : ";
      print_pre_order(i+1, current->left);
      std::cout << std::endl;

      for (int n = 0; n <= i; ++n)
        std::cout << " ";
      std::cout << "Right: ";
      print_pre_order(i+1, current->right);
      std::cout << std::endl;
    }

    void print_in_order(int i, Node *current) {
      if (current == NULL) {
        std::cout << "NULL";
        return;
      }

      for (int n = 0; n <= i; ++n)
        std::cout << " ";
      std::cout << "Left : ";
      print_in_order(i+1, current->left);
      std::cout << std::endl;

      std::cout << "Current: " << "key: " << current->key << " value: " << current->value << std::endl;

      for (int n = 0; n <= i; ++n)
        std::cout << " ";
      std::cout << "Right: ";
      print_in_order(i+1, current->right);
      std::cout << std::endl;

    }
  public:
    binary_tree() : root(NULL) {};
    virtual ~binary_tree() {};

    void insert(Key key, Value value) {
        Node *node = new Node(key, value);
        insertNode(root, node);
    }

    void remove(Key key, Value value) {
      
    }

    void print_pre_order() {
      print_pre_order(0, root);
    }

    void print_in_order() {
      print_in_order(0, root);
    }

};

#endif /* !_BINARY_TREE_H */

