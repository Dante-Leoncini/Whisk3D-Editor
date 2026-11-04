# This file will be configured to contain variables for CPack. These variables
# should be set in the CMake list file of the project before CPack module is
# included. The list of available CPACK_xxx variables and their associated
# documentation may be obtained using
#  cpack --help-variable-list
#
# Some variables are common to all generators (e.g. CPACK_PACKAGE_NAME)
# and some are specific to a generator
# (e.g. CPACK_NSIS_EXTRA_INSTALL_COMMANDS). The generator specific variables
# usually begin with CPACK_<GENNAME>_xxxx.


set(CPACK_BUILD_SOURCE_DIRS "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor;C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/platform/windows/build_gles2")
set(CPACK_CMAKE_GENERATOR "Visual Studio 18 2026")
set(CPACK_COMPONENT_UNSPECIFIED_HIDDEN "TRUE")
set(CPACK_COMPONENT_UNSPECIFIED_REQUIRED "TRUE")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_FILE "C:/Program Files/CMake/share/cmake-4.2/Templates/CPack.GenericDescription.txt")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_SUMMARY "Whisk3D built using CMake")
set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE "ON")
set(CPACK_GENERATOR "7Z;ZIP")
set(CPACK_IGNORE_FILES "/CVS/;/\\.svn/;/\\.bzr/;/\\.hg/;/\\.git/;\\.swp\$;\\.#;/#")
set(CPACK_INNOSETUP_ARCHITECTURE "x64")
set(CPACK_INSTALLED_DIRECTORIES "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor;/")
set(CPACK_INSTALL_CMAKE_PROJECTS "")
set(CPACK_INSTALL_PREFIX "C:/Program Files (x86)/Whisk3D")
set(CPACK_MODULE_PATH "")
set(CPACK_NSIS_CONTACT "dante_leoncini@outlook.com.ar")
set(CPACK_NSIS_CREATE_ICONS_EXTRA "
        CreateShortCut '$SMPROGRAMS\\$STARTMENU_FOLDER\\Whisk3D.lnk' '$INSTDIR\\whisk3d.exe'
        CreateShortCut '$DESKTOP\\Whisk3D.lnk' '$INSTDIR\\whisk3d.exe'
    ")
set(CPACK_NSIS_DELETE_ICONS_EXTRA "
        Delete '$SMPROGRAMS\\$STARTMENU_FOLDER\\Whisk3D.lnk'
        Delete '$DESKTOP\\Whisk3D.lnk'
    ")
set(CPACK_NSIS_DISPLAY_NAME "Whisk3D")
set(CPACK_NSIS_DISPLAY_NAME_SET "TRUE")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON")
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.w3d' '' 'Whisk3D.Project'
        WriteRegStr HKCR 'Whisk3D.Project' '' 'Proyecto Whisk3D'
        WriteRegStr HKCR 'Whisk3D.Project\\DefaultIcon' '' '$INSTDIR\\res\\Whisk3D.ico'
        WriteRegStr HKCR 'Whisk3D.Project\\shell\\open\\command' '' '\"$INSTDIR\\whisk3d.exe\" \"%1\"'
        System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
    ")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR '.w3d'
        DeleteRegKey HKCR 'Whisk3D.Project'
        DeleteRegKey HKCR 'Whisk3D.Model'
        System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
    ")
set(CPACK_NSIS_INSTALLED_ICON_NAME "whisk3d.exe")
set(CPACK_NSIS_INSTALLER_ICON_CODE "")
set(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_MODIFY_PATH "OFF")
set(CPACK_NSIS_MUI_ICON "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/res/Whisk3D.ico")
set(CPACK_NSIS_MUI_UNIICON "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/res/Whisk3D.ico")
set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/res/installer/uninstall.bmp")
set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/res/installer/welcome.bmp")
set(CPACK_NSIS_PACKAGE_NAME "Whisk3D")
set(CPACK_NSIS_UNINSTALL_NAME "Uninstall")
set(CPACK_OUTPUT_CONFIG_FILE "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/platform/windows/build_gles2/CPackConfig.cmake")
set(CPACK_PACKAGE_CONTACT "Dante Leoncini <dante_leoncini@outlook.com.ar>")
set(CPACK_PACKAGE_DEFAULT_LOCATION "/")
set(CPACK_PACKAGE_DESCRIPTION_FILE "C:/Program Files/CMake/share/cmake-4.2/Templates/CPack.GenericDescription.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Whisk3D - editor y motor 3D retro multiplataforma")
set(CPACK_PACKAGE_FILE_NAME "Whisk3D-26.11.01-Source")
set(CPACK_PACKAGE_ICON "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/res/installer/header.bmp")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Whisk3D")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "Whisk3D")
set(CPACK_PACKAGE_NAME "Whisk3D")
set(CPACK_PACKAGE_RELOCATABLE "true")
set(CPACK_PACKAGE_VENDOR "Dante Leoncini")
set(CPACK_PACKAGE_VERSION "26.11.01")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_PROJECT_CONFIG_FILE "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/platform/windows/CPackDate.cmake")
set(CPACK_RESOURCE_FILE_LICENSE "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/LICENSE.md")
set(CPACK_RESOURCE_FILE_README "C:/Program Files/CMake/share/cmake-4.2/Templates/CPack.GenericDescription.txt")
set(CPACK_RESOURCE_FILE_WELCOME "C:/Program Files/CMake/share/cmake-4.2/Templates/CPack.GenericWelcome.txt")
set(CPACK_RPM_PACKAGE_SOURCES "ON")
set(CPACK_SET_DESTDIR "OFF")
set(CPACK_SOURCE_7Z "ON")
set(CPACK_SOURCE_GENERATOR "7Z;ZIP")
set(CPACK_SOURCE_IGNORE_FILES "/CVS/;/\\.svn/;/\\.bzr/;/\\.hg/;/\\.git/;\\.swp\$;\\.#;/#")
set(CPACK_SOURCE_INSTALLED_DIRECTORIES "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor;/")
set(CPACK_SOURCE_OUTPUT_CONFIG_FILE "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/platform/windows/build_gles2/CPackSourceConfig.cmake")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "Whisk3D-26.11.01-Source")
set(CPACK_SOURCE_TOPLEVEL_TAG "win64-Source")
set(CPACK_SOURCE_ZIP "ON")
set(CPACK_STRIP_FILES "")
set(CPACK_SYSTEM_NAME "win64")
set(CPACK_THREADS "1")
set(CPACK_TOPLEVEL_TAG "win64-Source")
set(CPACK_WIX_SIZEOF_VOID_P "8")

if(NOT CPACK_PROPERTIES_FILE)
  set(CPACK_PROPERTIES_FILE "C:/Symbian/Carbide/workspace/Whisk3Decosystem/Whisk3D-Editor/platform/windows/build_gles2/CPackProperties.cmake")
endif()

if(EXISTS ${CPACK_PROPERTIES_FILE})
  include(${CPACK_PROPERTIES_FILE})
endif()
