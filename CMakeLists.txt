cmake_minimum_required(VERSION 3.10)
project(Whisk3D)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------
# Build type por defecto
# ---------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Flags de depuración
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

# ---------------------------------------------------
# Opciones de plataforma
# ---------------------------------------------------
option(WHISK3D_BUILD_LINUX "Build Linux version" ON)
option(WHISK3D_BUILD_WINDOWS "Build Windows version" OFF)
option(WHISK3D_BUILD_ANDROID "Build Android version" OFF)

# ---------------------------------------------------
# Buscar todos los .cpp del proyecto (main y core)
# ---------------------------------------------------
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/main/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.cpp
)

# ---------------------------------------------------
# Ejecutable principal
# ---------------------------------------------------
add_executable(whisk3d ${SRC_FILES})

# ---------------------------------------------------
# Includes: raíz + main + core + thirdparty
# ---------------------------------------------------
target_include_directories(whisk3d PRIVATE
    ${CMAKE_SOURCE_DIR}          
    ${CMAKE_SOURCE_DIR}/main 
    ${CMAKE_SOURCE_DIR}/core 
    ${CMAKE_SOURCE_DIR}/thirdparty
)

# ---------------------------------------------------
# Copiar carpeta de recursos al build
# ---------------------------------------------------
add_custom_command(
    TARGET whisk3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/res
            $<TARGET_FILE_DIR:whisk3d>/res
    COMMENT "Copiando carpeta 'res' al build"
)

# ---------------------------------------------------
# THIRD-PARTY: SDL2, SDL2_image, tinyfiledialogs
# ---------------------------------------------------
add_subdirectory(thirdparty)
target_link_libraries(whisk3d PRIVATE ${WHISK3D_THIRDPARTY_LIBS})

# ---------------------------------------------------
# CONFIGURACIÓN SEGÚN PLATAFORMA
# ---------------------------------------------------
if(WHISK3D_BUILD_LINUX)
    message(STATUS "Building Whisk3D for Linux")
    target_compile_definitions(whisk3d PRIVATE WHISK3D_LINUX=1)

    cmake_policy(SET CMP0072 NEW)   # evita warning de GLVND
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)

    # Linkear OpenGL y GLU
    target_link_libraries(whisk3d PRIVATE pthread dl OpenGL::GL GLU)
endif()

if(WHISK3D_BUILD_WINDOWS)
    message(STATUS "Building Whisk3D for Windows")
    target_compile_definitions(whisk3d PRIVATE WHISK3D_WINDOWS=1)
endif()

if(WHISK3D_BUILD_ANDROID)
    message(STATUS "Building Whisk3D for Android")
    target_compile_definitions(whisk3d PRIVATE WHISK3D_ANDROID=1)
endif()