cmake_minimum_required(VERSION 3.15)
project(battle_styles CXX)

# The mod's version (VERSION file, raised with every release); shown in the log.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION MOD_VERSION LIMIT_COUNT 1)
add_compile_definitions(MOD_VERSION="${MOD_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: runs on machines without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The shipped battle_styles.json is compiled in, so the mod can write it
# out when a player has none - and the two can never drift apart.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/battle_styles.json DEFAULT_CONFIG)
configure_file(src/default_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS battle_styles.json)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XV2ModLoader/sdk)  # xv2_search.h: the loader's search box

# Loaded by XV2 Mod Loader from <game>\XV2ModLoader\mods\BattleStyles\.
add_library(BattleStyles SHARED src/dllmain.cpp src/mod.cpp src/json.cpp src/ean_fit.cpp src/prop_fit.cpp src/acb_merge.cpp src/game_files.cpp src/iggy_abc.cpp src/style_menu.cpp)
set_target_properties(BattleStyles PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(BattleStyles PRIVATE /W4 /permissive- /utf-8)
endif()

# The offline checks read game files through the loader's library, built by
# XV2ModLoader\build.bat (lua54.dll is loaded from next to it).
set(LOADER_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../XV2ModLoader/build/Release/XV2ModLoaderLib.dll")

# Offline check: tests\ean_fit_test.cpp (fits real game files, see its header).
add_executable(ean_fit_test tests/ean_fit_test.cpp src/ean_fit.cpp src/prop_fit.cpp src/acb_merge.cpp src/game_files.cpp)
target_compile_definitions(ean_fit_test PRIVATE "XV2_LOADER_LIBRARY=L\"${LOADER_LIBRARY}\"")
if (MSVC)
    target_compile_options(ean_fit_test PRIVATE /W4 /permissive- /utf-8)
endif()

# Offline check: tests\prop_fit_test.cpp (style props for a race, see its header).
add_executable(prop_fit_test tests/prop_fit_test.cpp src/prop_fit.cpp src/ean_fit.cpp src/acb_merge.cpp src/game_files.cpp)
target_compile_definitions(prop_fit_test PRIVATE "XV2_LOADER_LIBRARY=L\"${LOADER_LIBRARY}\"")
if (MSVC)
    target_compile_options(prop_fit_test PRIVATE /W4 /permissive- /utf-8)
endif()
