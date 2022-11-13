CC = g++

INCLUDE_DIR = ../include
OBJECT_DIR = objects

_create_object_dir := $(shell mkdir -p $(OBJECT_DIR))

CFLAGS = -I$(INCLUDE_DIR) -Wall -O3
LFLAGS = -lpthread

HEADER_FILES = $(INCLUDE_DIR)/thread_comm.h
SOURCE_FILES = simple_usage_1.cpp simple_usage_2.cpp

default: all

simple_usage_1: $(OBJECT_DIR)/simple_usage_1.o
	$(CC) -o simple_usage_1 $(OBJECT_DIR)/simple_usage_1.o $(LFLAGS)

simple_usage_2: $(OBJECT_DIR)/simple_usage_2.o
	$(CC) -o simple_usage_2 $(OBJECT_DIR)/simple_usage_2.o $(LFLAGS)

all: simple_usage_1 simple_usage_2

$(OBJECT_DIR)/%.o: %.cpp $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf simple_usage_1 simple_usage_2 $(OBJECT_DIR)
