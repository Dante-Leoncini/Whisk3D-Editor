# macOS-specific target configuration.

set_target_properties(whisk3d PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Whisk3D"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.whisk3d.editor"
    MACOSX_BUNDLE_INFO_STRING "Whisk3D retro 2D/3D editor"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${W3D_VER}"
    OUTPUT_NAME "Whisk3D"
)

install(TARGETS whisk3d BUNDLE DESTINATION .)
