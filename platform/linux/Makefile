# =============================
#   CONFIG GENERAL
# =============================
TARGET_LINUX = Whisk3D_Pre-alpha
TARGET_WIN   = Whisk3D_Pre-alpha.exe

BUILD  = ../../build/linux
ASSETS_SRC = ../../res
ASSETS_DST = $(BUILD)/res

SRC = ../../src/main.cpp ../../include/tinyfiledialogs.c

# Elegir SDL: 2 por defecto, 3 opcional
SDL ?= 2  

# Elegir arquitectura: 32bit por defecto
ARCH ?= 32


# =============================
#   FLAGS DE ARCH
# =============================
ifeq ($(ARCH),64)
    ARCH_FLAG = -m64
else
    ARCH_FLAG = -m32
endif


# =============================
#   LINUX
# =============================
CXX_LINUX = g++

ifeq ($(SDL),3)
    CXXFLAGS_SDL = -Wall -O2 $(ARCH_FLAG) -DUSE_SDL3 $(shell pkg-config --cflags sdl3)
    LIBS_SDL     = $(shell pkg-config --libs sdl3) -lGL -lGLU -ldl -lpthread

else # SDL2 (default)
    CXXFLAGS_SDL = -Wall -O2 $(ARCH_FLAG) -DUSE_SDL2 $(shell pkg-config --cflags sdl2)
    LIBS_SDL     = $(shell pkg-config --libs sdl2) -lSDL2_image -lGL -lGLU -ldl -lpthread
endif


# =============================
#   WINDOWS (MinGW)
# =============================
ifeq ($(ARCH),64)
    CXX_WIN = x86_64-w64-mingw32-g++
else
    CXX_WIN = i686-w64-mingw32-g++
endif


ifeq ($(SDL),3)
    CXXFLAGS_WIN = -Wall -O2 $(ARCH_FLAG) -I../include -DUSE_SDL3
    LIBS_WIN = -lmingw32 -lSDL3 -lopengl32 -lglu32

else # SDL2 default
    CXXFLAGS_WIN = -Wall -O2 $(ARCH_FLAG) -I../include -DUSE_SDL2
    LIBS_WIN = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lopengl32 -lglu32
endif


# =============================
#   BUILD TARGETS
# =============================
.PHONY: linux windows clean

linux:
	mkdir -p $(BUILD)
	mkdir -p $(ASSETS_DST)
	$(CXX_LINUX) $(CXXFLAGS_SDL) $(SRC) -I../../include -I../../Shared -o $(BUILD)/$(TARGET_LINUX) $(LIBS_SDL)
	cp -r $(ASSETS_SRC)/* $(ASSETS_DST)/
	@echo "\n---- BUILD COMPLETADO ----"
	@echo "Ejecutable en: $(BUILD)/$(TARGET_LINUX)"
	@echo "Assets copiados en: $(ASSETS_DST)"

windows:
	$(CXX_WIN) $(CXXFLAGS_WIN) $(SRC) -o $(TARGET_WIN) $(LIBS_WIN)

clean:
	rm -rf $(BUILD)
