CC = gcc
CFLAGS = -Wall -Wextra -I./include -pthread
LDFLAGS = -pthread
DEBUG_FLAGS = -g -O0
RELEASE_FLAGS = -O2
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer -fno-common

SRC_DIR = src
OBJ_DIR = obj
INC_DIR = include
BIN_DIR = .

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/proxy.c $(SRC_DIR)/proxy_connection.c $(SRC_DIR)/cache.c
OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/proxy.o $(OBJ_DIR)/proxy_connection.o $(OBJ_DIR)/cache.o

TARGET = $(BIN_DIR)/proxy

.PHONY: all debug release debug-asan clean

all: release

release: CFLAGS += $(RELEASE_FLAGS)
release: $(TARGET)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

debug-asan: CFLAGS += $(DEBUG_FLAGS) $(ASAN_FLAGS)
debug-asan: LDFLAGS += $(ASAN_FLAGS)
debug-asan: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(INC_DIR)/proxy.h
$(OBJ_DIR)/proxy.o: $(SRC_DIR)/proxy.c $(INC_DIR)/proxy.h $(INC_DIR)/cache.h
$(OBJ_DIR)/proxy_connection.o: $(SRC_DIR)/proxy_connection.c $(INC_DIR)/proxy.h $(INC_DIR)/cache.h
$(OBJ_DIR)/cache.o: $(SRC_DIR)/cache.c $(INC_DIR)/cache.h

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) *.o core vgcore.* *.log results/

distclean: clean
	rm -f *.log *.tmp *.out

rebuild: clean all

redebug: clean debug

reasan: clean debug-asan
