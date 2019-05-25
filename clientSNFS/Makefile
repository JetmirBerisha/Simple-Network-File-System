cc = gcc

# CFLAGS
CFLAGS = -g -Wall `pkg-config fuse --cflags --libs` -fno-omit-frame-pointer 

SOURCES = client.c 
OBJECTS = $(SOURCES:.c=.o)
TARGET = clientSNFS

#Magic
$(TARGET)  :$(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean

clean:
	@rm -f $(TARGET) $(OBJECTS) core
