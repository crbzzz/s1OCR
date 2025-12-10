CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm
TARGET = nn_c
SRCS = nn_c.c

OCR_TARGET = ocr_grid
OCR_SRCS = ocr_grid.c

.PHONY: all run clean

all: $(TARGET) $(OCR_TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

$(OCR_TARGET): $(OCR_SRCS)
	$(CC) $(CFLAGS) -o $(OCR_TARGET) $(OCR_SRCS) $(LDFLAGS)

run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

clean:
	-rm -f $(TARGET) $(OCR_TARGET) *.o
