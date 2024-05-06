
SRC=$(wildcard *.c)
DEP=$(SRC:.c=.o)
FLAGS= -g -Wall -Werror -I. -lpthread -lrt 

ifeq ($(CROSS_COMPILE),)
	CC=gcc
else
	CC=$(addprefix $(CROSS_COMPILE), gcc)
endif


all: $(DEP)  aesdsocket 

%: %.o
	$(CC) $(FLAG) -o $@ $(DEP) -Wl,-Map=mapfile.map

%.o: %.c %.h
	$(CC) $(FLAG) -c $< -o $@ 

clean:
	rm -rf *.o aesdsocket *.map

