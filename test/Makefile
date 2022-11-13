CC = g++

INCLUDE_DIR = ../include
OBJECT_DIR = objects

_create_object_dir := $(shell mkdir -p $(OBJECT_DIR))

CFLAGS = -I$(INCLUDE_DIR) -Wall -O3
LFLAGS = -lgtest -lgtest_main

HEADER_FILES = $(INCLUDE_DIR)/thread_comm.h

default: all

tests: $(OBJECT_DIR)/tests.o
	$(CC) -o tests $(OBJECT_DIR)/tests.o $(LFLAGS)

all: tests

$(OBJECT_DIR)/tests.o:  tests.cpp $(INCLUDE_DIR)/thread_comm.h
	$(CC) $(CFLAGS) -c tests.cpp -o $(OBJECT_DIR)/tests.o

clean:
	rm -rf tests $(OBJECT_DIR)
