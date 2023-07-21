CC = gcc
CFLAGS = -Wall -g -pthread -g -pedantic -std=gnu17 -Wextra
LIBS=-lm

TARGET=nyuenc

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LIBS)


clean:
	$(RM) $(TARGET)
