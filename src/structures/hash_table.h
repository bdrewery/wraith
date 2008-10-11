#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#include "../bits.h"
#include "ptrlist.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define HASH_TABLE_STRINGS	BIT0
#define HASH_TABLE_RFCSTRINGS	BIT1
#define HASH_TABLE_INTS		BIT2
#define HASH_TABLE_MIXED	BIT3
#define HASH_TABLE_NORESIZE	BIT4

#define DEFAULT_SIZE 		50

/* Turns a key into an unsigned int. */
typedef unsigned int (*hash_table_hash_alg)(const void *key);

/* Returns -1, 0, or 1 if left is <, =, or > than right. */
typedef int (*hash_table_cmp_alg)(const void *left, const void *right);

typedef int (*hash_table_node_func)(const void *key, void *data, void *param);

typedef struct hash_table_entry_b {
	struct hash_table_entry_b *next;
	const void *key;
	void *data;
	unsigned int hash;
} hash_table_entry_t;

typedef struct {
	int len;
	hash_table_entry_t *head;
} hash_table_row_t;

typedef struct hash_table_b {
	int flags;
	int max_rows;
	int cells_in_use;
	hash_table_hash_alg hash;
	hash_table_cmp_alg cmp;
	hash_table_row_t *rows;
} hash_table_t;

hash_table_t *hash_table_create(hash_table_hash_alg alg, hash_table_cmp_alg cmp, int nrows, int flags);
int hash_table_delete(hash_table_t *ht);
int hash_table_check_resize(hash_table_t *ht);
int hash_table_resize(hash_table_t *ht, int nrows);
int hash_table_insert(hash_table_t *ht, const void *key, void *data);
int hash_table_replace(hash_table_t *ht, const void *key, void *data);
int hash_table_find(hash_table_t *ht, const void *key, void *dataptr);
int hash_table_remove(hash_table_t *ht, const void *key, void *dataptr);
int hash_table_walk(hash_table_t *ht, hash_table_node_func callback, void *param);
int hash_table_rename(hash_table_t *ht, const void *key, const void *newkey);

template <class T, int HF> class Htree {
  private:
    hash_table_t *table;
    int my_entries;
  public:
    Htree() : table(hash_table_create(NULL, NULL, DEFAULT_SIZE, HF)), my_entries(0), sort(0) {  }
//    Htree() : table(hash_table_create(NULL, NULL, DEFAULT_SIZE, HASH_TABLE_MIXED)) {  }
//    Htree(int) : table(hash_table_create(NULL, NULL, DEFAULT_SIZE, HASH_TABLE_INTS)) {  }
//    Htree(char *) : table(hash_table_create(NULL, NULL, DEFAULT_SIZE, HASH_TABLE_STRINGS)) {  }
//    Htree(const char *) : table(hash_table_create(NULL, NULL, DEFAULT_SIZE, HASH_TABLE_STRINGS)) {  }
    ~Htree() {
      my_entries = 0;
      walk(&cleanup_data);
      hash_table_delete(table);
    }

    ptrlist<T> list;
    bool sort;

    //ptrlist<T>::link *start() { return list.start; };
    typename ptrlist<T>::link *start() { return list.start(); };

    int entries() { return my_entries; }
    int rename(const void *key, const void *newkey) {
      return hash_table_rename(table, key, newkey);
    }

    static int cleanup_data(T *x, void *data) {
      delete x;
      return 0;
    }
    
    int add(const void *key, T *data) {
      if (randint(2))
        list.add(data);
      else
        list.addLast(data);

      my_entries++;
      return hash_table_insert(table, key, (void *)data);
    }

    int add(T *data) {
      return add(data->GetKey(), data);
    }

    int sortAdd(const void *key, T *data) {
      list.sortAdd(data);
      my_entries++;
      return hash_table_insert(table, key, (void *)data);
    }

    int sortAdd(T *data) {
      return sortAdd(data->GetKey(), data);
    }

    int remove(const void *key, T *data) {
      int ret = hash_table_remove(table, key, NULL);

      if (!ret) {
        list.remove(data);
        my_entries--;
        return ret;
      }
      return ret;
    }

    int remove(T *data) {
      return remove(data->GetKey(), data);
    }

    T *find(const void *key) {
      T *x = NULL;

      if (hash_table_find(table, key, &x))
        x = NULL;

      return x;
    }

    T *find(const char *key) {
      T *x = NULL;

      if (hash_table_find(table, key, &x))
        x = NULL;

      return x;
    }

    T *find(T *d) {
      return find(d->GetKey());
    }

    T &find(T &d) {
      return find(d.GetKey());
    }

    static void _walk(const void *key, void *datap, struct temp_walk *tdata) {
      T *x = *(T **)datap;
      int (*fn)(T *, void *);

      fn = (int(*)(T *, void *)) tdata->fn;
      fn(x, tdata->data);
    }

    void walk(int(*fn)(T *, void *), void *data = NULL) {
      struct temp_walk tdata = { (void *) fn, (void *) data };

      hash_table_walk(table, (int (*)(const void*, void*, void*)) _walk, &tdata);
    }

    static void walk_idx(const void *key, void *data, int idx) {
      T *x = *(T **) data;
      x->dump_idx(idx);
    }

    void dump_idx(int idx) {
      hash_table_walk(table, (int (*)(const void*, void*, void*)) walk_idx, (void *) (long) idx);
    }
};
#endif /* !_HASH_TABLE_H_ */
