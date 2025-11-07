CC = gcc
CFLAGS = -g -Wall -Wextra -std=c99
TARGET = simulador_nivel_unico
SOURCE = simulador_nivel_unico.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)