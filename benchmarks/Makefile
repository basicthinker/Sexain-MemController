CC = gcc
CFLAGS = -static -std=c99
OBJ = array_rand.o array_strm.o array_slid.o

all: $(OBJ)

%.o: %.c
	$(CC) -g -o $@ $< $(CFLAGS)
clean:
	rm -rf *.o

