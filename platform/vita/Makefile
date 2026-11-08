# ---------------------------------------------------------------------------------------------------------------
#  Whisk3D para PS Vita: VitaSDK (gcc arm-vita-eabi, libre: nada de Sony) + vitaGL (OpenGL sobre la GPU de la Vita,
#  con el pipeline fijo emulado) + SDL2 de la Vita con vitaGL (ventana GL, tactil, botones, sonido). Es el MISMO
#  editor que la PC (main.cpp + controles.cpp con SDL2, el backend fijo de graficos w3dGraphics.cpp), compilado para
#  el ARM de la Vita. Lo propio de la consola va con #ifdef W3D_VITA.
#
#  No se usa directo: lo llama build_vita.sh (que carga el entorno de ~/vita). Salida en build/:
#  whisk3d.vpk (lo instala el emulador Vita3K o VitaShell en la consola) con los recursos de res/ adentro (app0:res).
# ---------------------------------------------------------------------------------------------------------------
RAIZ     := $(abspath ../..)
VERSION  ?= $(shell date +%y.%m.%d)
SALIDA   := build
OBJDIR   := $(SALIDA)/obj
TARGET   := $(SALIDA)/whisk3d.vpk
PREFIJO  := $(VITASDK)/bin/arm-vita-eabi-
CXX      := $(PREFIJO)g++
CC       := $(PREFIJO)gcc
# el ID de la aplicacion en la Vita: 4 letras y 5 numeros (la carpeta ux0:app/WHSK00001 y los datos)
TITLE_ID := WHSK00001

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
    libs/Whisk3DCore/physics/W3dNav.cpp \
    libs/Whisk3DCore/net/W3dRed.cpp \
    libs/Whisk3DCore/io/w3dFilesystem.cpp \
    libs/Whisk3DCore/graphics/w3dTexture.cpp \
    libs/Whisk3DCore/io/w3dCompress.cpp \
    libs/Whisk3DCore/io/W3dZip.cpp \
    libs/Whisk3DCore/io/W3dTexto.cpp \
    libs/Whisk3DCore/io/W3dMalla.cpp \
    libs/Whisk3DCore/io/W3dMallaBin.cpp \
    libs/Whisk3DCore/io/W3dAlmacen.cpp \
    libs/Whisk3DCore/io/W3dRecursos.cpp \
    libs/Whisk3DCore/graphics/opengl/w3dGraphics.cpp \
    libs/Whisk3DCore/base/w3dlog.cpp \
    libs/Whisk3DCore/base/W3dInteractionState.cpp \
    libs/Whisk3DCore/base/W3dConfig.cpp \
    libs/Whisk3DCore/input/W3dTactil.cpp \
    libs/Whisk3DCore/input/W3dVibrar.cpp \
    libs/Whisk3DCore/math/Vector3.cpp \
    libs/Whisk3DCore/math/Quaternion.cpp \
    libs/Whisk3DCore/math/Matrix4.cpp \
    libs/Whisk3DCore/audio/W3dAudio.cpp \
    libs/Whisk3DCore/audio/W3dAudioSDL.cpp \
    libs/Whisk3DCore/audio/W3dVolumen.cpp \
    libs/Whisk3DCore/audio/W3dMusic.cpp \
    libs/Whisk3DCore/input/W3dClipboardSDL.cpp \
    platform/vita/w3d_vita.cpp
