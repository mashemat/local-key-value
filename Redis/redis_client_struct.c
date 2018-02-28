#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "ccrand.h"
#include "papi.h"
#include <sys/times.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <sched.h>
#include <errno.h>
#include <time.h>

typedef struct lat { // Latency 
	 long long time; //unsigned
	 long long padding[7]; // unsigned
} LatencyStruct;

#define barrier()   asm volatile("" : : : "memory")   // Full memory barrier
#define number_of_server 16 
#define  NUMBER_OF_RECORD_ITEMS  4000000   ///number_of_server 
//#define RATIO 01
#define Key_size 32
__thread unsigned long next = 1;
pthread_t* thr;
__thread int id; // Thread identification number
int RATIO;
int nthreads; // Number of threads
int* ops; // Number of operations per core
long long * cm; // Number of data cache misses per core
int totops; // Total number of operations 
//int map[32]={0,8,16,24,1,9,17,25,2,10,18,26,3,11,19,27,4,12,20,28,5,13,21,29,6,14,22,30,7,15,23,31}; // Map threads to cores
int port[16]={6379,6380,6381,6382,6383,6384,6385,6386,6387,6388,6389,6390,6391,6392,6393,6394};   // It is equal to the number of server instance 
LatencyStruct* lats; // Latency of each thread
unsigned long long totlats; // Total latency
volatile int execution_status = 1; // Execution status
volatile int start_counting = 0; // Execution status
static pthread_barrier_t init_barr; // Barrier to synchronize thread executions
int testtime = 50;  // Default execution time (seconds) 
int warmup = 25; // // Default warmup time (seconds)  
__thread struct timespec start, end;
__thread int Index=0;
__thread  redisContext *c; 
__thread  redisReply *reply;
int totops, minops, maxops;
const char *hostname;
int32_t random_key[NUMBER_OF_RECORD_ITEMS];
struct timeval timeout = { 1, 500000 }; // 1.5 seconds
const int TIMEOUT_IN_MS = 1000; /* ms */
__thread long seed;
void rand_str(char *dest, size_t length);


void start_count(int sig)
{
  static int iter = 1;
  if (iter){
	start_counting = 1;
	iter=0;
	printf("here it is:%d",start_counting);
	fflush(stdout);
    alarm(testtime-warmup);
  }
  else{
	  	printf("here it is:0");
	fflush(stdout);
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
	
	
//    long long values[2];
//    int events[2] = { PAPI_L1_DCM ,  PAPI_FP_OPS };   // Computing data cache miss in L2	

	//   int EventSet = PAPI_NULL;	
  //  unsigned long long t;
	cpu_set_t cpuset;
	int iteration;
    struct timeval ;
	id = (int)(int64_t)threadid;
//	CPU_ZERO(&cpuset);
//	CPU_SET(map[id%32], &cpuset);	
	/* seed for the random number generator */
	simSRandom(id);

    /* pin the thread to a core */
//   if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
//   {
//        fprintf(stderr, "Thread pinning failed!\n");  
//        exit(1);
//    }    
	
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
	printf("%d Key set loaded...\n",NUMBER_OF_RECORD_ITEMS);
	fflush(stdout);
    c = redisConnectWithTimeout(hostname, port[id%number_of_server], timeout); 	 
	
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection %d:%d error: %s\n",id,id%number_of_server, c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }		
	printf("Connected\n");
	fflush(stdout);		
    /* wait for the others to initialize */
	
/*	int ret = pthread_barrier_wait(&init_barr);
	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		fprintf(stderr, "Waiting on the barrier failed!\n");
		exit(1);
	}    
*/
	/* Checking for Counter Register support and start the PAPI */

	printf("Start requests\n");
	fflush(stdout);
/*	  
        int retval = PAPI_register_thread();
	        if ( retval != PAPI_OK ) {
	           fprintf(stderr, "PAPI failed to start counters: %s\n");
	           exit(1);
	        } 
  
  

	if ((PAPI_start_counters(events, 2)) != PAPI_OK) {
	   fprintf(stderr, "PAPI failed to start counters: %s\n");
	   exit(1);
	} 
*/
    /* User iterations ..... common!!! */
    for (iteration = 0; execution_status; )  //  compute throughput and latency
    {

	  if (start_counting){
		  clock_gettime(CLOCK_REALTIME, &start);
//		  t = get_cycle_count();		  
		  iteration++;
	  }
	  
   if(random0(&seed) <= RATIO ){  
			reply = redisCommand(c,"HGET %d field1", random_key[Index]);
			freeReplyObject(reply);
			Index = (Index+1) % NUMBER_OF_RECORD_ITEMS;
		}

	
	else{
		char random_txt[Key_size];
		int random_size;
		random_size = ( (i) % Key_size );
		if (!random_size) // substitute it with the likely instruction
			random_size++;
		rand_str(random_txt,random_size);
		reply = redisCommand(c,"HSET %d field1 %s", random_key[Index], random_txt);
		freeReplyObject(reply);
		Index = (Index+1) % NUMBER_OF_RECORD_ITEMS;
		}		     
	  //gettimeofday(&tv2,NULL);
      //unsigned long time_in_micros1 =  1000000 * tv1.tv_sec + tv1.tv_usec;
      //unsigned long time_in_micros2 =  1000000 * tv2.tv_sec + tv2.tv_usec;
      //unsigned long latency = time_in_micros2-time_in_micros1;
   	   if (start_counting){
		   clock_gettime(CLOCK_REALTIME, &end);
		   long long seconds = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec) ;
	       lats[id].time += seconds; //(get_cycle_count() - t);
	   }
	}
	ops[id] = iteration;
	/*  Counting the number of cache misses*/
/*	if ( PAPI_read_counters(values, 2) != PAPI_OK) {
	  fprintf(stderr, "PAPI failed to read counters: %s\n");
	  exit(1);
	}
	
    printf( "Cache miss L1:%lld \t\n" , values[0] ); 
*/

	return NULL;
}

