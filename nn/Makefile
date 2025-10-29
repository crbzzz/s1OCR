CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm
TARGET = nn_c
SRCS = nn_c.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

clean:
	-rm -f $(TARGET) *.o
