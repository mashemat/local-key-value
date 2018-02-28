#include  <stdio.h>
#include "ccrand.h"
#include <math.h>
#include<poll.h>
#include<pmmintrin.h>
#include<time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include "papi.h"

__thread Index=0;
__thread redisContext *c;
__thread redisReply *reply;
const char *hostname = (argc > 1) ? argv[1] : "10.2.255.251"; // Infiniband IP Address
int port = (argc > 2) ? atoi(argv[2]) : 6379;
int32_t random_key[NUMBER_OF_RECORD_ITEMS];
struct timeval timeout = { 1, 500000 }; // 1.5 seconds
const int TIMEOUT_IN_MS = 1000; /* ms */
#define  NUMBER_OF_RECORD_ITEMS    4000000   //  1367264
__thread long seed;
__thread long_long values[2];


void rand_str(char *dest, size_t length);
#define barrier()   asm volatile("" : : : "memory")   // Full memory barrier

int events[2] = { PAPI_L2_DCM ,  PAPI_FP_OPS };   // Computing data cache miss

void global_init(void){
      
}

void thread_init(void){

 seed = id;

      FILE *fp;
      int i,counter=0;
   
      /* open the file  for different distributions*/
      fp = fopen("uniform_random_numbers.txt", "r");  // choose the right file to load the appropriate distribution {Zipf,uniform}
      if (fp == NULL) {
         printf("I couldn't open results.dat for reading.\n");
         exit(0);
      }
   
      while (fscanf(fp, "%d,\n", &i) == 1){
		  random_key[counter] = i ;		
    //      printf("i: %d\n", random_key[counter]);
		  fflush(stdout);
		  counter++;
	  }
	  fclose(fp);
					
	  c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }		
			

}

void user_iteration(int i){
    
    if(random0(&seed)< 1 ){  
			reply = redisCommand(c,"HSET %d field1 %s", random_key[Index], random_txt);
		 //   printf("SET: %s\n", reply->str);
			freeReplyObject(reply);
			Index++;
		}

	}
	else{
		char random_txt[100];
		int random_size;
		random_size = ( (i) % 100 );
		if (!random_size) // substitute it with the likely instruction
			random_size++;
		rand_str(random_txt,random_size);
		reply = redisCommand(c,"HSET %d field1 %s", random_key[i], random_txt);
		 //   printf("SET: %s\n", reply->str);
		freeReplyObject(reply);
		}	
    
}
void thread_cleanup() {
/* Compute and print the number of cache misses in the core */
//    if (id==2){  
//	if ( PAPI_read_counters(values, 2) != PAPI_OK) {
//	  fprintf(stderr, "PAPI failed to read counters: %s\n");
//	  exit(1);
//	}
//  printf( "%lld \t" , values[0]  );
}

void global_cleanup() {

}


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
