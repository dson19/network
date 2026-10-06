# Builds the HW2 domain name / IP address resolver.
#   make        -> builds ./resolver
#   make clean  -> removes build artifacts

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2
SRC_DIR := src
BUILD_DIR := build
TARGET  := resolver

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -lresolv

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
