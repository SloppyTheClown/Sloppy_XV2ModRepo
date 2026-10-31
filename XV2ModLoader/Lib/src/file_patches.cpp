// ---------------------------------------------------------------------------
//  File patches - mods have the loader change game files as the game reads
//  them, so nothing in data\ has to be replaced.
//
//  Every patched file has a pipeline of stages, one or more per mod. The
//  game's file goes in; each stage in turn gets the file as the stages
//  before it left it, may change it, and passes it on; what comes out is
//  what the game gets (like the systems of an entity component system
//  taking turns on the same data). Kinds of stage:
//
//  * a whole file served from the mod's own copy (ReplaceFile);
//  * a menu's ActionScript (AS3 ABC) swapped for the mod's own, only over
//    the script it was built from (ABC size and FNV-1a 64 hash, printed by
//    the ClaudeTools\iggy\*_abc.py builders), so a menu another mod replaced is
//    left alone (ReplaceIggyScript). A mod may register one script per
//    original (e.g. vanilla CHARASELE and xv2patcher's): each original
//    gets the script built from it;
//  * a DLL mod's own function (AddStage) - e.g. XV2UserInterfaceLib's,
//    which merges a mod's menu script edits into the current script.
//
//  The two Replace kinds are the old one-mod-per-file patches: they run
//  first (they want the game's own file) and a second owner's is refused.
//  AddStage stages stack in `order` (then registration) order.
//
//  Lua scripts use the "files" table; DLL mods the XV2Files_* exports
//  (sdk\xv2_files.h).
//
//  Every data file is read through a file request object (DBXV2.exe 1.27,
//  vtable 0xfa92f0; Research_Notes\exe-and-tooling.md "File request hook"):
//  slots 3/4 return the file's data, 6/7 its size, and the path is inline at
//  +8. Other mods (General_QoL, BattleStyles, ...) repoint the same four slots and
//  chain: whatever a slot holds is called through, under the named mutex
//  XV2Mods_FileRequestHook, so load order doesn't matter. The hook here is
//  the same as BattleStyles' (style_menu.cpp InstallFileHook / Lookup) and
//  is installed when a mod first registers a file - never otherwise.
//
//  Mods register on the render thread (Lua) or any thread (DLLs), the game
//  requests files on its loading threads: the registrations sit behind one
//  SRWLOCK, the hook installation behind another. Stages run without the
//  lock held, on a copy of the pipeline, so a slow one blocks no other file.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <climits>
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
#include "../../sdk/xv2_files.h"

