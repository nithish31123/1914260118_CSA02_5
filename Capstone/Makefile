CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic
TARGET = smart_energy
SOURCE = smart_energy.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	$(RM) $(TARGET) $(TARGET).exe

.PHONY: all clean
