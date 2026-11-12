# macOS-specific target configuration.

# Whisk3D sources intentionally use the portable desktop OpenGL include form
# (<GL/...>) shared by Windows and Linux. macOS exposes the same API from the
# OpenGL framework as <OpenGL/...>; this Apple-only shim preserves the common
# source includes on both Intel and Apple Silicon builds.
target_include_directories(whisk3d PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Apple deprecated the OpenGL framework at the OS level, but this platform
# backend intentionally remains the desktop fixed-pipeline OpenGL backend.
# Acknowledge that platform contract only for the macOS target; other platforms
# retain their native warning/deprecation behavior.
target_compile_definitions(whisk3d PRIVATE GL_SILENCE_DEPRECATION)

set_target_properties(whisk3d PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Whisk3D"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.whisk3d.editor"
    MACOSX_BUNDLE_INFO_STRING "Whisk3D retro 2D/3D editor"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${W3D_VER}"
    OUTPUT_NAME "Whisk3D"
)

install(TARGETS whisk3d BUNDLE DESTINATION .)
