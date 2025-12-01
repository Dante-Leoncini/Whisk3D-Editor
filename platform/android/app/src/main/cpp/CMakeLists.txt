cmake_minimum_required(VERSION 3.10)
project(Whisk3DAndroid)

# Rutas
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
set(SDL2_PATH ${PROJECT_ROOT}/external/SDL2)
set(SDL2_IMAGE_PATH ${PROJECT_ROOT}/external/SDL2_image)

# Comprobar SDL2
if(NOT EXISTS ${SDL2_PATH})
    message(FATAL_ERROR "❌ NO SE ENCONTRÓ SDL2 en: ${SDL2_PATH}")
endif()

# SDL2 como subdirectorio
add_subdirectory(${SDL2_PATH} SDL2_build)

# SDL2_image necesita saber dónde está SDL2
set(SDL2_IMAGE_SDL2_LIBRARY SDL2::SDL2)
set(SDL2_IMAGE_SDL2_INCLUDE_DIR ${SDL2_PATH}/include)
add_subdirectory(${SDL2_IMAGE_PATH} SDL2_image_build)

# Fuentes
file(GLOB_RECURSE WHISK3D_SRC
    ${PROJECT_ROOT}/src/*.cpp
)

add_library(whisk3d SHARED
    main.cpp
    ${WHISK3D_SRC}
)

target_include_directories(whisk3d PRIVATE
    ${PROJECT_ROOT}/src
    ${PROJECT_ROOT}/include
)

target_link_libraries(whisk3d
    SDL2::SDL2
    SDL2_image::SDL2_image
    GLESv1_CM
    android
    log
)
