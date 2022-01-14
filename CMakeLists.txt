cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

##
## PROJECT
## name and version
##
project(cachyos-kernel-manager
        VERSION 0.0.1
        LANGUAGES CXX)


##
## INCLUDE
##
include(GNUInstallDirs)
include(StandardProjectSettings)
include(CompilerWarnings)
include(EnableCcache)
include(Linker)
include(StaticAnalyzers)
include(Sanitizers)
include(FetchContent)

find_package(Threads REQUIRED)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

FetchContent_Declare(fmt
  GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
  GIT_TAG "c06bef7273e594bfa91739e29d339fba51fd4e0e"
)
FetchContent_MakeAvailable(fmt)
FetchContent_Declare(spdlog
  GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
  GIT_TAG "729d7f6d8837b6693e7b378408518ea1710f80cb"
)
FetchContent_Declare(cpr
  GIT_REPOSITORY "https://github.com/libcpr/cpr.git"
  GIT_TAG "9b2bda734dffdd76096e1364290c58fe3a30c59a"
)
FetchContent_MakeAvailable(spdlog cpr)
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
   FetchContent_Declare(range-v3
     GIT_REPOSITORY "https://github.com/ericniebler/range-v3.git"
     GIT_TAG "9aa032ccd0eb4bd77f58e5b181168f1038c222c6"
   )
   FetchContent_MakeAvailable(range-v3)
endif()

##
## CONFIGURATION
##
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fwhole-program")
endif()

# Link this 'library' to set the c++ standard / compile-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)

##
## Target
##
add_executable(${PROJECT_NAME}
    src/km-window.hpp src/km-window.cpp
    src/main.cpp
    )

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

# Add linker configuration
configure_linker(project_options)

# sanitizer options if supported by compiler
enable_sanitizers(project_options)

include_directories(${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE project_warnings project_options Qt6::Widgets spdlog::spdlog fmt::fmt cpr::cpr)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
   target_link_libraries(${PROJECT_NAME} PRIVATE range-v3::range-v3)
endif()

option(ENABLE_UNITY "Enable Unity builds of projects" OFF)
if(ENABLE_UNITY)
   # Add for any project you want to apply unity builds for
   set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
endif()

install(
   TARGETS ${PROJECT_NAME}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
