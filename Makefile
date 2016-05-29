CC = gcc
CFLAGS = -Wall -Werror
INCLUDE = include

all: sfmm.o 

debug: CFLAGS += -g
debug: sfmm.o

sfmm.o: sfmm.c 
	$(CC) -I$(INCLUDE) $(CFLAGS) -c $^

.PHONY: clean

clean:
	rm -f *.o
