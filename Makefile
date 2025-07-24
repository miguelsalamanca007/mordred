# Name of your program
TARGET = mordred
SRC = mordred.c

# Compiler
CC = clang

# Default ncurses flags (updated for Homebrew)
# You can override these by setting NCURSES_PATH manually if needed
NCURSES_PATH := $(shell brew --prefix ncurses 2>/dev/null)

ifeq ($(NCURSES_PATH),)
    # If ncurses not found with brew, fallback to system default
    CFLAGS = -g
    LDFLAGS = -lncurses
else
    CFLAGS = -I$(NCURSES_PATH)/include
    LDFLAGS = -L$(NCURSES_PATH)/lib -lncurses
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
