CC = gcc
CFLAGS = -static -std=c99
COBJ = array_rand.o array_strm.o array_slid.o hash_table.o hash_table_fixed.o

CCC = g++
CCFLAGS = -static -std=c++11
CCOBJ = rb_tree.o

all: $(COBJ) $(CCOBJ)

%.o: %.c
	$(CC) -g -o $@ $< $(CFLAGS)

%.o: %.cc
	$(CCC) -g -o $@ $< $(CCFLAGS)

clean:
	rm -rf *.o

