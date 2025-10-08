# Nombre del ejecutable
TARGET_LINUX = Whisk3D_Pre-alpha
TARGET_WIN   = Whisk3D_Pre-alpha.exe

SRC = main.cpp ../Shared/UI/tinyfiledialogs.c

# Incluir SDL3 automáticamente usando pkg-config
CXXFLAGS_LINUX = -Wall -O2 $(shell pkg-config --cflags sdl3) -I../Shared
LIBS_LINUX    = $(shell pkg-config --libs sdl3) -lGL -lGLU -lvulkan -ldl -lpthread

# Compilador Linux
CXX_LINUX = g++

# Compilador Windows (cross-compilación con mingw-w64)
CXX_WIN = x86_64-w64-mingw32-g++
CXXFLAGS_WIN = -Wall -O2 -I../Shared
LIBS_WIN = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lopengl32 -lglu32

.PHONY: all linux windows clean

all: linux

linux: $(SRC)
	$(CXX_LINUX) $(CXXFLAGS_LINUX) $(SRC) -o $(TARGET_LINUX) $(LIBS_LINUX)

windows: $(SRC)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(SRC) -o $(TARGET_WIN) $(LIBS_WIN)

clean:
	rm -f $(TARGET_LINUX) $(TARGET_WIN)
