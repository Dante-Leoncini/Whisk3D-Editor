# macOS-specific target configuration.

set_target_properties(whisk3d PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Whisk3D"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.whisk3d.editor"
    MACOSX_BUNDLE_INFO_STRING "Whisk3D retro 2D/3D editor"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${W3D_VER}"
    OUTPUT_NAME "Whisk3D"
)

target_include_directories(whisk3d PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

find_package(OpenGL REQUIRED)

target_link_libraries(whisk3d PRIVATE
    OpenGL::GL
    OpenGL::GLU
)

target_compile_definitions(whisk3d PRIVATE
    W3D_OPENGL
    GL_SILENCE_DEPRECATION
)

install(TARGETS whisk3d BUNDLE DESTINATION .)
