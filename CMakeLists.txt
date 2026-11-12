cmake_minimum_required(VERSION 3.10)

project(Whisk3D)

# ---------------------------------------------------
# C++
# ---------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Version = fecha de compilacion YY.MM.DD (mismo criterio que el APK / .deb / .exe). Se usa en el titulo de
# ventana y en el header del .obj exportado (asi una captura dice de que version es).
# OJO: string(TIMESTAMP) corre al CONFIGURAR, una sola vez. Con un build dir configurado hace dias, el binario
# salia con la fecha vieja. Por eso la version se genera en un header, en CADA build (el script solo reescribe
# el archivo si la fecha cambio, asi no fuerza un recompile por build).
set(W3D_VER_HDR "${CMAKE_BINARY_DIR}/w3dVersionGen.h")
add_custom_target(w3dversion ALL
    COMMAND ${CMAKE_COMMAND} "-DOUT=${W3D_VER_HDR}" -P "${CMAKE_SOURCE_DIR}/cmake/W3dGenVersion.cmake"
    BYPRODUCTS "${W3D_VER_HDR}"
    COMMENT "Actualizando la version (fecha del build)")
# el .cpp lo incluye si esta este define; Android y Symbian siguen pasando W3D_VERSION a mano
add_compile_definitions(W3D_VERSION_GEN)
include_directories("${CMAKE_BINARY_DIR}")

# ---------------------------------------------------
# Build type
# ---------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ---------------------------------------------------
# Compiler warnings
# ---------------------------------------------------
# Warning policy belongs to our executable, not to vendored dependencies.
# Keep MSVC diagnostics enabled instead of globally silencing the entire tree.
# ---------------------------------------------------
# Debug flags
# ---------------------------------------------------

# /Od es flag de MSVC (Windows). En Linux/gcc con -DCMAKE_BUILD_TYPE=Debug rompe -> solo aplicarlo con MSVC.
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od")
endif()

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
    # scripts LUA del Core (estilo Unity: un .lua por objeto)
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/script/W3dScript.cpp
    # fisica minima del Core (velocidad + rebotes AABB) que usan los scripts lua. Va SIEMPRE
    # junto a W3dScript.cpp: es el que registra sus binds (sin este .cpp el link falla).
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/physics/W3dFisica.cpp
    # cuerpos rigidos del Core (cajas + impulsos + raycast + eventos a lua)
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/physics/W3dRigido.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/physics/W3dColMalla.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/physics/W3dHitbox.cpp
    # ragdolls caseros (particulas + articulaciones con limites) e IK de dos huesos / FABRIK
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/physics/W3dRagdoll.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/physics/W3dNav.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/animation/W3dLocomocion.cpp
    # jugar en red (LAN, wifi, internet: UDP). Va junto a W3dScript.cpp: registra los binds red*
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/net/W3dRed.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/w3dFilesystem.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/w3dTexture.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/w3dCompress.cpp
    # zip minimo del Core: el formato de proyecto .w3d es un ZIP estandar
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/W3dZip.cpp
    # .w3dm: el formato de GEOMETRIA propio (texto por lineas). W3dTexto es su
    # escaner/escritor de numeros (round-trip exacto de float32, cero allocs).
    # OJO: los .cpp del Core de io/ NO entran por GLOB, se listan a mano aca.
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/W3dTexto.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/W3dMalla.cpp
    # .w3db: la MISMA geometria en binario lista para memoria (el formato por defecto al
    # guardar). W3dMalla.cpp lo llama (el freno REQ), asi que van SIEMPRE juntos.
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/W3dMallaBin.cpp
    # almacen del proyecto (contenedor .w3d montado / carpeta): los backends del
    # formato v4. El REGISTRO del montaje vive en w3dFilesystem.cpp, asi que los
    # runtimes que no lo usan no necesitan este .cpp.
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/W3dAlmacen.cpp
    # almacen de RECURSOS (refcount + streaming): la contabilidad generica que
    # comparten texturas (TexturaCache es su fachada), mallas y animaciones.
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io/W3dRecursos.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/opengl/w3dGraphics.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/base/w3dlog.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/base/W3dInteractionState.cpp
    # configuracion persistente del Core (clave=valor): W3dScript.cpp la usa para
    # los bindings de config/mute de lua. Sin este .cpp el link falla (w3dEngine::Config*).
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/base/W3dConfig.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/input/W3dVibrar.cpp
    # la pantalla tactil de las plataformas sin SDL (Symbian); el editor le engancha su receptor en todas
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/input/W3dTactil.cpp
    # matematica del engine (antes en core/math): Vector3/Quaternion/Matrix4.
    # Se incluyen como "math/X.h" (engine es include dir). No los globea core/.
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/math/Vector3.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/math/Quaternion.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/math/Matrix4.cpp
    # AUDIO (efectos del juego): mixer portable del Core + salida SDL2. Sin -DW3D_ENABLE_AUDIO
    # todo es stub no-op, asi que agregarlos no cambia nada hasta habilitar el flag (abajo).
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/audio/W3dAudio.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/audio/W3dAudioSDL.cpp
    # VOLUMEN maestro + volumen inicial por-proyecto (VolumenAplicarProyecto, lo llama import_w3d.cpp) y el
    # dispatcher de musica (W3dMusicRefrescarVolumenes, que W3dVolumen usa). Sin W3D_ENABLE_MUSIC = stubs no-op.
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/audio/W3dVolumen.cpp
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/audio/W3dMusic.cpp
    # PORTAPAPELES del sistema (Core): backend SDL (en Symbian se usa W3dClipboardSymbian.cpp via .mmp)
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/input/W3dClipboardSDL.cpp
)

