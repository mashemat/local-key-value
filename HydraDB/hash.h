//#include "types.h"
#include <stdint.h>

/*  Hash table data structures  */

typedef struct hash_item_s {	  
    __int128_t key;
    uint8_t flag;	
	char p[32];  // it was empty 
}  hash_item_t; 
 
typedef struct hash_key_s{
    uint8_t size; // int32_t 
    hash_item_t * addr;
	uint8_t busy;
	uint8_t lease; 
}  hash_key_t; //__attribute__ ((packed))

typedef struct hashtable_s {
    hash_key_t slot[7];
}  hashtable_t; //__attribute__ ((aligned))

typedef struct search_return_s {
   hash_key_t * descriptor;
   hash_item_t *  key_address;
   int ext_enabled;
}  search_return_t;

hash_item_t *buckets;
/* Hash table Primitives */ 

size_t calc_hash( int key);

search_return_t update_hash_key (hashtable_t *h, int key, const char * new_value, int32_t size);

void hashtable_create(hashtable_t *h, size_t size);

void hash_table_initialize(hashtable_t *h, size_t size);

//void hashtable_free(hashtable_t *h);

void hash_table_print(hashtable_t *h, size_t size);

int hashtable_insert(hashtable_t *h, int key, void *p, int size);

search_return_t hashtable_search(hashtable_t *h, int key);

int  find_free_slot(hashtable_t *h);

void hashtable_delete(hashtable_t *h, int key);

void rand_str(char *dest, size_t length);
