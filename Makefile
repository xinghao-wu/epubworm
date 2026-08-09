CXX := g++
CXXFLAGS := -std=c++23 -g3 -fsanitize=address -Wall -Wextra -Wpedantic \
	    -Wconversion -Wsign-conversion -Weffc++
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN := $(BUILD_DIR)/mnc
TEST_BIN := $(BUILD_DIR)/mnc_test
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
MAIN_SRC := $(SRC_DIR)/main.cpp
TEST_SRCS := $(SRC_DIR)/test_main.cpp $(SRC_DIR)/test.cpp
LIB_SRCS := $(filter-out $(MAIN_SRC) $(TEST_SRCS),$(SRCS))
LIB_OBJS := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
MAIN_OBJS := $(OBJ_DIR)/main.o $(LIB_OBJS)
TEST_OBJS := $(OBJ_DIR)/test_main.o $(OBJ_DIR)/test.o $(LIB_OBJS)
VENDORED := tinyxml2.cpp tinyxml2.hpp miniz_cpp.hpp stb_image.hpp base64.hpp
PROJECT_FILES := $(filter-out $(addprefix $(SRC_DIR)/,$(VENDORED)),$(wildcard \
		 $(SRC_DIR)/*.cpp $(SRC_DIR)/*.hpp))

.DELETE_ON_ERROR:
.PHONY: clean fmt fmt-check fmt-check-diff lint lint-fix test

$(BIN): $(BUILD_DIR) $(OBJ_DIR) $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJS) -o $@

$(TEST_BIN): $(BUILD_DIR) $(OBJ_DIR) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) -o $@

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

test: $(TEST_BIN)
	cd $(BUILD_DIR) && ./$(notdir $(TEST_BIN))

fmt:
	clang-format -i $(PROJECT_FILES)

fmt-check:
	clang-format --dry-run --Werror $(PROJECT_FILES)

fmt-check-diff:
	@status=0; \
	for f in $(PROJECT_FILES); do \
		clang-format $$f | diff -u $$f - || status=1; \
	done; \
	exit $$status

lint:
	clang-tidy --warnings-as-errors='*' $(PROJECT_FILES)

lint-fix:
	clang-tidy --fix $(PROJECT_FILES)
