#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>

#define  NUMBER_OF_RECORD_ITEMS    4000000   //  1367264
#define Key_size 32 
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

    redisContext *c;
    redisReply *reply;
    const char *hostname = (argc > 1) ? argv[1] : "10.2.255.251"; // Infiniband IP Address
    int port = (argc > 2) ? atoi(argv[2]) : 6379;
	int32_t random_key[NUMBER_OF_RECORD_ITEMS];
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    FILE *fp;
    int i,counter=0;  
      /* open the file  for different distributions*/
      fp = fopen("zip_random_numbers.txt", "r");  // choose the right file to load the appropriate distribution {Zipf,uniform}
      if (fp == NULL) {
         printf("I couldn't open results.dat for reading.\n");
         exit(0);
      }
   
      while (fscanf(fp, "%d,\n", &i) == 1){
		  random_key[counter] = i ;		
		  counter++;
	  }
	        fclose(fp);

          printf("Key set Loaded\n");
		  fflush(stdout);




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
 


          printf("Connected");
		  fflush(stdout);

 //char a[100];
// sprintf(a,"%d",random_key[0]);
 // printf("a is %s",a);

    /* Set a key */
   	
	char random_txt[Key_size];
	int random_size;
	for (i=0 ; i < NUMBER_OF_RECORD_ITEMS ; i++ ) {
		random_size = ( (i) % Key_size );
		if (!random_size) // substitute it with the likely instruction
			random_size++;
		rand_str(random_txt,random_size);
		reply = redisCommand(c,"HSET %d field1 %s", /*random_key[i]*/ i, random_txt);
	    printf("SET:%d\n", i);
		fflush(stdout);
		freeReplyObject(reply);
    }
	
    /* Set a key using binary safe API */
//    reply = redisCommand(c,"SET %b %b", "bar", (size_t) 3, "hello", (size_t) 5);
//    printf("SET (binary API): %s\n", reply->str);
//    freeReplyObject(reply);

    /* Try a GET */
//    reply = redisCommand(c,"HGET %d field1",random_key[0]);
//    printf("GET: %s\n", reply->str);
//    freeReplyObject(reply);


    /* Disconnects and frees the context */
    redisFree(c);

    return 0;
}
