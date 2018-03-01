PHONY: clean

CFLAGS  := -O3 -g -Wall -Werror -Wno-unused-result
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lrt -lpthread

APPS    := main

all: ${APPS}

main: common.o conn.o main.o hopscotch.o
	${LD} -O2 -o $@ $^ ${LDFLAGS}

clean:
	rm -f *.o ${APPS}
