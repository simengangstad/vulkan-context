CXX				:= g++
CXXFLAGS		:= -std=c++17 -pedantic-errors -Wall -Wextra
LDFLAGS			:= -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

GLSLC			:= glslc
GLSLCFLAGS		:=

BUILD_DIR		:= build
OBJ_DIR			:= $(BUILD_DIR)/objects
TARGET			:= program
INCLUDE			:= -Iinclude/
SRC				:= $(wildcard src/*.cpp)
OBJECTS			:= $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEPENDENCIES	:= $(OBJECTS:.o=.d)

SHADER_DIR		:= shaders
SHADER_SRC		:= $(wildcard shaders/*.glsl)
SHADER_OBJ		:= $(SHADER_SRC:%.glsl=$(BUILD_DIR)/%.spv)

all: build $(BUILD_DIR)/$(TARGET) $(SHADER_OBJ)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -MMD -o $@


$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$(TARGET) $^ $(LDFLAGS)

-include $(DEPENDENCIES)

$(BUILD_DIR)/$(SHADER_DIR)/%.vert.spv: $(SHADER_DIR)/%.vert.glsl
	@mkdir -p $(@D)
	$(GLSLC) $(GLSLFLAGS) -fshader-stage=vert $< -o $@

$(BUILD_DIR)/$(SHADER_DIR)/%.frag.spv: $(SHADER_DIR)/%.frag.glsl
	@mkdir -p $(@D)
	$(GLSLC) $(GLSLFLAGS) -fshader-stage=frag $< -o $@

build:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all
	cd ./$(BUILD_DIR) && ./$(TARGET)

release: CXXFLAGS += -O2 -Werror
release: all
	cd ./$(BUILD_DIR) && ./$(TARGET)


.PHONY: clean
clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -vf $(BUILD_DIR)/$(TARGET)
	-@rm -vf $(SHADER_OBJ)

info:
	@echo "[*] Build dir:		${BUILD_DIR}   "
	@echo "[*] Object dir:      ${OBJ_DIR}     "
	@echo "[*] Sources:         ${SRC}         "
	@echo "[*] Objects:         ${OBJECTS}     "
	@echo "[*] Shader Objects:  ${SHADER_OBJ}     "

