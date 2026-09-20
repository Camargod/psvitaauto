VITASDK ?= /opt/homebrew/vitasdk
BUILD_DIR := build

.PHONY: all clean

all:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE="$(VITASDK)/share/vita.toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
