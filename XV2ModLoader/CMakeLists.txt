cmake_minimum_required(VERSION 3.15)
project(xv2_mod_loader CXX)

# The mod's version (VERSION file, raised with every release); shown in the log.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION MOD_VERSION LIMIT_COUNT 1)
add_compile_definitions(MOD_VERSION="${MOD_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: runs on machines without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The shipped loader.json is compiled in, so the loader can write it out when a
# player has none - and the two can never drift apart.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/loader.json DEFAULT_CONFIG)
configure_file(src/default_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS loader.json)

# Lua 5.4 (third_party/lua, unmodified) as lua54.dll, shared by the loader
# and its libraries (Lib/) so they all work on the same Lua states. Compiled
# as C++ so a Lua error unwinds C++ frames properly (lua_error throws instead
# of longjmp) - its exports are therefore C++ names, not C ones.
file(GLOB LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/*.c)
add_library(lua SHARED ${LUA_SOURCES})
set_target_properties(lua PROPERTIES OUTPUT_NAME "lua54")
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
target_include_directories(lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua)
target_compile_definitions(lua PUBLIC LUA_BUILD_AS_DLL)     # dllexport here, dllimport for users
if (MSVC)
    target_compile_options(lua PRIVATE /TP)
    target_compile_definitions(lua PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Dear ImGui 1.92.4 (third_party/imgui, unmodified; MIT) for the library's
# in-game menu: the core and its DX11 renderer. No Win32 backend -
# Lib/src/overlay.cpp feeds input itself, from the render thread.
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp
                         ${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})

# The loader: forwards winmm's exports (src/winmm_exports.h - and exports
# nothing else), loads Lib/ and the mods, runs the Lua scripts.
add_library(winmm SHARED src/loader.cpp src/lua_scripts.cpp src/lua_api.cpp src/winmm.def)
set_target_properties(winmm PROPERTIES OUTPUT_NAME "winmm" PREFIX "")
target_include_directories(winmm PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
# lua54.dll lives in XV2ModLoader\Lib, not next to winmm.dll: delay-loaded,
# after lua_scripts.cpp has loaded it by its full path.
target_link_libraries(winmm PRIVATE lua delayimp)
if (MSVC)
    target_link_options(winmm PRIVATE /DELAYLOAD:lua54.dll)
endif()

# The loader's library (Lib/src): in-game menu, search boxes, game file edits.
file(GLOB LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Lib/src/*.cpp)
add_library(XV2ModLoaderLib SHARED ${LIB_SOURCES})
target_include_directories(XV2ModLoaderLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
# d3dcompiler_47 (ImGui's renderer compiles its shaders at start-up) is
# delay-loaded: the game only loads it once the menu is first opened.
target_link_libraries(XV2ModLoaderLib PRIVATE lua imgui d3dcompiler delayimp)
if (MSVC)
    target_link_options(XV2ModLoaderLib PRIVATE /DELAYLOAD:d3dcompiler_47.dll)
endif()

# The two launchers, one source.
add_executable(launch_modded   WIN32 src/launcher.cpp)
add_executable(launch_unmodded WIN32 src/launcher.cpp)
target_compile_definitions(launch_modded   PRIVATE MODDED=1 UNICODE _UNICODE)
target_compile_definitions(launch_unmodded PRIVATE MODDED=0 UNICODE _UNICODE)
set_target_properties(launch_modded   PROPERTIES OUTPUT_NAME "Launch Modded")
set_target_properties(launch_unmodded PROPERTIES OUTPUT_NAME "Launch Unmodded")
target_link_libraries(launch_unmodded PRIVATE shell32)

if (MSVC)
    foreach(t winmm XV2ModLoaderLib launch_modded launch_unmodded)
        target_compile_options(${t} PRIVATE /W4 /permissive-)
    endforeach()
endif()
