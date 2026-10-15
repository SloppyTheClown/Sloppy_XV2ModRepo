// ---------------------------------------------------------------------------
//  File patches - mods have the loader change game files as the game reads
//  them, so nothing in data\ has to be replaced:
//
//  * a whole file served from the mod's own copy (ReplaceFile);
//  * a menu's ActionScript (AS3 ABC) swapped for the mod's own, only over
//    the script it was built from (ABC size and FNV-1a 64 hash, printed by
//    the ClaudeTools\iggy\*_abc.py builders), so a menu another mod replaced is
//    left alone (ReplaceIggyScript). A mod may register one script per
//    original (e.g. vanilla CHARASELE and xv2patcher's): each original
//    gets the script built from it.
//
//  Lua scripts use the "files" table; DLL mods the XV2Files_* exports
//  (sdk\xv2_files.h). One patch per file: a second mod asking for a file
//  that already has one is refused; the same owner asking again replaces
//  its own patch (for a menu script: the one for the same original; one
//  for another original is added).
//
//  Every data file is read through a file request object (DBXV2.exe 1.26.01,
//  vtable 0xf9b270; Research_Notes\exe-and-tooling.md "File request hook"):
//  slots 3/4 return the file's data, 6/7 its size, and the path is inline at
//  +8. Other mods (General_QoL, BattleStyles, ...) repoint the same four slots and
//  chain: whatever a slot holds is called through, under the named mutex
//  XV2Mods_FileRequestHook, so load order doesn't matter. The hook here is
//  the same as BattleStyles' (style_menu.cpp InstallFileHook / Lookup) and
//  is installed when a mod first registers a file - never otherwise.
//
//  Mods register on the render thread (Lua) or any thread (DLLs), the game
//  requests files on its loading threads: the registrations sit behind one
//  SRWLOCK, the hook installation behind another.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "lua.h"
#include "lauxlib.h"
#include "file_patches.h"
#include "game_files.h"
#include "iggy_abc.h"
#include "host.h"