int main(int argc, char** argv)
{

	hostname = (argc > 1) ? argv[1] : "10.2.255.251"; // Infiniband IP Address

	if (argc > 2)
		nthreads = atoi(argv[2]);   // 1: Number of threads
	if (argc > 3)
		testtime = atoi(argv[3]);  // 2: Execution time
	if (argc > 4)
		warmup = atoi(argv[4]);  // 3: warmup time
	if (argc > 5)
		RATIO = atoi(argv[5]);  // 4: Ratio
     
	signal (SIGALRM,start_count); // Signal to start counting

	// allocating  computational variables
	thr = (pthread_t*) malloc(nthreads * sizeof(pthread_t));  
	ops = (int*) malloc(nthreads * sizeof(int));
	cm = (long long*) malloc(nthreads * sizeof(long long));
	memset(ops, 0, nthreads * sizeof(int));
	lats = (LatencyStruct*) malloc(nthreads * sizeof(LatencyStruct));
	memset(lats, 0, nthreads * sizeof(LatencyStruct));
	
 /*   if(pthread_barrier_init(&init_barr, NULL, nthreads+1))
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
*/

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
/*	int ret_thr = pthread_barrier_wait(&init_barr);
	if (ret_thr != 0 && ret_thr != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		fprintf(stderr, "Waiting on the barrier failed!\n");
		exit(1);
	}
*/	
	printf("fddg");
	fflush(stdout);
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

	
/*throughput*/ //  printf("Throughput:%d\t\t\n", totops/(testtime-warmup));  
/*latency*/     // printf("Latency:%f\t\t\n", (double) (totlats/1000) / totops ); // CLOCKS_PER_SEC
/* fairnes */   // printf("Fairnes:%f\n", ((float)maxops)/minops);
 //  fflush(stdout);
   
	
   FILE *out1 = fopen("throughput.txt", "a");  
//   FILE *out2 = fopen("latency.txt", "a");  
   printf("iteration:%d\n",totops);
   fprintf(out1, "%d\n", totops);  
//   fprintf(out2, "%f\n", (double) (testtime-warmup) / iteration);    // we dont use it anymore

   fclose(out1);
//   fclose(out2);	

	printf("Result is printed to the file\n");
	fflush(stdout);	   
   
   
   return 0;
}
