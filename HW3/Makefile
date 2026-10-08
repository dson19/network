# Builds the HW3 UDP resolver: the server and the client.
#   make        -> builds ./server and ./client
#   make clean  -> removes build artifacts

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -MMD -MP
BUILD_DIR := build

SERVER_DIR := UDP_Server
CLIENT_DIR := UDP_Client

SERVER_SOURCES := $(wildcard $(SERVER_DIR)/*.c)
CLIENT_SOURCES := $(wildcard $(CLIENT_DIR)/*.c)
SERVER_OBJECTS := $(patsubst $(SERVER_DIR)/%.c,$(BUILD_DIR)/server/%.o,$(SERVER_SOURCES))
CLIENT_OBJECTS := $(patsubst $(CLIENT_DIR)/%.c,$(BUILD_DIR)/client/%.o,$(CLIENT_SOURCES))

.PHONY: all clean

all: server client

server: $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -lresolv

client: $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/server/%.o: $(SERVER_DIR)/%.c | $(BUILD_DIR)/server
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/client/%.o: $(CLIENT_DIR)/%.c | $(BUILD_DIR)/client
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/server $(BUILD_DIR)/client:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) server client

-include $(SERVER_OBJECTS:.o=.d) $(CLIENT_OBJECTS:.o=.d)
