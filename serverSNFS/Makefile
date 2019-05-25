cc = gcc

# CFLAGS
CFLAGS = -g -Wall -lrt -pthread -fno-omit-frame-pointer

SOURCES = server.c 
OBJECTS = $(SOURCES:.c=.o)
TARGET = serverSNFS

#Magic
$(TARGET)  :$(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean

clean:
	@rm -f $(TARGET) $(OBJECTS) core
