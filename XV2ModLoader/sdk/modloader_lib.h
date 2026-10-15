// XV2 Mod Loader libraries: the interface between the loader (bin\winmm.dll)
// and the DLLs in XV2ModLoader\Lib\.
//
// The loader itself only forwards winmm's exports, loads the DLL mods and
// runs the Lua scripts (_OnInit, _OnFrame and the LuaBackend / Cheat Engine
// functions). Everything else - the in-game menu, the "ui", "search" and
// "files" tables, search boxes for DLL mods - lives in libraries it loads
// from Lib\ before any mod. A library exports
//
//     extern "C" __declspec(dllexport) bool ModLoaderLib_Init(const ModLoaderHost* host);
//
// which the loader calls once, on its own thread, right after loading it and
// before any Lua script runs. Returning false only gets it logged. The host
// stays valid for the life of the process.
//
// Lua is shared as Lib\lua54.dll, so a library links the same Lua the loader
// runs the scripts on (build with LUA_BUILD_AS_DLL).
#pragma once

struct lua_State;
struct IDXGISwapChain;

// Bumped when a field is added (fields are only ever appended).
constexpr unsigned kModLoaderHostVersion = 1;

// Message types of LuaBackend's ConsolePrint(text, type); kPlain = no label.
enum ScriptMessage { kPlain = -1, kMessage = 0, kSuccess = 1, kWarning = 2, kError = 3 };

struct ModLoaderHost {
    unsigned version;                       // kModLoaderHostVersion of the loader
    const wchar_t* folder;                  // <game>\XV2ModLoader

    // A line in loader.log (when loader.json says "logging": true).
    void (*Log)(const char* text);
    // "[LUA_NAME] TYPE: text" to the Lua console (when open) and loader.log;
    // state may be null for a library's own messages.
    void (*ScriptOutput)(lua_State* state, int type, const char* text);

    // The Add* functions keep their argument in an unlocked list: call them
    // only from ModLoaderLib_Init. Null arguments are ignored.

    // --- Lua scripts ------------------------------------------------------
    // Called for every new script state, after the loader's own functions
    // and before the script runs: put the library's tables in here.
    void (*AddLuaRegistrar)(void (*registrar)(lua_State* state));
    // A script that defines this global function is kept, as one that defines
    // _OnInit or _OnFrame is (e.g. "_OnMenu"). The name is copied.
    void (*AddScriptCallback)(const char* functionName);
    // Called before every script is closed for a reload (F1), on the render thread.
    void (*AddUnloadHandler)(void (*handler)());
    // Calls visit for every running script, in load order, on the render
    // thread. A script for which visit returns false is closed with
    // "<failure> failed; the script was unloaded."
    void (*ForEachScript)(bool (*visit)(lua_State* state, const char* name, void* context), void* context,
                          const char* failure);
    // Calls the script's global function with no arguments, if it defines
    // one (an error is printed with a traceback). Render thread, inside
    // ForEachScript. Returns false only when the call raised an error.
    bool (*CallScriptFunction)(lua_State* state, const char* functionName);

    // --- Frames -----------------------------------------------------------
    // Called on the render thread right before every IDXGISwapChain::Present,
    // after the scripts' _OnFrame.
    void (*AddFrameHandler)(void (*handler)(IDXGISwapChain* swapChain));
    // Installs the frame hook if nothing has yet (the loader only does when
    // there are Lua scripts). Any thread.
    void (*EnsureFrameHook)();
};
