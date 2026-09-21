CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra
OBJS = main.o edit.o build.o fb.o

ccde: $(OBJS)
	$(CC) $(CFLAGS) -o ccde $(OBJS)

clean:
	rm -f ccde $(OBJS)
