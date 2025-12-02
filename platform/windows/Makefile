# =============================
#   CONFIG GENERAL
# =============================

TARGET_WIN = Whisk3D_Pre-alpha.exe

# Prioridades: SDL2 por defecto, ARCH=64 por defecto
SDL ?= 2
ARCH ?= 64

SRC = ../../src/main.cpp ../../include/tinyfiledialogs.c

# Prefijo de instalación de MinGW64 en MSYS2
# (NO lo cambies, es correcto para MSYS2 Mingw64)
MINGW_PREFIX ?= /mingw64


# =============================
#   CARPETAS DE BUILD
# =============================
ifeq ($(ARCH),64)
    BUILD = ../../build/windows/x64
else
    BUILD = ../../build/windows/x86
endif


# =============================
#   COMPILADOR SEGÚN ARQUITECTURA
# =============================
ifeq ($(ARCH),64)
    CXX_WIN = x86_64-w64-mingw32-g++
    ARCH_FLAG = -m64
else
    CXX_WIN = i686-w64-mingw32-g++
    ARCH_FLAG = -m32
endif


# =============================
#   FLAGS SDL
# =============================
ifeq ($(SDL),3)

    # ---------- SDL3 ----------
    CXXFLAGS_WIN = -Wall -O2 $(ARCH_FLAG) -DUSE_SDL3 \
        -I../include -I$(MINGW_PREFIX)/include/SDL3
    LIBS_WIN = -L$(MINGW_PREFIX)/lib \
        -lmingw32 -lSDL3 -lopengl32 -lglu32

else

    # ---------- SDL2 (default) ----------
    CXXFLAGS_WIN = -Wall -O2 $(ARCH_FLAG) -DUSE_SDL2 \
        -I../include -I$(MINGW_PREFIX)/include/SDL2
    LIBS_WIN = -L$(MINGW_PREFIX)/lib \
        -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lopengl32 -lglu32

endif


# =============================
#   TARGETS
# =============================
.PHONY: windows clean

windows:
	mkdir -p $(BUILD)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(SRC) \
		-I../../include -I../../Shared \
		-o $(BUILD)/$(TARGET_WIN) $(LIBS_WIN)

	@echo ""
	@echo "================================"
	@echo "      WINDOWS BUILD OK"
	@echo "================================"
	@echo "   SDL:  $(SDL)"
	@echo "   ARCH: $(ARCH)-bit"
	@echo "   OUT:  $(BUILD)/$(TARGET_WIN)"
	@echo ""

clean:
	rm -rf ../../build/windows