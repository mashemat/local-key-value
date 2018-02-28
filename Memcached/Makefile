CFLAGS  := -O3 -g -Wall -D_GNU_SOURCE -I /home/mhemmatpour/libmemchached/include/libmemcached-1.0 -I /home/mhemmatpour/libmemchached/include/  -L /home/mhemmatpour/libmemchached/lib/  -I /home/mhemmatpour/papi/include -L /home/mhemmatpour/papi/lib 
#-Werror  -Wno-unused-result
LD      := gcc -O3
LDFLAGS := ${LDFLAGS} -lrt -lpthread -lmemcached  -lpapi 

APPS    := memcached.c
INIT    := initial_memcached.c

all: ${APPS}

memcached:  
	${LD} -o $@.run ${LDFLAGS} ${CFLAGS} ${APPS}
	
initial:
	${LD} -o $@.run ${LDFLAGS} ${CFLAGS} ${INIT}

PHONY: clean
clean:
	rm -f *.o 
