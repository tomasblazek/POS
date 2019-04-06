CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -pedantic -pthread
FILENAME=proj1


all: build

build: $(FILENAME).c
	$(CC) $(CFLAGS) $(FILENAME).c -o $(FILENAME)

run: build
	./$(FILENAME) 1024 100

zip: 
	zip xblaze31 $(FILENAME).c Makefile
