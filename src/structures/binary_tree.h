#ifndef _BINARY_TREE_H
#define _BINARY_TREE 1

#include <cstdlib>
#ifdef TESTING
# include <iostream>
#endif /* TESTING */
#include "iterator.h"

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

    int size;
    Node *root;

    void insertNode(Node*& search, Node* node) {
      if (search == NULL) {
        search = node;
        ++size;
      }
      else if (node->key < search->key)
        insertNode(search->left, node);
      else if (node->key > search->key)
        insertNode(search->right, node);
    }

    void deleteNode(Node*& node) {
      if (node->left == NULL) {
        Node* temp = node->right;
        delete node;
        node = temp;
        --size;
      } else if (node->right == NULL) {
        Node* temp = node->left;
        delete node;
        node = temp;
        --size;
      } else {
        //Two children, find max of left subtree and swap
        Node*& temp = node->left;

        while (temp->right != NULL)
         temp = temp->right;

        node->key = temp->key;
        node->value = temp->value;
 
        deleteNode(temp);
      }
    }

    Node*& findNode(Node*& search, Key key) {
/*
      Node*& temp = search;
      
      while (temp != NULL) {
        if (key < temp->key)
          temp = temp->left;
        else if (key > temp->key)
          temp = temp->right;
        else {
          std::cout << "Found key: " << search->key << std::endl;
          return temp;
        }
      }
      return temp;
*/
      if (key < search->key)
        return findNode(search->left, key);
      else if (key > search->key)
        return findNode(search->right, key);
      else {
        return search;
      }
    }

#ifdef TESTING
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
#endif /* TESTING */

  public:
    class IteratorHelper : public Iterator<Value> {
      private:
        int index;
        int size;
        Value *storage;

        void fillArray(const Node *node) {
          static int i = 0;

          if (node == NULL) return;
          fillArray(node->left);
          storage[i++] = node->value;
          fillArray(node->right);
        }
      public:
        IteratorHelper(Node *node, int _size) {
          index = 0;
          size = _size;
          storage = new Value[size];
          fillArray(node);
        }

        virtual ~IteratorHelper() {
          delete[] storage;
        }

        virtual bool hasNext() {
          return (index < size);
        }

        virtual Value next() {
          return storage[index++];
        }
/* not done
        virtual const Value& operator ++() { //prefix 
          Value v = Q.front();
          Q.pop();
          return v;
        }

        virtual const Value operator ++(int) { //postfix 
          
        }
*/
    };

    typedef IteratorHelper iterator;

    iterator begin() {
      return IteratorHelper(root, size);
    }

  public:
    binary_tree() : size(0), root(NULL) {};
    virtual ~binary_tree() {};

    void insert(Key key, Value value) {
      Node *node = new Node(key, value);
      insertNode(root, node);
    }

    void remove(Key key) {
      Node*& node = findNode(root, key);

      if (node != NULL)
        deleteNode(node);
    }

    Value find(Key key) {
      Node*& node = findNode(root, key);
      if (node)
        return node->value;
      return NULL;
    }

#ifdef TESTING
    void print_pre_order() {
      print_pre_order(0, root);
    }

    void print_in_order() {
      print_in_order(0, root);
    }
#endif /* TESTING */

};

#endif /* !_BINARY_TREE_H */

