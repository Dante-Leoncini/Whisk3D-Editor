cmake_minimum_required(VERSION 3.10)
project(Whisk3DAndroid)

add_subdirectory(../../../../external/SDL3 SDL3build)

file(GLOB WHISK3D_SRC
    ../../../../src/*.cpp
    ../../../../src/*/*.cpp
    ../../../../src/*/*/*.cpp
)

add_library(whisk3d SHARED ${WHISK3D_SRC})

target_link_libraries(whisk3d
    SDL3::SDL3
    GLESv2        # cambiamos a GLESv1 si tu motor aún es ES1.1
    android
    log
)