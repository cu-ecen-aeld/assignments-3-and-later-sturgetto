# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Target executable name
TARGET = aesdsocket

# Source files
SRC = aesdsocket.c
OBJ = $(SRC:.c=.o)

# Default rule: build the program
$(TARGET): $(OBJ)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Rule to compile .c to .o
%.o: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJ) $(TARGET)

all: $(TARGET)

# Phony targets (not actual files)
.PHONY: clean
