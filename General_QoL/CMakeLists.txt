cmake_minimum_required(VERSION 3.15)
project(general_qol CXX)

# The mod's version (VERSION file, raised with every release); shown in the log.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION MOD_VERSION LIMIT_COUNT 1)
add_compile_definitions(MOD_VERSION="${MOD_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: runs on machines without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The shipped general_qol.json is compiled in, so the mod can write it
# out when a player has none - and the two can never drift apart.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/general_qol.json DEFAULT_CONFIG)
configure_file(src/default_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS general_qol.json)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# Loaded by XV2 Mod Loader from <game>\XV2ModLoader\mods\General_QoL\.
add_library(General_QoL SHARED src/dllmain.cpp src/mod.cpp src/tsr.cpp src/data.cpp src/replay_npc.cpp src/json.cpp src/map_names.cpp src/hud_stages.cpp)
set_target_properties(General_QoL PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(General_QoL PRIVATE /W4 /permissive- /utf-8)
endif()

# Offline check of the script patcher (tests\run.bat); not part of the mod.
add_executable(tsrtest EXCLUDE_FROM_ALL tests/tsrtest.cpp src/tsr.cpp)
add_executable(datatest EXCLUDE_FROM_ALL tests/datatest.cpp src/data.cpp)
add_executable(npctest EXCLUDE_FROM_ALL tests/npctest.cpp src/replay_npc.cpp)
add_executable(maptest EXCLUDE_FROM_ALL tests/maptest.cpp src/map_names.cpp)
add_executable(hudtest EXCLUDE_FROM_ALL tests/hudtest.cpp src/hud_stages.cpp)
