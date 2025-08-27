# Name of your program
TARGET = mordred
SRC = mordred.c

# Compiler
CC = clang

CFLAGS = -g
LDFLAGS = -lncurses

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

debug: $(SRC)
	$(CC) $(CFLAGS) -DDEBUG $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
