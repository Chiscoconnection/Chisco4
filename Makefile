# Makefile for Chiscoconnection OS

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS = -lm

# Target executable
TARGET = chiscoconnection_os
SOURCES = chiscoconnection_os_fixed.c
OBJECTS = $(SOURCES:.c=.o)

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CC_FLAGS += -D LINUX
endif
ifeq ($(UNAME_S),Darwin)
    CC_FLAGS += -D MACOS
endif
ifeq ($(OS),Windows_NT)
    CC_FLAGS += -D WINDOWS
    TARGET = chiscoconnection_os.exe
endif

# Default target
all: $(TARGET)

# Build executable
$(TARGET): $(OBJECTS)
	@echo "[LINKING] Creating $(TARGET)..."
	$(CC) $(CFLAGS) $(CC_FLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "[SUCCESS] Build complete: $(TARGET)"
	@ls -lh $(TARGET)

# Compile source files
%.o: %.c
	@echo "[COMPILING] $<..."
	$(CC) $(CFLAGS) $(CC_FLAGS) -c $< -o $@

# Run the OS
run: $(TARGET)
	@echo "[RUN] Starting Chiscoconnection OS..."
	./$(TARGET)

# Clean build artifacts
clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -f $(OBJECTS) $(TARGET) $(TARGET).exe
	@echo "[CLEAN] Done"

# Help target
help:
	@echo "Chiscoconnection OS - Build Commands"
	@echo "====================================="
	@echo "  make          - Build the OS"
	@echo "  make run      - Build and run the OS"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make help     - Show this help message"

.PHONY: all run clean help
