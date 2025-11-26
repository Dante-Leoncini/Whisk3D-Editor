# =============================
#   CONFIGURACIÓN GENERAL
# =============================
TARGET_LINUX = Whisk3D_Pre-alpha
TARGET_WIN   = Whisk3D_Pre-alpha.exe

SRC = main.cpp ../Shared/UI/tinyfiledialogs.c

# Se usa:   make linux sdl=2
#           make linux sdl=3
SDL ?= 2   # Si no se elige nada, compila con SDL2 por defecto

# =============================
#   LINUX
# =============================
CXX_LINUX = g++

ifeq ($(SDL),3)
    CXXFLAGS_SDL = -Wall -O2 -DUSE_SDL3 $(shell pkg-config --cflags sdl3)
    LIBS_SDL     = $(shell pkg-config --libs sdl3) -lGL -lGLU -ldl -lpthread
else
    CXXFLAGS_SDL = -Wall -O2 -DUSE_SDL2 $(shell pkg-config --cflags sdl2)
    LIBS_SDL     = $(shell pkg-config --libs sdl2) -lSDL2_image -lGL -lGLU -ldl -lpthread
endif

# =============================
#   WINDOWS
# =============================
CXX_WIN = x86_64-w64-mingw32-g++

ifeq ($(SDL),3)
    CXXFLAGS_WIN = -Wall -O2 -I../Shared -DUSE_SDL3
    LIBS_WIN = -lmingw32 -lSDL3 -lopengl32 -lglu32
else
    CXXFLAGS_WIN = -Wall -O2 -I../Shared -DUSE_SDL2
    LIBS_WIN = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lopengl32 -lglu32
endif

# =============================
#   BUILD TARGETS
# =============================
.PHONY: linux windows clean

linux:
	$(CXX_LINUX) $(CXXFLAGS_SDL) $(SRC) -I../Shared -o $(TARGET_LINUX) $(LIBS_SDL)

windows:
	$(CXX_WIN) $(CXXFLAGS_WIN) $(SRC) -o $(TARGET_WIN) $(LIBS_WIN)

clean:
	rm -f $(TARGET_LINUX) $(TARGET_WIN)