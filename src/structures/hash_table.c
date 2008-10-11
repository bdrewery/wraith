/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* hash_table.c: hash table implementation
 *
 */


#include "common.h"
#include "hash_table.h"
#include "../rfc1459.h"

static unsigned int my_string_hash(const void *key);
static unsigned int my_rfcstring_hash(const void *key);
static unsigned int my_int_hash(const void *key);
static unsigned int my_mixed_hash (const void *key);
static int my_int_cmp(const void *left, const void *right);

hash_table_t *hash_table_create(hash_table_hash_alg alg, hash_table_cmp_alg cmp, int nrows, int flags)
{
	hash_table_t *ht = NULL;

	if (nrows <= 0) nrows = 13; /* Give them a small table to start with. */

	ht = (hash_table_t *) my_calloc(1, sizeof(*ht));
	ht->rows = (hash_table_row_t *) my_calloc(nrows, sizeof(*ht->rows));

	if (alg) ht->hash = alg;
	else {
		if (flags & HASH_TABLE_STRINGS) ht->hash = my_string_hash;
		else if (flags & HASH_TABLE_RFCSTRINGS) ht->hash = my_rfcstring_hash;
		else if (flags & HASH_TABLE_INTS) ht->hash = my_int_hash;
		else ht->hash = my_mixed_hash;
	}
	if (cmp) ht->cmp = cmp;
	else {
		if (flags & HASH_TABLE_INTS) ht->cmp = my_int_cmp;
		else if (flags & HASH_TABLE_RFCSTRINGS) ht->cmp = (int (*)(const void*, const void*)) rfc_casecmp;
		else ht->cmp = (hash_table_cmp_alg) strcmp;
	}
	ht->flags = flags;
	ht->max_rows = nrows;
	return(ht);
}

int hash_table_delete(hash_table_t *ht)
{
	hash_table_entry_t *entry = NULL, *next = NULL;
	hash_table_row_t *row = NULL;
	int i;

	if (!ht) return(-1);

	for (i = 0; i < ht->max_rows; i++) {
		row = ht->rows+i;
		for (entry = row->head; entry; entry = next) {
			next = entry->next;
			free(entry);
		}
	}
	free(ht->rows);
	free(ht);

	return(0);
}

int hash_table_check_resize(hash_table_t *ht)
{
	if (!ht) return(-1);

        /* This 100 allows (ht->max_rows) linked lists each with an average of 100 elements in the list
         * before actually resizing.
         * This is done to avoid a very slow and cpu intensive resize which requires recalculating all hashes.
         * Having (ht->max_rows) linked lists is still more effecient than one large linked list.
         */

	if (ht->cells_in_use / ht->max_rows > 100) {
		hash_table_resize(ht, ht->max_rows * 3);
	}
	return(0);
}

int hash_table_resize(hash_table_t *ht, int nrows)
{
	int i, newidx;
	hash_table_row_t *oldrow = NULL, *newrows = NULL;
	hash_table_entry_t *entry = NULL, *next = NULL;

	if (!ht) return(-1);

	/* First allocate the new rows. */
	newrows = (hash_table_row_t *) my_calloc(nrows, sizeof(*newrows));

	/* Now populate it with the old entries. */
	for (i = 0; i < ht->max_rows; i++) {
		oldrow = ht->rows+i;
		for (entry = oldrow->head; entry; entry = next) {
			next = entry->next;
			newidx = entry->hash % nrows;
			entry->next = newrows[newidx].head;
			newrows[newidx].head = entry;
			newrows[newidx].len++;
		}
	}

	free(ht->rows);
	ht->rows = newrows;
	ht->max_rows = nrows;
	return(0);
}

int hash_table_insert(hash_table_t *ht, const void *key, void *data)
{
	unsigned int hash;
	int idx;
	hash_table_entry_t *entry = NULL;
	hash_table_row_t *row = NULL;

	if (!ht) return(-1);

	hash = ht->hash(key);
	idx = hash % ht->max_rows;
	row = ht->rows+idx;

	/* Allocate an entry. */
	entry = (hash_table_entry_t *) my_calloc(1, sizeof(*entry));
	entry->key = key;
	entry->data = data;
	entry->hash = hash;

	/* Insert it into the list. */
	entry->next = row->head;
	row->head = entry;

	/* Update stats. */
	row->len++;
	ht->cells_in_use++;

	/* See if we need to update the table. */
	if (!(ht->flags & HASH_TABLE_NORESIZE)) {
		hash_table_check_resize(ht);
	}

	return(0);
}

