CC = gcc
CFLAGS = -Wall -Wextra -O2
PKG_CONFIG ?= pkg-config

TARGET = ocr
SRCS = main.c ocr_window.c ../nn/nn_c.c ../binary/binary_api.c ../solver/solver.c
OBJS = main.o ocr_window.o nn_c.o binary_api.o solver.o

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 glib-2.0 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 glib-2.0 2>/dev/null)
LDFLAGS = -lm $(GTK_LIBS)

INCLUDE_DIRS = -I.. -I../nn -I../binary -I../solver

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.c ocr_window.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

ocr_window.o: ocr_window.c ocr_window.h ../nn/nn_ocr.h ../binary/binary_api.h ../solver/solver.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

nn_c.o: ../nn/nn_c.c ../nn/nn_ocr.h
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

binary_api.o: ../binary/binary_api.c ../binary/binary_api.h
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

solver.o: ../solver/solver.c ../solver/solver.h
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

clean:
	-rm -f $(TARGET) $(OBJS)

