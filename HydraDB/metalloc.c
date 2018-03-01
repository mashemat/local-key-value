//defines
#define BLOCKSIZE 3000000000		//Total space for memory block in bytes.  Dont forget to modify this with memory allocation
#define SMALL_SPACE 5120	//Amount of bytes dedicated for small allocations
#define MAX_SMALL 100		//Max size of small allocations
//includes
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "rdma-primitives.h"



//Structs

//The header for each allocated chunk of memory.
typedef struct MemEntry_t{
	struct MemEntry_t *succ;	//pointer to next block header
	struct MemEntry_t *prev;	//pointer to previous block header
	unsigned int isFree :1;		//1 bit to signal if block is free or not. 1 = true, 0 = false.
	unsigned int size;			//size of allocated block.
} MemEntry;

typedef struct FreeEntry_s{
//	struct FreeEntry_s *succ;	//pointer to next block header
	 MemEntry * addr ;
	unsigned int size;			//size of free block.
} FreeEntry_t;


//Globals
//static char block[BLOCKSIZE];

static MemEntry * smallRoot, * last;
static FreeEntry_t * free_list;
static MemEntry * bigRoot;
static int initialized = 0;
//static int32_t capacity = 0;


//Main code
//Leak detection

//MemEntry * find_free(unsigned int size, int initial){

//   if(initial)
//     return last_initial;
//
//}

int  update_free_list(MemEntry * free_address, int size, int type ){
     int j,i ;	 
	 static int free_flag=0;
	 /*   Finding the free address and update the size*/

    /* Release the merge addresses from the list */		   				
			switch(type) {
			  case 0 :
				 for (j = 0 ; j < 10000 ; j++)
					 if (free_list[j].addr==free_address)
						 free_list[j].size = size;			  			  
			  
				 for (i = 0 ; i < 10000 ; i++)
				    if (free_list[i].addr==free_address->succ){
						free_list[i].addr = NULL;												
					    break;
					}
				 break;
			  case 1 :
			  	  	  			  
				 for (j = 0 ; j < 10000 ; j++)
				    if (free_list[j].addr==free_address->prev){
						free_list[j].size = size;	
						break;
					}
					
				 for (i = 0 ; i < 10000 ; i++)
					 if (free_list[i].addr==free_address){
						 free_list[i].addr = NULL;	
						 break;
					 }						 										
				 break;
/*				 
			  case 2 :
				 for (j = 0 ; j < 10000 ; i++){
				    if (free_list[j].addr==free_address->succ || free_list[j].addr==free_address->prev ){
						free_list[j].addr = NULL;						
						free_flag++;
						if(free_flag==2){
						  free_flag=0;
						  break;
						}
					}
				}
				 break;		*/
				 
		   }
		   
		return 0;


}
 void *  find_free( int size ){
     int i ;	 
	 for (i = 0 ; i < 10000 ; i++)
		 if ( (free_list[i].addr) && (free_list[i].size>=size) ){
//	if(size==15+sizeof(int32_t) +sizeof(uint64_t)){
//		printf("find free:%p,i:%d-size:%d\n",free_list[i].addr,i,size);
//		fflush(stdout);
//	}
		    return(free_list[i].addr);
		 }
	return NULL;
}

int insert_free_list( MemEntry * free_address, int size){
     int i ;	 
	 for (i = 0 ; i < 10000 ; i++){
		 if (!free_list[i].addr){
//			 if(size==33+sizeof(int32_t) +sizeof(uint64_t))
//			 printf("insert free:%p,i:%d-size:%d\n",free_address,i,size);
//			 fflush(stdout);
		    free_list[i].addr = free_address;
		    free_list[i].size = size;
			return 0;
		 }
	 }
	 return -1;
}

int remove_from_free_list( MemEntry * address){
     int i ;	
     for(i=0 ; i < 10000 ; i++){
		if (free_list[i].addr == address){
			free_list[i].addr = NULL;	
			free_list[i].size = 0;
			return 0;
		}
	 }
	return -1;
}


 void leekHarvest(void){	//Cuz it finds leaks.
    
   long leekSize = 0;
   
   MemEntry * ptr = smallRoot;
   while(ptr != NULL){
      if(!(ptr->isFree))
	leekSize += ptr->size;
      ptr = ptr->succ;
    }
   
    ptr = bigRoot;
    while(ptr != NULL){
      if(!(ptr->isFree))
	leekSize += ptr->size;
      ptr = ptr->succ;
    }
    
    fprintf(stderr,"There are %ld bytes of unfreed memory.\n",leekSize);
    if(leekSize == 0)
      fprintf(stderr,"GOOD ON YOU!\n");
    else
      fprintf(stderr,"President Lincoln frowns upon you. FROWNS!!![|>:C\n");
    
    return;
}

