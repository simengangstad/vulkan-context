CXX := g++
CXXFLAGS := -std=c++17 -pedantic-errors -Wall -Wextra
LDFLAGS := -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
BUILD_DIR := build
OBJ_DIR  := $(BUILD_DIR)/objects
TARGET   := program
INCLUDE  := -Iinclude/
SRC      :=                      \
   $(wildcard src/*.cpp)
OBJECTS  := $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEPENDENCIES := $(OBJECTS:.o=.d)

all: build $(BUILD_DIR)/$(TARGET)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -MMD -o $@

$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$(TARGET) $^ $(LDFLAGS)

-include $(DEPENDENCIES)

.PHONY: build clean debug release

build:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all
	./$(BUILD_DIR)/$(TARGET)

release: CXXFLAGS += -O2 -Werror
release: all
	./$(BUILD_DIR)/$(TARGET)

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -vf $(BUILD_DIR)/$(TARGET)

info:
	@echo "[*] Build dir:		${BUILD_DIR}   "
	@echo "[*] Object dir:      ${OBJ_DIR}     "
	@echo "[*] Sources:         ${SRC}         "
	@echo "[*] Objects:         ${OBJECTS}     "

