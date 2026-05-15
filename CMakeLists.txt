cmake_minimum_required(VERSION 3.10)

project(Whisk3D)

# ---------------------------------------------------
# C++
# ---------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------
# Build type
# ---------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ---------------------------------------------------
# MSVC
# ---------------------------------------------------

if(MSVC)
    add_compile_options(/W0)
endif()

# ---------------------------------------------------
# Debug flags
# ---------------------------------------------------

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od")

# ---------------------------------------------------
# Sources
# ---------------------------------------------------

file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/main/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.cpp
)

# ---------------------------------------------------
# Platform specific renderer
# ---------------------------------------------------

if(WIN32 OR (UNIX AND NOT ANDROID))

    list(APPEND SRC_FILES
        ${CMAKE_SOURCE_DIR}/engine/opengl/w3dOpenGL.cpp
    )

endif()

list(APPEND SRC_FILES
    ${CMAKE_SOURCE_DIR}/engine/w3dFilesystem.cpp
)

if(ANDROID)

    message(STATUS "Building Whisk3D for Android")

    list(APPEND SRC_FILES
        ${CMAKE_SOURCE_DIR}/engine/gles1/w3dGLES1.cpp
    )

endif()

# ---------------------------------------------------
# Remove Android-only helpers on non-Android
# ---------------------------------------------------

if(NOT ANDROID)

    list(REMOVE_ITEM SRC_FILES
        ${CMAKE_SOURCE_DIR}/main/render/GLES_Android_helpers.cpp
    )

endif()

# ---------------------------------------------------
# Executable
# ---------------------------------------------------

add_executable(whisk3d ${SRC_FILES})

# ---------------------------------------------------
# Include directories
# ---------------------------------------------------

target_include_directories(whisk3d PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/main
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/thirdparty
)

# ---------------------------------------------------
# Compile definitions
# ---------------------------------------------------

if(WIN32)

    target_compile_definitions(whisk3d PRIVATE
        W3D_OPENGL
        _CRT_SECURE_NO_WARNINGS
    )

endif()

if(ANDROID)

    target_compile_definitions(whisk3d PRIVATE
        W3D_GLES1
    )

endif()

# ---------------------------------------------------
# OpenGL (Windows)
# ---------------------------------------------------

if(WIN32)

    target_link_libraries(whisk3d PRIVATE
        opengl32
        glu32
    )
    target_compile_definitions(whisk3d PRIVATE SDL_MAIN_HANDLED)

endif()

# ---------------------------------------------------
# OpenGL (Linux)
# ---------------------------------------------------

if(UNIX AND NOT ANDROID)

    find_package(OpenGL REQUIRED)

    target_link_libraries(whisk3d PRIVATE
        pthread
        dl
        OpenGL::GL
        GLU
    )

    target_compile_definitions(whisk3d PRIVATE
        W3D_OPENGL
    )

endif()

# ---------------------------------------------------
# Thirdparty
# ---------------------------------------------------

add_subdirectory(thirdparty)

target_link_libraries(whisk3d PRIVATE
    ${WHISK3D_THIRDPARTY_LIBS}
)

# ---------------------------------------------------
# Copy resources
# ---------------------------------------------------

add_custom_command(
    TARGET whisk3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:whisk3d>/res
)

# ---------------------------------------------------
# INSTALACION LINUX
# ---------------------------------------------------

if(UNIX AND NOT ANDROID)
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