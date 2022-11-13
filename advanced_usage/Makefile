CC = g++

INCLUDE_DIR = ../include
OBJECT_DIR = objects

_create_object_dir := $(shell mkdir -p $(OBJECT_DIR))

CFLAGS = -I$(INCLUDE_DIR) -Wall -O3
LFLAGS = -lpthread

HEADER_FILES = $(INCLUDE_DIR)/thread_comm.h

default: all

advanced_usage: $(OBJECT_DIR)/advanced_usage.o
	$(CC) -o advanced_usage $(OBJECT_DIR)/advanced_usage.o $(LFLAGS)

all: advanced_usage

$(OBJECT_DIR)/advanced_usage.o:  advanced_usage.cpp $(INCLUDE_DIR)/thread_comm.h
	$(CC) $(CFLAGS) -c advanced_usage.cpp -o $(OBJECT_DIR)/advanced_usage.o

clean:
	rm -rf advanced_usage $(OBJECT_DIR)
