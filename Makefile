CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Iinclude
LDFLAGS  ?=

SRC := src/main.cpp src/ecc.cpp src/utils.cpp
OBJ := $(SRC:src/%.cpp=build/%.o)
BIN := bin/solution

.PHONY: all clean
all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

bin build:
	@mkdir -p $@

clean:
	@rm -rf build $(BIN)
