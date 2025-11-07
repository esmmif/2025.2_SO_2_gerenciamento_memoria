CC = gcc
CFLAGS = -g -Wall -Wextra -std=c99
TARGET = simulador_multinivel
SOURCE = simulador_multinivel.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)