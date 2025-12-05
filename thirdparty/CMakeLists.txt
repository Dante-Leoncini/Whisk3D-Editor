set(WHISK3D_THIRDPARTY_LIBS "")

# ===============================
# FORZAR COMPILACIÓN ESTÁTICA
# ===============================
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
set(SDL_STATIC_ENABLED_BY_DEFAULT ON)

set(BUILD_SHARED_LIBS OFF)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)

set(SDL2IMAGE_BUILD_SHARED OFF)
set(SDL2IMAGE_BUILD_STATIC ON)

set(SDL2IMAGE_SDL2_FOUND TRUE)

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

# ===============================
# SDL2_image
# ===============================
add_subdirectory(SDL2_image)

if (TARGET SDL2_image-static)
    list(APPEND WHISK3D_THIRDPARTY_LIBS SDL2_image-static)
elseif (TARGET SDL2_image)
    list(APPEND WHISK3D_THIRDPARTY_LIBS SDL2_image)
endif()

# ===============================
# tinyfiledialogs
# ===============================
if(NOT WHISK3D_ANDROID)
    set(TINYFD_SRC "${CMAKE_CURRENT_SOURCE_DIR}/tinyfiledialogs/tinyfiledialogs.c")
    set(TINYFD_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/tinyfiledialogs")

    # Lo agregamos como biblioteca estática
    add_library(tinyfiledialogs STATIC ${TINYFD_SRC})
    target_include_directories(tinyfiledialogs PUBLIC ${TINYFD_INCLUDE})
    list(APPEND WHISK3D_THIRDPARTY_LIBS tinyfiledialogs)
endif()

# ===============================
# Exportar variable
# ===============================
set(WHISK3D_THIRDPARTY_LIBS "${WHISK3D_THIRDPARTY_LIBS}" PARENT_SCOPE)