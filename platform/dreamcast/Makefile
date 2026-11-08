# ---------------------------------------------------------------------------------------------------------------
#  Whisk3D para Sega Dreamcast: KallistiOS + GLdc (OpenGL 1.x de la PVR) + SDL2 de Dreamcast (mouse, teclado,
#  control, ventana GL). Es el MISMO editor que la PC (main.cpp + controles.cpp con SDL2, el backend fijo de
#  graficos w3dGraphics.cpp), compilado para el SH-4. Lo propio de la consola va con #ifdef W3D_DREAMCAST.
#
#  No se usa directo: lo llama build_dreamcast.sh (que carga el entorno de KallistiOS). Salida en build/:
#  whisk3d.elf (lo abre el emulador Flycast tal cual) y romdisk.img (los recursos de res/, montados en /rd).
# ---------------------------------------------------------------------------------------------------------------
RAIZ    := $(abspath ../..)
SDL2_DC ?= $(HOME)/dc/sdl2-dc-inst
VERSION ?= $(shell date +%y.%m.%d)
SALIDA  := build
OBJDIR  := $(SALIDA)/obj
TARGET  := $(SALIDA)/whisk3d.elf

# ---- fuentes: las mismas que el CMakeLists de PC (main entero, el modelo del Core, la UI y la lista a mano del
#      Core), menos el harness de pruebas (main/test: solo corre en PC con --script) ----
FUENTES_CPP := $(shell cd $(RAIZ) && find main libs/Whisk3DCore/objects libs/Whisk3DCore/animation libs/WhiskUI \
                   -name '*.cpp' -not -path 'main/test/*' | sort)
FUENTES_CPP += \
    libs/Whisk3DCore/script/W3dScript.cpp \
    libs/Whisk3DCore/physics/W3dFisica.cpp \
    libs/Whisk3DCore/physics/W3dRigido.cpp \
    libs/Whisk3DCore/physics/W3dColMalla.cpp \
    libs/Whisk3DCore/physics/W3dHitbox.cpp \
    libs/Whisk3DCore/physics/W3dRagdoll.cpp \
    libs/Whisk3DCore/nav/W3dNav.cpp \
    libs/Whisk3DCore/animation/W3dLocomocion.cpp \
    libs/Whisk3DCore/net/W3dRed.cpp \
    libs/Whisk3DCore/io/w3dFilesystem.cpp \
    libs/Whisk3DCore/gfx/w3dTexture.cpp \
    libs/Whisk3DCore/io/w3dCompress.cpp \
    libs/Whisk3DCore/io/W3dZip.cpp \
    libs/Whisk3DCore/io/W3dTexto.cpp \
    libs/Whisk3DCore/io/W3dMalla.cpp \
    libs/Whisk3DCore/io/W3dMallaBin.cpp \
    libs/Whisk3DCore/io/W3dAlmacen.cpp \
    libs/Whisk3DCore/io/W3dRecursos.cpp \
    libs/Whisk3DCore/gfx/w3dGraphics.cpp \
    libs/Whisk3DCore/base/w3dlog.cpp \
    libs/Whisk3DCore/base/W3dInteractionState.cpp \
    libs/Whisk3DCore/base/W3dConfig.cpp \
    libs/Whisk3DCore/base/W3dTactil.cpp \
    libs/Whisk3DCore/math/Vector3.cpp \
    libs/Whisk3DCore/math/Quaternion.cpp \
    libs/Whisk3DCore/math/Matrix4.cpp \
    libs/Whisk3DCore/audio/W3dAudio.cpp \
    libs/Whisk3DCore/audio/W3dAudioSDL.cpp \
    libs/Whisk3DCore/audio/W3dVolumen.cpp \
    libs/Whisk3DCore/audio/W3dMusic.cpp \
    libs/Whisk3DCore/base/W3dClipboardSDL.cpp \
    platform/dreamcast/w3d_dreamcast.cpp
