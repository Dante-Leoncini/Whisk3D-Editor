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
    # el modelo de datos (antes en core/) se unio al engine: engine/objects, animation, ui
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/objects/*.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/animation/*.cpp
    ${CMAKE_SOURCE_DIR}/libs/WhiskUI/*.cpp
)

list(APPEND SRC_FILES
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/w3dFilesystem.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/w3dTexture.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/w3dGraphics.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/w3dlog.cpp
    # matematica del engine (antes en core/math): Vector3/Quaternion/Matrix4.
    # Se incluyen como "math/X.h" (engine es include dir). No los globea core/.
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/math/Vector3.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/math/Quaternion.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/math/Matrix4.cpp
)

if(ANDROID)

    message(STATUS "Building Whisk3D for Android")

    list(APPEND SRC_FILES
        ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/gles1/w3dGLES1.cpp
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
    # capas reutilizables en libs/: "objects/Mesh.h" (Whisk3DCore) y "WhiskUI/icons.h"
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/thirdparty # stb: dep del engine, vive CON el core ("stb/stb_image.h")
    ${CMAKE_SOURCE_DIR}/libs
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
    target_compile_definitions(whisk3d PRIVATE SDL_MAIN_HANDLED)

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

    target_compile_definitions(whisk3d PRIVATE
        W3D_OPENGL
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
# INSTALACION LINUX
# ---------------------------------------------------

if(UNIX AND NOT ANDROID)
    install(TARGETS whisk3d
        RUNTIME DESTINATION bin
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/
        DESTINATION share/Whisk3d/res
        FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.svg"
        PATTERN "*.ini"
        PATTERN "Skins/*"
    )

    install(FILES ${CMAKE_SOURCE_DIR}/platform/linux/whisk3d.desktop
        DESTINATION share/applications
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME whisk3d.svg
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME whisk3d.png
    )

    set(CPACK_GENERATOR "DEB;AppImage;RPM")

    # Deb
    set(CPACK_PACKAGE_NAME "whisk3d")
    set(CPACK_PACKAGE_VERSION "25.12.04")
    set(CPACK_PACKAGE_CONTACT "Dante Leoncini <dante_leoncini@outlook.com.ar>")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dante Leoncini <dante_leoncini@outlook.com.ar>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Whisk3D - editor y motor 3D retro multiplataforma")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Whisk3D - editor y motor 3D retro multiplataforma")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libglu1-mesa, libsdl2-2.0-0, libsdl2-image-2.0-0")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

    # rpm
    set(CPACK_RPM_PACKAGE_REQUIRES "mesa-libGL, mesa-libGLU, SDL2, SDL2_image")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Graphics")

    # Appimage
    #set(CPACK_APPIMAGE_TOOL_EXECUTABLE "/usr/bin/appimagetool")

    set(CPACK_APPIMAGE_DESKTOP_FILE "whisk3d.desktop")
    set(CPACK_APPIMAGE_ICON_FILE "whisk3d.png")
    set(CPACK_PACKAGE_ICON "whisk3d.png")
    set(CPACK_APPIMAGE_MAIN_EXECUTABLE "whisk3d")

    #  AppImageUpdate info
    #set(CPACK_APPIMAGE_UPDATE_INFORMATION "gh-releases-zsync|user|repo|latest|*.AppImage.zsync")

    include(CPack)
endif()

# ---------------------------------------------------
# INSTALACION / INSTALADOR WINDOWS (CPack + NSIS)
#   genera: cpack -G NSIS  -> Whisk3D-<ver>-win64.exe  (necesita NSIS)
#   tambien: cpack -G ZIP   -> portable .zip (sin tools extra)
# ---------------------------------------------------

if(WIN32)
    # el exe + los recursos van JUNTOS: GetResDir() busca ./res al lado del exe
    install(TARGETS whisk3d RUNTIME DESTINATION .)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/
        DESTINATION res
        FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.svg"
        PATTERN "*.ico"
        PATTERN "*.ini"
        PATTERN "Skins/*"
    )

    # runtime de Visual C++ adentro del instalador (que no falte msvcp/vcruntime)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "Whisk3D")
    set(CPACK_PACKAGE_VENDOR "Dante Leoncini")
    # version = fecha AAAA.MM.DD del momento en que se CONFIGURA (cmake -S -B).
    # Asi el .exe queda como Whisk3D-2026.06.20-win64.exe y se sabe que build es.
    # (Para refrescar la fecha hay que reconfigurar, no solo --build.)
    string(TIMESTAMP CPACK_PACKAGE_VERSION "%Y.%m.%d")
    set(CPACK_PACKAGE_CONTACT "Dante Leoncini <dante_leoncini@outlook.com.ar>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Whisk3D - editor y motor 3D retro multiplataforma")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Whisk3D")
    # pagina de Licencia del instalador = el texto de LICENSE.md (MIT)
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")

    # accesos directos (Menu Inicio + Escritorio) al exe, que esta en la RAIZ de
    # $INSTDIR. NO usamos CPACK_PACKAGE_EXECUTABLES porque arma el acceso a
    # $INSTDIR\bin\whisk3d.exe (y el exe no esta en bin\). Mismo escaping que la
    # asociacion: \\\\ = una barra en el .nsi final.
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "
        CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Whisk3D.lnk' '$INSTDIR\\\\whisk3d.exe'
        CreateShortCut '$DESKTOP\\\\Whisk3D.lnk' '$INSTDIR\\\\whisk3d.exe'
    ")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "
        Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Whisk3D.lnk'
        Delete '$DESKTOP\\\\Whisk3D.lnk'
    ")

    # iconos del instalador / desinstalador / Agregar-o-quitar-programas
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/res/Whisk3D.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/res/Whisk3D.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "whisk3d.exe")

    # imagenes del instalador (BMP 24-bit; NSIS NO acepta png/jpg). Placeholders
    # en blanco editables en res/installer/ (ver LEEME.txt). Tamaños exactos:
    #   header.bmp 150x57  |  welcome.bmp / uninstall.bmp 164x314
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/res/installer/header.bmp")
    set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/res/installer/welcome.bmp")
    set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/res/installer/uninstall.bmp")
    set(CPACK_NSIS_DISPLAY_NAME "Whisk3D")
    set(CPACK_NSIS_PACKAGE_NAME "Whisk3D")
    set(CPACK_NSIS_CONTACT "dante_leoncini@outlook.com.ar")

    # ACTUALIZAR: al instalar una version nueva, desinstala la anterior primero
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)

    # ASOCIAR los .w3d con Whisk3D: doble click -> abre el modelo (argv[1]).
    # Se escriben en HKCR (el instalador NSIS corre elevado). $INSTDIR = carpeta
    # de instalacion. SHChangeNotify refresca el icono en el Explorador.
    # (corchetes [=[ ]=] = literal CMake: sin escapar barras ni comillas)
    # NSIS usa comillas SIMPLES para los strings (asi no hay que escapar las
    # dobles). \\\\ = una barra en NSIS; \\\" = una comilla literal (las del
    # comando, para rutas con espacios). El doble round-trip CMake->CPackConfig
    # ->plantilla.nsi consume los backslashes (por eso van de a 4).
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.w3d' '' 'Whisk3D.Model'
        WriteRegStr HKCR 'Whisk3D.Model' '' 'Modelo 3D de Whisk3D'
        WriteRegStr HKCR 'Whisk3D.Model\\\\DefaultIcon' '' '$INSTDIR\\\\res\\\\Whisk3D.ico'
        WriteRegStr HKCR 'Whisk3D.Model\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\whisk3d.exe\\\" \\\"%1\\\"'
        System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR '.w3d'
        DeleteRegKey HKCR 'Whisk3D.Model'
        System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
    ")

    include(CPack)
endif()