# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(sdl-populate NONE)



include(ExternalProject)
ExternalProject_Add(sdl-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.10.zip"
                    SOURCE_DIR          "/home/dante/Documentos/personal/Whisk3D/android/app/.cxx/Debug/6l6zoi28/x86/external/SDL2"
                    BINARY_DIR          "/home/dante/Documentos/personal/Whisk3D/android/app/.cxx/Debug/6l6zoi28/x86/_deps/sdl-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


