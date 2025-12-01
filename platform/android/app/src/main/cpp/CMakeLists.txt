cmake_minimum_required(VERSION 3.10)
project(Whisk3DAndroid)

# SDL2 — ruta correcta (5 niveles hacia arriba)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../external/SDL2 SDL2build)

# SDL2 IMAGE — ruta correcta (5 niveles hacia arriba)
set(SDL2_IMAGE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../external/SDL2_image)

# Si no existe → ERROR claro
if(NOT EXISTS ${SDL2_IMAGE_PATH})
    message(FATAL_ERROR "❌ NO SE ENCONTRÓ SDL2_image en: ${SDL2_IMAGE_PATH}")
endif()

add_library(SDL2_image STATIC
    ${SDL2_IMAGE_PATH}/SDL_image.c
)

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