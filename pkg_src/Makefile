# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

# Files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = ip_monitor

# Default target
all: directories $(EXECUTABLE)

# Create necessary directories
directories:
	mkdir -p $(OBJ_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Link the executable
$(EXECUTABLE): $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(EXECUTABLE)

# Run the application
run: all
	./$(EXECUTABLE)

# Install cJSON dependency (for Debian/Ubuntu systems)
install-deps:
	apt-get update && apt-get install -y libcjson-dev

.PHONY: all directories clean run install-deps