namespace {

// File request class, DBXV2.exe 1.27 (exe-and-tooling.md).
const uintptr_t kRequestVtable = 0xfa92f0;
const size_t kRequestPath = 8, kRequestPathMax = 0x118;
// Largest file a patch may hold: far above any game file (the biggest AWB
// sound banks are ~100 MB; ITEMLIST's ABC is 66 KB).
const size_t kMaxContentBytes = 512u * 1024 * 1024;
// The old Replace* patches run before every AddStage stage.
const int kReplaceOrder = INT_MIN;

enum class StageKind { kWholeFile, kIggyScript, kFunction };

// A menu script and the original script it replaces.
struct IggyVariant {
    uint32_t vanillaAbcSize = 0;
    uint64_t vanillaAbcHash = 0;
    std::vector<uint8_t> abc;
};

// One step of a file's pipeline.
struct Stage {
    StageKind kind = StageKind::kWholeFile;
    std::string owner;              // the script (LUA_NAME) or the DLL's name, for messages and Remove
    bool fromScript = false;        // a Lua script's: dropped on an F1 reload (the script registers again)
    int order = 0;                  // lower runs first
    uint64_t sequence = 0;          // registration number: ties of `order` run in this order
    std::vector<uint8_t> content;   // kWholeFile: the file
    std::vector<IggyVariant> variants;  // kIggyScript: one per original script
    XV2FileStage function = nullptr;    // kFunction
    void* user = nullptr;
};

// One file mods change, and its pipeline.
struct PatchedFile {
    std::string pathSuffix;         // lower case, '/' ("ui/iggy/itemlist.iggy")
    std::vector<std::shared_ptr<const Stage>> stages;   // in running order
    uint64_t generation = 0;        // raised whenever `stages` changes
    // Copies handed to the game per source content, (hash, size) -> copy, or
    // null when nothing changed it. The game may hold on to a buffer it was
    // given, so copies are never freed.
    std::map<std::pair<uint64_t, uint64_t>, std::vector<uint8_t>*> served;
    std::string status;             // the last request's outcome, for Status
};

SRWLOCK g_lock = SRWLOCK_INIT;                         // g_files, g_retiredCopies, g_nextSequence
std::vector<std::unique_ptr<PatchedFile>> g_files;
std::vector<std::vector<uint8_t>*> g_retiredCopies;    // served copies of changed pipelines (kept alive)
uint64_t g_nextSequence = 0;

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

// The patched file a file request is for, or null (call with g_lock held).
// The game's requests use backslashes (data\ui\iggy\ITEMLIST.iggy), so '/'
// and '\' compare equal, case-insensitively, and the suffix must start after
// a separator ("itemlist.iggy" doesn't match "myitemlist.iggy").
PatchedFile* RequestedFile(void* request)
{
    const char* path = (const char*)request + kRequestPath;
    size_t length = strnlen(path, kRequestPathMax);
    for (auto& file : g_files) {
        size_t suffix = file->pathSuffix.size();
        if (length < suffix + 1) continue;
        const char* tail = path + length - suffix;
        bool same = tail[-1] == '/' || tail[-1] == '\\';
        for (size_t i = 0; same && i < suffix; ++i) {
            char have = tail[i] == '\\' ? '/' : (char)tolower((unsigned char)tail[i]);
            same = have == file->pathSuffix[i];
        }
        if (same) return file.get();
    }
    return nullptr;
}

// The patched file with this (normalised) path, or null (call with g_lock held).
PatchedFile* FileForPath(const std::string& pathSuffix)
{
    for (auto& file : g_files)
        if (file->pathSuffix == pathSuffix) return file.get();
    return nullptr;
}

// What a stage's function writes its result through (XV2FileOutput::Set).
struct StageResult {
    XV2FileOutput output;   // first: the function gets a pointer to this
    std::vector<uint8_t> data;
    bool set = false;
};
void __cdecl SetStageOutput(XV2FileOutput* output, const void* data, size_t size)
{
    StageResult* result = reinterpret_cast<StageResult*>(output);
    if (!data || size == 0 || size > kMaxContentBytes) return;
    result->data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    result->set = true;
}

// Calls a stage function, catching a crash in it (the file then goes on
// unchanged instead of taking the game down). Separate because __try can't
// sit in a function with C++ objects to destroy.
const int kStageCrashed = INT_MIN;
int CallStage(XV2FileStage function, void* user, const char* path, const void* original, size_t originalSize,
              const void* current, size_t currentSize, XV2FileOutput* output)
{
    __try {
        return function(user, path, original, originalSize, current, currentSize, output);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return kStageCrashed;
    }
}

// Runs one stage on `current` (replaced when the stage changes it). Returns
// the stage's outcome for the status line.
std::string RunStage(const Stage& stage, const std::string& pathSuffix, const std::vector<uint8_t>& original,
                     std::vector<uint8_t>& current)
{
    switch (stage.kind) {
    case StageKind::kWholeFile:
        current = stage.content;
        return "served: file replaced (" + std::to_string(stage.content.size()) + " bytes)";
    case StageKind::kIggyScript: {
        std::vector<uint8_t> patched;
        std::string note;
        for (const IggyVariant& variant : stage.variants) {
            patched.clear();
            if (ReplaceIggyAbc(current.data(), current.size(), variant.vanillaAbcSize, variant.vanillaAbcHash,
                               variant.abc.data(), variant.abc.size(), patched, note)) {
                current.swap(patched);
                std::string status = "served: " + note;
                if (stage.variants.size() > 1) status += " (over the " + std::to_string(variant.vanillaAbcSize) + "-byte original)";
                return status;
            }
        }
        return "NOT patched: " + note;
    }
    case StageKind::kFunction: {
        StageResult result{};
        result.output.Set = &SetStageOutput;
        const int outcome = CallStage(stage.function, stage.user, pathSuffix.c_str(), original.data(), original.size(),
                                      current.data(), current.size(), &result.output);
        if (outcome == kStageCrashed) return "NOT patched: the stage crashed";
        result.output.note[sizeof(result.output.note) - 1] = '\0';
        const std::string note = result.output.note;
        if (outcome > 0 && result.set) {
            current.swap(result.data);
            return "served: " + (note.empty() ? "changed (" + std::to_string(current.size()) + " bytes)" : note);
        }
        if (outcome == 0) return "unchanged" + (note.empty() ? std::string() : ": " + note);
        return "NOT patched: " + (note.empty() ? std::string("failed") : note);
    }
    }
    return "NOT patched: unknown stage";
}

// Runs a pipeline over the game's file. Returns the result, or null when no
// stage changed it; `status` gets every stage's outcome.
std::vector<uint8_t>* RunPipeline(const std::vector<std::shared_ptr<const Stage>>& stages, const std::string& pathSuffix,
                                  const uint8_t* data, size_t size, std::string& status)
{
    const std::vector<uint8_t> original(data, data + size);
    std::vector<uint8_t> current = original;
    bool changed = false;
    status.clear();
    for (const auto& stage : stages) {
        const size_t before = current.size();
        const std::string outcome = RunStage(*stage, pathSuffix, original, current);
        changed = changed || outcome.compare(0, 7, "served:") == 0 || current.size() != before;
        // one stage: its outcome as is (what scripts showed before stages)
        status += stages.size() == 1 ? outcome : (status.empty() ? "" : "; ") + stage->owner + ": " + outcome;
    }
    return changed ? new std::vector<uint8_t>(std::move(current)) : nullptr;
}

// The patched copy to hand out for this request, or null for the original.
//   data, size   the file as the game (and the mods before us) loaded it
std::vector<uint8_t>* Lookup(void* request, void* data, uint64_t size)
{
    if (!data) return nullptr;
    // Re-hashed every time: a changed file (another mod, a reinstall) is
    // patched afresh rather than served from the cache.
    const uint64_t hash = Fnv64((const uint8_t*)data, (size_t)size);
    std::vector<std::shared_ptr<const Stage>> stages;
    std::string pathSuffix;
    uint64_t generation = 0;
    AcquireSRWLockExclusive(&g_lock);
    PatchedFile* file = g_files.empty() ? nullptr : RequestedFile(request);
    if (file) {
        auto found = file->served.find({hash, size});
        if (found != file->served.end()) {
            std::vector<uint8_t>* served = found->second;
            ReleaseSRWLockExclusive(&g_lock);
            return served;
        }
        stages = file->stages;
        pathSuffix = file->pathSuffix;
        generation = file->generation;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (stages.empty()) return nullptr;

    std::string status;
    std::vector<uint8_t>* served = RunPipeline(stages, pathSuffix, (const uint8_t*)data, (size_t)size, status);
    Log("file %s (%llu bytes): %s", pathSuffix.c_str(), (unsigned long long)size, status.c_str());

    AcquireSRWLockExclusive(&g_lock);
    file = FileForPath(pathSuffix);
    if (file && file->generation == generation) {
        auto found = file->served.find({hash, size});
        if (found == file->served.end()) {
            file->served[{hash, size}] = served;
            file->status = status;
        } else {   // another loading thread finished first: serve its copy
            if (served) g_retiredCopies.push_back(served);
            served = found->second;
        }
    } else if (served) {
        g_retiredCopies.push_back(served);   // the pipeline changed meanwhile; hand this one out once
    }
    ReleaseSRWLockExclusive(&g_lock);
    return served;
}

// Whether a mod patches this request's file (shared lock: the hook runs on
// several loading threads at once).
bool WantsRequest(void* request)
{
    AcquireSRWLockShared(&g_lock);
    bool wanted = !g_files.empty() && RequestedFile(request);
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
        if (!IsSupportedGame()) {
            g_hookFailure = "needs DBXV2.exe 1.27";
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

// After a pipeline changed: its served copies go to g_retiredCopies (the
// game may still use them) and later reads run the new pipeline. Call with
// g_lock held.
void PipelineChanged(PatchedFile& file)
{
    for (auto& copy : file.served)
        if (copy.second) g_retiredCopies.push_back(copy.second);
    file.served.clear();
    file.status = file.stages.empty() ? "" : "registered, not loaded by the game yet";
    ++file.generation;
}

// Drops files with no stages left. Call with g_lock held.
void DropEmptyFiles()
{
    g_files.erase(std::remove_if(g_files.begin(), g_files.end(), [](const std::unique_ptr<PatchedFile>& file) {
                      return file->stages.empty();
                  }),
                  g_files.end());
}

bool IsReplace(const Stage& stage) { return stage.kind != StageKind::kFunction; }

// Adds `stage` to the pipeline of `pathSuffix` (kind, owner, fromScript,
// order and its data set). A Replace stage is refused when another owner
// already has one on the file and replaces the same owner's (a menu script
// for another original is added to it); a function stage replaces the same
// owner's function stage. False with the reason.
bool AddStage(const std::string& pathSuffix, std::unique_ptr<Stage> stage, std::string& reason)
{
    if (pathSuffix.empty()) {
        reason = "empty path";
        return false;
    }
    if (!EnsureFileHook(reason)) return false;
    const std::string owner = stage->owner;
    AcquireSRWLockExclusive(&g_lock);
    PatchedFile* file = FileForPath(pathSuffix);
    if (!file) {
        g_files.push_back(std::make_unique<PatchedFile>());
        file = g_files.back().get();
        file->pathSuffix = pathSuffix;
    }
    auto& stages = file->stages;
    for (auto it = stages.begin(); it != stages.end(); ++it) {
        const Stage& existing = **it;
        if (IsReplace(*stage) && IsReplace(existing) && existing.owner != owner) {
            reason = "already patched by " + existing.owner;
            DropEmptyFiles();
            ReleaseSRWLockExclusive(&g_lock);
            return false;
        }
        if (existing.owner != owner || IsReplace(existing) != IsReplace(*stage)) continue;
        // the same owner's scripts for other originals stay
        if (existing.kind == StageKind::kIggyScript && stage->kind == StageKind::kIggyScript) {
            const IggyVariant& added = stage->variants.front();
            for (const IggyVariant& kept : existing.variants)
                if (kept.vanillaAbcSize != added.vanillaAbcSize || kept.vanillaAbcHash != added.vanillaAbcHash)
                    stage->variants.push_back(kept);
        }
        stage->sequence = existing.sequence;   // keeps its place
        stages.erase(it);
        break;
    }
    if (!stage->sequence) stage->sequence = ++g_nextSequence;
    stages.push_back(std::move(stage));
    std::stable_sort(stages.begin(), stages.end(), [](const std::shared_ptr<const Stage>& a, const std::shared_ptr<const Stage>& b) {
        return a->order != b->order ? a->order < b->order : a->sequence < b->sequence;
    });
    PipelineChanged(*file);
    const size_t count = stages.size();
    ReleaseSRWLockExclusive(&g_lock);
    Log("file %s: %s registered a patch (%zu in its pipeline)", pathSuffix.c_str(), owner.c_str(), count);
    return true;
}

// The Replace* registrations: a stage that runs before every function stage.
bool RegisterReplace(const std::string& pathSuffix, std::unique_ptr<Stage> stage, std::string& reason)
{
    stage->order = kReplaceOrder;
    return AddStage(pathSuffix, std::move(stage), reason);
}

// Removes `owner`'s stages of `path` (a mod path, normalised here). False
// when there are none.
bool RemoveStages(const std::string& owner, const char* path)
{
    const std::string pathSuffix = NormalisePath(path);
    AcquireSRWLockExclusive(&g_lock);
    bool removed = false;
    if (PatchedFile* file = FileForPath(pathSuffix)) {
        auto& stages = file->stages;
        const size_t before = stages.size();
        stages.erase(std::remove_if(stages.begin(), stages.end(),
                                    [&](const std::shared_ptr<const Stage>& stage) { return stage->owner == owner; }),
                     stages.end());
        removed = stages.size() != before;
        if (removed) PipelineChanged(*file);
        DropEmptyFiles();
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (removed) Log("file %s: %s removed its patch", pathSuffix.c_str(), owner.c_str());
    return removed;
}

// The status of the patched file `path` (a mod path), or "" when no mod
// patches it: "registered, not loaded by the game yet", "served: ...",
// "NOT patched: ..." (with several stages, "owner: outcome; ...").
std::string PatchStatus(const char* path)
{
    const std::string pathSuffix = NormalisePath(path);
    std::string status;
    AcquireSRWLockShared(&g_lock);
    if (PatchedFile* file = FileForPath(pathSuffix)) status = file->status;
    ReleaseSRWLockShared(&g_lock);
    return status;
}

// The owners of the stages of `path` (a mod path) in running order, or
// none when no mod patches it.
std::vector<std::string> PatchOwners(const char* path)
{
    const std::string pathSuffix = NormalisePath(path);
    std::vector<std::string> owners;
    AcquireSRWLockShared(&g_lock);
    if (PatchedFile* file = FileForPath(pathSuffix))
        for (const auto& stage : file->stages)
            if (std::find(owners.begin(), owners.end(), stage->owner) == owners.end()) owners.push_back(stage->owner);
    ReleaseSRWLockShared(&g_lock);
    return owners;
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
    auto stage = std::make_unique<Stage>();
    stage->kind = StageKind::kWholeFile;
    const std::string pathSuffix = NormalisePath(luaL_checkstring(L, 1));
    const std::wstring file = ArgFilePath(L, 2);
    stage->owner = ScriptName(L);
    stage->fromScript = true;
    if (!ReadFileBytes(file, stage->content)) return PushRegistered(L, false, "can't read the file");
    std::string reason;
    return PushRegistered(L, RegisterReplace(pathSuffix, std::move(stage), reason), reason);
}

// files.ReplaceIggyScript(path, abcFile, vanillaAbcSize, vanillaAbcHash)
//   -> true | false, reason
//   path            the menu ("ui/iggy/ITEMLIST.iggy")
//   abcFile         the compiled AS3 ABC, relative to the script's mod folder
//   vanillaAbcSize  size of the ABC it may replace (bytes)
//   vanillaAbcHash  FNV-1a 64 of that ABC (integer or "0x..." string)
int FilesReplaceIggyScript(lua_State* L)
{
    auto stage = std::make_unique<Stage>();
    stage->kind = StageKind::kIggyScript;
    const std::string pathSuffix = NormalisePath(luaL_checkstring(L, 1));
    const std::wstring abcFile = ArgFilePath(L, 2);
    const lua_Integer vanillaAbcSize = luaL_checkinteger(L, 3);
    luaL_argcheck(L, vanillaAbcSize > 0 && vanillaAbcSize <= 0xffffffff, 3, "not an ABC size");
    IggyVariant variant;
    variant.vanillaAbcSize = (uint32_t)vanillaAbcSize;
    variant.vanillaAbcHash = ArgHash(L, 4);
    stage->owner = ScriptName(L);
    stage->fromScript = true;
    if (!ReadFileBytes(abcFile, variant.abc)) return PushRegistered(L, false, "can't read the ABC file");
    stage->variants.push_back(std::move(variant));
    std::string reason;
    return PushRegistered(L, RegisterReplace(pathSuffix, std::move(stage), reason), reason);
}

// files.Owner(path) -> owner | nil
//   The first script (LUA_NAME) or DLL mod in the file's pipeline; nil when none.
int FilesOwner(lua_State* L)
{
    const std::vector<std::string> owners = PatchOwners(luaL_checkstring(L, 1));
    if (owners.empty()) lua_pushnil(L);
    else lua_pushstring(L, owners.front().c_str());
    return 1;
}

// files.Owners(path) -> { owner, ... }
//   Every mod in the file's pipeline, in running order (empty when none).
int FilesOwners(lua_State* L)
{
    const std::vector<std::string> owners = PatchOwners(luaL_checkstring(L, 1));
    lua_createtable(L, (int)owners.size(), 0);
    for (size_t i = 0; i < owners.size(); ++i) {
        lua_pushstring(L, owners[i].c_str());
        lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
}

// files.Remove(path) -> removed
//   Drops this script's patches of `path`; copies the game already holds stay.
int FilesRemove(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, RemoveStages(ScriptName(L), path));
    return 1;
}

// files.Status(path) -> status | nil   (also files.IggyScriptStatus)
//   What happened when the game last read the file ("served: ...",
//   "NOT patched: ...", "registered, not loaded by the game yet"; with
//   several mods "owner: outcome; ..."); nil when no mod patches it.
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
        {"Owners",            FilesOwners},
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
    for (auto& file : g_files) {
        auto& stages = file->stages;
        const size_t before = stages.size();
        stages.erase(std::remove_if(stages.begin(), stages.end(),
                                    [](const std::shared_ptr<const Stage>& stage) { return stage->fromScript; }),
                     stages.end());
        if (stages.size() != before) PipelineChanged(*file);
    }
    DropEmptyFiles();
    ReleaseSRWLockExclusive(&g_lock);
}

// ---------------------------------------------------------------------------
//  For DLL mods (sdk\xv2_files.h). Any thread. Registrations stay until
//  removed (an F1 reload only drops the Lua scripts').
// ---------------------------------------------------------------------------

// Shared by the exports: checks owner and path, registers, copies a refusal's
// reason out. Returns 1 / 0.
static int RegisterFromDll(std::unique_ptr<Stage> stage, const char* owner, const char* path, bool replace, char* reason,
                           int reasonBytes)
{
    std::string failure;
    if (!owner || !*owner || !path) {
        failure = "bad arguments (owner and path are required)";
    } else {
        stage->owner = owner;
        const std::string pathSuffix = NormalisePath(path);
        if (replace ? RegisterReplace(pathSuffix, std::move(stage), failure) : AddStage(pathSuffix, std::move(stage), failure))
            return 1;
    }
    CopyOut(failure, reason, reasonBytes);
    return 0;
}

// The data argument of the Replace exports: copied, or false when missing / too big.
static bool CopyData(const void* data, size_t size, std::vector<uint8_t>& out)
{
    if (!data || size == 0 || size > kMaxContentBytes) return false;
    out.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    return true;
}

extern "C" __declspec(dllexport) int XV2Files_ReplaceFile(const char* owner, const char* path, const void* data,
                                                          size_t size, char* reason, int reasonBytes)
{
    auto stage = std::make_unique<Stage>();
    stage->kind = StageKind::kWholeFile;
    if (!CopyData(data, size, stage->content)) return CopyOut("bad arguments (no data)", reason, reasonBytes), 0;
    return RegisterFromDll(std::move(stage), owner, path, true, reason, reasonBytes);
}

extern "C" __declspec(dllexport) int XV2Files_ReplaceIggyScript(const char* owner, const char* path, const void* abc,
                                                                size_t abcSize, uint32_t vanillaAbcSize,
                                                                uint64_t vanillaAbcHash, char* reason,
                                                                int reasonBytes)
{
    auto stage = std::make_unique<Stage>();
    stage->kind = StageKind::kIggyScript;
    IggyVariant variant;
    variant.vanillaAbcSize = vanillaAbcSize;
    variant.vanillaAbcHash = vanillaAbcHash;
    if (!CopyData(abc, abcSize, variant.abc)) return CopyOut("bad arguments (no data)", reason, reasonBytes), 0;
    stage->variants.push_back(std::move(variant));
    return RegisterFromDll(std::move(stage), owner, path, true, reason, reasonBytes);
}

extern "C" __declspec(dllexport) int XV2Files_AddStage(const char* owner, const char* path, int order,
                                                       XV2FileStage function, void* user, char* reason, int reasonBytes)
{
    if (!function) return CopyOut("bad arguments (no function)", reason, reasonBytes), 0;
    auto stage = std::make_unique<Stage>();
    stage->kind = StageKind::kFunction;
    stage->order = order == INT_MIN ? INT_MIN + 1 : order;   // INT_MIN is the Replace patches'
    stage->function = function;
    stage->user = user;
    return RegisterFromDll(std::move(stage), owner, path, false, reason, reasonBytes);
}

extern "C" __declspec(dllexport) int XV2Files_Remove(const char* owner, const char* path)
{
    return owner && path && RemoveStages(owner, path) ? 1 : 0;
}

extern "C" __declspec(dllexport) int XV2Files_Status(const char* path, char* out, int outBytes)
{
    return CopyOut(path ? PatchStatus(path) : std::string(), out, outBytes);
}

extern "C" __declspec(dllexport) int XV2Files_Owner(const char* path, char* out, int outBytes)
{
    const std::vector<std::string> owners = path ? PatchOwners(path) : std::vector<std::string>();
    return CopyOut(owners.empty() ? std::string() : owners.front(), out, outBytes);
}

extern "C" __declspec(dllexport) int XV2Files_Owners(const char* path, char* out, int outBytes)
{
    std::string joined;
    if (path)
        for (const std::string& owner : PatchOwners(path)) joined += (joined.empty() ? "" : "\n") + owner;
    return CopyOut(joined, out, outBytes);
}
