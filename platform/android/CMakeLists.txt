cmake_minimum_required(VERSION 3.14.7 FATAL_ERROR)
cmake_policy(VERSION 3.14.7...3.28.1)

set(PROJECT_NAME hello_sdl2)
project(${PROJECT_NAME})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Make sure all configurations output libraries in the same directory instead of in sub-directories per config
# This allows easier linking from the Xcode iOS project
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

if (IOS)
  # Disable ImageIO for SDL2_image as it's not available for iOS, just OS X, and will fail at link time
  set(SDL2IMAGE_BACKEND_IMAGEIO 0)
endif()

include(FetchContent)

FetchContent_Declare(
  SDL
  URL            https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.10.zip
  SOURCE_DIR     ${CMAKE_BINARY_DIR}/external/SDL2
)

FetchContent_Declare(
  SDL_IMAGE
  URL            https://github.com/libsdl-org/SDL_image/archive/release-2.8.2.zip
  SOURCE_DIR     ${CMAKE_BINARY_DIR}/external/SDL2_image
)

# Only build the shared library for SDL2
set(BUILD_SHARED_LIBS ON)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
set(SDL_TEST_ENABLED_BY_DEFAULT OFF)

# SDL main is only needed for desktop apps (more specifically Windows)
if (IOS OR ANDROID)
  set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
endif()

# Make sure debug libs have the same name as release ones
set(SDL_CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
set(SDL2IMAGE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(SDL SDL_IMAGE)

# Making sure libraries don't generate symlinks
set_property(TARGET SDL2 PROPERTY VERSION)
set_property(TARGET SDL2 PROPERTY SOVERSION)
set_property(TARGET SDL2_image PROPERTY VERSION)
set_property(TARGET SDL2_image PROPERTY SOVERSION)

# Copying needed files for the android project
set(SDL_ACTIVITY_FOLDER app/src/main/java/org/libsdl/app/)
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/android/${SDL_ACTIVITY_FOLDER})
file(COPY ${CMAKE_BINARY_DIR}/external/SDL2/android-project/${SDL_ACTIVITY_FOLDER} DESTINATION ${CMAKE_SOURCE_DIR}/android/${SDL_ACTIVITY_FOLDER})
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/android/app/src/main/assets/res)
file(COPY ${CMAKE_SOURCE_DIR}/res DESTINATION ${CMAKE_SOURCE_DIR}/android/app/src/main/assets)

# Find sources recursively
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")
file(GLOB_RECURSE HEADER_FILES "src/*.h")

# Add the folders with headers to the include path
foreach (_headerFile ${HEADER_FILES})
    get_filename_component(_dir ${_headerFile} PATH)
    list (APPEND INCLUDE_DIRS ${_dir})
endforeach()

include_directories(${INCLUDE_DIRS})

if (IOS) 
  # Remove main.cpp as it's manually added to the XCode project directly as the entry point
  list(REMOVE_ITEM SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/main.cpp)

  add_library( ${PROJECT_NAME} SHARED ${SOURCE_FILES} )
elseif(ANDROID)
  add_library( ${PROJECT_NAME} SHARED ${SOURCE_FILES})

  # Link libraries for Android GLES1
  target_link_libraries(${PROJECT_NAME}
      SDL2
      SDL2_image
      GLESv1_CM   # <- OpenGL ES 1.1
      EGL         # <- para inicializar el contexto GLES
      log         # <- necesario en Android para printf/log
      android     # <- para algunas llamadas nativas Android
  )
else()
  add_executable( ${PROJECT_NAME} ${SOURCE_FILES} )
  target_link_libraries( ${PROJECT_NAME} SDL2main )
endif()

target_link_libraries( ${PROJECT_NAME} SDL2 SDL2_image )