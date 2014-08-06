
# prompt> make
# builds everything and links in test program test_rb
#
# prompt> make mem_check
# Rebuilds everything using dmalloc and does memory testing.
# Only works if you have dmalloc installed (see http://dmalloc.com).

SRCS = rb_tree.c red_black_tree.c stack.c misc.c

HDRS = red_black_tree.h stack.h misc.h

OBJS = red_black_tree.o stack.o rb_tree.o misc.o

CC = gcc

CFLAGS = -g -Wall -Wno-pointer-to-int-cast -static -std=c99

PROGRAM = rb_tree

.PHONY:	mem_check clean

all: $(PROGRAM)

$(PROGRAM): 	$(OBJS)
		$(CC) $(CFLAGS) $(OBJS) -o $(PROGRAM) $(DMALLOC_LIB)

mem_check:	
		@if [ -e makefile.txt ] ; then \
			echo "Using makefile.txt" ; \
			$(MAKE) clean -f makefile.txt ; \
			$(MAKE) $(PROGRAM) "CFLAGS=$(CFLAGS) -DDMALLOC" "DMALLOC_LIB=-ldmalloc" -f makefile.txt ; \
		else \
			echo "Using default makefile (i.e. no -f flag)." ; \
			$(MAKE) clean ; \
			$(MAKE) $(PROGRAM) "CFLAGS=$(CFLAGS) -DDMALLOC" "DMALLOC_LIB=-ldmalloc" ; \
		fi
		./simple_test.sh
		@if [ -s unfreed.txt ] ; then \
			echo " " ; \
			echo "Leaked some memory.  See logfile for details." ;\
		else \
			echo " " ; \
			echo "No memory leaks detected. " ;\
			echo " " ; \
			echo "Test passed. " ; \
			echo " " ; \
		fi


rb_tree.o:	rb_tree.c red_black_tree.c stack.c stack.h red_black_tree.h misc.h

red_black_tree.o:	red_black_tree.h stack.h red_black_tree.c stack.c misc.h misc.c

stack.o:		stack.c stack.h misc.h misc.c

clean:			
	rm -f *.o *~ $(PROGRAM)

