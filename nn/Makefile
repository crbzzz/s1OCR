CC = gcc
CFLAGS = -Wall -Wextra -O2 -I../binary
LDFLAGS = -lm
TARGET = nn_c
SRCS = nn_c.c

OCR_TARGET = ocr_grid
OCR_SRCS = ocr_grid.c

TRAIN_TARGET = train_nn
TRAIN_SRCS = train_nn.c

.PHONY: all run clean

all: $(TARGET) $(OCR_TARGET) $(TRAIN_TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

$(OCR_TARGET): $(OCR_SRCS)
	$(CC) $(CFLAGS) -o $(OCR_TARGET) $(OCR_SRCS) $(LDFLAGS)

$(TRAIN_TARGET): $(TRAIN_SRCS)
	$(CC) $(CFLAGS) -o $(TRAIN_TARGET) $(TRAIN_SRCS) $(LDFLAGS)

run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

clean:
	-rm -f $(TARGET) $(OCR_TARGET) $(TRAIN_TARGET) *.o