FUENTES_C := $(filter-out thirdparty/lua/src/lua.c thirdparty/lua/src/luac.c, \
                 $(shell cd $(RAIZ) && ls thirdparty/lua/src/*.c))

OBJS := $(addprefix $(OBJDIR)/,$(FUENTES_CPP:.cpp=.o) $(FUENTES_C:.c=.o))

# (platform/vita/include va primero: su GL/gl.h es vitaGL)
INCS := -I$(RAIZ)/platform/vita/include \
        -I$(RAIZ) -I$(RAIZ)/main -I$(RAIZ)/main/app -I$(RAIZ)/main/config -I$(RAIZ)/main/io -I$(RAIZ)/main/undo \
        -I$(RAIZ)/main/ui -I$(RAIZ)/main/ui/ViewPorts -I$(RAIZ)/main/ui/GeometriaUI \
        -I$(RAIZ)/libs/Whisk3DCore -I$(RAIZ)/libs/Whisk3DCore/base -I$(RAIZ)/libs/Whisk3DCore/graphics \
        -I$(RAIZ)/libs/Whisk3DCore/io -I$(RAIZ)/libs/Whisk3DCore/thirdparty -I$(RAIZ)/libs \
        -I$(RAIZ)/libs/WhiskUI/widgets -I$(RAIZ)/libs/WhiskUI/text -I$(RAIZ)/libs/WhiskUI/draw \
        -I$(RAIZ)/libs/WhiskUI/theme -I$(RAIZ)/libs/WhiskUI/core -I$(RAIZ)/thirdparty -I$(RAIZ)/thirdparty/lua/src \
        -I$(VITASDK)/arm-vita-eabi/include/SDL2

# el Cortex-A9 de la Vita (con NEON: float por hardware)
ARCH := -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard
DEFS := -DW3D_VITA -DW3D_OPENGL -DW3D_VERSION=\"$(VERSION)\"
OPT  := -O2 -ffunction-sections -fdata-sections
# CAPTURA=n: despues del cuadro n la pantalla queda en ux0:data/Whisk3D/captura.ppm (captura_vita3k.sh la saca del
# emulador). Solo la usa w3d_vita.cpp: se recompila cuando cambia (prueba.flags)
PRUEBA_DEFS :=
ifneq ($(CAPTURA),)
PRUEBA_DEFS += -DW3D_VITA_CAPTURA=$(CAPTURA)
endif
# (-include climits: el limits.h de newlib define LLONG_MAX solo en C; el <climits> de libstdc++ lo trae en C++. Sin
# eso Lua dice que no hay 'long long' y el lua_Integer de C y el de C++ no coincidirian)
# (-include sys/syslimits.h: el limits.h del gcc de VitaSDK no sigue al de newlib, y PATH_MAX queda en ese otro)
# (-MMD -MP: cada objeto anota los headers que usa, en su .d; si un header cambia, se recompila lo que lo incluye)
CXXFLAGS_W3D := $(ARCH) $(OPT) -std=gnu++17 -include climits -include sys/syslimits.h $(DEFS) $(INCS) -w -MMD -MP
CFLAGS_W3D   := $(ARCH) $(OPT) -std=gnu99 $(DEFS) $(INCS) -w -MMD -MP

# SDL2 (con vitaGL) y lo que piden: vitaGL, el compilador de shaders (vitaShaRK) y las librerias del sistema
LIBS := -lSDL2 -lvitaGL -lvitashark -lSceShaccCgExt -lmathneon -ltaihen_stub -lSceShaccCg_stub \
        -lSceKernelDmacMgr_stub -lSceIme_stub -lSceGxm_stub -lSceDisplay_stub -lSceCtrl_stub -lSceAppMgr_stub \
        -lSceAppUtil_stub -lSceAudio_stub -lSceAudioIn_stub -lSceSysmodule_stub -lSceIofilemgr_stub \
        -lSceCommonDialog_stub -lSceTouch_stub -lSceHid_stub -lSceMotion_stub -lScePower_stub -lSceProcessmgr_stub \
        -lSceLibKernel_stub -lpthread -lm

all: $(TARGET)

$(OBJDIR)/platform/vita/w3d_vita.o: CXXFLAGS_W3D += $(PRUEBA_DEFS)
$(OBJDIR)/platform/vita/w3d_vita.o: $(SALIDA)/prueba.flags
$(SALIDA)/prueba.flags: FORCE
	@mkdir -p $(SALIDA)
	@echo "$(PRUEBA_DEFS)" | cmp -s - $@ || echo "$(PRUEBA_DEFS)" > $@
FORCE:

$(OBJDIR)/%.o: $(RAIZ)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  c++ $*"
	@$(CXX) $(CXXFLAGS_W3D) -c $< -o $@

$(OBJDIR)/%.o: $(RAIZ)/%.c
	@mkdir -p $(dir $@)
	@echo "  cc  $*"
	@$(CC) $(CFLAGS_W3D) -c $< -o $@

# (-Wl,-q: deja las reubicaciones, que vita-elf-create necesita para armar el ejecutable de la Vita. __sce_headroom:
# el lugar que vita-elf-create necesita al final del codigo para los datos del modulo; el script del linker de
# VitaSDK lo deja solo si se define. Sin eso, segun cuanto mida el codigo, no entra)
$(SALIDA)/whisk3d.elf: $(OBJS)
	@echo "  link $@"
	@$(CXX) $(ARCH) -Wl,-q -Wl,--gc-sections -Wl,--defsym=__sce_headroom=0x4000 -o $@ $(OBJS) $(LIBS)

$(SALIDA)/whisk3d.velf: $(SALIDA)/whisk3d.elf
	@$(VITASDK)/bin/vita-elf-create $< $@ > /dev/null

# eboot.bin: el ejecutable firmado para homebrew. Sin -s (modo "unsafe"): vitaGL carga el compilador de shaders de
# ur0:data/external y pide la memoria extra
$(SALIDA)/eboot.bin: $(SALIDA)/whisk3d.velf
	@$(VITASDK)/bin/vita-make-fself -c $< $@

# param.sfo: el nombre y el ID que muestra el menu (ATTRIBUTE2=12: la memoria extendida para la aplicacion)
$(SALIDA)/param.sfo: FORCE
	@mkdir -p $(SALIDA)
	@$(VITASDK)/bin/vita-mksfoex -s TITLE_ID=$(TITLE_ID) -s APP_VER=01.00 -d ATTRIBUTE2=12 \
	    "Whisk3D" $(SALIDA)/param.sfo.nuevo
	@cmp -s $(SALIDA)/param.sfo.nuevo $@ && rm $(SALIDA)/param.sfo.nuevo || mv $(SALIDA)/param.sfo.nuevo $@

# ---- el icono (128x128, con paleta: es lo que pide el menu) y la pantalla de la burbuja (LiveArea) ----
$(SALIDA)/sce_sys/icon0.png: $(RAIZ)/res/Whisk3D.png
	@mkdir -p $(SALIDA)/sce_sys/livearea/contents
	@convert $< -resize 128x128 -background '#1d1d1d' -flatten PNG8:$@
	@convert -size 840x500 xc:'#1d1d1d' \( $< -resize 360x360 \) -gravity center -composite \
	    PNG8:$(SALIDA)/sce_sys/livearea/contents/bg.png
	@convert -size 280x158 xc:'#1d1d1d' \( $< -resize 120x120 \) -gravity center -composite \
	    PNG8:$(SALIDA)/sce_sys/livearea/contents/startup.png
	@cp $(RAIZ)/platform/vita/template.xml $(SALIDA)/sce_sys/livearea/contents/template.xml

# ---- res/ (sin el instalador ni los iconos de escritorio): va adentro del .vpk (app0:res) ----
$(SALIDA)/res.listo: $(addprefix $(RAIZ)/,$(shell cd $(RAIZ) && find res -type f))
	@rm -rf $(SALIDA)/res && mkdir -p $(SALIDA)/res
	@cp -r $(RAIZ)/res/. $(SALIDA)/res/
	@rm -rf $(SALIDA)/res/installer $(SALIDA)/res/*.ico $(SALIDA)/res/*.svg
	@touch $@

$(TARGET): $(SALIDA)/eboot.bin $(SALIDA)/param.sfo $(SALIDA)/sce_sys/icon0.png $(SALIDA)/res.listo
	@cd $(SALIDA) && $(VITASDK)/bin/vita-pack-vpk -s param.sfo -b eboot.bin \
	    -a sce_sys/icon0.png=sce_sys/icon0.png \
	    -a sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
	    -a sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
	    -a sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	    $$(cd res && find . -type f | sed 's|^\./||' | while read f; do printf -- '-a res/%s=res/%s ' "$$f" "$$f"; done) \
	    whisk3d.vpk > /dev/null
	@echo "  vpk $@ ($$(du -h $@ | cut -f1))"

clean:
	rm -rf $(SALIDA)

-include $(OBJS:.o=.d)

.PHONY: all clean FORCE