//Malloc code
void * metalloc(unsigned int size, char * file, int line){

	MemEntry * p;
	MemEntry * succ;
	
	//Initializes block of memory to use.
	if (!initialized){
		smallRoot = (MemEntry *)dynamic_block;					//Ready small root header.
		smallRoot->prev = NULL;	smallRoot->succ = NULL;//Point predecessor/successor of small root header to NULL.
		smallRoot->size = BLOCKSIZE-sizeof(MemEntry);					//Tell small root header it has size of SMALL_SPACE to work with.
		smallRoot->isFree = 1;							//Tell small root header it is free..
		
	//	bigRoot = (MemEntry *)(dynamic_block+SMALL_SPACE + sizeof(MemEntry));		//Ready big root header.
	//	bigRoot->prev = NULL;	bigRoot->succ = NULL;	//Point predecessor/successor of big root header to NULL.
	//	bigRoot->size = BLOCKSIZE - SMALL_SPACE - 2 * sizeof(MemEntry);	//Tell big root header it has size of \
									(block - area dedicated to small entries - size of big root itself) \
											to work with.
	//	bigRoot->isFree = 1;							//Tell big root header it is free..
		
		//atexit(leekHarvest);
		free_list = ( FreeEntry_t *) malloc ( sizeof(FreeEntry_t) * 10000  );
		memset ( free_list, NULL , 10000 );
		free_list[0].addr = smallRoot;
		free_list[0].size = BLOCKSIZE - sizeof(MemEntry);
		last = smallRoot ;
		//free_last=smallRoot;
		initialized = 1;								//Initialized block of memory.
	}
	
	p = last ;
//	if (size <= MAX_SMALL)		//IF size is of small allocations...
//	 p = (	MemEntry *) find_free( size );
	 //p = last ;
	  /*if (last->size > size)
		p = last;
	else
	    p = (MemEntry *) find_free(size); */
	// p = smallRoot;			//...start at small root.  Modified by Masoud hemmatpour
//	else						//ELSE size is of big allocation...
//		p = bigRoot;			//...start at big root.
	
	//Loop to find a suitable chunk of block to allocate.

//	do {		
//		if (p->isFree == 0)				//current chunk not free. move to next.
			//p = p->succ;
//			printf("Memory is not Free");
//		else if (p->size < size)		//current free chunk is too small to use. move to next.
			//p = p->succ;
//			printf("Memory is not Fit");
//		else if (p->size <= (size + sizeof(MemEntry))){//current free chunk too small to fit allocated chunk + a new header.
//			printf("Memory is FULL");
//			p->isFree = 0;
//			return NULL; //(char *)p + sizeof(MemEntry);		//return whatever we can. modified by masoud hemmatpour
		
//		}
//		else{	//Suitable free chunk found. Prep it.
			succ = (MemEntry *)( (char*)p + sizeof(MemEntry) + size );//Ready the new succeeding header
			succ->prev = p;						//Point previous of new succeeding header to current header
			succ->succ = p->succ;			//Point next of new succeeding header to old succeeding header (may be NULL).
			if(p->succ != NULL)							//IF there is an old succeeding header...
				p->succ->prev = succ;					// ...have its previous point to new succeeding header.
			p->succ = succ;						//Point successor of current pointer to new succeeding header.
			succ->size = p->size - sizeof(MemEntry) - size;		//Tell new succeeding header it has rest of memory block to use.
			succ->isFree = 1;							//Tell new succeeding header it is a free chunk
			p->size = size;									//Tell current header how big its chunk is.
			p->isFree=0;
			if(succ->succ!=NULL){
              printf("last address is %p\n",(char*)succ );
            fflush(stdout);	}
            //capacity = capacity + size;						
							//Tell current header it is not free.
			last = succ ;				
			remove_from_free_list(p); 
			insert_free_list(succ, succ->size);
			
			MemEntry * ptr;
            ptr = smallRoot;
 /*			printf("last is:%p\n", last);
			fflush(stdout);
           while (ptr!=NULL){
				printf("%p:%d\n",ptr,ptr->size);
				fflush(stdout);
				ptr=ptr->succ;
			}				
            printf("Loop finished\n");
			fflush(stdout); */
			
 //           printf("Before returning the addr, this is the prev addr:%p this is the next addr:%p, this is the next_next addr:%p\n",p, p->succ,p->succ->succ);
//			fflush(stdout);
			return (char*)p + sizeof(MemEntry);		//returns the allocated chunk, but not the header of the allocated chunk.
//else		}
//	}while ( p != NULL);
//	fprintf(stderr, "No space available to allocate, returning null pointer.\n\
		Error occurred at line %d of file %s.: size of allocated : %d \n",__LINE__,__FILE__ );
		
//	return NULL;				//No suitable chunk of memory block to allocate. Return null.
}

