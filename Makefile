CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c99
LIBS = -lm -pthread

TARGET = ring_network

SOURCES = main.c common.c crc32.c queue.c packet.c network_ops.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "Compilação concluída: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Arquivos de compilação removidos"

run: $(TARGET)
	./$(TARGET) config.txt

debug: CFLAGS += -g -DDEBUG
debug: clean $(TARGET)
	gdb ./$(TARGET)

.PHONY: all clean run debug
