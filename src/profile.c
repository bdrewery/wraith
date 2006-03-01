/* profile.c
 *
 * used for testing/profiling different code
 *
 */

#ifdef DEBUG
#include "common.h"
#include "structures/hash_table.h"
#include <sys/times.h>
#include <signal.h>


double gettime(clock_t start)
{
  clock_t end = times(NULL);
  clock_t diff = end - start;

  double cpu_time_used = (double) ((double) diff / 100.00);
//  double cpu_time_used = ((double) diff / 100);

  return cpu_time_used;
}


static int my_walk(const void *key, void *dataptr, void *param)
{
  char *data = *(char **)dataptr;
  printf("key: %s data: %s\n", (char *) key, data);

  return 0;
}

#define display_results(_start, _name, _iterations) 	do { 	\
	total = gettime(start);					\
	printf("%s: %d iterations; total: %.12fs; avg: %.12fs\n", _name, _iterations, total, total / _iterations); \
} while (0)

void profile(int argc, char **argv)
{
  clock_t p_start = times(NULL);
  hash_table_t *ht = NULL;
  clock_t start;
  double total;

  start = times(NULL);
  ht = hash_table_create(NULL, NULL, 100, HASH_TABLE_STRINGS);
  printf("Hash table created with %d rows\n", ht->max_rows);
  hash_table_insert(ht, "key1", (void *) "data1");
  hash_table_insert(ht, "key2", (void *) "data2");
  hash_table_insert(ht, "key3", (void *) "data3");
  hash_table_insert(ht, "key4", (void *) "data4");
  hash_table_insert(ht, "key5", (void *) "data5");
  hash_table_insert(ht, "key6", (void *) "data6");
  printf("%d%%\n", ht->cells_in_use / ht->max_rows);
  hash_table_walk(ht, my_walk, NULL);

  display_results(start, "hash table", 6);

  sleep(3);

  printf("END: %.12fs\n", gettime(p_start));
  exit(0);
}
#endif /* DEBUG */
