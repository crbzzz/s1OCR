CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm
TARGET = binary
SRCS = binary.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

run: $(TARGET)
	@if [ -z "$(IMG)" ]; then \
		echo "Usage: make run IMG=path/to/image"; exit 1; \
	fi
	./$(TARGET) "$(IMG)"

clean:
	-rm -f $(TARGET) *.o


