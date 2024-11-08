cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

##
## PROJECT
## name and version
##
project(cachyos-kernel-manager
        VERSION 1.10.2
        LANGUAGES CXX)


##
## INCLUDE
##
include(GNUInstallDirs)
include(StandardProjectSettings)
include(CheckSymbolExists)
include(CompilerWarnings)
include(EnableCcache)
include(Linker)
include(StaticAnalyzers)
include(Sanitizers)
include(CPM)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Qt6 COMPONENTS Widgets LinguistTools Concurrent REQUIRED)
pkg_check_modules(
  LIBALPM
  REQUIRED
  IMPORTED_TARGET
  libalpm>=13.0.0)
pkg_check_modules(
  LIBGLIB
  REQUIRED
  IMPORTED_TARGET
  glib-2.0>=2.72.1)

CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
  EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
  NAME frozen
  GITHUB_REPOSITORY serge-sans-paille/frozen
  GIT_TAG c241d64279d30a013be2601d42564280f950bcb8
  EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
  NAME Corrosion
  GITHUB_REPOSITORY corrosion-rs/corrosion
  GIT_TAG b45275c8b5ee16118008a6a140ce842557b3e47a
  EXCLUDE_FROM_ALL YES
)

find_package(PolkitQt6-1 REQUIRED)

##
## CONFIGURATION
##
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=thin -fwhole-program-vtables")
   set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=thin -fwhole-program-vtables")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fwhole-program -fuse-linker-plugin")
endif()

list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-lalpm")
check_symbol_exists(alpm_pkg_get_installed_db "alpm.h" HAVE_ALPM_INSTALLED_DB)
if(HAVE_ALPM_INSTALLED_DB)
    add_compile_options(-DHAVE_ALPM_INSTALLED_DB)
endif()

# Link this 'library' to set the c++ standard / compile-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_23)

# Files with translation sources. You should run lupdate manually when needed to regenerate them
file(GLOB TS_FILES "${PROJECT_SOURCE_DIR}/lang/*.ts")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lang")

set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/lang") # set qm files output directory
qt_add_translation(QM_FILES ${TS_FILES})   # generate qm files

add_custom_command(
    DEPENDS "${QM_FILES}"
    OUTPUT "${CMAKE_BINARY_DIR}/cachyoskm_locale.qrc"
    COMMAND cp "${CMAKE_SOURCE_DIR}/cachyoskm_locale.qrc" "${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

##
## Target
##
add_custom_command(
    DEPENDS "${CMAKE_SOURCE_DIR}/src/mkoptions.py"
    OUTPUT "${CMAKE_BINARY_DIR}/compile_options.hpp"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/src/mkoptions.py" "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_SOURCE_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
qt_add_executable(${PROJECT_NAME}
    src/ini.hpp
    src/string_utils.hpp
    src/alpm_utils.hpp src/alpm_utils.cpp
    src/utils.hpp src/utils.cpp
    src/kernel.hpp src/kernel.cpp
    src/aur_kernel.hpp src/aur_kernel.cpp
    src/km-window.hpp src/km-window.cpp
    "${CMAKE_BINARY_DIR}/compile_options.hpp"
    src/config-options.hpp src/config-options.cpp
    src/conf-window.hpp src/conf-window.cpp
    src/schedext-window.hpp src/schedext-window.cpp
    src/conf-patches-page.hpp src/conf-patches-page.ui
    src/conf-options-page.hpp src/conf-options-page.ui
    src/km-window.ui src/conf-window.ui
    src/schedext-window.ui
    src/main.cpp "${CMAKE_BINARY_DIR}/cachyoskm_locale.qrc"
    )

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

# Add linker configuration
configure_linker(project_options)

# sanitizer options if supported by compiler
enable_sanitizers(project_options)

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})

corrosion_import_crate(MANIFEST_PATH "config-option-lib/Cargo.toml" FLAGS "${CARGO_FLAGS}")
corrosion_add_cxxbridge(config-option-lib-cxxbridge CRATE config_option_lib MANIFEST_PATH "config-option-lib/Cargo.toml" FILES lib.rs)

target_link_libraries(${PROJECT_NAME} PRIVATE project_warnings project_options Qt6::Widgets Qt6::Concurrent Threads::Threads fmt::fmt frozen::frozen config-option-lib-cxxbridge PkgConfig::LIBALPM PkgConfig::LIBGLIB)

option(ENABLE_UNITY "Enable Unity builds of projects" OFF)
if(ENABLE_UNITY)
   # Add for any project you want to apply unity builds for
   set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
endif()

install(
   TARGETS ${PROJECT_NAME}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
   PROGRAMS ${CMAKE_SOURCE_DIR}/src/terminal-helper
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cachyos-kernel-manager
)

install(
   PROGRAMS ${CMAKE_SOURCE_DIR}/src/rootshell.sh
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cachyos-kernel-manager
)

install(
   FILES org.cachyos.cachyos-kernel-manager.pkexec.policy
   DESTINATION "${POLKITQT-1_POLICY_FILES_INSTALL_DIR}"
)

install(
   FILES cachyos-kernel-manager.desktop
   DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(
   FILES cachyos-kernel-manager.png
   DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