# BACKEND DE SHADERS EN PC (para comparar): -DW3D_PC_GLES2=ON compila el editor de escritorio con el backend que
# usan la web y Android (graphics/gles2/w3dGraphicsGLES2.cpp) en vez del fijo. Asi el harness puede dibujar la MISMA escena
# con los dos y comparar las capturas (uishot) sin pasar por el navegador ni el telefono. En un build dir aparte.
option(W3D_PC_GLES2 "PC con el backend de shaders de web/Android (para comparar con el fijo)" OFF)
if(W3D_PC_GLES2)
    list(REMOVE_ITEM SRC_FILES ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/opengl/w3dGraphics.cpp)
    list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/gles2/w3dGraphicsGLES2.cpp)
    add_compile_definitions(W3D_WEBGL)
endif()

# VARIOS BACKENDS EN UN EJECUTABLE (libs/Whisk3DCore/graphics/w3dBackend.h): en la PC el editor trae OpenGL fijo Y OpenGL ES 2.0
# (y despues Vulkan); el que se usa se elige AL ARRANCAR (Ajustes > Graphics). Cada backend se compila en su namespace
# (w3dEngine::gl / ::es2) y graphics/w3dGraphicsDespacho.cpp reparte las llamadas al activo. (Con W3D_PC_GLES2 queda uno solo.)
if(NOT W3D_PC_GLES2 AND (WIN32 OR (UNIX AND NOT APPLE AND NOT ANDROID)) AND NOT EMSCRIPTEN)
    option(W3D_MULTIBACKEND "Varios backends graficos en el ejecutable, elegidos al arrancar" ON)
else()
    set(W3D_MULTIBACKEND OFF)
