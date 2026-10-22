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


set(CPACK_BUILD_SOURCE_DIRS "/localStore/JuBrain/software/ImageEditor;/localStore/JuBrain/software/ImageEditor/build_deb")
set(CPACK_CMAKE_GENERATOR "Unix Makefiles")
set(CPACK_COMPONENT_UNSPECIFIED_HIDDEN "TRUE")
set(CPACK_COMPONENT_UNSPECIFIED_REQUIRED "TRUE")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6 | libqt6core6t64, libqt6gui6 | libqt6gui6t64, libqt6widgets6 | libqt6widgets6t64, libqt6opengl6 | libqt6opengl6t64, libqt6svg6, libqt6network6 | libqt6network6t64, libgl1, libtiff6 | libtiff5")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Hartmut Mohlberg <h.mohlberg@fz-juelich.de>")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_FILE "/usr/share/cmake-3.31/Templates/CPack.GenericDescription.txt")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_SUMMARY "ImageEditor built using CMake")
set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE "ON")
set(CPACK_GENERATOR "DEB")
set(CPACK_INNOSETUP_ARCHITECTURE "x64")
set(CPACK_INSTALL_CMAKE_PROJECTS "/localStore/JuBrain/software/ImageEditor/build_deb;ImageEditor;ALL;/")
set(CPACK_INSTALL_PREFIX "/usr/local")
set(CPACK_MODULE_PATH "/usr/lib/x86_64-linux-gnu/cmake/Qt6;/usr/lib/x86_64-linux-gnu/cmake/Qt6/3rdparty/extra-cmake-modules/find-modules;/usr/lib/x86_64-linux-gnu/cmake/Qt6/3rdparty/kwin")
set(CPACK_NSIS_DISPLAY_NAME "imageeditor 1.0.2")
set(CPACK_NSIS_INSTALLER_ICON_CODE "")
set(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
set(CPACK_NSIS_PACKAGE_NAME "imageeditor 1.0.2")
set(CPACK_NSIS_UNINSTALL_NAME "Uninstall")
set(CPACK_OBJCOPY_EXECUTABLE "/usr/bin/objcopy")
set(CPACK_OBJDUMP_EXECUTABLE "/usr/bin/objdump")
set(CPACK_OUTPUT_CONFIG_FILE "/localStore/JuBrain/software/ImageEditor/build_deb/CPackConfig.cmake")
set(CPACK_PACKAGE_CONTACT "Hartmut Mohlberg <h.mohlberg@fz-juelich.de>")
set(CPACK_PACKAGE_DEFAULT_LOCATION "/")
set(CPACK_PACKAGE_DESCRIPTION "ImageEditor is a versatile Qt6 image processing tool for interactive and
batch image manipulation. Features include lasso cut, polygon selection,
layer transforms (move, rotate, scale, mirror, perspective warp, cage warp),
AI inpainting (LaMa/PatchMatch), BigTIFF/HDF5 pyramid viewers, and a
JSON-based project history for reproducible workflows.")
set(CPACK_PACKAGE_DESCRIPTION_FILE "/usr/share/cmake-3.31/Templates/CPack.GenericDescription.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt6-based image processing tool with JSON-history support")
set(CPACK_PACKAGE_FILE_NAME "imageeditor_1.0.2_amd64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "imageeditor 1.0.2")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "imageeditor 1.0.2")
set(CPACK_PACKAGE_NAME "imageeditor")
set(CPACK_PACKAGE_RELOCATABLE "true")
set(CPACK_PACKAGE_VENDOR "Humanity")
set(CPACK_PACKAGE_VERSION "1.0.2")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "2")
set(CPACK_READELF_EXECUTABLE "/usr/bin/readelf")
set(CPACK_RESOURCE_FILE_LICENSE "/localStore/JuBrain/software/ImageEditor/LICENSE.txt")
set(CPACK_RESOURCE_FILE_README "/usr/share/cmake-3.31/Templates/CPack.GenericDescription.txt")
set(CPACK_RESOURCE_FILE_WELCOME "/usr/share/cmake-3.31/Templates/CPack.GenericWelcome.txt")
set(CPACK_SET_DESTDIR "OFF")
set(CPACK_SOURCE_GENERATOR "TBZ2;TGZ;TXZ;TZ")
set(CPACK_SOURCE_OUTPUT_CONFIG_FILE "/localStore/JuBrain/software/ImageEditor/build_deb/CPackSourceConfig.cmake")
set(CPACK_SOURCE_RPM "OFF")
set(CPACK_SOURCE_TBZ2 "ON")
set(CPACK_SOURCE_TGZ "ON")
set(CPACK_SOURCE_TXZ "ON")
set(CPACK_SOURCE_TZ "ON")
set(CPACK_SOURCE_ZIP "OFF")
set(CPACK_STRIP_FILES "TRUE")
set(CPACK_SYSTEM_NAME "Linux")
set(CPACK_THREADS "1")
set(CPACK_TOPLEVEL_TAG "Linux")
set(CPACK_WIX_SIZEOF_VOID_P "8")

if(NOT CPACK_PROPERTIES_FILE)
  set(CPACK_PROPERTIES_FILE "/localStore/JuBrain/software/ImageEditor/build_deb/CPackProperties.cmake")
endif()

if(EXISTS ${CPACK_PROPERTIES_FILE})
  include(${CPACK_PROPERTIES_FILE})
endif()
