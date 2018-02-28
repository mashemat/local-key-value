#include "/home/mhemmatpour/libmemchached/include/libmemcached-1.0/memcached.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "ccrand.h"
#include "papi.h"
#include <sys/times.h>
#include <sched.h>
#include <errno.h>

#define number_of_server 1
#define  NUMBER_OF_RECORD_ITEMS   4000000   // 4000000 
#define Key_size 32 

int testtime = 20;  // Default execution time (seconds) 
int warmup = 10; // // Default warmup time (seconds)  
__thread int Index=0;
const char *hostname;
int32_t random_key[NUMBER_OF_RECORD_ITEMS];
__thread long seed;
//void rand_str(char *dest, size_t length);

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
 
 
int main(int argc, char **argv) {
	
  memcached_server_st *servers = NULL;
  memcached_st *memc;
  memcached_return rc;
  char key[9] ;
  char value[Key_size] ;
  char *retrieved_value;
  size_t value_length;
  int i;
  uint32_t flags;
  
	hostname = (argc > 1) ? argv[1] : "10.2.255.240"; // Infiniband IP Address


	/* seed for the random number generator */
	 
  memc = memcached_create(NULL);
  servers = memcached_server_list_append(servers, hostname, atoi(argv[2]), &rc); // "10.2.255.241"
  rc = memcached_server_push(memc, servers);

  if (rc == MEMCACHED_SUCCESS)
    fprintf(stderr, "Added server successfully\n"); 
  else
    fprintf(stderr, "Couldn't add server: %s\n", memcached_strerror(memc, rc));	
	
    for ( i=0; i<NUMBER_OF_RECORD_ITEMS; i++){
		char random_txt[Key_size];
		int random_size;
		random_size = ( (i) % Key_size );
		if (!random_size) // substitute it with the likely instruction
			random_size++;
		rand_str(random_txt,random_size);	
		sprintf(key,"%d", i);
		sprintf(value,"%s", random_txt );
        rc = memcached_set(memc, key, strlen(key), value, strlen(value), (time_t)0, (uint32_t)0);

        if (rc == MEMCACHED_SUCCESS)
          fprintf(stderr, "%d Key stored successfully\n", i);
       else
          fprintf(stderr, "Couldn't store key: %s\n", memcached_strerror(memc, rc));
	}
	

	 i=110;
     sprintf(key,"%d", i );
	 value_length=(i) % Key_size ;
	 retrieved_value = memcached_get(memc, key, strlen(key), &value_length, &flags, &rc);
     printf("Yay!\n");

        if (rc == MEMCACHED_SUCCESS) {
          fprintf(stderr, "Key retrieved successfully\n");
          printf("The key '%s' returned value '%s'.\n", key, retrieved_value);
          free(retrieved_value);
    }else fprintf(stderr, "Couldn't retrieve key: %s\n", memcached_strerror(memc, rc));
	
	
	
   return 0;	
	
}