int hash_table_rename(hash_table_t *ht, const void *key, const void *newkey)
{
        int idx, newidx;
        unsigned int hash, newhash;
        hash_table_entry_t *entry = NULL, *last = NULL;
        hash_table_row_t *row = NULL, *newrow = NULL;

        if (!ht) return(-1);

        hash = ht->hash(key);
        idx = hash % ht->max_rows;
        row = ht->rows+idx;

        last = NULL;
//sdprintf("R key: %s hash: %d", key, hash);
        for (entry = row->head; entry; entry = entry->next) {
//sdprintf("  loop - key: %s hash: %d", entry->key, entry->hash);
                if (hash == entry->hash && !ht->cmp(key, entry->key)) {
//sdprintf("  FOUND ENTRY: key: %s hash: %d", entry->key, entry->hash);
                        /* Remove it from the row's list. */
                        if (last) last->next = entry->next;
                        else row->head = entry->next;

			row->len--;

			/* create a new hash - get it's new row */
		        newhash = ht->hash(newkey);
		        newidx = newhash % ht->max_rows;
		        newrow = ht->rows+newidx;

			/* Fix the entry */
			entry->hash = newhash;

//sdprintf("  RENAME (new) - key: %s hash: %d", entry->key, entry->hash);
		        /* Insert it into the list. */
		        entry->next = newrow->head;
		        newrow->head = entry;

		        /* Update stats. */
		        newrow->len++;
		        /* See if we need to update the table. */
		        if (!(ht->flags & HASH_TABLE_NORESIZE)) {
		                hash_table_check_resize(ht);
		        }

                        return(1);
                }
                last = entry;
        }


        return(-1);
}

int hash_table_find(hash_table_t *ht, const void *key, void *dataptr)
{
	int idx;
	unsigned int hash;
	hash_table_entry_t *entry = NULL;
	hash_table_row_t *row = NULL;

	if (!ht) return (-1);

	hash = ht->hash(key);
	idx = hash % ht->max_rows;
	row = ht->rows+idx;
//sdprintf("F key: %s hash: %d", key, hash);
	for (entry = row->head; entry; entry = entry->next) {
//sdprintf("  loop - key: %s hash: %d", entry->key, entry->hash);
		if (hash == entry->hash && !ht->cmp(key, entry->key)) {
//sdprintf("  FOUND ENTRY: key: %s hash: %d", entry->key, entry->hash);
			*(void **)dataptr = entry->data;
			return(0);
		}
	}
	return(-1);
}

int hash_table_remove(hash_table_t *ht, const void *key, void *dataptr)
{
	int idx;
	unsigned int hash;
	hash_table_entry_t *entry = NULL, *last = NULL;
	hash_table_row_t *row = NULL;

	if (!ht) return(-1);

	hash = ht->hash(key);
	idx = hash % ht->max_rows;
	row = ht->rows+idx;

	last = NULL;
	for (entry = row->head; entry; entry = entry->next) {
		if (hash == entry->hash && !ht->cmp(key, entry->key)) {
			if (dataptr) *(void **)dataptr = entry->data;

			/* Remove it from the row's list. */
			if (last) last->next = entry->next;
			else row->head = entry->next;

			free(entry);
			ht->cells_in_use--;
			row->len--;
			return(0);
		}
		last = entry;
	}
	return(-1);
}

int hash_table_walk(hash_table_t *ht, hash_table_node_func callback, void *param)
{
	if (!ht) return(-1);

	hash_table_row_t *row = NULL;
	hash_table_entry_t *entry = NULL, *next = NULL;
	int i;

	for (i = 0; i < ht->max_rows; i++) {
		row = ht->rows+i;
		for (entry = row->head; entry;) {
			next = entry->next;
			callback(entry->key, &entry->data, param);
			entry = next;
		}
	}
	return(0);
}

static int my_int_cmp(const void *left, const void *right)
{
	return((long) left - (long) right);
}

static unsigned int my_string_hash(const void *key)
{
	int hash, loop;
        size_t keylen;
	const unsigned char *k = NULL;

#define HASHC hash = *(k++) + 65599 * hash
	hash = 0;
	k = (const unsigned char *) key;
	keylen = strlen((const char *) key);

	if (!keylen) return(0);

	loop = (keylen + 8 - 1) >> 3;
	switch (keylen & (8 - 1)) {
		case 0:
			do {
				HASHC;
		case 7:
				HASHC;
		case 6:
				HASHC;
		case 5:
				HASHC;
		case 4:
				HASHC;
		case 3:
				HASHC;
		case 2:
				HASHC;
		case 1:
				HASHC;
			} while (--loop);
	}
	return(hash);
#undef HASHC
}

static unsigned int my_rfcstring_hash(const void *key)
{
	const unsigned char *k = (const unsigned char*) strdup((const char*) key), *kp = k;
        size_t keylen = strlen((const char*) key);

#define HASHC hash = rfc_toupper(*(k++)) + 65599 * hash

	if (!keylen) return(0);

        int hash = 0, loop = (keylen + 8 - 1) >> 3;
	switch (keylen & (8 - 1)) {
		case 0:
			do {
				HASHC;
		case 7:
				HASHC;
		case 6:
				HASHC;
		case 5:
				HASHC;
		case 4:
				HASHC;
		case 3:
				HASHC;
		case 2:
				HASHC;
		case 1:
				HASHC;
			} while (--loop);
	}
        free((void*)kp);
	return(hash);
#undef HASHC
}

static unsigned int my_int_hash(const void *key)
{
	return((unsigned long)key);
}

static unsigned int my_mixed_hash (const void *key)
{
	const unsigned char *k = NULL;
	unsigned int hash;

        k = (const unsigned char *) key;
	hash = 0;
	while (*k) {
		hash *= 16777619;
		hash ^= *k++;
	}
	return(hash);
}
