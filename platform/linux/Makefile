# =============================
#   CONFIG GENERAL
# =============================
TARGET = Whisk3D_Pre-alpha

BUILD       = ../../build/linux
ASSETS_SRC  = ../../res
ASSETS_DST  = $(BUILD)/res

SRC = \
    ../../src/main.cpp \
    ../../src/GLES_Android_helpers.cpp \
    ../../src/OpcionesRender.cpp \
    ../../src/variables.cpp \
    ../../src/clases.cpp \
    ../../src/render.cpp \
    ../../src/UI/colores.cpp \
    ../../src/Animation.cpp \
    ../../src/tinyfiledialogs.c \
    ../../src/Floor.cpp \
    ../../src/UI/GeometriaUI.cpp \
    ../../src/UI/UI.cpp \
    ../../src/UI/font.cpp \
    ../../src/UI/icons.cpp \
    ../../src/UI/text.cpp \
    ../../src/UI/object2D.cpp \
    ../../src/UI/rectangle.cpp \
    ../../src/Objects/Camera.cpp \
    ../../src/Objects/Collection.cpp \
    ../../src/Objects/Gamepad.cpp \
    ../../src/Objects/Instance.cpp \
    ../../src/Objects/Light.cpp \
    ../../src/Objects/Materials.cpp \
    ../../src/Objects/Mesh.cpp \
    ../../src/Objects/Mirror.cpp \
    ../../src/Objects/Modifier.cpp \
    ../../src/Objects/ObjectMode.cpp \
    ../../src/Objects/Objects.cpp \
    ../../src/Objects/Primitivas.cpp \
    ../../src/Objects/Scene.cpp \
    ../../src/Objects/Textures.cpp \
    ../../src/importers/import_obj.cpp \
    ../../src/importers/import_vertex_animation.cpp \
    ../../src/importers/import_w3d.cpp \
    ../../src/importers/import_wobj.cpp \
    ../../src/ViewPorts/ViewPorts.cpp \
    ../../src/ViewPorts/ViewPort3D.cpp \
    ../../src/ViewPorts/Outliner.cpp \
    ../../src/controles.cpp \
    ../../src/lectura-escritura.cpp \
    ../../src/constructor.cpp

# =============================
#   CONFIGURACION
# =============================
ARCH ?= 32

ifeq ($(ARCH),64)
    ARCH_FLAG = -m64
else
    ARCH_FLAG = -m32
endif

CXX = g++

# =============================
#   FLAGS Y LIBS SDL2
# =============================
CXXFLAGS = -Wall -O2 $(ARCH_FLAG) -DUSE_SDL2 \
           $(shell pkg-config --cflags sdl2) \
           -I../../include -I../../include/UI -I../../include/Objects -I../../Shared

LIBS = $(shell pkg-config --libs sdl2) -lSDL2_image -lGL -lGLU -ldl -lpthread

# =============================
#   TARGETS
# =============================
.PHONY: linux clean

linux:
	mkdir -p $(BUILD)
	mkdir -p $(ASSETS_DST)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BUILD)/$(TARGET) $(LIBS)
	cp -r $(ASSETS_SRC)/* $(ASSETS_DST)/
	@echo "\n---- BUILD COMPLETADO ----"
	@echo "Ejecutable en: $(BUILD)/$(TARGET)"
	@echo "Assets copiados en: $(ASSETS_DST)"

clean:
	rm -rf $(BUILD)
