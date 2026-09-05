CXX ?= g++
COMMON_CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Weffc++ -MMD -MP
RELEASE_CXXFLAGS ?= -O2
DEBUG_CXXFLAGS ?= -g3 -fsanitize=address
RELEASE_BUILD_CXXFLAGS = $(COMMON_CXXFLAGS) $(RELEASE_CXXFLAGS) $(CXXFLAGS)
DEBUG_BUILD_CXXFLAGS = $(COMMON_CXXFLAGS) $(DEBUG_CXXFLAGS) $(CXXFLAGS)
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
OBJ_DIR_DEBUG := $(BUILD_DIR)/obj_debug
BIN := $(BUILD_DIR)/epubworm
DEBUG_BIN := $(BUILD_DIR)/epubworm_debug
TEST_BIN := $(BUILD_DIR)/epubworm_test
PREFIX ?= /usr/local
USER_PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
USER_BINDIR ?= $(USER_PREFIX)/bin
DOCDIR ?= $(PREFIX)/share/doc/epubworm
USER_DOCDIR ?= $(USER_PREFIX)/share/doc/epubworm
UNAME_S := $(shell uname -s)
PLATFORM_LDLIBS :=
ifeq ($(UNAME_S),Darwin)
PLATFORM_LDLIBS += -liconv
endif
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
MAIN_SRC := $(SRC_DIR)/main.cpp
TEST_SRCS := $(SRC_DIR)/test_main.cpp $(SRC_DIR)/test.cpp
LIB_SRCS := $(filter-out $(MAIN_SRC) $(TEST_SRCS),$(SRCS))
LIB_OBJS := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
MAIN_OBJS := $(OBJ_DIR)/main.o $(LIB_OBJS)
LIB_OBJS_DEBUG := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR_DEBUG)/%.o)
MAIN_OBJS_DEBUG := $(OBJ_DIR_DEBUG)/main.o $(LIB_OBJS_DEBUG)
TEST_OBJS := $(OBJ_DIR_DEBUG)/test_main.o $(OBJ_DIR_DEBUG)/test.o $(LIB_OBJS_DEBUG)
VENDORED_FILES := tinyxml2.cpp tinyxml2.hpp miniz_cpp.hpp stb_image.hpp base64.hpp
PROJECT_FILES := $(filter-out $(addprefix $(SRC_DIR)/,$(VENDORED_FILES)), $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*.hpp))

.DELETE_ON_ERROR:
.PHONY: clean fmt fmt-check fmt-check-diff lint lint-fix test release debug install install-user uninstall uninstall-user

release: $(BIN)

debug: $(DEBUG_BIN)

install: $(BIN)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 "$(BIN)" "$(DESTDIR)$(BINDIR)/$(notdir $(BIN))"
	install -d "$(DESTDIR)$(DOCDIR)"
	install -m 644 LICENSE THIRD_PARTY_LICENSES "$(DESTDIR)$(DOCDIR)"

install-user: $(BIN)
	install -d "$(DESTDIR)$(USER_BINDIR)"
	install -m 755 "$(BIN)" "$(DESTDIR)$(USER_BINDIR)/$(notdir $(BIN))"
	install -d "$(DESTDIR)$(USER_DOCDIR)"
	install -m 644 LICENSE THIRD_PARTY_LICENSES "$(DESTDIR)$(USER_DOCDIR)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(notdir $(BIN))"
	rm -f "$(DESTDIR)$(DOCDIR)/LICENSE" "$(DESTDIR)$(DOCDIR)/THIRD_PARTY_LICENSES"
	rmdir "$(DESTDIR)$(DOCDIR)" 2>/dev/null || true

uninstall-user:
	rm -f "$(DESTDIR)$(USER_BINDIR)/$(notdir $(BIN))"
	rm -f "$(DESTDIR)$(USER_DOCDIR)/LICENSE" "$(DESTDIR)$(USER_DOCDIR)/THIRD_PARTY_LICENSES"
	rmdir "$(DESTDIR)$(USER_DOCDIR)" 2>/dev/null || true

$(BIN): $(BUILD_DIR) $(OBJ_DIR) $(MAIN_OBJS)
	$(CXX) $(RELEASE_BUILD_CXXFLAGS) $(LDFLAGS) $(MAIN_OBJS) -o $@ $(LDLIBS) $(PLATFORM_LDLIBS)

$(DEBUG_BIN): $(BUILD_DIR) $(OBJ_DIR_DEBUG) $(MAIN_OBJS_DEBUG)
	$(CXX) $(DEBUG_BUILD_CXXFLAGS) $(LDFLAGS) $(MAIN_OBJS_DEBUG) -o $@ $(LDLIBS) $(PLATFORM_LDLIBS)

$(TEST_BIN): $(BUILD_DIR) $(OBJ_DIR_DEBUG) $(TEST_OBJS)
	$(CXX) $(DEBUG_BUILD_CXXFLAGS) $(LDFLAGS) $(TEST_OBJS) -o $@ $(LDLIBS) $(PLATFORM_LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(RELEASE_BUILD_CXXFLAGS) -c $< -o $@

$(OBJ_DIR_DEBUG)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(DEBUG_BUILD_CXXFLAGS) -c $< -o $@

# specialized object compilation for tinyxml2.cpp to suppress warnings
$(OBJ_DIR)/tinyxml2.o: $(SRC_DIR)/tinyxml2.cpp
	$(CXX) $(CPPFLAGS) $(RELEASE_BUILD_CXXFLAGS) -w -c $< -o $@

$(OBJ_DIR_DEBUG)/tinyxml2.o: $(SRC_DIR)/tinyxml2.cpp
	$(CXX) $(CPPFLAGS) $(DEBUG_BUILD_CXXFLAGS) -w -c $< -o $@

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

# Compiler-generated dependency files for incremental rebuilds on header changes
DEPS := $(MAIN_OBJS:.o=.d) $(MAIN_OBJS_DEBUG:.o=.d) $(TEST_OBJS:.o=.d)
-include $(DEPS)
