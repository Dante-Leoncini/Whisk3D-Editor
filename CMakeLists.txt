cmake_minimum_required(VERSION 3.20)
project(Whisk3D VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Project root ---
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
message(STATUS "Project root: ${PROJECT_ROOT}")

# --- Submodules ---
add_subdirectory(${PROJECT_ROOT}/external/SDL2)
add_subdirectory(${PROJECT_ROOT}/external/SDL2_image)

# --- Archivos fuente ---
file(GLOB_RECURSE WHISK_SRC
    "${PROJECT_ROOT}/src/**/*.cpp"
)

# --- Include directories ---
include_directories(
    "${PROJECT_ROOT}/include"
    "${PROJECT_ROOT}/external/SDL2/include"
    "${PROJECT_ROOT}/external/SDL2_image"
)

# --- Crear ejecutable ---
add_executable(Whisk3D ${WHISK_SRC})

# --- Linkear SDL2 + SDL2_image ---
target_link_libraries(Whisk3D PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2_image::SDL2_image)

# --- Copiar DLLs de SDL2 y recursos ---
if(WIN32)
    add_custom_command(TARGET Whisk3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_ROOT}/external/SDL2/bin"
        $<TARGET_FILE_DIR:Whisk3D>
    )

    add_custom_command(TARGET Whisk3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_ROOT}/res"
        $<TARGET_FILE_DIR:Whisk3D>/res
    )
endif()

# --- Carpeta de salida ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT}/build/windows/$<IF:$<CONFIG:Debug>,Debug,Release>")
