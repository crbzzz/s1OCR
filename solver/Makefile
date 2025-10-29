CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = solver_test
SRCS = main.c solver.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

clean:
	-rm -f $(TARGET) *.o
