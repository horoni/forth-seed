.PHONY = all

CFLAGS := -Wall -pedantic -std=c89 -O0 -g

all: main

main: main.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o main
