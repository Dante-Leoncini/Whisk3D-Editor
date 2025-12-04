cmake_minimum_required(VERSION 3.20)
project(Whisk3D VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Paths ---
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
message(STATUS "Project root: ${PROJECT_ROOT}")

# --- Forzar SDL2_image a usar SDL2 (no SDL3) ---
set(SDL2_IMAGE_USE_SDL3 OFF CACHE BOOL "" FORCE)

# --- Submodules ---
add_subdirectory(${PROJECT_ROOT}/external/SDL2 ${CMAKE_BINARY_DIR}/SDL2_build)
add_subdirectory(${PROJECT_ROOT}/external/SDL2_image ${CMAKE_BINARY_DIR}/SDL2_image_build)

# --- Archivos fuente ---
file(GLOB_RECURSE WHISK_SRC
    "${PROJECT_ROOT}/src/**/*.cpp"
)

# --- Headers ---
include_directories("${PROJECT_ROOT}/include")
include_directories(${PROJECT_ROOT}/external/SDL2/include)
include_directories(${PROJECT_ROOT}/external/SDL2_image)

# --- Crear ejecutable ---
add_executable(Whisk3D ${WHISK_SRC})

# --- Linkear SDL2 y SDL2_image ---
target_link_libraries(Whisk3D PRIVATE SDL2 SDL2_image SDL2main)

# --- Copiar DLLs al exe ---
if(WIN32)
    add_custom_command(TARGET Whisk3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_ROOT}/external/SDL2/build/Release"  # O Debug si compilás así
        $<TARGET_FILE_DIR:Whisk3D>
    )
    add_custom_command(TARGET Whisk3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_ROOT}/res"
        $<TARGET_FILE_DIR:Whisk3D>/res
    )
endif()
