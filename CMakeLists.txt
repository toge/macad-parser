cmake_minimum_required(VERSION 3.25)
project(macad-parser VERSION 0.1.0 LANGUAGES CXX)

## add stdc++ compiler flag
# MSVC は -std=c++XX を未知オプションとして警告 D9002 で無視するだけでビルドが
# 成功するため、CHECK_CXX_COMPILER_FLAG が誤って「対応済み」と判定してしまう。
# その結果 C++14 のままコンパイルされ concepts / string_view / uz 等が壊れる。
# MSVC の場合は /std:c++latest を明示的に指定する。
include(CheckCXXCompilerFlag)
if(MSVC)
    set(STD_CPP "")
    string(APPEND CMAKE_CXX_FLAGS " /std:c++latest")
else()
    CHECK_CXX_COMPILER_FLAG("-std=c++26" COMPILER_SUPPORTS_CXX26)
    CHECK_CXX_COMPILER_FLAG("-std=c++23" COMPILER_SUPPORTS_CXX23)
    if(COMPILER_SUPPORTS_CXX26)
        set(STD_CPP cxx_std_26)
    elseif(COMPILER_SUPPORTS_CXX23)
        set(STD_CPP cxx_std_23)
    else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
    endif()
endif()

## add warning
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3")
endif()

if (MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

## optimization
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
endif()
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
endif()

## other flag
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

## options
option(BUILD_TEST "Build tests" ON)
option(ENABLE_FREESTANDING "Define MACAD_PARSER_FREESTANDING and add the freestanding check test" OFF)

## install dirs
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

## find dependencies
find_path(SIMDE_INCLUDE_DIRS "simde/arm/neon.h")

## create header-only interface target
add_library(macad-parser INTERFACE)
add_library(macad-parser::macad-parser ALIAS macad-parser)

target_include_directories(macad-parser INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${SIMDE_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(macad-parser INTERFACE ${STD_CPP})

install(TARGETS macad-parser EXPORT macad-parserTargets)
install(EXPORT macad-parserTargets
    FILE macad-parserTargets.cmake
    NAMESPACE macad-parser::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/macad-parser
)
install(FILES macad-parser.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macad-parserConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/macad-parserConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/macad-parser
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/macad-parserConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/macad-parserConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/macad-parserConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/macad-parser
)

## test
if(BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
