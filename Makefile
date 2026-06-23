CXX := g++
CXXFLAGS := -std=c++23 -g3 -fsanitize=address -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Weffc++
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN := $(BUILD_DIR)/mnc
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

.DELETE_ON_ERROR:
.PHONY: clean

$(BIN): $(BUILD_DIR) $(OBJ_DIR) $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

# object compilation, every object gets recompiled for any change in src
$(OBJ_DIR)/%.o: $(wildcard $(SRC_DIR)/*)
	$(CXX) $(CXXFLAGS) -c $(@:$(OBJ_DIR)/%.o=$(SRC_DIR)/%.cpp) -o $@

# specialized object compilation for tinyxml2.cpp to suppress warnings
$(OBJ_DIR)/tinyxml2.o: $(wildcard $(SRC_DIR)/*)
	$(CXX) $(CXXFLAGS) -w -c $(@:$(OBJ_DIR)/%.o=$(SRC_DIR)/%.cpp) -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -r $(BUILD_DIR)
