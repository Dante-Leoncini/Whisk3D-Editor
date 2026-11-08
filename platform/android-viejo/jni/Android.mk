LOCAL_PATH := $(call my-dir)
MY_PATH := $(LOCAL_PATH)
# ============================================================================
#  jni/Android.mk -- Whisk3D para Android 2.3 (API 10) en ARMv6, con ndk-build del NDK r15c y SDL 2.0.7.
#  Lo llama build_android_viejo.sh, que pasa SDL_DIR (las fuentes de SDL 2.0.7, fuera del proyecto) y W3D_RAIZ (la
#  raiz del repo). Las fuentes son las del Android de siempre (platform/android/jni/Android.mk): al agregar un .cpp
#  al Core, hay que sumarlo en los dos.
# ============================================================================
PROJECT_ROOT := $(W3D_RAIZ)

# 1) SDL 2.0.7 (el ultimo que anda en Android 2.3), con su propio Android.mk
include $(SDL_DIR)/Android.mk

# 2) el editor, linkeado contra esa libSDL2.so
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE := main
CORE := Whisk3DCore

# main/ entero menos main/test (el harness de pruebas corre solo en la PC; W3dRunScript va en w3d_android_viejo.cpp)
SRC_FILES := $(shell find $(PROJECT_ROOT)/main $(PROJECT_ROOT)/libs/$(CORE)/objects \
$(PROJECT_ROOT)/libs/$(CORE)/animation $(PROJECT_ROOT)/libs/WhiskUI \
-name '*.cpp' -not -path '*/main/test/*')
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/w3dFilesystem.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/graphics/w3dTexture.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/w3dCompress.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/W3dZip.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/W3dTexto.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/W3dMalla.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/W3dMallaBin.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/W3dAlmacen.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/io/W3dRecursos.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/base/w3dlog.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/base/W3dInteractionState.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/base/W3dConfig.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/input/W3dTactil.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/input/W3dVibrar.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/graphics/gles2/w3dGraphicsGLES2.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/math/Vector3.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/math/Quaternion.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/math/Matrix4.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/script/W3dScript.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/physics/W3dFisica.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/audio/W3dAudio.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/audio/W3dAudioSDL.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/audio/W3dVolumen.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/input/W3dClipboardSDL.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/physics/W3dRigido.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/physics/W3dColMalla.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/physics/W3dHitbox.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/physics/W3dRagdoll.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/physics/W3dNav.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/audio/W3dMusic.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/net/W3dRed.cpp
SRC_FILES += $(filter-out %/lua.c %/luac.c,$(wildcard $(PROJECT_ROOT)/thirdparty/lua/src/*.c))
SRC_FILES += $(MY_PATH)/w3d_android_viejo.cpp

LOCAL_SRC_FILES := $(SRC_FILES)

# shim/ primero (su <filesystem> le gana al del sistema), despues el del Android de siempre (SDL2/SDL.h, GL/*.h)
LOCAL_C_INCLUDES := \
$(MY_PATH)/shim \
$(PROJECT_ROOT)/platform/android/jni/shim \
$(PROJECT_ROOT) \
$(PROJECT_ROOT)/main \
$(PROJECT_ROOT)/main/app \
$(PROJECT_ROOT)/main/config \
$(PROJECT_ROOT)/main/io \
$(PROJECT_ROOT)/main/undo \
$(PROJECT_ROOT)/main/ui \
$(PROJECT_ROOT)/main/ui/ViewPorts \
$(PROJECT_ROOT)/main/ui/GeometriaUI \
$(PROJECT_ROOT)/libs/$(CORE) \
$(PROJECT_ROOT)/libs/$(CORE)/base \
$(PROJECT_ROOT)/libs/$(CORE)/graphics \
$(PROJECT_ROOT)/libs/$(CORE)/io \
$(PROJECT_ROOT)/libs/$(CORE)/thirdparty \
$(PROJECT_ROOT)/libs \
$(PROJECT_ROOT)/libs/WhiskUI/widgets \
$(PROJECT_ROOT)/libs/WhiskUI/text \
$(PROJECT_ROOT)/libs/WhiskUI/draw \
$(PROJECT_ROOT)/libs/WhiskUI/theme \
$(PROJECT_ROOT)/libs/WhiskUI/core \
$(PROJECT_ROOT)/thirdparty \
$(PROJECT_ROOT)/thirdparty/lua/src \
$(SDL_DIR)/include

LOCAL_CPP_FEATURES := exceptions rtti
# instrucciones ARM y no Thumb: en un ARMv6 el Thumb es el de 16 bits, sin la FPU, y el clang del r15c se cae
# armando algunas funciones grandes del editor (Properties, LayoutInput...) en ese modo
LOCAL_ARM_MODE := arm
# el ARM11 del telefono tiene FPU (VFP): sin esto armeabi hace las cuentas de punto flotante por software.
# softfp = las mismas llamadas que armeabi (anda con las librerias del sistema)
LOCAL_CFLAGS := -march=armv6 -mfpu=vfp -mfloat-abi=softfp -DW3D_ANDROID_VIEJO \
                -DLUA_USE_POSIX -DW3D_ENABLE_AUDIO '-Dl_fseek(f,o,w)=fseek(f,o,w)' '-Dl_ftell(f)=ftell(f)' -Dl_seeknum=long
LOCAL_CPPFLAGS := -std=c++1z -include $(MY_PATH)/shim/w3d_sdl207.h -DW3D_VERSION=\"$(shell date +%y.%m.%d)\"

LOCAL_SHARED_LIBRARIES := SDL2
# (-latomic: en ARMv5/v6 las operaciones atomicas de libc++ son llamadas a esa libreria)
LOCAL_LDLIBS := -lGLESv2 -llog -landroid -latomic

include $(BUILD_SHARED_LIBRARY)
