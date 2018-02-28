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
#include <time.h>


typedef struct lat { // Latency 
	unsigned long long time;
	unsigned long long padding[7];
} LatencyStruct;

#define barrier()   asm volatile("" : : : "memory")   // Full memory barrier
#define number_of_server 16
#define  NUMBER_OF_RECORD_ITEMS  4000000  
#define RATIO 0.5
#define Key_size 32  
__thread unsigned long next = 1;
pthread_t* thr;
__thread int id; // Thread identification number
int nthreads; // Number of threads
int* ops; // Number of operations per core
long long * cm; // Number of data cache misses per core
int totops; // Total number of operations
int map[32]={0,8,16,24,1,9,17,25,2,10,18,26,3,11,19,27,4,12,20,28,5,13,21,29,6,14,22,30,7,15,23,31}; // Map threads to cores
int port[16]={11211,11212,11213,11214,11215,11216,11217,11218,11219,11220,11221,11223,11224,11225,11226,11227};   // It is equal to the number of server instance 
LatencyStruct* lats; // Latency of each thread
unsigned long long totlats; // Total latency
volatile int execution_status = 1; // Execution status
volatile int start_counting = 0; // Execution status
static pthread_barrier_t init_barr; // Barrier to synchronize thread executions
int testtime = 200;  // Default execution time (seconds) 
int warmup = 100;
__thread int Index=0;
int totops, minops, maxops;
const char *hostname; 
int32_t random_key[NUMBER_OF_RECORD_ITEMS];
struct timeval timeout = { 1, 500000 }; // 1.5 seconds
const int TIMEOUT_IN_MS = 1000; /* ms */
__thread long seed;
struct timespec start, end;
void rand_str(char *dest, size_t length);


void start_count(int sig)
{
  static int iter = 1;
  if (iter){
	start_counting = 1;
	iter=0;
    alarm(testtime-warmup);
  }
  else{
	execution_status = 0; 
  }
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

static __inline__ unsigned long long get_cycle_count(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}



void* my_thread(void* threadid)  // Thread body consists of : init, iterations, finilize
{
	
    long long values[2];
    int events[2] = { PAPI_L1_DCM ,  PAPI_FP_OPS };   // Computing data cache miss in L2	
//    int EventSet = PAPI_NULL;	
//    unsigned long long t;
	cpu_set_t cpuset;
	int iteration;
    struct timeval ;
	id = (int)(int64_t)threadid;

	/* seed for the random number generator */
	simSRandom(id);	
	
  memcached_server_st *servers = NULL;
  memcached_st *memc;
  memcached_return rc;
  char key[9] ;
  char value[Key_size];
  char *retrieved_value;
  size_t value_length;
  uint32_t flags;
  

  
  	CPU_ZERO(&cpuset);
	CPU_SET(map[id%32], &cpuset);	
    /* pin the thread to a core */
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
    {
        fprintf(stderr, "Thread pinning failed!\n");  
        exit(1);
    }    
	
	 seed = id;

      FILE *fp;
      int i,counter=0; 
   
      /* open the file  for different distributions*/ 
      fp = fopen("zip_random_numbers.txt", "r");  // choose the right file to load the appropriate distribution {Zipf,uniform}
      if (fp == NULL) {
         printf("I couldn't open results.dat for reading.\n");
         exit(0);
      }
   
      while (fscanf(fp, "%d,\n", &i) == 1){
		  random_key[counter] = i;		
		  fflush(stdout);
		  counter++;
	  }
	  fclose(fp);
	printf("Key set loaded...\n");
	fflush(stdout);


  memc = memcached_create(NULL);
  servers = memcached_server_list_append(servers, hostname, port[id%number_of_server], &rc); // "10.2.255.241"
  rc = memcached_server_push(memc, servers);

  if (rc == MEMCACHED_SUCCESS)
    fprintf(stderr, "Added server successfully\n");
  else
    fprintf(stderr, "Couldn't add server: %s\n", memcached_strerror(memc, rc));	

	int ret = pthread_barrier_wait(&init_barr);
	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		fprintf(stderr, "Waiting on the barrier failed!\n");
		exit(1);
	}    

	/* Checking for Counter Register support and start the PAPI */

	printf("Start requests\n");
	fflush(stdout);
	  
        int retval = PAPI_register_thread();
	        if ( retval != PAPI_OK ) {
	           fprintf(stderr, "PAPI failed to start counters: %s\n");
	           exit(1);
	        } 
  
  

	if ((PAPI_start_counters(events, 2)) != PAPI_OK) {
	   fprintf(stderr, "PAPI failed to start counters: %s\n");
	   exit(1);
	 } 
	 
    // User iterations ..... common!!! 
    for (iteration = 0; execution_status; )  //  compute throughput and latency
    {

	  if (start_counting){
		  clock_gettime(CLOCK_REALTIME, &start);

//	  t = get_cycle_count();		  
		  iteration++;
	  }
	  
   if(random0(&seed) < RATIO ){  
 
 	sprintf(key,"%d", random_key[Index]);
     retrieved_value = memcached_get(memc, key, strlen(key), &value_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) 
          fprintf(stderr, "Couldn't retrieve key: %s\n", memcached_strerror(memc, rc));
	Index = (Index+1) % NUMBER_OF_RECORD_ITEMS;	  
	  
    }else{
		char random_txt[Key_size];
		int random_size;
		random_size = ( (i) % Key_size );
		if (!random_size) // substitute it with the likely instruction
			random_size++;
		rand_str(random_txt,random_size);
 	    sprintf(key,"%d", random_key[Index] );
	    sprintf(value,"%s", random_txt );		
        rc = memcached_set(memc, key, strlen(key), value, strlen(value), (time_t)0, (uint32_t)0);

       if (rc != MEMCACHED_SUCCESS)
          fprintf(stderr, "Couldn't store key: %s\n", memcached_strerror(memc, rc));
	 
	   Index = (Index+1) % NUMBER_OF_RECORD_ITEMS;    	
	}
	 
  	   if (start_counting){
		   clock_gettime(CLOCK_REALTIME, &end);
		   long long seconds = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec) ;

	   lats[id].time += seconds ; //(get_cycle_count() - t);

		  }
	}

    ops[id] = iteration;
	//  Counting the number of cache misses 
	if ( PAPI_read_counters(values, 2) != PAPI_OK) {
	  fprintf(stderr, "PAPI failed to read counters: %s\n");
	  exit(1);
	}
	
    printf( "Cache miss L1:%lld \t\n" , values[0] ); 

	return NULL;
}