namespace {

// File request class, DBXV2.exe 1.26.01 (exe-and-tooling.md).
const uintptr_t kRequestVtable = 0xf9b270;
const size_t kRequestPath = 8, kRequestPathMax = 0x118;
// Largest file a patch may hold: far above any game file (the biggest AWB
// sound banks are ~100 MB; ITEMLIST's ABC is 66 KB).
const size_t kMaxContentBytes = 512u * 1024 * 1024;

enum class PatchKind { kWholeFile, kIggyScript };

// A menu script and the original script it replaces.
struct IggyVariant {
    uint32_t vanillaAbcSize = 0;
    uint64_t vanillaAbcHash = 0;
    std::vector<uint8_t> abc;
};

// One file a mod changes.
struct FilePatch {
    PatchKind kind = PatchKind::kWholeFile;
    std::string pathSuffix;         // lower case, '/' ("ui/iggy/itemlist.iggy")
    std::string owner;              // the script (LUA_NAME) or the DLL's name, for messages and Remove
    bool fromScript = false;        // a Lua script's: dropped on an F1 reload (the script registers again)
    std::vector<uint8_t> content;   // the whole file (kWholeFile) or, until registered, the new ABC (kIggyScript)
    uint32_t vanillaAbcSize = 0;    // kIggyScript, until registered: the script it may replace
    uint64_t vanillaAbcHash = 0;
    std::vector<IggyVariant> variants;  // kIggyScript, once registered: one per original script
    // Copies handed to the game per source content, (hash, size) -> copy, or
    // null when that content couldn't be patched. The game may hold on to a
    // buffer it was given, so copies are never freed.
    std::map<std::pair<uint64_t, uint64_t>, std::vector<uint8_t>*> served;
    std::string status;             // the last request's outcome, for Status
};

SRWLOCK g_lock = SRWLOCK_INIT;                         // g_patches, g_retiredCopies
std::vector<std::unique_ptr<FilePatch>> g_patches;
std::vector<std::vector<uint8_t>*> g_retiredCopies;    // served copies of removed patches (kept alive)

typedef void*    (__fastcall* GetDataFn)(void* request);
typedef uint64_t (__fastcall* GetSizeFn)(void* request);
GetDataFn g_origData18 = nullptr, g_origData20 = nullptr;
GetSizeFn g_origSize30 = nullptr, g_origSize38 = nullptr;
enum class HookState { kNotTried, kInstalled, kFailed };
SRWLOCK g_hookLock = SRWLOCK_INIT;                     // g_hookState, g_hookFailure
HookState g_hookState = HookState::kNotTried;
std::string g_hookFailure;

// A mod's path in the form patches are matched in:
// "\\ui\\iggy\\ITEMLIST.iggy" -> "ui/iggy/itemlist.iggy" (lower case, '/',
// no leading separator).
std::string NormalisePath(const char* path)
{
    while (*path == '/' || *path == '\\') ++path;
    std::string out;
    for (; *path; ++path) out += *path == '\\' ? '/' : (char)tolower((unsigned char)*path);
    return out;
}

// The patch a file request is for, or null (call with g_lock held). The
// game's requests use backslashes (data\ui\iggy\ITEMLIST.iggy), so '/' and
// '\' compare equal, case-insensitively, and the suffix must start after a
// separator ("itemlist.iggy" doesn't match "myitemlist.iggy").
FilePatch* RequestedPatch(void* request)
{
    const char* path = (const char*)request + kRequestPath;
    size_t length = strnlen(path, kRequestPathMax);
    for (auto& patch : g_patches) {
        size_t suffix = patch->pathSuffix.size();
        if (length < suffix + 1) continue;
        const char* tail = path + length - suffix;
        bool same = tail[-1] == '/' || tail[-1] == '\\';
        for (size_t i = 0; same && i < suffix; ++i) {
            char have = tail[i] == '\\' ? '/' : (char)tolower((unsigned char)tail[i]);
            same = have == patch->pathSuffix[i];
        }
        if (same) return patch.get();
    }
    return nullptr;
}

// The patch with this (normalised) path, or null (call with g_lock held).
std::unique_ptr<FilePatch>* PatchForPath(const std::string& pathSuffix)
{
    for (auto& patch : g_patches)
        if (patch->pathSuffix == pathSuffix) return &patch;
    return nullptr;
}

// Builds the copy of `data` (the file as the game loaded it) that `patch`
// makes; null, with the reason in patch.status, when it can't be patched.
std::vector<uint8_t>* MakePatchedCopy(FilePatch& patch, const uint8_t* data, size_t size)
{
    if (patch.kind == PatchKind::kWholeFile) {
        patch.status = "served: file replaced (" + std::to_string(patch.content.size()) + " bytes)";
        return new std::vector<uint8_t>(patch.content);
    }
    auto copy = new std::vector<uint8_t>;
    std::string note;
    for (const IggyVariant& variant : patch.variants) {
        copy->clear();
        if (ReplaceIggyAbc(data, size, variant.vanillaAbcSize, variant.vanillaAbcHash, variant.abc.data(),
                           variant.abc.size(), *copy, note)) {
            patch.status = "served: " + note;
            if (patch.variants.size() > 1) patch.status += " (over the " + std::to_string(variant.vanillaAbcSize) + "-byte original)";
            return copy;
        }
    }
    delete copy;
    patch.status = "NOT patched: " + note;
    return nullptr;
}

// The patched copy to hand out for this request, or null for the original.
//   data, size   the file as the game (and the mods before us) loaded it
std::vector<uint8_t>* Lookup(void* request, void* data, uint64_t size)
{
    if (!data) return nullptr;
    AcquireSRWLockExclusive(&g_lock);
    FilePatch* patch = g_patches.empty() ? nullptr : RequestedPatch(request);
    std::vector<uint8_t>* served = nullptr;
    if (patch) {
        // Re-hashed every time: a changed file (another mod, a reinstall) is
        // patched afresh rather than served from the cache.
        uint64_t hash = Fnv64((const uint8_t*)data, (size_t)size);
        auto found = patch->served.find({hash, size});
        if (found != patch->served.end()) {
            served = found->second;
        } else {
            served = MakePatchedCopy(*patch, (const uint8_t*)data, (size_t)size);
            Log("file %s (%llu bytes) for %s: %s", patch->pathSuffix.c_str(), (unsigned long long)size,
                patch->owner.c_str(), patch->status.c_str());
            patch->served[{hash, size}] = served;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    return served;
}

// Whether a mod patches this request's file (shared lock: the hook runs on
// several loading threads at once).
bool WantsRequest(void* request)
{
    AcquireSRWLockShared(&g_lock);
    bool wanted = !g_patches.empty() && RequestedPatch(request);
    ReleaseSRWLockShared(&g_lock);
    return wanted;
}

// The data / size slots' answer: the patched copy's, or what the chain gave.
// Every mod in the file-request chain calls the next one's data and size;
// fetching them for files this mod never edits multiplied the calls ~600x
// down a six-mod chain (host hitches online), so the path is checked first.
void* Serve(void* request, void* data)
{
    if (!data || !WantsRequest(request)) return data;
    std::vector<uint8_t>* served = Lookup(request, data, g_origSize38(request));
    return served ? served->data() : data;
}
uint64_t ServeSize(void* request, uint64_t size)
{
    if (!WantsRequest(request)) return size;
    void* data = g_origData20(request);
    std::vector<uint8_t>* served = Lookup(request, data, data ? g_origSize38(request) : 0);
    return served ? served->size() : size;
}
// The four hooked slots, named after their byte offset in the table.
void*    __fastcall HkData18(void* request) { return Serve(request, g_origData18(request)); }
void*    __fastcall HkData20(void* request) { return Serve(request, g_origData20(request)); }
uint64_t __fastcall HkSize30(void* request) { return ServeSize(request, g_origSize30(request)); }
uint64_t __fastcall HkSize38(void* request) { return ServeSize(request, g_origSize38(request)); }

// Chains into the file request vtable once (.rdata: patchable at once, no
// DRM wait); later calls return the first outcome. Any thread. False, with
// the reason, when it can't.
bool EnsureFileHook(std::string& reason)
{
    AcquireSRWLockExclusive(&g_hookLock);
    if (g_hookState == HookState::kNotTried) {
        if (!IsGame12601()) {
            g_hookFailure = "needs DBXV2.exe 1.26.01";
        } else {
            HANDLE lock = CreateMutexW(nullptr, FALSE, L"XV2Mods_FileRequestHook");
            if (lock) WaitForSingleObject(lock, 10000);
            uintptr_t* vtable = (uintptr_t*)((uintptr_t)GetModuleHandleW(nullptr) + kRequestVtable);
            g_origData18 = (GetDataFn)vtable[3];
            g_origData20 = (GetDataFn)vtable[4];
            g_origSize30 = (GetSizeFn)vtable[6];
            g_origSize38 = (GetSizeFn)vtable[7];
            DWORD oldProtect;
            if (VirtualProtect(&vtable[3], 5 * sizeof(uintptr_t), PAGE_READWRITE, &oldProtect)) {
                vtable[3] = (uintptr_t)&HkData18;
                vtable[4] = (uintptr_t)&HkData20;
                vtable[6] = (uintptr_t)&HkSize30;
                vtable[7] = (uintptr_t)&HkSize38;
                VirtualProtect(&vtable[3], 5 * sizeof(uintptr_t), oldProtect, &oldProtect);
            } else {
                g_hookFailure = "could not patch the file request vtable";
            }
            if (lock) { ReleaseMutex(lock); CloseHandle(lock); }
        }
        g_hookState = g_hookFailure.empty() ? HookState::kInstalled : HookState::kFailed;
        Log(g_hookFailure.empty() ? "file request hook installed" : "file request hook NOT installed: %s",
            g_hookFailure.c_str());
    }
    reason = g_hookFailure;
    const bool installed = g_hookState == HookState::kInstalled;
    ReleaseSRWLockExclusive(&g_hookLock);
    return installed;
}

// Moves a patch's served copies to g_retiredCopies (the game may still use
// them). Call with g_lock held.
void RetireCopies(FilePatch& patch)
{
    for (auto& copy : patch.served)
        if (copy.second) g_retiredCopies.push_back(copy.second);
    patch.served.clear();
}

// Adds `patch` (its kind, pathSuffix, owner, fromScript and content set).
// Refused when the path is empty, the hook can't go in, or another owner
// already patches the file; the same owner registering again replaces its
// patch. False with the reason.
bool RegisterPatch(std::unique_ptr<FilePatch> patch, std::string& reason)
{
    if (patch->pathSuffix.empty()) {
        reason = "empty path";
        return false;
    }
    if (!EnsureFileHook(reason)) return false;
    patch->status = "registered, not loaded by the game yet";
    if (patch->kind == PatchKind::kIggyScript) {
        patch->variants.push_back({patch->vanillaAbcSize, patch->vanillaAbcHash, std::move(patch->content)});
        patch->content.clear();
    }
    const std::string pathSuffix = patch->pathSuffix, owner = patch->owner;
    AcquireSRWLockExclusive(&g_lock);
    std::unique_ptr<FilePatch>* existing = PatchForPath(pathSuffix);
    if (existing && (*existing)->owner != owner) {
        reason = "already patched by " + (*existing)->owner;
        ReleaseSRWLockExclusive(&g_lock);
        return false;
    }
    // the same owner's scripts for other originals stay
    if (existing && (*existing)->kind == PatchKind::kIggyScript && patch->kind == PatchKind::kIggyScript) {
        const IggyVariant& added = patch->variants.front();
        for (IggyVariant& kept : (*existing)->variants)
            if (kept.vanillaAbcSize != added.vanillaAbcSize || kept.vanillaAbcHash != added.vanillaAbcHash)
                patch->variants.push_back(std::move(kept));
    }
    if (existing) {
        RetireCopies(**existing);
        *existing = std::move(patch);
    } else {
        g_patches.push_back(std::move(patch));
    }
    ReleaseSRWLockExclusive(&g_lock);
    Log("file %s: %s registered a patch", pathSuffix.c_str(), owner.c_str());
    return true;
}

// Removes `owner`'s patch of `path` (a mod path, normalised here). False
// when there is none (or it is another owner's).
bool RemovePatch(const std::string& owner, const char* path)
{
    const std::string pathSuffix = NormalisePath(path);
    AcquireSRWLockExclusive(&g_lock);
    bool removed = false;
    for (auto it = g_patches.begin(); it != g_patches.end(); ++it) {
        if ((*it)->pathSuffix != pathSuffix || (*it)->owner != owner) continue;
        RetireCopies(**it);
        g_patches.erase(it);
        removed = true;
        break;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (removed) Log("file %s: %s removed its patch", pathSuffix.c_str(), owner.c_str());
    return removed;
}

// The status of the patch of `path` (a mod path), or "" when the file has
// none: "registered, not loaded by the game yet", "served: ...",
// "NOT patched: ...".
std::string PatchStatus(const char* path)
{
    const std::string pathSuffix = NormalisePath(path);
    std::string status;
    AcquireSRWLockShared(&g_lock);
    if (std::unique_ptr<FilePatch>* patch = PatchForPath(pathSuffix)) status = (*patch)->status;
    ReleaseSRWLockShared(&g_lock);
    return status;
}

// The owner of the patch of `path` (a mod path), or "" when the file has none.
std::string PatchOwner(const char* path)
{
    const std::string pathSuffix = NormalisePath(path);
    std::string owner;
    AcquireSRWLockShared(&g_lock);
    if (std::unique_ptr<FilePatch>* patch = PatchForPath(pathSuffix)) owner = (*patch)->owner;
    ReleaseSRWLockShared(&g_lock);
    return owner;
}

// Copies `text` into a caller's buffer, cut to fit and 0-terminated (nothing
// when out is null or outBytes <= 0); returns its full length in bytes.
int CopyOut(const std::string& text, char* out, int outBytes)
{
    if (out && outBytes > 0) {
        const size_t copied = text.copy(out, static_cast<size_t>(outBytes) - 1);
        out[copied] = '\0';
    }
    return static_cast<int>(text.size());
}

// UTF-8 -> UTF-16 (for file paths).
std::wstring Wide(const char* text)
{
    int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::wstring wide(length > 1 ? (size_t)length - 1 : 0, L'\0');
    if (length > 1) MultiByteToWideChar(CP_UTF8, 0, text, -1, &wide[0], length);
    return wide;
}

// The script's mod folder (the loader keeps it in the registry as
// "xv2_script_folder", UTF-8); empty when unknown.
std::wstring ScriptFolder(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "xv2_script_folder");
    const char* folder = lua_tostring(L, -1);
    std::wstring wide = folder ? Wide(folder) : std::wstring();
    lua_pop(L, 1);
    return wide;
}

// The file path argument at `index` (UTF-8), relative to the script's mod
// folder unless it is absolute ("C:\..." or starting with a separator).
std::wstring ArgFilePath(lua_State* L, int index)
{
    std::wstring wide = Wide(luaL_checkstring(L, index));
    bool absolute = (wide.size() > 1 && wide[1] == L':') || (!wide.empty() && (wide[0] == L'\\' || wide[0] == L'/'));
    return absolute ? wide : ScriptFolder(L) + L"\\" + wide;
}

// The 64-bit hash argument at `index`: an integer (a hex literal above
// 0x7fff... wraps to a negative Lua integer, which keeps the bits) or a
// "0x..." string. Raises a Lua error for anything else.
uint64_t ArgHash(lua_State* L, int index)
{
    if (lua_type(L, index) == LUA_TSTRING) {
        const char* text = lua_tostring(L, index);
        char* end = nullptr;
        uint64_t value = _strtoui64(text, &end, 0);
        luaL_argcheck(L, end && end != text && *end == 0, index, "not a number");
        return value;
    }
    return (uint64_t)luaL_checkinteger(L, index);
}

// Reads a whole file into `out`. False when it can't be opened, is empty or
// is larger than kMaxContentBytes.
bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") || !file) return false;
    uint8_t buffer[65536];
    size_t got = 0;
    while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0 && out.size() <= kMaxContentBytes)
        out.insert(out.end(), buffer, buffer + got);
    fclose(file);
    return !out.empty() && out.size() <= kMaxContentBytes;
}

// Pushes Lua's result of a registration: true, or false and the reason.
int PushRegistered(lua_State* L, bool registered, const std::string& reason)
{
    lua_pushboolean(L, registered);
    if (registered) return 1;
    lua_pushstring(L, reason.c_str());
    return 2;
}

// files.ReplaceFile(path, file) -> true | false, reason
//   path   the game file, as a path suffix ("ui/texture/X.emb"; '/' or '\',
//          any case)
//   file   the replacement, relative to the script's mod folder unless absolute
int FilesReplaceFile(lua_State* L)
{
    auto patch = std::make_unique<FilePatch>();
    patch->kind = PatchKind::kWholeFile;
    patch->pathSuffix = NormalisePath(luaL_checkstring(L, 1));
    const std::wstring file = ArgFilePath(L, 2);
    patch->owner = ScriptName(L);
    patch->fromScript = true;
    if (!ReadFileBytes(file, patch->content)) return PushRegistered(L, false, "can't read the file");
    std::string reason;
    return PushRegistered(L, RegisterPatch(std::move(patch), reason), reason);
}

// files.ReplaceIggyScript(path, abcFile, vanillaAbcSize, vanillaAbcHash)
//   -> true | false, reason
//   path            the menu ("ui/iggy/ITEMLIST.iggy")
//   abcFile         the compiled AS3 ABC, relative to the script's mod folder
//   vanillaAbcSize  size of the ABC it may replace (bytes)
//   vanillaAbcHash  FNV-1a 64 of that ABC (integer or "0x..." string)
int FilesReplaceIggyScript(lua_State* L)
{
    auto patch = std::make_unique<FilePatch>();
    patch->kind = PatchKind::kIggyScript;
    patch->pathSuffix = NormalisePath(luaL_checkstring(L, 1));
    const std::wstring abcFile = ArgFilePath(L, 2);
    const lua_Integer vanillaAbcSize = luaL_checkinteger(L, 3);
    luaL_argcheck(L, vanillaAbcSize > 0 && vanillaAbcSize <= 0xffffffff, 3, "not an ABC size");
    patch->vanillaAbcSize = (uint32_t)vanillaAbcSize;
    patch->vanillaAbcHash = ArgHash(L, 4);
    patch->owner = ScriptName(L);
    patch->fromScript = true;
    if (!ReadFileBytes(abcFile, patch->content)) return PushRegistered(L, false, "can't read the ABC file");
    std::string reason;
    return PushRegistered(L, RegisterPatch(std::move(patch), reason), reason);
}

// files.Owner(path) -> owner | nil
//   The script (LUA_NAME) or DLL mod that patches the file; nil when none.
int FilesOwner(lua_State* L)
{
    const std::string owner = PatchOwner(luaL_checkstring(L, 1));
    if (owner.empty()) lua_pushnil(L);
    else lua_pushstring(L, owner.c_str());
    return 1;
}

// files.Remove(path) -> removed
//   Drops this script's patch of `path`; copies the game already holds stay.
int FilesRemove(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, RemovePatch(ScriptName(L), path));
    return 1;
}

