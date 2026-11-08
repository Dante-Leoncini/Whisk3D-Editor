# ---------------------------------------------------------------------------------------------------------------
#  Whisk3D para Nintendo 3DS: devkitARM + libctru + citro3d (la GPU PICA200) + SDL2 de 3DS (tactil, botones,
#  eventos), todo libre (devkitPro: nada de Nintendo). Es el MISMO editor que la PC (main.cpp + controles.cpp con
#  SDL2); el dibujo va por el backend citro3d (libs/Whisk3DCore/c3d) en vez del de OpenGL. Lo propio de la consola va
#  con #ifdef W3D_3DS.
#
#  No se usa directo: lo llama build_3ds.sh (que carga el entorno de ~/3ds). Salida en build/:
#  whisk3d.3dsx (lo abre el emulador Azahar tal cual, o el Homebrew Launcher de la consola) con los recursos de res/
#  adentro (romfs).
# ---------------------------------------------------------------------------------------------------------------
RAIZ     := $(abspath ../..)
SDL2_3DS ?= $(HOME)/3ds/sdl2-3ds-inst
VERSION  ?= $(shell date +%y.%m.%d)
SALIDA   := build
OBJDIR   := $(SALIDA)/obj
TARGET   := $(SALIDA)/whisk3d.3dsx
PREFIJO  := $(DEVKITARM)/bin/arm-none-eabi-
CXX      := $(PREFIJO)g++
CC       := $(PREFIJO)gcc
TOOLS    := $(DEVKITPRO)/tools/bin

# ---- fuentes: las mismas que el CMakeLists de PC (main entero, el modelo del Core, la UI y la lista a mano del
#      Core), menos el harness de pruebas (main/test: solo corre en PC con --script). El dibujo: c3d en vez de gfx ----
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
    libs/Whisk3DCore/c3d/w3dGraphicsC3D.cpp \
    libs/Whisk3DCore/c3d/w3dGLc3d.cpp \
    libs/Whisk3DCore/base/w3dlog.cpp \
    libs/Whisk3DCore/base/W3dInteractionState.cpp \
    libs/Whisk3DCore/base/W3dConfig.cpp \
    libs/Whisk3DCore/base/W3dTactil.cpp \
    libs/Whisk3DCore/base/W3dVibrar.cpp \
    libs/Whisk3DCore/math/Vector3.cpp \
    libs/Whisk3DCore/math/Quaternion.cpp \
    libs/Whisk3DCore/math/Matrix4.cpp \
    libs/Whisk3DCore/audio/W3dAudio.cpp \
    libs/Whisk3DCore/audio/W3dAudioSDL.cpp \
    libs/Whisk3DCore/audio/W3dVolumen.cpp \
    libs/Whisk3DCore/audio/W3dMusic.cpp \
    libs/Whisk3DCore/base/W3dClipboardSDL.cpp \
    platform/3ds/w3d_3ds.cpp
