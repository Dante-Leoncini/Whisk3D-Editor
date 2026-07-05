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
# Exportar variable
# ===============================
set(WHISK3D_THIRDPARTY_LIBS "${WHISK3D_THIRDPARTY_LIBS}" PARENT_SCOPE)