// files.Status(path) -> status | nil   (also files.IggyScriptStatus)
//   What happened when the game last read the file ("served: ...",
//   "NOT patched: ...", "registered, not loaded by the game yet"); nil when
//   no mod patches it.
int FilesStatus(lua_State* L)
{
    const std::string status = PatchStatus(luaL_checkstring(L, 1));
    if (status.empty()) lua_pushnil(L);
    else lua_pushstring(L, status.c_str());
    return 1;
}

}  // namespace

void RegisterLuaFiles(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {"ReplaceFile",       FilesReplaceFile},
        {"ReplaceIggyScript", FilesReplaceIggyScript},
        {"Remove",            FilesRemove},
        {"Status",            FilesStatus},
        {"Owner",             FilesOwner},
        {"ReadGameFile",      FilesReadGameFile},   // game_files.cpp
        {"IggyScriptStatus",  FilesStatus},     // the first name of Status (Item Search uses it)
        {nullptr, nullptr},
    };
    luaL_newlib(L, functions);
    lua_setglobal(L, "files");
}

void ClearScriptFilePatches()
{
    AcquireSRWLockExclusive(&g_lock);
    for (auto it = g_patches.begin(); it != g_patches.end();) {
        if (!(*it)->fromScript) { ++it; continue; }
        RetireCopies(**it);
        it = g_patches.erase(it);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

// ---------------------------------------------------------------------------
//  For DLL mods (sdk\xv2_files.h). Any thread. Registrations stay until
//  removed (an F1 reload only drops the Lua scripts').
// ---------------------------------------------------------------------------

// Shared by the two Replace exports: checks the arguments, fills the common
// fields and registers. Returns 1 / 0, the reason copied into `reason`.
static int RegisterFromDll(std::unique_ptr<FilePatch> patch, const char* owner, const char* path, const void* data,
                           size_t size, char* reason, int reasonBytes)
{
    std::string failure;
    if (!owner || !*owner || !path || !data || size == 0 || size > kMaxContentBytes) {
        failure = "bad arguments (owner, path and data are required)";
    } else {
        patch->owner = owner;
        patch->pathSuffix = NormalisePath(path);
        patch->content.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
        if (RegisterPatch(std::move(patch), failure)) return 1;
    }
    CopyOut(failure, reason, reasonBytes);
    return 0;
}

extern "C" __declspec(dllexport) int XV2Files_ReplaceFile(const char* owner, const char* path, const void* data,
                                                          size_t size, char* reason, int reasonBytes)
{
    auto patch = std::make_unique<FilePatch>();
    patch->kind = PatchKind::kWholeFile;
    return RegisterFromDll(std::move(patch), owner, path, data, size, reason, reasonBytes);
}

extern "C" __declspec(dllexport) int XV2Files_ReplaceIggyScript(const char* owner, const char* path, const void* abc,
                                                                size_t abcSize, uint32_t vanillaAbcSize,
                                                                uint64_t vanillaAbcHash, char* reason,
                                                                int reasonBytes)
{
    auto patch = std::make_unique<FilePatch>();
    patch->kind = PatchKind::kIggyScript;
    patch->vanillaAbcSize = vanillaAbcSize;
    patch->vanillaAbcHash = vanillaAbcHash;
    return RegisterFromDll(std::move(patch), owner, path, abc, abcSize, reason, reasonBytes);
}

extern "C" __declspec(dllexport) int XV2Files_Remove(const char* owner, const char* path)
{
    return owner && path && RemovePatch(owner, path) ? 1 : 0;
}

extern "C" __declspec(dllexport) int XV2Files_Status(const char* path, char* out, int outBytes)
{
    return CopyOut(path ? PatchStatus(path) : std::string(), out, outBytes);
}

extern "C" __declspec(dllexport) int XV2Files_Owner(const char* path, char* out, int outBytes)
{
    return CopyOut(path ? PatchOwner(path) : std::string(), out, outBytes);
}
