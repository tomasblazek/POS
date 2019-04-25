CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -pedantic -pthread
FILENAME=proj2


all: build

build: $(FILENAME).c
	$(CC) $(CFLAGS) $(FILENAME).c -o $(FILENAME)

run: build
	./$(FILENAME)

zip: 
	zip xblaze31 $(FILENAME).c Makefile
