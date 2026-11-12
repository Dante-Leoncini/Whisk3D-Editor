set(WHISK3D_THIRDPARTY_LIBS "")

# ===============================
# FORZAR COMPILACIÓN ESTÁTICA
# ===============================
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
set(SDL_STATIC_ENABLED_BY_DEFAULT ON)

set(BUILD_SHARED_LIBS OFF)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)

# ===============================
# SDL2
# ===============================
# SDL2 is vendored upstream code. Keep warnings from our sources enabled, but
# don't turn new Apple SDK deprecations inside the vendor into project noise.
# This is scoped to the SDL target and only on Apple; no source patch is carried.
# New CMake versions also diagnose SDL2's historical policy minimum.
# CMake explicitly supports raising the effective policy version for an
# unmodified third-party subdirectory through CMAKE_POLICY_VERSION_MINIMUM.
if(DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(W3D_SAVED_POLICY_VERSION_MINIMUM "${CMAKE_POLICY_VERSION_MINIMUM}")
    set(W3D_HAD_POLICY_VERSION_MINIMUM TRUE)
else()
    set(W3D_HAD_POLICY_VERSION_MINIMUM FALSE)
endif()
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
add_subdirectory(SDL2)
if(W3D_HAD_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM "${W3D_SAVED_POLICY_VERSION_MINIMUM}")
else()
    unset(CMAKE_POLICY_VERSION_MINIMUM)
endif()
unset(W3D_SAVED_POLICY_VERSION_MINIMUM)
unset(W3D_HAD_POLICY_VERSION_MINIMUM)

if(APPLE)
    foreach(W3D_SDL_TARGET SDL2-static SDL2)
        if(TARGET ${W3D_SDL_TARGET})
            target_compile_options(${W3D_SDL_TARGET} PRIVATE
                -Wno-deprecated-declarations
                -Wno-gnu-folding-constant
            )
        endif()
    endforeach()
endif()

# SDL2 puede exportar SDL2-static o SDL2
if (TARGET SDL2-static)
    list(APPEND WHISK3D_THIRDPARTY_LIBS SDL2-static)
elseif (TARGET SDL2)
    list(APPEND WHISK3D_THIRDPARTY_LIBS SDL2)
endif()

# (tinyfiledialogs se borro: los dialogos de archivo nativos los reemplaza el
#  File browser COMPARTIDO, main/ViewPorts/PopUp/FileBrowser)

# ===============================
# LUA 5.4 (interprete de scripts del Core; C plano, compila en cualquier lado)
# ===============================
file(GLOB LUA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/lua/src/*.c)
list(REMOVE_ITEM LUA_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/src/lua.c    # el REPL standalone no va en la lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/src/luac.c)  # el compilador es un ejecutable aparte
add_library(w3dlua STATIC ${LUA_SRC})
target_include_directories(w3dlua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lua/src)
if(UNIX)
    target_compile_definitions(w3dlua PRIVATE LUA_USE_POSIX)
endif()
list(APPEND WHISK3D_THIRDPARTY_LIBS w3dlua)

# luac: el COMPILADOR de lua (el panel "Compilar juego" del editor lo invoca para
# convertir los .lua legibles del proyecto en .luac). Solo PC.
add_executable(luac ${CMAKE_CURRENT_SOURCE_DIR}/lua/src/luac.c)
target_link_libraries(luac PRIVATE w3dlua)
if(UNIX)
    target_link_libraries(luac PRIVATE m)
endif()

# ===============================
# Exportar variable
# ===============================
set(WHISK3D_THIRDPARTY_LIBS "${WHISK3D_THIRDPARTY_LIBS}" PARENT_SCOPE)