FUENTES_C := $(filter-out thirdparty/lua/src/lua.c thirdparty/lua/src/luac.c, \
                 $(shell cd $(RAIZ) && ls thirdparty/lua/src/*.c))

OBJS := $(addprefix $(OBJDIR)/,$(FUENTES_CPP:.cpp=.o) $(FUENTES_C:.c=.o)) $(OBJDIR)/w3d_shbin.o

# (platform/3ds/include va primero: su GL/gl.h es el de la 3DS, sobre citro3d)
INCS := -I$(RAIZ)/platform/3ds/include -I$(OBJDIR) \
        -I$(RAIZ) -I$(RAIZ)/main -I$(RAIZ)/main/app -I$(RAIZ)/main/config -I$(RAIZ)/main/io -I$(RAIZ)/main/undo \
        -I$(RAIZ)/main/ui -I$(RAIZ)/main/ui/ViewPorts -I$(RAIZ)/main/ui/GeometriaUI \
        -I$(RAIZ)/libs/Whisk3DCore -I$(RAIZ)/libs/Whisk3DCore/base -I$(RAIZ)/libs/Whisk3DCore/gfx \
        -I$(RAIZ)/libs/Whisk3DCore/io -I$(RAIZ)/libs/Whisk3DCore/thirdparty -I$(RAIZ)/libs \
        -I$(RAIZ)/libs/WhiskUI/widgets -I$(RAIZ)/libs/WhiskUI/text -I$(RAIZ)/libs/WhiskUI/draw \
        -I$(RAIZ)/libs/WhiskUI/theme -I$(RAIZ)/libs/WhiskUI/core -I$(RAIZ)/thirdparty -I$(RAIZ)/thirdparty/lua/src \
        -I$(SDL2_3DS)/include -I$(SDL2_3DS)/include/SDL2 -I$(DEVKITPRO)/libctru/include

# el ARM11 de la 3DS (con su FPU VFP: float y double por hardware)
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
DEFS := -D__3DS__ -DW3D_3DS -DW3D_OPENGL -DW3D_VERSION=\"$(VERSION)\"
OPT  := -O2 -ffunction-sections -fdata-sections
# CAPTURA=n: despues del cuadro n la pantalla de abajo queda en la SD (sdmc:/whisk3d_captura.ppm); PRUEBA=n: las
# pruebas de w3d_3ds.cpp (1 = rendimiento). Solo las usa ese archivo: se recompila cuando cambian (prueba.flags)
PRUEBA_DEFS :=
ifneq ($(CAPTURA),)
PRUEBA_DEFS += -DW3D_3DS_CAPTURA=$(CAPTURA)
endif
ifneq ($(PRUEBA),)
PRUEBA_DEFS += -DW3D_3DS_PRUEBA=$(PRUEBA)
endif
# (-include climits: el limits.h de newlib define LLONG_MAX solo en C; el <climits> de libstdc++ lo trae en C++. Sin
# eso Lua dice que no hay 'long long' y el lua_Integer de C y el de C++ no coincidirian)
# (-MMD -MP: cada objeto anota los headers que usa, en su .d; si un header cambia, se recompila lo que lo incluye)
CXXFLAGS_W3D := $(ARCH) $(OPT) -std=gnu++17 -include climits $(DEFS) $(INCS) -w -MMD -MP
CFLAGS_W3D   := $(ARCH) $(OPT) -std=gnu99 $(DEFS) $(INCS) -w -MMD -MP

all: $(TARGET)

$(OBJDIR)/platform/3ds/w3d_3ds.o: CXXFLAGS_W3D += $(PRUEBA_DEFS)
$(OBJDIR)/platform/3ds/w3d_3ds.o: $(SALIDA)/prueba.flags
$(SALIDA)/prueba.flags: FORCE
	@mkdir -p $(SALIDA)
	@echo "$(PRUEBA_DEFS)" | cmp -s - $@ || echo "$(PRUEBA_DEFS)" > $@
FORCE:

$(OBJDIR)/%.o: $(RAIZ)/%.cpp $(OBJDIR)/w3d_shbin.h
	@mkdir -p $(dir $@)
	@echo "  c++ $*"
	@$(CXX) $(CXXFLAGS_W3D) -c $< -o $@

$(OBJDIR)/%.o: $(RAIZ)/%.c
	@mkdir -p $(dir $@)
	@echo "  cc  $*"
	@$(CC) $(CFLAGS_W3D) -c $< -o $@

# ---- el vertex shader: picasso lo ensambla y bin2s lo vuelve un arreglo (w3d_shbin + w3d_shbin_size) ----
$(OBJDIR)/w3d.shbin: $(RAIZ)/libs/Whisk3DCore/c3d/w3d.v.pica
	@mkdir -p $(OBJDIR)
	@echo "  picasso $(notdir $<)"
	@$(TOOLS)/picasso -o $@ $<
$(OBJDIR)/w3d_shbin.h $(OBJDIR)/w3d_shbin.s: $(OBJDIR)/w3d.shbin
	@cd $(OBJDIR) && $(TOOLS)/bin2s -a 4 -H w3d_shbin.h w3d.shbin > w3d_shbin.s
$(OBJDIR)/w3d_shbin.o: $(OBJDIR)/w3d_shbin.s
	@$(CC) $(ARCH) -x assembler-with-cpp -c $< -o $@

# ---- romfs: res/ (sin el instalador ni los iconos de escritorio), adentro del .3dsx ----
$(SALIDA)/romfs.listo: $(addprefix $(RAIZ)/,$(shell cd $(RAIZ) && find res -type f))
	@rm -rf $(SALIDA)/romfs && mkdir -p $(SALIDA)/romfs
	@cp -r $(RAIZ)/res/. $(SALIDA)/romfs/
	@rm -rf $(SALIDA)/romfs/installer $(SALIDA)/romfs/*.ico $(SALIDA)/romfs/*.svg
	@touch $@

# ---- el icono y los datos que muestra el menu (smdh) ----
$(SALIDA)/whisk3d.smdh: $(RAIZ)/res/Whisk3D.png
	@mkdir -p $(SALIDA)
	@convert $< -resize 48x48 $(SALIDA)/icono48.png
	@$(TOOLS)/smdhtool --create "Whisk3D" "Editor 3D (Pre-Alpha $(VERSION))" "Dante Leoncini" $(SALIDA)/icono48.png $@

$(SALIDA)/whisk3d.elf: $(OBJS)
	@echo "  link $@"
	@$(CXX) $(ARCH) -specs=3dsx.specs -o $@ $(OBJS) -Wl,--gc-sections -Wl,-Map,$(SALIDA)/whisk3d.map \
	    -L$(SDL2_3DS)/lib -L$(DEVKITPRO)/libctru/lib -lSDL2main -lSDL2 -lcitro3d -lctru -lm

$(TARGET): $(SALIDA)/whisk3d.elf $(SALIDA)/whisk3d.smdh $(SALIDA)/romfs.listo
	@$(TOOLS)/3dsxtool $< $@ --smdh=$(SALIDA)/whisk3d.smdh --romfs=$(SALIDA)/romfs
	@echo "  3dsx $@ ($$(du -h $@ | cut -f1))"

clean:
	rm -rf $(SALIDA)

-include $(OBJS:.o=.d)

.PHONY: all clean FORCE
