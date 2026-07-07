LOCAL_PATH := $(call my-dir)
MY_PATH := $(LOCAL_PATH)
PROJECT_ROOT := $(MY_PATH)/..

# ============================================================================
#  jni/Android.mk - build nativo de Whisk3D para Android via ndk-build
# ----------------------------------------------------------------------------
#  Analogo a build_web.sh pero en formato ndk-build, porque SDL2 (thirdparty/SDL2)
#  ya trae su propio Android.mk pensado para compilarse asi.
# ============================================================================

# 1) SDL2 real: importamos el Android.mk que YA viene en thirdparty/SDL2
include $(PROJECT_ROOT)/thirdparty/SDL2/Android.mk

# 2) Modulo: el editor Whisk3D, linkeado contra esa libSDL2.so
# OJO: el include de arriba (y el cpufeatures que importa adentro) PISAN LOCAL_PATH
# con su propio directorio. Hay que restaurarlo a mano antes de seguir, sino
# LOCAL_SRC_FILES/LOCAL_C_INCLUDES quedan relativos a la carpeta de SDL2/cpufeatures.
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)

LOCAL_MODULE := main
# ^ "main" por convencion de SDLActivity.java (carga libmain.so via System.loadLibrary("main")).

CORE := Whisk3DCore

# Archivos incluidos

SRC_FILES := $(shell find $(PROJECT_ROOT)/main $(PROJECT_ROOT)/libs/$(CORE)/objects \
$(PROJECT_ROOT)/libs/$(CORE)/animation $(PROJECT_ROOT)/libs/WhiskUI \
-name '*.cpp')
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/w3dFilesystem.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/w3dTexture.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/w3dlog.cpp
# SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/w3dGraphics.cpp #1.1
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/gles2/w3dGraphicsGLES2.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/math/Vector3.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/math/Quaternion.cpp
SRC_FILES += $(PROJECT_ROOT)/libs/$(CORE)/math/Matrix4.cpp

LOCAL_SRC_FILES := $(patsubst $(MY_PATH)/%,%,$(SRC_FILES))

# shim:
LOCAL_C_INCLUDES := \
$(MY_PATH)/shim \
$(PROJECT_ROOT) \
$(PROJECT_ROOT)/main \
$(PROJECT_ROOT)/libs/$(CORE) \
$(PROJECT_ROOT)/libs/$(CORE)/thirdparty \
$(PROJECT_ROOT)/libs \
$(PROJECT_ROOT)/thirdparty \
$(PROJECT_ROOT)/thirdparty/SDL2/include

LOCAL_CPP_FEATURES := exceptions rtti
LOCAL_CPPFLAGS := -std=c++17

LOCAL_SHARED_LIBRARIES := SDL2
#LOCAL_LDLIBS := -lGLESv1_CM -llog -landroid # Descomentar si en un futuro se necesitan builds 1.1
LOCAL_LDLIBS := -lGLESv2 -llog -landroid

include $(BUILD_SHARED_LIBRARY)