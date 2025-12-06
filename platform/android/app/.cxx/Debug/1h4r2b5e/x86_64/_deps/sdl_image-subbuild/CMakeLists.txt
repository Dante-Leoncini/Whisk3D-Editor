# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(sdl_image-populate NONE)



include(ExternalProject)
ExternalProject_Add(sdl_image-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://github.com/libsdl-org/SDL_image/archive/release-2.8.2.zip"
                    SOURCE_DIR          "/home/dante/Documentos/personal/Whisk3D/platform/android/app/.cxx/Debug/1h4r2b5e/x86_64/external/SDL2_image"
                    BINARY_DIR          "/home/dante/Documentos/personal/Whisk3D/platform/android/app/.cxx/Debug/1h4r2b5e/x86_64/_deps/sdl_image-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


