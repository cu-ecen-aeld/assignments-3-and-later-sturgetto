# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Target executable name
TARGET = writer

# Source files
SRC = writer.c
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

# Phony targets (not actual files)
.PHONY: clean
