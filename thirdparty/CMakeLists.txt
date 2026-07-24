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
add_subdirectory(SDL2)

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