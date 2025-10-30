CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -I./src -I../binary
LDFLAGS := -lm

BINS := grid_splitter mots_extraction
BIN := grid_splitter
MOTS_BIN := mots_extraction
MOTS_SRC := src2/mots_extraction.c
SRC_DIR := src
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all mots clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/grid_splitter.h
	$(CC) $(CFLAGS) -c $< -o $@

mots: $(MOTS_BIN)

$(MOTS_BIN): $(MOTS_SRC)
	$(CC) $(CFLAGS) -I./src2 -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) $(OBJS)