int main(int argc, char **argv) {
	
	
	hostname = (argc > 1) ? argv[1] : "10.2.255.244"; // Infiniband IP Address

	if (argc > 2)
		nthreads = atoi(argv[2]);   // 1: Number of threads
	if (argc > 3)
		testtime = atoi(argv[3]);  // 2: Execution time
	if (argc > 4)
		warmup = atoi(argv[4]);  // 2: warmup time

	signal (SIGALRM,start_count); // Signal to start counting

	// allocating  computational variables
	thr = (pthread_t*) malloc(nthreads * sizeof(pthread_t));  
	ops = (int*) malloc(nthreads * sizeof(int));
	cm = (long long*) malloc(nthreads * sizeof(long long));
	memset(ops, 0, nthreads * sizeof(int));
	lats = (LatencyStruct*) malloc(nthreads * sizeof(LatencyStruct));
	memset(lats, 0, nthreads * sizeof(LatencyStruct));
	
    if(pthread_barrier_init(&init_barr, NULL, nthreads+1))
    {
        printf("Could not create a barrier\n");
        exit(1);
    }


	int retval = PAPI_library_init( PAPI_VER_CURRENT );
	        if ( retval != PAPI_VER_CURRENT ) 	               
	                        printf("fail lib init");
                else
	                        printf("PAPI main Library Started\n");

						
	        retval = PAPI_thread_init( ( unsigned long ( * )( void ) ) ( pthread_self ) );
	        if ( retval != PAPI_OK ) 	               
	                        printf("fail thread init");
                else
	                        printf("PAPI Thread Started\n");	        	

	/* create the threads */
	int i;
    for( i = 0; i < nthreads; i++)
    {
        if(pthread_create(&thr[i], NULL, &my_thread, (void*)(int64_t)i))
        {
            printf("Could not create thread %d\n", i);
            exit(1);
        }
    }

    /* wait to initialize all threads, then set the alarm */
	int ret_thr = pthread_barrier_wait(&init_barr);
	if (ret_thr != 0 && ret_thr != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		fprintf(stderr, "Waiting on the barrier failed!\n");
		exit(1);
	}
	
	
    alarm(warmup);

    /* wait for the threads to finish and count*/  
    totops = 0; minops = 1 << 30; maxops = 0;	
	
    for( i = 0; i < nthreads; i++)
    {
    	int er;
        if(er=pthread_join(thr[i], NULL))
        {
            fprintf(stderr, "Could not join thread %s\n", strerror(er) );
            exit(1);
        } 
        if (ops[i] > maxops) maxops = ops[i];
        if (ops[i] < minops) minops = ops[i];		
        totops += ops[i]; 
        totlats += lats[i].time;		
    }

	
/*throughput*/   printf("Throughput:%d\t\t\n", totops/(testtime-warmup));  
/*latency*/      printf("Latency:%f\t\t\n", (double) (totlats/1000) / totops); // CLOCKS_PER_SEC
/* fairnes */    printf("Fairnes:%f\n", ((float)maxops)/minops);
   fflush(stdout);
   return 0;	
	

}
