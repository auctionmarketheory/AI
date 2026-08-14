TARGET = App_MicTest

DEVICE ?= PC
RES_PATH ?= "./res"

ifeq ($(DEVICE),PC)
	CC = g++
	SDL2_CONFIG = sdl2-config
else
	CC = $(CXX)
	SDL2_CONFIG = /usr/bin/sdl2-config
endif

COMPILER_FLAGS = $(shell $(SDL2_CONFIG) --cflags) -O2 -Wall -DRES_PATH=\"$(RES_PATH)\"
LINKER_FLAGS   = $(shell $(SDL2_CONFIG) --libs) -pthread

all: $(TARGET)

$(TARGET): src/MicTest.o
	$(CC) $< -o $@ $(LINKER_FLAGS)

src/%.o: src/%.cpp
	$(CC) -c $< -o $@ $(COMPILER_FLAGS)

clean:
	rm -f src/*.o $(TARGET)
