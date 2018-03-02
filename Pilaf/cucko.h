// C++ program to demonstrate working of Cuckoo
// hashing.
//#include<bits/stdc++.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "common.h"
#include "crc64.h"
// upper bound on number of elements in our set
//#define MAXN 5
  
// choices for position
//#define ver 3
 
// Auxiliary space bounded by a small multiple
// of MAXN, minimizing wastage
//int hashtable[ver][MAXN];
 
 
// Array to store possible positions for a key

 
/* function to fill hash table with dummy value
 * dummy value: INT_MIN
 * number of hashtables: ver */
void initTable()
{
/*	int j,i;
    for ( j=0; j<MAXN; j++)
        for ( i=0; i<ver; i++)
            hashtable[i][j] = INT_MIN;*/
		memset((fix_cucko_bucket *)hashtable, INT_MIN, MAXN * ver * sizeof(fix_cucko_bucket));
}
 
/* return hashed value for a key
 * function: ID of hash function according to which
    key has to hashed
 * key: item to be hashed */
int32_t hash(int function, int key)
{
    switch (function)
    {
        case 1: return key%MAXN;
		break;
        case 2: return key%MAXN;
		break;
        case 3: return key%MAXN;
    }
	return 0;
}
 
/* function to place a key in one of its possible positions
 * tableID: table in which key has to be placed, also equal
   to function according to which key must be hashed
 * cnt: number of times function has already been called
   in order to place the first input key
 * n: maximum number of times function can be recursively
   called before stopping and declaring presence of cycle */
void place(int64_t key, int64_t value, int tableID, int cnt, int n)
{	
	int pos[ver];
	char item[32] ="abcdefghijklmnopqrstuwxyzaaaaaa";
    /* if function has been recursively called max number
       of times, stop and declare cycle. Rehash. */
    if (cnt==n)
    {
        printf("unpositioned\n");
        printf("Cycle present. REHASH.\n");
        return;
    }
 
    /* calculate and store possible positions for the key.
     * check if key already present at any of the positions.
      If YES, return. */
    int i;
	for ( i=0; i<ver; i++)
    {
        pos[i] = hash(i+1, key);
        if (hashtable[i*(MAXN-1) + pos[i]].key == key)
           return;
    }
 
    /* check if another key is already present at the
       position for the new key in the table
     * If YES: place the new key in its position
     * and place the older key in an alternate position
       for it in the next table */
    if (hashtable[tableID*(MAXN-1)+pos[tableID]].key!=INT_MIN)
    {
        int dis = hashtable[tableID*(MAXN-1)+pos[tableID]].key;
        hashtable[tableID*(MAXN-1)+pos[tableID]].key = key;
		hashtable[tableID*(MAXN-1)+pos[tableID]].offset = value;
		hashtable[tableID*(MAXN-1)+pos[tableID]].in_use = 1;		
		hashtable[tableID*(MAXN-1)+pos[tableID]].hash_bucket = crc64(0,(unsigned const char *)item,32);;
		hashtable[tableID*(MAXN-1)+pos[tableID]].hash_key_value = crc64(0,(unsigned const char *)item,32);;
		hashtable[tableID*(MAXN-1)+pos[tableID]].size = 16;		
        place(dis,value, (tableID+1)%ver, cnt+1, n);
    }
    else //else: place the new key in its position
       hashtable[tableID*(MAXN-1)+pos[tableID]].key = key;
}
 
/* function to print hash table contents */
void printTable()
{
    printf("Final hash tables:\n");
    int i,j;
    for ( i=0; i<ver; i++, printf("\n"))
        for ( j=0; j<MAXN; j++)
            (hashtable[i*(MAXN-1)+j].key==INT_MIN)? printf("- "):
                     printf("%" PRIu64 "\n", hashtable[i*(MAXN-1)+j].key);
 
    printf("\n");
	fflush(stdout);
}

int lookup (int key)
{
	int hash_function=1;
	int index=0;
	for (; hash_function <=ver;  hash_function++ )	{	
	  index=hash(hash_function, key);
	  if ( hashtable[(hash_function-1)*(MAXN-1)+index].key==key )
		  return index;
	}
	return -1;
}
/* function for Cuckoo-hashing keys
 * keys[]: input array of keys
 * n: size of input array */
void cuckoo(int64_t keys[],int64_t values[], int n)
{
    // initialize hash tables to a dummy value (INT-MIN)
    // indicating empty position
    initTable();
 
    // start with placing every key at its position in
    // the first hash table according to first hash
    // function
    int i,cnt=0; 
	for ( i=0, cnt=0; i<n; i++, cnt=0)
        place(keys[i],values[i], 0, cnt, n);
    printf("It is done");
    fflush(stdout);
    //print the final hash tables
    //printTable();
}

