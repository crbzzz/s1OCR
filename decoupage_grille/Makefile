CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -I./src -I../binary
LDFLAGS := -lm

# --- EXECUTABLES ---
GRID_BIN := grid_splitter
WORDS_BIN := mots_extraction
FIND_BIN := find_words

# --- SOURCES ---
SRC_DIR := src
SRC2_DIR := src2

GRID_SRCS := $(wildcard $(SRC_DIR)/*.c)
GRID_OBJS := $(GRID_SRCS:.c=.o)

WORDS_SRC := $(SRC2_DIR)/mots_extraction.c
FIND_SRC := $(SRC2_DIR)/find_words.c

.PHONY: all clean

all: $(GRID_BIN) $(WORDS_BIN) $(FIND_BIN)

# ---- GRID SPLITTER ----
$(GRID_BIN): $(GRID_OBJS)
	$(CC) $(GRID_OBJS) -o $@ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/grid_splitter.h
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Mots Extraction ----
$(WORDS_BIN): $(WORDS_SRC)
	$(CC) $(CFLAGS) -I$(SRC2_DIR) -o $@ $< $(LDFLAGS)

# ---- Find Words ----
$(FIND_BIN): $(FIND_SRC)
	$(CC) $(CFLAGS) -I$(SRC2_DIR) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(GRID_BIN) $(WORDS_BIN) $(FIND_BIN) $(GRID_OBJS)
