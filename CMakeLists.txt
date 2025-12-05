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

# ---------------------------------------------------
# INSTALACION LINUX
# ---------------------------------------------------

if(WHISK3D_BUILD_LINUX)
    install(TARGETS whisk3d
        RUNTIME DESTINATION bin
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/
        DESTINATION share/Whisk3d/res
        FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.svg"
        PATTERN "*.ini"
        PATTERN "Skins/*"
    )

    install(FILES ${CMAKE_SOURCE_DIR}/main/whisk3d.desktop
        DESTINATION share/applications
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME whisk3d.svg
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME whisk3d.png
    )

    set(CPACK_GENERATOR "DEB;AppImage;RPM")

    # Deb
    set(CPACK_PACKAGE_NAME "whisk3d")
    set(CPACK_PACKAGE_VERSION "25.12.04")
    set(CPACK_PACKAGE_CONTACT "Dante Leoncini <dante_leoncini@outlook.com.ar>")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dante Leoncini <dante_leoncini@outlook.com.ar>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Whisk3D - editor y motor 3D retro multiplataforma")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Whisk3D - editor y motor 3D retro multiplataforma")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libglu1-mesa, libsdl2-2.0-0, libsdl2-image-2.0-0")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

    # rpm
    set(CPACK_RPM_PACKAGE_REQUIRES "mesa-libGL, mesa-libGLU, SDL2, SDL2_image")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Graphics")

    # Appimage
    #set(CPACK_APPIMAGE_TOOL_EXECUTABLE "/usr/bin/appimagetool")

    set(CPACK_APPIMAGE_DESKTOP_FILE "whisk3d.desktop")
    set(CPACK_APPIMAGE_ICON_FILE "whisk3d.png")
    set(CPACK_PACKAGE_ICON "whisk3d.png")
    set(CPACK_APPIMAGE_MAIN_EXECUTABLE "whisk3d")

    #  AppImageUpdate info
    #set(CPACK_APPIMAGE_UPDATE_INFORMATION "gh-releases-zsync|user|repo|latest|*.AppImage.zsync")

    include(CPack)
endif()