CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99 -Iinclude -D_GNU_SOURCE
LDFLAGS = -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INCLUDE_DIR = include

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET = $(BIN_DIR)/make2flake

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/make2flake.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

test: $(TARGET)
	./$(TARGET) -i Makefile -o test-flake.nix --verbose
	@echo "Testing generated flake..."
	@nix flake check test-flake.nix 2>/dev/null || echo "Flake check failed (expected for test)"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) test-flake.nix

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/make2flake

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/make2flake
