# ---------------------------------------------------------------------------------------------------------------
#  Whisk3D para PSP: pspdev (gcc psp-gcc para el MIPS Allegrex y el PSPSDK, libre: nada de Sony) + SDL2 de la PSP
#  (eventos, el control, el sonido). Es el MISMO editor que la PC (main.cpp + controles.cpp con SDL2); el dibujo va por
#  el backend sceGu del Core (libs/Whisk3DCore/graphics/gu: el GE de la PSP) en vez del de OpenGL. Lo propio de la consola va
#  con #ifdef W3D_PSP.
#
#  No se usa directo: lo llama build_psp.sh (que carga el entorno de ~/psp). Salida en build/:
#    PSP/GAME/Whisk3D/      la carpeta que va al Memory Stick (EBOOT.PBP y res/ al lado)
#    whisk3d-psp.zip        esa carpeta, para bajar: se descomprime en la raiz del Memory Stick
# ---------------------------------------------------------------------------------------------------------------
RAIZ     := $(abspath ../..)
VERSION  ?= $(shell date +%y.%m.%d)
# la version del PARAM.SFO: el formato de Sony es NN.NN (dos cifras, punto, dos cifras: "01.00"). La fecha entera
# (26.11.08) el emulador la acepta, pero el menu de una PSP de verdad lee el SFO a su manera: va el anio y el mes
APP_VER  := $(shell date +%y.%m)
SALIDA   := build
OBJDIR   := $(SALIDA)/obj
CARPETA  := $(SALIDA)/PSP/GAME/Whisk3D
TARGET   := $(SALIDA)/whisk3d-psp.zip
PSPSDK   := $(PSPDEV)/psp/sdk
CXX      := $(PSPDEV)/bin/psp-g++
CC       := $(PSPDEV)/bin/psp-gcc

# ---- fuentes: las mismas que el CMakeLists de PC (main entero, el modelo del Core, la UI y la lista a mano del
#      Core), menos el harness de pruebas (main/test: solo corre en PC con --script). El dibujo: gu en vez de gfx ----
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
    libs/Whisk3DCore/graphics/gu/w3dGraphicsGu.cpp \
    libs/Whisk3DCore/graphics/gu/w3dGLgu.cpp \
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
    platform/psp/w3d_psp.cpp
