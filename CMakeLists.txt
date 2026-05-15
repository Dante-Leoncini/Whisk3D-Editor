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

if(WIN32)

    message(STATUS "Building Whisk3D for Windows")

    list(APPEND SRC_FILES
        ${CMAKE_SOURCE_DIR}/engine/opengl/w3dOpenGL.cpp
    )

endif()

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
# CPack (Linux only)
# ---------------------------------------------------

if(UNIX AND NOT ANDROID)

    install(TARGETS whisk3d
        RUNTIME DESTINATION bin
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/
        DESTINATION share/Whisk3d/res
    )

endif()