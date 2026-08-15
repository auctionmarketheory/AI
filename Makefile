TARGET = AI_Tamagotchi

CXX = aarch64-linux-gnu-g++
ifeq ($(shell uname -m), x86_64)
	CXX = g++
endif

CFLAGS = -Wall -O3 -Iinclude -Isrc -std=c++11
LDFLAGS = -lSDL2 -lcurl -pthread -lm

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
