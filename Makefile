CC      := gcc
TARGET  := waylance
BUILD   := build

SRCS    := $(wildcard src/*.c src/*/*.c protocols/*.c)
OBJS    := $(SRCS:%.c=$(BUILD)/%.o)

CFLAGS  := -Wall -Wextra -Isrc -Iprotocols $(shell pkg-config --cflags wayland-client cairo)
LDFLAGS := $(shell pkg-config --libs wayland-client cairo)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD) $(TARGET)