void freealloc(void * p, char * file, int line){
   
//     MemEntry * new_p;
//	 new_p = (MemEntry *)( (char *) p - sizeof(MemEntry) );
 //    p = (MemEntry *)( (char *) p ); // (char *)
//	 hash_item_t * new_pp;
//	 new_pp = (hash_item_t *)( (new_p)->succ + sizeof(MemEntry) );
//	 p = p->succ;
 //   char * tmp = find_free( sizeof(int32_t) + sizeof(uint64_t) + 15 );
//    printf("Free Memory:%p--free memory next:%p,next_next:%p\n", new_p, new_p->succ, new_p->succ->succ);
//	fflush(stdout);
	
//	exit(1);
	if(initialized == 0){
		fprintf(stderr, "Memory block uninitialized. Nothing to free.\
			Error occurred at line %d of file %s.\n", __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
		return;
	}

	MemEntry * ptr = smallRoot;				//Search in the small set.
	MemEntry * pred, * succ;
	
	while(ptr != NULL){						//Until we hit null pointer...
//	    printf("ptr:%p\n",ptr);
//		fflush(stdout);

 /*       if ( (char *)ptr == (char *)p - sizeof(MemEntry) )
		 break;
		else{
		  pred= ptr;
		  ptr = ptr->succ;
		}*/
		if ( ((char *)ptr) < (char *)p - sizeof(MemEntry) ) { //...If not at the right header...  +sizeof(MemEntry))
			pred= ptr;
			ptr = ptr->succ;				//... ... move to next header		    
		} else if (((char *)ptr) > (char *)p - sizeof(MemEntry) ) {			//... else ...  +sizeof(MemEntry)
			fprintf(stderr, "Space at %p is somewhere on the inside of the block that was originally allocated, or the allocated memory is being freed twice.\n\
				Use the pointer that was originally returned by malloc when the block was first allocated.\n\
				Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
			fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
			exit(1); 
			return;
		} else {
			break;							//... ... Found the right header. break loop.
		}  
	}
//		printf("ptr:%p-ptr->succ:%p,last:%p,last->pred:%p\n",(char *)ptr, ptr->succ,last,last->prev);
//		fflush(stdout);
	
		
//	exit(1); 
/*	
	if(ptr == NULL){						//If we haven't found the right header...
		ptr = bigRoot;						//... Look in the big set
		while(ptr != NULL){					//... ... Until we hit null...
			if (((char *)ptr+sizeof(MemEntry)) < (char *)p) { //...If not at the right header...
				ptr = ptr->succ;				//... ... move to next header
			} else if (((char *)ptr+sizeof(MemEntry)) > (char *)p) {			//... else ...
				fprintf(stderr, "Space at %x is somewhere on the inside of the block that was originally allocated, or the allocated memory is being freed twice.\n	\
					Use the pointer originally returned by malloc when the block was first allocated.\n	\
					Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
				fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
				return;
			} else {
				break;							//... ... Found the right header. break loop.
			}
		}
	}
*/	
/*
  if (ptr != NULL){
	   ptr=smallRoot;
	  while(ptr!=NULL){
		
		if (ptr->succ->succ==NULL ){
		  printf("Not NULL ptr:%p-ptr->succ:%p,succ_succ:%p\n",(char *)ptr, ptr->succ,ptr->succ->succ);
		  fflush(stdout);	     
		break;}
		ptr=ptr->succ;
	  
	  }
  }
*/

	if(ptr == NULL){						//Pointer not found in memory block.
	   ptr=smallRoot;
	  while(ptr!=NULL){
		
		if (ptr->succ->succ==NULL ){
		  printf("NULL ptr:%p-ptr->succ:%p,succ_succ:%p\n",(char *)ptr, ptr->succ,ptr->succ->succ);
		  fflush(stdout);	     
		break;}
		ptr=ptr->succ;
	  
	  }
	
		fprintf(stderr, "%p is not an allocated memory chunk in the memory block.\n	\
			Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
		exit(1);
		return;
	}

	
	if(ptr->isFree){									//If memory at p is already free...
		fprintf(stderr, "Space at %x is already free.\n	\
			Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
    	exit(1);
		return;											//Return.
	}

	pred = ptr->prev;									//Get predecessor of allocated chunk header.

	if(pred != NULL && pred->isFree == 1){				//IF predecessor header exists and has free chunk...
//    printf("Yes!!");
//	fflush(stdout);
	pred->size +=  (sizeof(MemEntry) + ptr->size); 	//...merge current header + chunk w/ predecessor (current header still exists)
		pred->succ == ptr->succ;						//...Point successor of predecessor to current successor
		if (ptr->succ != NULL )							//...IF current successor exists...
			ptr->succ->prev = pred;					//... ... point predecessor of current successor to predecessor
				update_free_list(ptr, pred->size, 1 ); 
	}else{												//ELSE predecessor header does NOT exist...
//    printf("OK!!");
//	fflush(stdout);
     	ptr->isFree = 1;								//... tell current header it is free.
		pred = ptr;		//!!!CURRENT HEADER IS NOW REFERRED TO AS PREDECESSOR, OTHERWISE IS ALREADY MERGED WITH PREDECESSOR!!!
		insert_free_list(ptr,ptr->size);		//!!!CURRENT HEADER IS NOW REFERRED TO AS PREDECESSOR, OTHERWISE IS ALREADY MERGED WITH PREDECESSOR!!!
					//!!!CURRENT HEADER IS NOW REFERRED TO AS PREDECESSOR, OTHERWISE IS ALREADY MERGED WITH PREDECESSOR!!!
	} // one of the above will be executed

/*	succ = ptr->succ;									//Get successor of allocated chunk header

	if(succ != NULL && succ->isFree == 1){				//IF successor exists and is free...
    printf("Yes!!!!!");
	fflush(stdout);
	pred->size += sizeof(MemEntry) + succ->size; 	//... merge successor + its chunk w/ predecessor
		pred->succ = succ->succ;					//... point successor of predecessor to successor of successor
		if (succ->succ != NULL)							//... IF successor of successor exists...
			succ->succ->prev = pred;					//... ... set its predecessor to predecessor.
	  update_free_list(ptr,pred->size,0); // pred is either ptr or ptr->prev
	}  */
	
	return;		//End of function.
	
}

void* metallurgilloc(int quantity, unsigned int size, char * file, int line){
	char * ptr = (char*)metalloc(quantity * size, __FILE__, __LINE__);		//obtains allocated block.
	if(ptr == NULL){
		fprintf(stderr, "Error occurred in call to calloc at line %d of file %s.\n", line, file);
		return NULL;
	}
	memset(ptr,0,size);						//memset to 0 out allocated block.
	return ptr;								//returns the 0'd out allocated block.	
}

void *alchemalloc(void * p, int size, char * file, int line) {
	MemEntry * ptr = smallRoot;				//Search in the small set.
	while(ptr != NULL){						//Until we hit null pointer...
		if (((char *)ptr+sizeof(MemEntry)) < (char *)p) { //...If not at the right header...
			ptr = ptr->succ;				//... ... move to next header
		} else if (((char *)ptr+sizeof(MemEntry)) > (char *)p) {			//... else ...
			fprintf(stderr, "Space at %x is somewhere on the inside of the block that was originally allocated.\n\
				Use the pointer that was originally returned by malloc when the block was first allocated.\n\
				Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
			fprintf(stderr, "Error occured in call to realloc at line %d of file %s.\n", line, file);
			return NULL;  // modified by masoud hemmatpour on 3 May
		} else {
			break;							//... ... Found the right header. break loop.
		}
	}
	
	if(ptr == NULL){						//If we haven't found the right header...
		ptr = bigRoot;						//... Look in the big set
		while(ptr != NULL){					//... ... Until we hit null...
			if (((char *)ptr+sizeof(MemEntry)) < (char *)p) { //...If not at the right header...
				ptr = ptr->succ;				//... ... move to next header
			} else if (((char *)ptr+sizeof(MemEntry)) > (char *)p) {			//... else ...
				fprintf(stderr, "Space at %x is somewhere on the inside of the block that was originally allocated.\n	\
					Use the pointer originally returned by malloc when the block was first allocated.\n	\
					Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
				fprintf(stderr, "Error occured in call to realloc at line %d of file %s.\n", line, file);
				return NULL; // modified by masoud hemmatpour on 3 May
			} else {
				break;							//... ... Found the right header. break loop.
			}
		}
	}
	
	if(ptr == NULL){						//Pointer not found in memory block.
		fprintf(stderr, "%x is not an allocated memory chunk in the memory block.\n	\
			Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
		return NULL;
	}

	freealloc(p, __FILE__, __LINE__);
	char * new = (char*)metalloc(size, __FILE__, __LINE__);		//obtains allocated block.
	if(new == NULL){
		fprintf(stderr, "Error occurred in call to realloc at line %d of file %s.\n", line, file);
		return NULL;
	}
	char * old = (char *)p;
	int i = 0;
	while(i < size) {
		new[i] = old[i];
		i++;
	}
}
