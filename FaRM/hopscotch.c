/*_
 * Copyright (c) 2016-2017 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "hopscotch.h"
#include <stdlib.h>
#include <string.h>

/*
 * Jenkins Hash Function
 */
static __inline__ int32_t
_jenkins_hash(int32_t key, size_t len)
{
    int32_t hash;
   // size_t i;
   /*
    hash = 0;
    for ( i = 0; i < len; i++ ) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    */
   hash= ( key % (1<<HOPSCOTCH_INIT_BSIZE_EXPONENT) );
   return hash;
}

/*

*/
 /* Release the hash table
 */
void
hopscotch_release(struct hopscotch_hash_table *ht)
{
    free(ht->buckets);
    if ( ht->_allocated ) {
        free(ht);
    }
}
/*
 * Update
 */
int32_t
hopscotch_update(struct hopscotch_hash_table *ht, int32_t key, int32_t nval )
{
    uint32_t h;
    size_t idx;
    size_t i;

    h = _jenkins_hash(key, ht->keylen);
    idx = h ;  

    if ( !ht->buckets[idx].hopinfo ) {
        return 0;
    }
    for ( i = 0; i < HOPSCOTCH_HOPINFO_SIZE; i++ ) {
        if ( ht->buckets[idx].hopinfo & (1 << i) ) {
            if ( key == ht->buckets[idx + i].key ) { 

			/* Found Update*/
			__sync_val_compare_and_swap(&ht->buckets[idx + i].lock, ht->buckets[idx + i].lock, 1);  // atomic CAS
			ht->buckets[idx + i].header_version++;  // Atomic 
            ht->buckets[idx + i].data=nval;
            ht->buckets[idx + i].c_ver++; // Atomic 
            __sync_val_compare_and_swap(&ht->buckets[idx + i].lock, ht->buckets[idx + i].lock, 0);  // atomic CAS

            return 1;
            }
        }
    }

    return 0;
}

/*
 * Lookup
 */
int32_t
hopscotch_lookup(struct hopscotch_hash_table *ht, int32_t key)
{
    uint32_t h;
    size_t idx;
    size_t i;

    h = _jenkins_hash(key, ht->keylen);
    idx = h ;  

    if ( !ht->buckets[idx].hopinfo ) {
        return 0;
    }
    for ( i = 0; i < HOPSCOTCH_HOPINFO_SIZE; i++ ) {
        if ( ht->buckets[idx].hopinfo & (1 << i) ) {
            if ( key == ht->buckets[idx + i].key ) { 
			/* Found */
                return ht->buckets[idx + i].data;
            }
        }
    }

    return 0;
}

/*
 * Insert an entry to the hash table
 */
int
hopscotch_insert(struct hopscotch_hash_table *ht, int32_t key, int32_t data)
{
    uint32_t h;
    size_t idx;
    size_t i;
    size_t sz;
    size_t off;
    size_t j;

    sz =  (1ULL << ht->exponent);
    h = _jenkins_hash(key, ht->keylen);
    idx = h ; //& (sz - 1);

    /* Linear probing to find an empty bucket */
    for ( i = idx; i < sz; i++ ) {
        if ( -1 == ht->buckets[i].key ) {
            /* Found an available bucket */
            while ( i - idx >= HOPSCOTCH_HOPINFO_SIZE ) {
                for ( j = 1; j < HOPSCOTCH_HOPINFO_SIZE; j++ ) {						
                    if ( ht->buckets[i - j].hopinfo ) {
                        off = __builtin_ctz(ht->buckets[i - j].hopinfo);
                        if ( off >= j ) {
                            continue;
                        }
                        ht->buckets[i].key = ht->buckets[i - j + off].key;
                        ht->buckets[i].data = ht->buckets[i - j + off].data;
                        ht->buckets[i].header_version = ht->buckets[i - j + off].header_version  ;
                        ht->buckets[i].lock = ht->buckets[i - j + off].lock  ;
                        ht->buckets[i].c_ver = ht->buckets[i - j + off].c_ver  ;                                              
                        ht->buckets[i - j].hopinfo &= ~(1ULL << off);
                        ht->buckets[i - j].hopinfo |= (1ULL << j);
                        i = i - j + off;
                        break;
                    }
                }
                if ( j >= HOPSCOTCH_HOPINFO_SIZE ) {
                    return -1;
                }
            }

            off = i - idx;
            ht->buckets[i].header_version = 1;
            ht->buckets[i].lock = 0;
            ht->buckets[i].c_ver =  1; 
            ht->buckets[i].key = key;
            ht->buckets[i].data=data;
            ht->buckets[idx].hopinfo |= (1ULL << off);

            return 0;
        }
    }

    return -1;
}

/*
 * Remove an item
 */
int32_t
hopscotch_remove(struct hopscotch_hash_table *ht, int32_t key)
{
    uint32_t h;
    size_t idx;
    size_t i;
    size_t sz;
    int32_t data;

    sz = 1ULL << ht->exponent;
    h = _jenkins_hash(key, ht->keylen);
    idx = h & (sz - 1);

    if ( !ht->buckets[idx].hopinfo ) {
        return 0;
    }
    for ( i = 0; i < HOPSCOTCH_HOPINFO_SIZE; i++ ) {
        if ( ht->buckets[idx].hopinfo & (1 << i) ) {
             if (  key==ht->buckets[idx + i].key ) {  
                /* Found */
                data = ht->buckets[idx + i].data;
                ht->buckets[idx].hopinfo &= ~(1ULL << i);
                ht->buckets[idx + i].key = -1;
                ht->buckets[idx + i].data = -1;
                return data;
            }
        }
    }

    return -1;
}

/*
 * Resize the bucket size of the hash table
 */
int
hopscotch_resize(struct hopscotch_hash_table *ht, int delta)
{
    size_t sz;
    size_t oexp;
    size_t nexp;
    ssize_t i;
    struct hopscotch_bucket *nbuckets;
    struct hopscotch_bucket *obuckets;
    int ret;

    oexp = ht->exponent;
    nexp = ht->exponent + delta;
    sz = 1ULL << nexp;

    nbuckets = malloc(sizeof(struct hopscotch_bucket) * sz);
    if ( NULL == nbuckets ) {
        return -1;
    }
    memset(nbuckets, 0, sizeof(struct hopscotch_bucket) * sz);
    obuckets = ht->buckets;

    ht->buckets = nbuckets;
    ht->exponent = nexp;

    for ( i = 0; i < (1LL << oexp); i++ ) {
        if ( obuckets[i].key ) {
            ret = hopscotch_insert(ht, obuckets[i].key, obuckets[i].data);
            if ( ret < 0 ) {
                ht->buckets = obuckets;
                ht->exponent = oexp;
                free(nbuckets);
                return -1;
            }
        }
    }
    free(obuckets);

    return 0;
}
