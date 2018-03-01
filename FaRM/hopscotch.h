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

#ifndef _HOPSCOTCH_H
#define _HOPSCOTCH_H

#include <stdint.h>
#include <stdlib.h>

/* Initial size of buckets.  2 to the power of this value will be allocated. */
#define HOPSCOTCH_INIT_BSIZE_EXPONENT   22
/* Bitmap size used for linear probing in hopscotch hashing */
#define HOPSCOTCH_HOPINFO_SIZE          6

/*
 * Buckets
 */
typedef struct data_s{
 char value[32];
 uint8_t c_ver;
} data_t;

struct hopscotch_bucket{
    int32_t key;
	uint64_t header_version;
    int32_t data;
    int32_t lock;
	uint8_t c_ver;
	data_t value;
    uint32_t hopinfo;
} __attribute__ ((aligned (8)));

/*
 * Hash table of hopscotch hashing
 */
struct hopscotch_hash_table {
    size_t exponent;
    size_t keylen;
    struct hopscotch_bucket *buckets;
    int _allocated;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* in hopscotch.c */
    struct hopscotch_hash_table *
    hopscotch_init(struct hopscotch_hash_table *, size_t);
    void hopscotch_release(struct hopscotch_hash_table *);
    int32_t hopscotch_lookup(struct hopscotch_hash_table *, int32_t);
    int32_t hopscotch_update(struct hopscotch_hash_table *, int32_t, int32_t);
    int hopscotch_insert(struct hopscotch_hash_table *, int32_t, int32_t);
   int32_t hopscotch_remove(struct hopscotch_hash_table *, int32_t);
    int hopscotch_resize(struct hopscotch_hash_table *, int);

#ifdef __cplusplus
}
#endif


#endif /* _HOPSCOTCH_H */