endif()
if(W3D_MULTIBACKEND)
    list(APPEND SRC_FILES
        ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/gles2/w3dGraphicsGLES2.cpp
        ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/w3dGraphicsDespacho.cpp)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/opengl/w3dGraphics.cpp PROPERTIES COMPILE_DEFINITIONS "W3D_BACKEND_NS=gl")
    set_source_files_properties(${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/gles2/w3dGraphicsGLES2.cpp PROPERTIES COMPILE_DEFINITIONS "W3D_BACKEND_NS=es2")
    add_compile_definitions(W3D_MULTIBACKEND)
    message(STATUS "Whisk3D: varios backends graficos (OpenGL + OpenGL ES 2.0), elegidos al arrancar")
    # VULKAN (graphics/vulkan/w3dVk.h): el backend de ES 2.0 compilado otra vez sobre un ES 2.0 hecho con Vulkan. Nada se enlaza: el
    # cargador lo abre la ventana (SDL) y el compilador de shaders (shaderc_shared, del SDK) va al lado del ejecutable;
    # sin ellos el editor sigue con OpenGL. Los headers de Vulkan vienen en libs/Whisk3DCore/thirdparty/vulkan.
    # (32 bits -Windows 7 / XP, netbooks-: apagado por defecto; el SDK de Vulkan trae shaderc solo de 64 bits)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(W3D_VULKAN_DEFAULT ON)
    else()
        set(W3D_VULKAN_DEFAULT OFF)
    endif()
    option(W3D_VULKAN "Backend Vulkan (opcional al arrancar: sin el driver o sin shaderc queda OpenGL)" ${W3D_VULKAN_DEFAULT})
    if(W3D_VULKAN AND EXISTS ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/thirdparty/vulkan/include/vulkan/vulkan_core.h)
        list(APPEND SRC_FILES
            ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/vulkan/w3dGraphicsVk.cpp
            ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/vulkan/w3dVkGL.cpp)
        set_source_files_properties(${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/vulkan/w3dGraphicsVk.cpp PROPERTIES COMPILE_DEFINITIONS "W3D_BACKEND_NS=vk;W3D_VKGL")
        set_source_files_properties(${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/vulkan/w3dVkGL.cpp ${CMAKE_SOURCE_DIR}/main/render/RTGpu.cpp PROPERTIES
            INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/thirdparty/vulkan/include)
        add_compile_definitions(W3D_VULKAN)
        set(W3D_VULKAN_ON ON)
        message(STATUS "Whisk3D: backend Vulkan (opcional: necesita el driver y shaderc_shared al lado del ejecutable)")
    endif()
endif()

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

if(MSVC)
    target_compile_options(whisk3d PRIVATE /W4)
endif()

# el header de version se regenera antes de compilar (ver el bloque de arriba)
add_dependencies(whisk3d w3dversion)

# AUDIO habilitado (efectos del juego via SDL2, ya linkeado). Sin esto el modulo es stub.
target_compile_definitions(whisk3d PRIVATE W3D_ENABLE_AUDIO)

# MCP (whisk3d --mcp, main/test/W3dMcp.cpp): el editor atiende a una IA por stdin/stdout. Solo en la PC: Android y la
# web no lo compilan (y Symbian, Dreamcast y 3DS tienen su propio build, sin main/test)
if(NOT ANDROID AND NOT EMSCRIPTEN)
    target_compile_definitions(whisk3d PRIVATE W3D_MCP)
endif()

# Whisk3DCore owns graphics backend selection and native graphics dependencies.
include(${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics/w3dGraphicsBackend.cmake)
w3d_configure_graphics_backend(whisk3d)

# ---- LOS DENOISERS del trazado de rayos (render/Denoiser.cpp): OPCIONALES. Nada se enlaza: se cargan al usarse.
#      OptiX: si estan sus headers (tools/bajar_terceros.ps1 -> libs/Whisk3DCore/thirdparty/optix), sin CUDA Toolkit
#      (un cuda.h minimo; las funciones las trae el driver). Open Image Denoise: sus DLL al lado del ejecutable.
set(W3D_TERCEROS ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/thirdparty)
# (los dos son SOLO de 64 bits: OptiX necesita el CUDA de 64 de NVIDIA y las DLL de OIDN son x64. En 32 bits no van.)
if((WIN32 OR (UNIX AND NOT APPLE AND NOT ANDROID)) AND EXISTS ${W3D_TERCEROS}/optix/include/optix.h AND NOT EMSCRIPTEN
   AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_include_directories(whisk3d PRIVATE ${W3D_TERCEROS}/optix/include ${CMAKE_SOURCE_DIR}/main/render/cuda_min)
    target_compile_definitions(whisk3d PRIVATE W3D_OPTIX)
    message(STATUS "Whisk3D: denoiser OptiX (opcional, solo con una NVIDIA)")
endif()
if(WIN32 AND EXISTS ${W3D_TERCEROS}/oidn/bin/OpenImageDenoise.dll AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    # (la CPU y la GPU de NVIDIA; las de AMD / Intel GPU -hip, sycl- no se copian: pesan y casi no se usan)
    foreach(dll OpenImageDenoise.dll OpenImageDenoise_core.dll OpenImageDenoise_device_cpu.dll OpenImageDenoise_device_cuda.dll tbb12.dll)
        add_custom_command(TARGET whisk3d POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${W3D_TERCEROS}/oidn/bin/${dll} $<TARGET_FILE_DIR:whisk3d>)
    endforeach()
    message(STATUS "Whisk3D: denoiser Open Image Denoise (opcional)")
endif()

# shaderc_shared (el compilador de shaders del backend Vulkan): del SDK de Vulkan instalado, al lado del ejecutable
if(W3D_VULKAN_ON)
    if(UNIX)
        target_link_libraries(whisk3d PRIVATE ${CMAKE_DL_LIBS})
    endif()
    if(DEFINED ENV{VULKAN_SDK})
        file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" W3D_VULKAN_SDK)
    elseif(WIN32)   # (la variable no siempre llega a la consola del build: el lugar donde lo instala el SDK, el mas nuevo)
        file(GLOB W3D_VULKAN_SDKS "C:/VulkanSDK/*")
        if(W3D_VULKAN_SDKS)
            list(SORT W3D_VULKAN_SDKS)
            list(GET W3D_VULKAN_SDKS -1 W3D_VULKAN_SDK)
        endif()
    endif()
    if(WIN32 AND W3D_VULKAN_SDK AND EXISTS "${W3D_VULKAN_SDK}/Bin/shaderc_shared.dll")
        add_custom_command(TARGET whisk3d POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${W3D_VULKAN_SDK}/Bin/shaderc_shared.dll $<TARGET_FILE_DIR:whisk3d>)
    endif()
endif()

if(APPLE)
    include(${CMAKE_SOURCE_DIR}/platform/mac/CMakeLists.txt)
endif()

# ---------------------------------------------------
# Include directories
# ---------------------------------------------------

target_include_directories(whisk3d PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/main
    # main/ reorganizado por tema (app/config/io/undo): cada carpeta es include dir para que los includes
    # "variables.h" / "Undo.h" / "W3dLang.h" resuelvan sin reescribir cada #include ni tocar los submodulos.
    ${CMAKE_SOURCE_DIR}/main/app
    ${CMAKE_SOURCE_DIR}/main/config
    ${CMAKE_SOURCE_DIR}/main/io
    ${CMAKE_SOURCE_DIR}/main/undo
    # la UI del editor se consolido bajo main/ui/ (ViewPorts, GeometriaUI): main/ui como include dir para que
    # "ViewPorts/ViewPort3D.h" resuelva como main/ui/ViewPorts/...; y las subcarpetas para los includes pelados.
    ${CMAKE_SOURCE_DIR}/main/ui
    ${CMAKE_SOURCE_DIR}/main/ui/ViewPorts
    ${CMAKE_SOURCE_DIR}/main/ui/GeometriaUI
    # capas reutilizables en libs/: "objects/Mesh.h" (Whisk3DCore) y "WhiskUI/icons.h"
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore
    # Core reorganizado: base/gfx/io como include dir para los includes pelados ("w3dTexture.h", "w3dlog.h")
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/base
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/graphics
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/io
    ${CMAKE_SOURCE_DIR}/libs/Whisk3DCore/thirdparty # stb: dep del engine, vive CON el core ("stb/stb_image.h")
    ${CMAKE_SOURCE_DIR}/libs
    # WhiskUI reorganizado por tema: sus subcarpetas son include dirs para que los includes INTERNOS pelados
    # ("icons.h", "card.h") resuelvan entre subcarpetas (los externos usan "WhiskUI/draw/icons.h").
    ${CMAKE_SOURCE_DIR}/libs/WhiskUI/widgets
    ${CMAKE_SOURCE_DIR}/libs/WhiskUI/text
    ${CMAKE_SOURCE_DIR}/libs/WhiskUI/draw
    ${CMAKE_SOURCE_DIR}/libs/WhiskUI/theme
    ${CMAKE_SOURCE_DIR}/libs/WhiskUI/core
    ${CMAKE_SOURCE_DIR}/thirdparty
)

# ---------------------------------------------------
# Compile definitions
# ---------------------------------------------------

if(WIN32)

    target_compile_definitions(whisk3d PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )

    # Icono embebido en el exe (menu inicio, Explorador, barra de tareas, Alt+Tab). El .rc se genera
    # con la ruta absoluta del .ico y se compila junto al exe (MSVC compila los .rc del target solo).
    set(W3D_ICON_ABS "${CMAKE_SOURCE_DIR}/res/Whisk3D.ico")
    configure_file(
        ${CMAKE_SOURCE_DIR}/platform/windows/whisk3d.rc.in
        ${CMAKE_BINARY_DIR}/whisk3d.rc @ONLY)
    target_sources(whisk3d PRIVATE ${CMAKE_BINARY_DIR}/whisk3d.rc)

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
    # res/installer = imagenes SOLO para el instalador de Windows (NSIS las lee al empaquetar, ver
    # CPACK_NSIS_MUI_* mas abajo). NO son recursos de runtime -> no van al lado del exe. Los install()
    # de win/linux ya las excluyen (FILES_MATCHING solo png/svg/ico/ini/Skins). Aca se copia todo res
    # y se saca installer para que el build de desarrollo tampoco las arrastre.
    COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:whisk3d>/res/installer
)

# ---------------------------------------------------
# INSTALACION LINUX
# ---------------------------------------------------

if(UNIX AND NOT APPLE AND NOT ANDROID)
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

    # el tipo MIME de los .w3d: doble click en un proyecto -> abre Whisk3D
    # (el trigger de dpkg sobre share/mime actualiza la base al instalar)
    install(FILES ${CMAKE_SOURCE_DIR}/platform/linux/whisk3d-mime.xml
        DESTINATION share/mime/packages
        RENAME whisk3d.xml
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME whisk3d.svg
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME whisk3d.png
    )

    # icono del TIPO DE ARCHIVO (Nautilus/hicolor): el mismo logo, instalado en el
    # contexto mimetypes/ con el nombre por defecto de cada tipo, que es el tipo con
    # '/' cambiado por '-'. Tienen que coincidir con el <icon name="..."/> del XML:
    #   application/vnd.whisk3d.proyecto+zip -> application-vnd.whisk3d.proyecto+zip
    #   application/x-whisk3d-ui             -> application-x-whisk3d-ui
    # (el nombre viejo application-x-whisk3d ya no se instala: ese tipo quedo como
    #  ALIAS y la base de MIME lo resuelve al tipo nuevo antes de pedir el icono)
    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.svg
        DESTINATION share/icons/hicolor/scalable/mimetypes
        RENAME application-vnd.whisk3d.proyecto+zip.svg
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.png
        DESTINATION share/icons/hicolor/256x256/mimetypes
        RENAME application-vnd.whisk3d.proyecto+zip.png
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.svg
        DESTINATION share/icons/hicolor/scalable/mimetypes
        RENAME application-x-whisk3d-ui.svg
    )

    install(FILES ${CMAKE_SOURCE_DIR}/res/Whisk3D.png
        DESTINATION share/icons/hicolor/256x256/mimetypes
        RENAME application-x-whisk3d-ui.png
    )

    # Solo DEB por cpack. El cmake estandar NO trae generador "AppImage" (cpack aborta con "Could not create CPack
    # generator: AppImage") -> el AppImage se arma aparte con platform/linux/build_appimage.sh (linuxdeploy+appimagetool).
    # RPM se puede agregar aca si esta rpmbuild instalado.
    set(CPACK_GENERATOR "DEB")

    # Deb
    set(CPACK_PACKAGE_NAME "whisk3d")
    # version = fecha AA.MM.DD (2 digitos) del dia en que se EMPAQUETA (cpack), igual que Windows.
    # El string(TIMESTAMP) de aca corre al CONFIGURAR (default/fallback), pero CPackDate.cmake
    # (CPACK_PROJECT_CONFIG_FILE, abajo) lo RECALCULA en cada corrida de cpack -> el .deb siempre
    # queda con la fecha del dia en que se genera, sin reconfigurar.
    string(TIMESTAMP CPACK_PACKAGE_VERSION "%y.%m.%d")
    set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/platform/linux/CPackDate.cmake")
    set(CPACK_PACKAGE_CONTACT "Dante Leoncini <dante_leoncini@outlook.com.ar>")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dante Leoncini <dante_leoncini@outlook.com.ar>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Whisk3D - editor y motor 3D retro multiplataforma")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Whisk3D - editor y motor 3D retro multiplataforma")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libglu1-mesa, libsdl2-2.0-0, libsdl2-image-2.0-0")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

    # postinst/postrm: refrescan las caches de mime/desktop/iconos al instalar y
    # desinstalar (icono + doble click de los .w3d al toque, sin reloguear). En
    # Debian/Ubuntu los triggers de dpkg (shared-mime-info, desktop-file-utils,
    # libgtk) ya lo hacen solos: los scripts son red de seguridad, todo guardado
    # con command -v y || true asi nunca rompen la instalacion.
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/platform/linux/debian/postinst;${CMAKE_SOURCE_DIR}/platform/linux/debian/postrm")
    # dpkg exige los scripts de control con permisos 0755: esto los fuerza al empaquetar
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

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
    # version = fecha AA.MM.DD (2 digitos) del momento en que se EMPAQUETA (cpack).
    # El string(TIMESTAMP) de aca corre al CONFIGURAR (default/fallback), pero CPackDate.cmake
    # (CPACK_PROJECT_CONFIG_FILE, abajo) lo RECALCULA en cada corrida de cpack -> el .exe siempre
    # queda con la fecha del dia en que se genera (Whisk3D-AA.MM.DD-win64.exe), sin reconfigurar.
    string(TIMESTAMP CPACK_PACKAGE_VERSION "%y.%m.%d")
    set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/platform/windows/CPackDate.cmake")
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

    # ASOCIAR los .w3d con Whisk3D: doble click -> abre el proyecto (argv[1]).
    # ProgID Whisk3D.Project (el .w3d es un PROYECTO zip+json, no un modelo suelto).
    # Se escriben en HKCR (el instalador NSIS corre elevado). $INSTDIR = carpeta
    # de instalacion. SHChangeNotify refresca el icono en el Explorador.
    # NSIS usa comillas SIMPLES para los strings (asi no hay que escapar las
    # dobles). \\\\ = una barra en NSIS; \\\" = una comilla literal (las del
    # comando, para rutas con espacios). El doble round-trip CMake->CPackConfig
    # ->plantilla.nsi consume los backslashes (por eso van de a 4).
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.w3d' '' 'Whisk3D.Project'
        WriteRegStr HKCR 'Whisk3D.Project' '' 'Proyecto Whisk3D'
        WriteRegStr HKCR 'Whisk3D.Project\\\\DefaultIcon' '' '$INSTDIR\\\\res\\\\Whisk3D.ico'
        WriteRegStr HKCR 'Whisk3D.Project\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\whisk3d.exe\\\" \\\"%1\\\"'
        System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
    ")
    # el uninstall borra TAMBIEN el ProgID viejo (Whisk3D.Model, instaladores
    # anteriores) para no dejar claves colgadas al actualizar
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR '.w3d'
        DeleteRegKey HKCR 'Whisk3D.Project'
        DeleteRegKey HKCR 'Whisk3D.Model'
        System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
    ")

    include(CPack)
endif()
