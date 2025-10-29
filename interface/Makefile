CC = gcc
CFLAGS = -Wall -Wextra -O2
PKG_CONFIG ?= pkg-config

TARGET = interface
SRCS = main.c ocr_window.c
OBJS = $(SRCS:.c=.o)

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 2>/dev/null)
LDFLAGS = -lm $(GTK_LIBS)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

clean:
	-rm -f $(TARGET) $(OBJS)

