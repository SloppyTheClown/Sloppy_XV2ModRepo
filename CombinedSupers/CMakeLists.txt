cmake_minimum_required(VERSION 3.15)
project(xv2_combined_supers CXX)

# The mod's version (VERSION file, raised with every release); shown in the log.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION MOD_VERSION LIMIT_COUNT 1)
add_compile_definitions(MOD_VERSION="${MOD_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: runs on machines without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(MOD_SOURCES src/dllmain.cpp src/mod.cpp src/psc.cpp)

# The shipped combined_supers.json is compiled in, so the mod can write it out
# when a player has none - and the two can never drift apart.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/combined_supers.json DEFAULT_CONFIG)
configure_file(src/default_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS combined_supers.json)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# Plugin: loaded by XV2 Mod Loader from <game>\XV2ModLoader\mods\CombinedSupers\.
add_library(CombinedSupers SHARED ${MOD_SOURCES})
set_target_properties(CombinedSupers PROPERTIES PREFIX "")

# Standalone: the same mod as <game>\bin\winmm.dll, for players without the
# loader. Forwards all of winmm to System32; config in <game>\XV2CombinedSupers.
add_library(standalone SHARED ${MOD_SOURCES} src/winmm.def)
target_compile_definitions(standalone PRIVATE COMBINED_STANDALONE=1)
set_target_properties(standalone PROPERTIES OUTPUT_NAME "winmm" PREFIX "")

if (MSVC)
    target_compile_options(CombinedSupers PRIVATE /W4 /permissive-)
    target_compile_options(standalone     PRIVATE /W4 /permissive-)
endif()
