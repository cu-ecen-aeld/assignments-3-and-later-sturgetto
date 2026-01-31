# Makefile for a simple Yocto cross compile example
# NOTE: Do NOT manually set CC, CFLAGS, LDFLAGS within the Makefile 
# unless you intend to override the SDK environment variables.

TARGET = aesdsocket
OBJS = aesdsocket.o
SRC = aesdsocket.c

# The CC, CFLAGS, etc. variables are provided by the sourced SDK environment script.

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

aesdoscket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean:
	rm -f $(OBJS) $(TARGET)
	
all: $(TARGET)

# Phony targets (not actual files)
.PHONY: clean
