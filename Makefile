CXX := g++
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra -Wpedantic -Wconversion \
	    -Wsign-conversion -Weffc++
DEBUG_CXXFLAGS := -std=c++23 -g3 -fsanitize=address -Wall -Wextra \
		   -Wpedantic -Wconversion -Wsign-conversion -Weffc++
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
OBJ_DIR_DEBUG := $(BUILD_DIR)/obj_debug
BIN := $(BUILD_DIR)/mnc
DEBUG_BIN := $(BUILD_DIR)/mnc_debug
TEST_BIN := $(BUILD_DIR)/mnc_test
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
MAIN_SRC := $(SRC_DIR)/main.cpp
TEST_SRCS := $(SRC_DIR)/test_main.cpp $(SRC_DIR)/test.cpp
LIB_SRCS := $(filter-out $(MAIN_SRC) $(TEST_SRCS),$(SRCS))
LIB_OBJS := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
MAIN_OBJS := $(OBJ_DIR)/main.o $(LIB_OBJS)
LIB_OBJS_DEBUG := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR_DEBUG)/%.o)
MAIN_OBJS_DEBUG := $(OBJ_DIR_DEBUG)/main.o $(LIB_OBJS_DEBUG)
TEST_OBJS := $(OBJ_DIR_DEBUG)/test_main.o $(OBJ_DIR_DEBUG)/test.o \
	     $(LIB_OBJS_DEBUG)
VENDORED_FILES := tinyxml2.cpp tinyxml2.hpp miniz_cpp.hpp stb_image.hpp \
		  base64.hpp
PROJECT_FILES := $(filter-out $(addprefix $(SRC_DIR)/,$(VENDORED_FILES)), \
		 $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*.hpp))

.DELETE_ON_ERROR:
.PHONY: clean fmt fmt-check fmt-check-diff lint lint-fix test release debug

release: $(BIN)

debug: $(DEBUG_BIN)

$(BIN): $(BUILD_DIR) $(OBJ_DIR) $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJS) -o $@

$(DEBUG_BIN): $(BUILD_DIR) $(OBJ_DIR_DEBUG) $(MAIN_OBJS_DEBUG)
	$(CXX) $(DEBUG_CXXFLAGS) $(MAIN_OBJS_DEBUG) -o $@

$(TEST_BIN): $(BUILD_DIR) $(OBJ_DIR_DEBUG) $(TEST_OBJS)
	$(CXX) $(DEBUG_CXXFLAGS) $(TEST_OBJS) -o $@

# object compilation, every object gets recompiled for any change in src
$(OBJ_DIR)/%.o: $(wildcard $(SRC_DIR)/*)
	$(CXX) $(CXXFLAGS) -c $(@:$(OBJ_DIR)/%.o=$(SRC_DIR)/%.cpp) -o $@

$(OBJ_DIR_DEBUG)/%.o: $(wildcard $(SRC_DIR)/*)
	$(CXX) $(DEBUG_CXXFLAGS) \
	    -c $(@:$(OBJ_DIR_DEBUG)/%.o=$(SRC_DIR)/%.cpp) -o $@

# specialized object compilation for tinyxml2.cpp to suppress warnings
$(OBJ_DIR)/tinyxml2.o: $(wildcard $(SRC_DIR)/*)
	$(CXX) $(CXXFLAGS) -w -c $(@:$(OBJ_DIR)/%.o=$(SRC_DIR)/%.cpp) -o $@

$(OBJ_DIR_DEBUG)/tinyxml2.o: $(wildcard $(SRC_DIR)/*)
	$(CXX) $(DEBUG_CXXFLAGS) \
	    -w -c $(@:$(OBJ_DIR_DEBUG)/%.o=$(SRC_DIR)/%.cpp) -o $@

$(BUILD_DIR) $(OBJ_DIR) $(OBJ_DIR_DEBUG):
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
