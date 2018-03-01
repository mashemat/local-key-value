void * metalloc(unsigned int size, char * file, int line);

void * metallurgilloc(unsigned int quantity, unsigned int size, char * file, int line);

void tentative_freealloc(void * p, char * file, int line);

void freealloc(void * p, char * file, int line);

void *  find_free( int size );

int insert_free_list( void * free_address, int size);

int remove_from_free_list( void * address);
