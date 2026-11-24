# Build: make           -> ./server
#        make test      -> unit test + integration test
#        make clean
#
# Bắt buộc -std=c11 vì bài được chấm bằng GCC 9.4 trên Ubuntu 20.04.
# -D_GNU_SOURCE: dưới -std=c11 glibc ẩn accept4, strdup, strcasestr...

CC      ?= gcc
CFLAGS  ?= -O2          # cờ tùy chọn, ví dụ: make CFLAGS="-O0 -g -fsanitize=address"
LDFLAGS ?=
BASE_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -D_GNU_SOURCE -Isrc/common -Isrc/TCP_Server -MMD -MP
LDLIBS  = -lsqlite3 -lcrypto -lpthread

BUILD   = build

COMMON_SRC = src/common/cJSON.c src/common/protocol.c src/common/linebuf.c
SERVER_SRC = src/TCP_Server/main.c src/TCP_Server/config.c src/TCP_Server/cache.c src/TCP_Server/auth.c src/TCP_Server/storage.c \
             src/TCP_Server/pool.c src/TCP_Server/log.c src/TCP_Server/loop.c src/TCP_Server/dispatch.c src/TCP_Server/session.c

COMMON_OBJ = $(COMMON_SRC:%.c=$(BUILD)/%.o)
SERVER_OBJ = $(SERVER_SRC:%.c=$(BUILD)/%.o)

.PHONY: all clean test test-unit test-integration

all: server

server: $(SERVER_OBJ) $(COMMON_OBJ)
	$(CC) $(BASE_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -c -o $@ $<

# cJSON là thư viện ngoài: không bật -Wpedantic cho nó.
$(BUILD)/src/common/cJSON.o: src/common/cJSON.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CFLAGS) -Isrc/common -c -o $@ $<

$(BUILD)/test_linebuf: tests/test_linebuf.c $(BUILD)/src/common/linebuf.o
	$(CC) $(BASE_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-unit: $(BUILD)/test_linebuf
	./$(BUILD)/test_linebuf

test-integration: server
	python3 -I tests/test_session.py ./server

test: test-unit test-integration

clean:
	rm -rf $(BUILD) server

-include $(SERVER_OBJ:.o=.d) $(COMMON_OBJ:.o=.d)