FUENTES_C := $(filter-out thirdparty/lua/src/lua.c thirdparty/lua/src/luac.c, \
                 $(shell cd $(RAIZ) && ls thirdparty/lua/src/*.c))

OBJS := $(addprefix $(OBJDIR)/,$(FUENTES_CPP:.cpp=.o) $(FUENTES_C:.c=.o))

# (platform/psp/include va primero: su GL/gl.h es el de la PSP, sobre el GE; el de pspdev es PSPGL, que no se usa)
INCS := -I$(RAIZ)/platform/psp/include \
        -I$(RAIZ) -I$(RAIZ)/main -I$(RAIZ)/main/app -I$(RAIZ)/main/config -I$(RAIZ)/main/io -I$(RAIZ)/main/undo \
        -I$(RAIZ)/main/ui -I$(RAIZ)/main/ui/ViewPorts -I$(RAIZ)/main/ui/GeometriaUI \
        -I$(RAIZ)/libs/Whisk3DCore -I$(RAIZ)/libs/Whisk3DCore/base -I$(RAIZ)/libs/Whisk3DCore/graphics \
        -I$(RAIZ)/libs/Whisk3DCore/io -I$(RAIZ)/libs/Whisk3DCore/thirdparty -I$(RAIZ)/libs \
        -I$(RAIZ)/libs/WhiskUI/widgets -I$(RAIZ)/libs/WhiskUI/text -I$(RAIZ)/libs/WhiskUI/draw \
        -I$(RAIZ)/libs/WhiskUI/theme -I$(RAIZ)/libs/WhiskUI/core -I$(RAIZ)/thirdparty -I$(RAIZ)/thirdparty/lua/src \
        -I$(PSPDEV)/psp/include/SDL2 -I$(PSPDEV)/psp/include -I$(PSPSDK)/include

# el MIPS Allegrex de la PSP (FPU de precision simple: el double va por software). -G0: sin datos "chicos" (el
# PSPSDK no los usa)
ARCH := -G0
# (-Umips: el gcc de MIPS define el macro 'mips' en 1, y el Core tiene variables que se llaman asi)
DEFS := -DW3D_PSP -DW3D_VERSION=\"$(VERSION)\" -D_PSP_FW_VERSION=600 -Umips
OPT  := -O2 -ffunction-sections -fdata-sections
# CAPTURA=n: despues del cuadro n la pantalla queda en captura.ppm, al lado del EBOOT.PBP. PRUEBA="x,y x,y>x2,y2":
# clicks y arrastres del cursor sin el control (captura_ppsspp.sh). Solo los usa w3d_psp.cpp: se recompila cuando
# cambian (prueba.flags)
PRUEBA_DEFS :=
ifneq ($(CAPTURA),)
PRUEBA_DEFS += -DW3D_PSP_CAPTURA=$(CAPTURA)
endif
ifneq ($(PRUEBA),)
PRUEBA_DEFS += '-DW3D_PSP_PRUEBA="$(PRUEBA)"'
endif
# (-include climits: el limits.h de newlib define LLONG_MAX solo en C; el <climits> de libstdc++ lo trae en C++. Sin
# eso Lua dice que no hay 'long long' y el lua_Integer de C y el de C++ no coincidirian)
# (-MMD -MP: cada objeto anota los headers que usa, en su .d; si un header cambia, se recompila lo que lo incluye)
CXXFLAGS_W3D := $(ARCH) $(OPT) -std=gnu++17 -include climits $(DEFS) $(INCS) -w -MMD -MP
CFLAGS_W3D   := $(ARCH) $(OPT) -std=gnu99 $(DEFS) $(INCS) -w -MMD -MP

# SDL2 de la PSP y las bibliotecas del sistema que pide (las de siempre, newlib y pthread, las agrega psp-gcc)
LIBS := -lSDL2main -lSDL2 -lpspvram -lpspaudio -lpspvfpu -lpspdisplay -lpspgu -lpspge -lpsphprm -lpspctrl -lpsppower

all: $(TARGET)

$(OBJDIR)/platform/psp/w3d_psp.o: CXXFLAGS_W3D += $(PRUEBA_DEFS)
$(OBJDIR)/platform/psp/w3d_psp.o: $(SALIDA)/prueba.flags
$(SALIDA)/prueba.flags: FORCE
	@mkdir -p $(SALIDA)
	@printf '%s|%s\n' '$(CAPTURA)' '$(PRUEBA)' | cmp -s - $@ || printf '%s|%s\n' '$(CAPTURA)' '$(PRUEBA)' > $@
FORCE:

$(OBJDIR)/%.o: $(RAIZ)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  c++ $*"
	@$(CXX) $(CXXFLAGS_W3D) -c $< -o $@

$(OBJDIR)/%.o: $(RAIZ)/%.c
	@mkdir -p $(dir $@)
	@echo "  cc  $*"
	@$(CC) $(CFLAGS_W3D) -c $< -o $@

# EL EJECUTABLE: un PRX (reubicable: el cargador lo pone donde haya lugar, como piden los firmwares modificados de
# ahora) dentro del EBOOT.PBP. psp-fixup-imports ordena las llamadas al sistema; psp-prxgen arma el PRX
$(SALIDA)/whisk3d.elf: $(OBJS)
	@echo "  link $@"
	@$(CXX) $(ARCH) -L$(PSPDEV)/psp/lib -L$(PSPSDK)/lib -specs=$(PSPSDK)/lib/prxspecs -Wl,-q,-T$(PSPSDK)/lib/linkfile.prx \
	    -Wl,-zmax-page-size=128 -Wl,--gc-sections -o $@ $(PSPSDK)/lib/prxexports.o $(OBJS) $(LIBS)
	@$(PSPDEV)/bin/psp-fixup-imports $@

$(SALIDA)/whisk3d.prx: $(SALIDA)/whisk3d.elf
	@$(PSPDEV)/bin/psp-prxgen $< $@

# PARAM.SFO: el nombre que muestra el menu (MEMSIZE=1: la memoria grande de las PSP 2000 en adelante, 52 MB en vez
# de 24)
$(SALIDA)/PARAM.SFO: FORCE
	@mkdir -p $(SALIDA)
	@$(PSPDEV)/bin/mksfoex -d MEMSIZE=1 -s APP_VER=$(APP_VER) 'Whisk3D' $(SALIDA)/PARAM.SFO.nuevo
	@cmp -s $(SALIDA)/PARAM.SFO.nuevo $@ && rm $(SALIDA)/PARAM.SFO.nuevo || mv $(SALIDA)/PARAM.SFO.nuevo $@

# ---- el icono del menu (144x80) y el fondo (480x272) ----
# PNG de 8 bits por canal y sin bloques de mas (-depth 8 -strip, PNG24): el lienzo de convert sale de 16 bits con
# cHRM/bKGD/tIME, y el decodificador del menu de la PSP es el de un PNG simple
$(SALIDA)/ICON0.PNG: $(RAIZ)/res/Whisk3D.png Makefile
	@mkdir -p $(SALIDA)
	@convert -size 144x80 xc:'#1d1d1d' \( $< -resize 72x72 \) -gravity center -composite -depth 8 -strip PNG24:$@
	@convert -size 480x272 xc:'#1d1d1d' \( $< -resize 200x200 \) -gravity center -composite -depth 8 -strip \
	    PNG24:$(SALIDA)/PIC1.PNG

$(CARPETA)/EBOOT.PBP: $(SALIDA)/whisk3d.prx $(SALIDA)/PARAM.SFO $(SALIDA)/ICON0.PNG
	@mkdir -p $(CARPETA)
	@$(PSPDEV)/bin/pack-pbp $@ $(SALIDA)/PARAM.SFO $(SALIDA)/ICON0.PNG NULL NULL $(SALIDA)/PIC1.PNG NULL \
	    $(SALIDA)/whisk3d.prx NULL > /dev/null
	@echo "  EBOOT.PBP $@ ($$(du -h $@ | cut -f1))"

# ---- res/ (sin el instalador ni los iconos de escritorio): al lado del EBOOT.PBP ----
$(SALIDA)/res.listo: $(addprefix $(RAIZ)/,$(shell cd $(RAIZ) && find res -type f))
	@rm -rf $(CARPETA)/res && mkdir -p $(CARPETA)/res
	@cp -r $(RAIZ)/res/. $(CARPETA)/res/
	@rm -rf $(CARPETA)/res/installer $(CARPETA)/res/*.ico $(CARPETA)/res/*.svg
	@touch $@

$(TARGET): $(CARPETA)/EBOOT.PBP $(SALIDA)/res.listo
	@rm -f $@ && cd $(SALIDA) && zip -qr $(notdir $@) PSP/GAME/Whisk3D -x 'PSP/GAME/Whisk3D/*.log' \
	    'PSP/GAME/Whisk3D/captura.ppm' 'PSP/GAME/Whisk3D/config.ini'
	@echo "  zip $@ ($$(du -h $@ | cut -f1))"

clean:
	rm -rf $(SALIDA)

-include $(OBJS:.o=.d)

.PHONY: all clean FORCE