FUENTES_C := $(filter-out thirdparty/lua/src/lua.c thirdparty/lua/src/luac.c, \
                 $(shell cd $(RAIZ) && ls thirdparty/lua/src/*.c))

OBJS := $(addprefix $(OBJDIR)/,$(FUENTES_CPP:.cpp=.o) $(FUENTES_C:.c=.o))

INCS := -I$(RAIZ) -I$(RAIZ)/main -I$(RAIZ)/main/app -I$(RAIZ)/main/config -I$(RAIZ)/main/io -I$(RAIZ)/main/undo \
        -I$(RAIZ)/main/ui -I$(RAIZ)/main/ui/ViewPorts -I$(RAIZ)/main/ui/GeometriaUI \
        -I$(RAIZ)/libs/Whisk3DCore -I$(RAIZ)/libs/Whisk3DCore/base -I$(RAIZ)/libs/Whisk3DCore/gfx \
        -I$(RAIZ)/libs/Whisk3DCore/io -I$(RAIZ)/libs/Whisk3DCore/thirdparty -I$(RAIZ)/libs \
        -I$(RAIZ)/libs/WhiskUI/widgets -I$(RAIZ)/libs/WhiskUI/text -I$(RAIZ)/libs/WhiskUI/draw \
        -I$(RAIZ)/libs/WhiskUI/theme -I$(RAIZ)/libs/WhiskUI/core -I$(RAIZ)/thirdparty -I$(RAIZ)/thirdparty/lua/src \
        -I$(SDL2_DC)/include -I$(SDL2_DC)/include/SDL2

# -Os + secciones por funcion: el linker tira lo que no se usa (la Dreamcast tiene 16 MB de RAM para todo)
# LUA_32BITS: enteros de 32 bits y float (el SH-4 hace float por hardware; double es por software, lentisimo).
# Tiene que ir IGUAL en C y en C++ (cambia el tamanio de lua_Integer / lua_Number)
DEFS   := -DW3D_DREAMCAST -DW3D_OPENGL -DDREAMCAST -DLUA_32BITS=1 -DW3D_VERSION=\"$(VERSION)\"
OPT    := -Os -ffunction-sections -fdata-sections
# CAPTURA=n: en el cuadro n la pantalla sale por la consola serie (en base64) para verla en la PC sin la consola
# (tools: platform/dreamcast/captura_flycast.sh). Sin CAPTURA no hay nada de esto en el ejecutable.
ifneq ($(CAPTURA),)
DEFS   += -DW3D_DC_CAPTURA=$(CAPTURA)
endif
# PRUEBA=n (con CAPTURA): antes de capturar se empujan eventos de teclado y mouse (ver w3d_dreamcast.cpp)
ifneq ($(PRUEBA),)
DEFS   += -DW3D_DC_PRUEBA=$(PRUEBA)
endif
CXXFLAGS_W3D := $(OPT) -std=gnu++17 $(DEFS) $(INCS) -w
CFLAGS_W3D   := $(OPT) -std=gnu99 $(DEFS) $(INCS) -w

all: $(TARGET)

$(OBJDIR)/%.o: $(RAIZ)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  c++ $*"
	@kos-c++ $(CXXFLAGS_W3D) -c $< -o $@

$(OBJDIR)/%.o: $(RAIZ)/%.c
	@mkdir -p $(dir $@)
	@echo "  cc  $*"
	@kos-cc $(CFLAGS_W3D) -c $< -o $@

# ---- romdisk: res/ (sin el instalador ni los iconos de escritorio), montado por KallistiOS en /rd ----
$(SALIDA)/romdisk.img: $(addprefix $(RAIZ)/,$(shell cd $(RAIZ) && find res -type f))
	@rm -rf $(SALIDA)/romdisk && mkdir -p $(SALIDA)/romdisk
	@cp -r $(RAIZ)/res/. $(SALIDA)/romdisk/
	@rm -rf $(SALIDA)/romdisk/installer $(SALIDA)/romdisk/*.ico $(SALIDA)/romdisk/*.svg
	@$(KOS_GENROMFS) -f $@ -d $(SALIDA)/romdisk > /dev/null

# (como la regla de KallistiOS: el arreglo 'romdisk_data' + libromdiskbase, que lo monta solo en /rd al arrancar)
$(SALIDA)/romdisk.o: $(SALIDA)/romdisk.img
	@$(KOS_BASE)/utils/bin2c/bin2c $< $(SALIDA)/romdisk_tmp.c romdisk
	@kos-cc -o $(SALIDA)/romdisk_tmp.o -c $(SALIDA)/romdisk_tmp.c
	@$(KOS_CC) -o $@ -r $(SALIDA)/romdisk_tmp.o $(KOS_LIB_PATHS) -Wl,--whole-archive -lromdiskbase -nostdlib
	@rm -f $(SALIDA)/romdisk_tmp.c $(SALIDA)/romdisk_tmp.o

# whisk3d_debug.elf conserva los simbolos (para traducir las direcciones de un cuelgue con sh-elf-addr2line);
# whisk3d.elf es el mismo sin simbolos (lo que se carga: Flycast no abre un ELF de casi 100 MB)
$(SALIDA)/whisk3d_debug.elf: $(OBJS) $(SALIDA)/romdisk.o
	@echo "  link $@"
	@kos-c++ -o $@ $(OBJS) $(SALIDA)/romdisk.o -Wl,--gc-sections -L$(SDL2_DC)/lib \
	    -lSDL2 -lSDL2main -lGL -lGLU -lm -lkosutils -lpthread

$(TARGET): $(SALIDA)/whisk3d_debug.elf
	@$(KOS_CC_BASE)/bin/$(KOS_CC_PREFIX)-strip -o $@ $<
	@$(KOS_CC_BASE)/bin/$(KOS_CC_PREFIX)-size $@

# DISCO: build/whisk3d.cdi (autoarrancable: emuladores, un CD-R grabado o una unidad que emula el lector, tipo GDEMU).
# -N: sin el relleno hasta el borde del CD (pesa 6 MB en vez de 700; en un CD-R real carga un poco mas lento)
MKDCDISC ?= $(HOME)/dc/mkdcdisc/build_manual/mkdcdisc
cdi: $(SALIDA)/whisk3d.cdi
$(SALIDA)/whisk3d.cdi: $(TARGET)
	@rm -f $@
	@$(MKDCDISC) -q -N -e $(TARGET) -o $@ -n "Whisk3D" -a "Dante Leoncini" -V WHISK3D
	@echo "  disco $@"

clean:
	rm -rf $(SALIDA)

.PHONY: all clean cdi
