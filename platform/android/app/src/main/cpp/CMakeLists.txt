cmake_minimum_required(VERSION 3.10)
project(Whisk3DAndroid)

# SDL2 — ruta correcta (5 niveles hacia arriba)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../external/SDL2 SDL2build)

# ===========================
# SDL2_image (rama SDL2)
# ===========================
set(SDL2_IMAGE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../external/SDL2_image)

if(NOT EXISTS ${SDL2_IMAGE_PATH})
    message(FATAL_ERROR "❌ NO SE ENCONTRÓ SDL2_image en: ${SDL2_IMAGE_PATH}")
endif()

add_subdirectory(${SDL2_IMAGE_PATH} SDL2_image_build)

# Fuentes del motor (si tu código real está en include/Shared/)
file(GLOB_RECURSE WHISK3D_SRC
    ../../../../../src/*.cpp
)

# Main de Android
add_library(whisk3d SHARED
    main.cpp            # este vive en src/main/cpp/
    ${WHISK3D_SRC}
)

# Headers del engine
target_include_directories(whisk3d PRIVATE
    ../../../../../src
    ../../../../../include
)

target_link_libraries(whisk3d
    SDL2::SDL2
    SDL2_image                    
    GLESv1_CM
    android
    log
)