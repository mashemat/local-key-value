#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include "metalloc.h"
#include "hash.h"
//#define __STDC_FORMAT_MACROS
#define DEFAULT_LEASE 1
#define MAX_POOL 1000000
#define Key_size 32

static int index_ =0;

void print_progress(size_t count, size_t max)
{
    static size_t prev=0;
    if(( count<max) && (prev!=count)  ){
	const char prefix[] = "Progress: [";
	const char suffix[] = "]";
	const size_t prefix_length = sizeof(prefix) - 1;
	const size_t suffix_length = sizeof(suffix) - 1;
	char *buffer = calloc(max + prefix_length + suffix_length + 1, 1); // +1 for \0
	size_t i = 0;

	strcpy(buffer, prefix);
	for (; i < max; ++i)
	{
		buffer[prefix_length + i] = i < count ? '#' : ' ';
	}

	strcpy(&buffer[prefix_length + i], suffix);
	printf("\b%c[2K\r%s", 27, buffer);
//	printf("\b%c[2K\r%s", 27, buffer);
	fflush(stdout);
	free(buffer);
  }
  prev=count;
}

//#define NUM_SECS 10


void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}


size_t calc_hash(int key) {
    return key % MAX_POOL;
}

int find_free_slot(hashtable_t * h){
   int i;
   for (i=0 ; i < 7 ; ++i ) {
          if (h->slot[i].busy == 0)
             return i;
   }
   return -1;
}


void hashtable_create(hashtable_t *h,size_t size) {
     int i,j;
    
   for (i=0 ; i < size ; i++ ) {
       for (j=0 ; j < 7 ; j++ ) {
          h[i].slot[j].addr = NULL;
          h[i].slot[j].lease = DEFAULT_LEASE; 		  
          h[i].slot[j].busy = 0; 
      }
   }     
}


int hashtable_insert(hashtable_t *h, int key, void *p, int size) { // const char *

    size_t hashv = calc_hash(key);  
    int i;     
    hashtable_t * new ; new = &h[hashv]; 
    if( (i=find_free_slot(new))!= -1 ){
	new->slot[i].addr = &buckets[index_];   // metalloc(sizeof(int32_t)+sizeof(uint64_t)+ size, NULL,1);	  // modify according to the bucket index
    index_++; 
    new->slot[i].size = size;  // Maybe here we should modify
    new->slot[i].lease = DEFAULT_LEASE;
    new->slot[i].busy = 1;
    hash_item_t *item = new->slot[i].addr;
    item->key = key;	
    item->flag = 0;	  	  
    strncpy(item->p,p,size);		  
    return 0;
    }
   return -1;
}
search_return_t hashtable_search(hashtable_t *h, int key) {
	
    size_t hashv = calc_hash(key);
    hash_item_t *iter ;
    hashtable_t * new ;
	search_return_t  ret;
	new = &h[hashv]; 
       int i;
       for(i=0 ; i<7 ; i++){
        iter = new->slot[i].addr;
        if (iter->key == key) {
			 //key_descriptor = &(new->slot[i]);
//			 printf("Search function:%d-%s", iter->key,iter->p);
//           fflush(stdout);
			 ret.key_address = new->slot[i].addr; 
			 ret.descriptor =  &(new->slot[i]);
			 ret.ext_enabled = 0;
//			 if(key==89383){
//			 printf("key:%d",key);
//			 printf("Inside search:%" PRIu64 "\n", ret.descriptor->hash_key_value);
//			 fflush(stdout); 
            return ret;
        } 
		
	   }	
	   			 ret.key_address = NULL; 
			 ret.descriptor =  NULL;
			 ret.ext_enabled = 0;
	//fprintf(stderr, "in hashtable_search, didn't find such key, %d\n", key);   
	return ret;
}

//  Note that scramle should be done firstly 
search_return_t update_hash_key (hashtable_t *h, int key, const char * new_value, int32_t size){
	
	search_return_t  ret;
    ret =  hashtable_search( h, key);			
//	printf("key:%d-value:%s\n", key,(ret.descriptor)->addr->p);	 
//	fflush(stdout);	

	(ret.key_address)->flag=1;  // Set flag of previous data
	
//	freealloc( (ret.key_address) , NULL, 1); // free previous memory address  pass to groomer


    (ret.descriptor)->size = size;
	(ret.descriptor)->addr = &buckets[index_];//metalloc(sizeof(int32_t) +sizeof(uint64_t)+size , NULL,1);
     strncpy((ret.descriptor)->addr->p, new_value, size); // set the value
	(ret.descriptor)->lease=DEFAULT_LEASE;
	(ret.descriptor)->addr->key = key;  // set the key   
 	 ret.key_address = (ret.descriptor)->addr;
	 index_++;
//	printf("New afterr addr:%p\n",(ret.descriptor)->addr  );	 
//	fflush(stdout);	
	return ret;
}

void hashtable_delete(hashtable_t *h, int key) {
    size_t hashv = calc_hash(key);
    hashtable_t * new ; new = &h[hashv];
    int i;
    for ( i=0 ; i <7 ; i++ ){
      if (new->slot[i].addr){
       hash_item_t *item = new->slot[i].addr;
       if (key == item->key) {
         new->slot[i].busy=0;
         item->key = 0;
 //        freealloc(item,NULL,1);
        }
    }
 }
}


void hash_table_initialize(hashtable_t *h,size_t size){
int i,random_size;	
char  random_txt[Key_size];

  size_t  prg, interval, max_secs = 10;
  interval = (size / 10);
  printf("***Initializing the Hash Table***\n");
  fflush(stdout);
  print_progress(0, max_secs);
  for (i=0 ; i < size ; i++ ) {
	random_size = ( (i) % Key_size ); 
	if (!random_size) // substitute it with the likely instruction
		random_size++;
	rand_str(random_txt,random_size);
    if(hashtable_insert(h, i,random_txt ,random_size )){
	  printf("Error inserting into hash table %d\n",i);
	  exit(1);
	}
      prg =  i / interval;
      print_progress(prg, max_secs);
  }
  printf("\n");  
}

void hash_table_print(hashtable_t *h, size_t size){
 printf("Start to Print..\n");
 fflush(stdout);
 search_return_t  ret;

  int i;
 
  for (i=0 ; i < size ; i++ ) {
	   printf("Search key:%d\n",i);
	   fflush(stdout);
	   ret =  hashtable_search(h, i);
       char  a[Key_size]; 
	   memset(a, '\0', Key_size); 
       strncpy( a, (ret.key_address)->p, (ret.descriptor)->size);  
       printf("  a is %s:%d \n", a, (ret.descriptor)->size);  
	   fflush(stdout); 
  }
  
}
