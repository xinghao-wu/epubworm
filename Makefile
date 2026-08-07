CXX := g++
CXXFLAGS := -std=c++23 -g3 -fsanitize=address -Wall -Wextra -Wpedantic \
            -Wconversion -Wsign-conversion -Weffc++
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN := $(BUILD_DIR)/mnc
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
VENDORED := tinyxml2.cpp tinyxml2.hpp miniz_cpp.hpp stb_image.hpp base64.hpp
PROJECT_FILES := $(filter-out $(addprefix $(SRC_DIR)/,$(VENDORED)),$(wildcard \
                 $(SRC_DIR)/*.cpp $(SRC_DIR)/*.hpp))

.DELETE_ON_ERROR:
.PHONY: clean fmt fmt-diff lint lint-fix

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

fmt:
	clang-format -i $(PROJECT_FILES)

fmt-diff:
	@status=0; \
	for f in $(PROJECT_FILES); do \
		clang-format $$f | diff -u $$f - || status=1; \
	done; \
	exit $$status

lint:
	clang-tidy $(PROJECT_FILES)

lint-fix:
	clang-tidy --fix $(PROJECT_FILES)
