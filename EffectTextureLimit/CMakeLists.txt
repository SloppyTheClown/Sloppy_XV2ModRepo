cmake_minimum_required(VERSION 3.15)
project(xv2_effect_texture_limit CXX)

# The mod's version (VERSION file, raised with every release); shown in the log.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION MOD_VERSION LIMIT_COUNT 1)
add_compile_definitions(MOD_VERSION="${MOD_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: runs on machines without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The shipped effect_texture_limit.json is compiled in, so the mod can write
# it out when a player has none - and the two can never drift apart.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/effect_texture_limit.json DEFAULT_CONFIG)
configure_file(src/default_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS effect_texture_limit.json)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# Loaded by XV2 Mod Loader from <game>\XV2ModLoader\mods\EffectTextureLimit\.
add_library(EffectTextureLimit SHARED src/dllmain.cpp)
set_target_properties(EffectTextureLimit PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(EffectTextureLimit PRIVATE /W4 /permissive- /utf-8)
endif()
