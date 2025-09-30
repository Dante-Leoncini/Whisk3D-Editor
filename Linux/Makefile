# Nombre del ejecutable
TARGET_LINUX = Whisk3D_Pre-alpha
TARGET_WIN   = Whisk3D_Pre-alpha.exe

SRC = main.cpp ../Shared/tinyfiledialogs.c
INCLUDES = -I../Shared
#SRC_SHARED = ../Shared/ficha.h ../Shared/tablero.h

# Compilador Linux
CXX_LINUX = g++
CXXFLAGS_LINUX = -Wall -O2 $(INCLUDES)
LIBS_LINUX = -lSDL2 -lSDL2_image -lGL -lGLU

# Compilador Windows (cross-compilación con mingw-w64)
CXX_WIN = x86_64-w64-mingw32-g++
CXXFLAGS_WIN = -Wall -O2 $(INCLUDES)
LIBS_WIN = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lopengl32 -lglu32

.PHONY: all linux windows clean

all: linux

linux: $(SRC) $(SRC_SHARED)
	$(CXX_LINUX) $(CXXFLAGS_LINUX) $(SRC) -o $(TARGET_LINUX) $(LIBS_LINUX)

windows: $(SRC) $(SRC_SHARED)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(SRC) -o $(TARGET_WIN) $(LIBS_WIN)

clean:
	rm -f $(TARGET_LINUX) $(TARGET_WIN)
