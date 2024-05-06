GCC=gcc
SRC=$(wildcard *.c)
DEP=$(SRC:.c=.o)
FLAGS=-g -Wall -I.

ifneq ($(CROSS_COMPILE),)
        CC=$(addprefix $(CROSS_COMPILE), $(GCC))
else
        CC=$(GCC)
endif


all: $(DEP) writer

%: %.o
	$(CC) $(FLAGS) -o $@ $(DEP) -Wl,-Map=mapfile.map


%.o: %.c %.h
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -f *.o writer *.map
