// ---------------------------------------------------------------------------
//  XV2UserInterfaceLib.dll - lets several mods edit the same menu.
//
//  A mod registers a script edit: its whole edited ABC (AS3 script of an
//  .iggy menu), plus what it was made from. Each edit becomes a stage of the
//  loader's file pipeline (XV2Files::AddStage, sdk\xv2_files.h): when the game
//  reads the menu, every mod's stage in turn takes the menu as the mods
//  before it left it, merges in only what that mod changed
//  (formats\abc_merge.h) and passes it on. Nothing is written to disk.
//  A mod may register a script per version of the menu (the cpk's,
//  XV2Patcher's loose one, ...): the stage uses the one made from the script
//  the game loads.
//  Image edits work the same way on texture files (.iggytex / .iggyted /
//  .emb): a mod names the images it replaces or adds; the stage applies them
//  over the file as the mods before it left it (formats\image_edit.h), so
//  mods changing different images of one file all apply.
//  Message edits do the same with the game's text (.msg): a mod names the
//  entries whose text it sets (formats\msg_file.h).
//
//  DLL mods: the XV2UI_* exports (sdk\xv2_ui.h). Lua scripts: the "uilib"
//  table (sdk\modloader_lib.lua); a script's edits are dropped on an F1
//  reload (the script registers them again).
//
//  Loaded by the loader from XV2ModLoader\Lib\ (ModLoaderLib_Init,
//  sdk\modloader_lib.h); needs XV2ModLoaderLib.dll's file pipeline.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <cctype>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "lua.h"
#include "lauxlib.h"
#include "modloader_lib.h"
#include "xv2_files.h"
#include "formats/abc_merge.h"
#include "formats/iggy_file.h"
#include "formats/image_edit.h"
#include "formats/msg_file.h"

namespace {

using xv2ui::Bytes;

// Largest script an edit may hold: far above any menu's (CHARASELE's ABC is
// ~270 KB with the search added).
const size_t kMaxScriptBytes = 64u * 1024 * 1024;

const ModLoaderHost* g_host = nullptr;

// A line in loader.log (printf-style); dropped before ModLoaderLib_Init.
void Log(const char* format, ...)
{
    if (!g_host) return;
    char line[1024];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    g_host->Log(line);
}

// One script a mod made for one version of the menu.
struct ScriptVariant {
    Bytes mod;                      // the mod's whole edited ABC
    Bytes base;                     // what it was made from; empty = `game`
    Bytes game;                     // the game script `base` comes from; empty = the menu's own
};

// One mod's edit of one menu script: a script per version of the menu it
// was made for (e.g. the cpk's CHARASELE and XV2Patcher's loose one). The
// stage picks the one whose `game` is the script the game loads.
struct ScriptEdit {
    std::string owner, path;
    std::string key;                // path, normalised (lower case, '/'), to find it again
    bool fromScript = false;        // a Lua script's: dropped on an F1 reload
    std::vector<ScriptVariant> variants;
};

// Every edit ever registered. Never freed: a stage may still be running on
// a loading thread with its edit when the mod replaces or removes it (they
// are a few hundred KB, registered once per mod and file and version).
SRWLOCK g_editsLock = SRWLOCK_INIT;
std::vector<std::unique_ptr<ScriptEdit>> g_edits;
std::vector<ScriptEdit*> g_liveEdits;     // the one in the pipeline per owner and path

// The loader library's file pipeline (loaded before every mod; looked up on
// first use, so the order the loader loads Lib\ in doesn't matter).
XV2Files* Files(std::string& reason)
{
    static XV2Files files;
    static bool loaded = false;
    if (!loaded) loaded = files.Load() && files.AddStage;
    if (!loaded) reason = "needs XV2ModLoaderLib.dll with file stages (update the mod loader)";
    return loaded ? &files : nullptr;
}

// Copies `text` into a caller's buffer, cut to fit and 0-terminated.
void CopyOut(const std::string& text, char* out, size_t outBytes)
{
    if (!out || outBytes == 0) return;
    const size_t copied = text.copy(out, outBytes - 1);
    out[copied] = '\0';
}

// The ABC inside an .iggy. False when it has none.
bool IggyScript(const uint8_t* data, size_t size, Bytes& abc, xv2ui::IggyDocument& document)
{
    document.Parse(data, size);
    const uint8_t* script = nullptr;
    uint32_t scriptSize = 0;
    if (!document.movie.Script(script, scriptSize)) return false;
    abc.assign(script, script + scriptSize);
    return true;
}

// The pipeline stage of one edit (an XV2FileStage): merges the edit into
// the current menu. Never throws (the loader is C).
int __cdecl MergeStage(void* user, const char* path, const void* original, size_t originalSize, const void* current,
                       size_t currentSize, XV2FileOutput* output)
{
    const ScriptEdit& edit = *static_cast<const ScriptEdit*>(user);
    std::string note;
    int outcome = -1;
    try {
        xv2ui::IggyDocument currentMenu, originalMenu;
        Bytes currentAbc, originalAbc;
        if (!IggyScript(static_cast<const uint8_t*>(current), currentSize, currentAbc, currentMenu))
            throw xv2ui::FormatError("the menu has no script");
        if (!IggyScript(static_cast<const uint8_t*>(original), originalSize, originalAbc, originalMenu))
            throw xv2ui::FormatError("the game's menu has no script");

        // The script made for this menu's version; else the one made for
        // "whatever the menu is"; else the first, merged over its own game
        // script (the menu's differences then count as an earlier edit).
        const ScriptVariant* variant = nullptr;
        for (const ScriptVariant& candidate : edit.variants)
            if (!variant && candidate.game == originalAbc) variant = &candidate;
        for (const ScriptVariant& candidate : edit.variants)
            if (!variant && candidate.game.empty()) variant = &candidate;
        const bool foreign = !variant;
        if (foreign) variant = &edit.variants.front();
        const Bytes& gameAbc = foreign ? variant->game : originalAbc;

        xv2ui::abc::MergeReport report;
        const Bytes merged = xv2ui::abc::MergeScript(gameAbc, currentAbc, variant->mod, &report,
                                                     variant->base.empty() ? nullptr : &variant->base);
        for (const std::string& line : report.notes) Log("uilib %s, %s: %s", path, edit.owner.c_str(), line.c_str());
        note = report.Summary();
        if (edit.variants.size() > 1)
            note += " (script " + std::to_string(variant - edit.variants.data() + 1) + " of " +
                    std::to_string(edit.variants.size()) + ")";
        if (foreign) note += " (made for another version of the menu)";
        if (report.conflicts) note += " - see loader.log";
        if (report.Changed()) {
            currentMenu.movie.SetScript(merged.data(), static_cast<uint32_t>(merged.size()));
            const Bytes written = currentMenu.Write();
            output->Set(output, written.data(), written.size());
            outcome = 1;
        } else {
            outcome = 0;
        }
    } catch (const std::exception& error) {
        note = std::string("script merge failed: ") + error.what();
    } catch (...) {
        note = "script merge failed";
    }
    CopyOut(note, output->note, sizeof(output->note));
    return outcome;
}

// A path as the loader compares them: lower case, '/' (ASCII only, as game paths are).
std::string PathKey(const std::string& path)
{
    std::string key = path;
    for (char& c : key) c = c == '\\' ? '/' : (char)tolower((unsigned char)c);
    return key;
}

// Drops owner's live edit of `key` from g_liveEdits (lock held). Returns it, or null.
ScriptEdit* TakeLive(const std::string& owner, const std::string& key)
{
    for (auto it = g_liveEdits.begin(); it != g_liveEdits.end(); ++it) {
        if ((*it)->owner == owner && (*it)->key == key) {
            ScriptEdit* edit = *it;
            g_liveEdits.erase(it);
            return edit;
        }
    }
    return nullptr;
}

// Registers one script of owner's edit of `path` as a stage: its others
// stay, except one made for the same game script (replaced). False with
// the reason.
bool Register(const std::string& owner, const std::string& path, bool fromScript, ScriptVariant variant, int order,
              std::string& reason)
{
    XV2Files* files = Files(reason);
    if (!files) return false;
    if (variant.mod.empty() || variant.mod.size() > kMaxScriptBytes) {
        reason = "no script (or larger than 64 MB)";
        return false;
    }
    // A bad script is refused now, not at every menu load.
    try {
        xv2ui::abc::AbcFile check;
        check.Parse(variant.mod.data(), variant.mod.size());
        if (!variant.base.empty()) check.Parse(variant.base.data(), variant.base.size());
        if (!variant.game.empty()) check.Parse(variant.game.data(), variant.game.size());
    } catch (const std::exception& error) {
        reason = std::string("not a valid script: ") + error.what();
        return false;
    }
    // A new edit object (the old one may be in use by a running stage):
    // the live edit's other scripts + this one.
    auto edit = std::make_unique<ScriptEdit>();
    edit->owner = owner;
    edit->path = path;
    edit->key = PathKey(path);
    edit->fromScript = fromScript;
    AcquireSRWLockExclusive(&g_editsLock);
    for (ScriptEdit* live : g_liveEdits)
        if (live->owner == owner && live->key == edit->key)
            for (const ScriptVariant& kept : live->variants)
                if (kept.game != variant.game) edit->variants.push_back(kept);
    ReleaseSRWLockExclusive(&g_editsLock);
    edit->variants.push_back(std::move(variant));
    ScriptEdit* stored = edit.get();
    AcquireSRWLockExclusive(&g_editsLock);
    g_edits.push_back(std::move(edit));
    ReleaseSRWLockExclusive(&g_editsLock);

    char failure[256] = "";
    if (!files->AddStage(stored->owner.c_str(), stored->path.c_str(), order, &MergeStage, stored, failure,
                         sizeof(failure))) {
        reason = failure;
        return false;
    }
    AcquireSRWLockExclusive(&g_editsLock);
    TakeLive(stored->owner, stored->key);   // the one it replaced
    g_liveEdits.push_back(stored);
    ReleaseSRWLockExclusive(&g_editsLock);
    Log("uilib: %s edits the script of %s (%zu script%s)", stored->owner.c_str(), stored->path.c_str(),
        stored->variants.size(), stored->variants.size() == 1 ? "" : "s");
    return true;
}

// --- Image edits -------------------------------------------------------------

// One mod's images in one texture file (.iggytex / .iggyted / .emb): name ->
// data, applied over the file as the mods before it left it
// (formats\image_edit.h). One stage per owner and file.
struct ImageEdit {
    std::string owner, path;
    std::string key;                // as ScriptEdit::key
    bool fromScript = false;
    std::map<std::string, Bytes> images;
};

// As g_edits / g_liveEdits (same lock, never freed for the same reason).
std::vector<std::unique_ptr<ImageEdit>> g_imageEdits;
std::vector<ImageEdit*> g_liveImageEdits;

// The pipeline stage of one image edit. Never throws.
int __cdecl ImageStage(void* user, const char* path, const void* original, size_t originalSize, const void* current,
                       size_t currentSize, XV2FileOutput* output)
{
    const ImageEdit& edit = *static_cast<const ImageEdit*>(user);
    std::string note;
    int outcome = -1;
    try {
        const auto* originalBytes = static_cast<const uint8_t*>(original);
        const auto* currentBytes = static_cast<const uint8_t*>(current);
        xv2ui::ImageReport report;
        const Bytes written = xv2ui::ApplyImages(Bytes(originalBytes, originalBytes + originalSize),
                                                 Bytes(currentBytes, currentBytes + currentSize), edit.images, &report);
        for (const std::string& line : report.notes) Log("uilib %s, %s: %s", path, edit.owner.c_str(), line.c_str());
        note = report.Summary();
        if (report.Changed()) {
            output->Set(output, written.data(), written.size());
            outcome = 1;
        } else {
            outcome = 0;
        }
    } catch (const std::exception& error) {
        note = std::string("image edit failed: ") + error.what();
    } catch (...) {
        note = "image edit failed";
    }
    CopyOut(note, output->note, sizeof(output->note));
    return outcome;
}

// Drops owner's live image edit of `key` (lock held). Returns it, or null.
ImageEdit* TakeLiveImages(const std::string& owner, const std::string& key)
{
    for (auto it = g_liveImageEdits.begin(); it != g_liveImageEdits.end(); ++it) {
        if ((*it)->owner == owner && (*it)->key == key) {
            ImageEdit* edit = *it;
            g_liveImageEdits.erase(it);
            return edit;
        }
    }
    return nullptr;
}

// Sets (data non-null) or drops (data null) one image of owner's edit of
// `path`, as a new edit object holding the live one's other images; with no
// image left the stage is removed. False with the reason.
bool SetImage(const std::string& owner, const std::string& path, bool fromScript, const std::string& name,
              const Bytes* data, int order, std::string& reason)
{
    XV2Files* files = Files(reason);
    if (!files) return false;
    if (data) {
        if (data->size() > kMaxScriptBytes) {
            reason = "image larger than 64 MB";
            return false;
        }
        reason = xv2ui::CheckImage(path, name, *data);
        if (!reason.empty()) return false;
    }
    auto edit = std::make_unique<ImageEdit>();
    edit->owner = owner;
    edit->path = path;
    edit->key = PathKey(path);
    edit->fromScript = fromScript;
    AcquireSRWLockExclusive(&g_editsLock);
    for (ImageEdit* live : g_liveImageEdits)
        if (live->owner == owner && live->key == edit->key) edit->images = live->images;
    ReleaseSRWLockExclusive(&g_editsLock);
    if (data) {
        edit->images[name] = *data;
    } else if (!edit->images.erase(name)) {
        reason = "no image " + name + " of yours in " + path;
        return false;
    }

    if (edit->images.empty()) {
        AcquireSRWLockExclusive(&g_editsLock);
        TakeLiveImages(owner, edit->key);
        ReleaseSRWLockExclusive(&g_editsLock);
        files->Remove(owner.c_str(), path.c_str());
        return true;
    }
    ImageEdit* stored = edit.get();
    AcquireSRWLockExclusive(&g_editsLock);
    g_imageEdits.push_back(std::move(edit));
    ReleaseSRWLockExclusive(&g_editsLock);
    char failure[256] = "";
    if (!files->AddStage(stored->owner.c_str(), stored->path.c_str(), order, &ImageStage, stored, failure,
                         sizeof(failure))) {
        reason = failure;
        return false;
    }
    AcquireSRWLockExclusive(&g_editsLock);
    TakeLiveImages(stored->owner, stored->key);
    g_liveImageEdits.push_back(stored);
    ReleaseSRWLockExclusive(&g_editsLock);
    Log("uilib: %s %s image %s of %s (%zu image%s)", stored->owner.c_str(), data ? "sets" : "drops", name.c_str(),
        stored->path.c_str(), stored->images.size(), stored->images.size() == 1 ? "" : "s");
    return true;
}

// --- Message edits -----------------------------------------------------------

// One mod's texts in one .msg file: entry name -> text as the file stores it
// (formats\msg_file.h MsgText), applied over the file as the mods before it
// left it. One stage per owner and file.
struct MessageEdit {
    std::string owner, path;
    std::string key;                // as ScriptEdit::key
    bool fromScript = false;
    std::map<std::string, std::u16string> messages;
};

// As g_edits / g_liveEdits (same lock, never freed for the same reason).
std::vector<std::unique_ptr<MessageEdit>> g_messageEdits;
std::vector<MessageEdit*> g_liveMessageEdits;

// The pipeline stage of one message edit. Never throws.
int __cdecl MessageStage(void* user, const char* path, const void* original, size_t originalSize, const void* current,
                         size_t currentSize, XV2FileOutput* output)
{
    const MessageEdit& edit = *static_cast<const MessageEdit*>(user);
    std::string note;
    int outcome = -1;
    try {
        const auto* originalBytes = static_cast<const uint8_t*>(original);
        const auto* currentBytes = static_cast<const uint8_t*>(current);
        xv2ui::MessageReport report;
        const Bytes written = xv2ui::ApplyMessages(Bytes(originalBytes, originalBytes + originalSize),
                                                   Bytes(currentBytes, currentBytes + currentSize), edit.messages, &report);
        for (const std::string& line : report.notes) Log("uilib %s, %s: %s", path, edit.owner.c_str(), line.c_str());
        note = report.Summary();
        if (report.Changed()) {
            output->Set(output, written.data(), written.size());
            outcome = 1;
        } else {
            outcome = 0;
        }
    } catch (const std::exception& error) {
        note = std::string("message edit failed: ") + error.what();
    } catch (...) {
        note = "message edit failed";
    }
    CopyOut(note, output->note, sizeof(output->note));
    return outcome;
}

// Drops owner's live message edit of `key` (lock held). Returns it, or null.
MessageEdit* TakeLiveMessages(const std::string& owner, const std::string& key)
{
    for (auto it = g_liveMessageEdits.begin(); it != g_liveMessageEdits.end(); ++it) {
        if ((*it)->owner == owner && (*it)->key == key) {
            MessageEdit* edit = *it;
            g_liveMessageEdits.erase(it);
            return edit;
        }
    }
    return nullptr;
}

// Sets (text non-null, UTF-8) or drops (text null) one entry of owner's edit
// of `path`, as SetImage does with images. False with the reason.
bool SetMessage(const std::string& owner, const std::string& path, bool fromScript, const std::string& name,
                const char* text, int order, std::string& reason)
{
    XV2Files* files = Files(reason);
    if (!files) return false;
    reason = xv2ui::CheckMessage(path, name);
    if (!reason.empty()) return false;
    std::u16string storedText;
    if (text) {
        try {
            storedText = xv2ui::MsgText(text);
        } catch (const std::exception& error) {
            reason = error.what();
            return false;
        }
    }
    auto edit = std::make_unique<MessageEdit>();
    edit->owner = owner;
    edit->path = path;
    edit->key = PathKey(path);
    edit->fromScript = fromScript;
    AcquireSRWLockExclusive(&g_editsLock);
    for (MessageEdit* live : g_liveMessageEdits)
        if (live->owner == owner && live->key == edit->key) edit->messages = live->messages;
    ReleaseSRWLockExclusive(&g_editsLock);
    if (text) {
        edit->messages[name] = std::move(storedText);
    } else if (!edit->messages.erase(name)) {
        reason = "no message " + name + " of yours in " + path;
        return false;
    }

    if (edit->messages.empty()) {
        AcquireSRWLockExclusive(&g_editsLock);
        TakeLiveMessages(owner, edit->key);
        ReleaseSRWLockExclusive(&g_editsLock);
        files->Remove(owner.c_str(), path.c_str());
        return true;
    }
    MessageEdit* stored = edit.get();
    AcquireSRWLockExclusive(&g_editsLock);
    g_messageEdits.push_back(std::move(edit));
    ReleaseSRWLockExclusive(&g_editsLock);
    char failure[256] = "";
    if (!files->AddStage(stored->owner.c_str(), stored->path.c_str(), order, &MessageStage, stored, failure,
                         sizeof(failure))) {
        reason = failure;
        return false;
    }
    AcquireSRWLockExclusive(&g_editsLock);
    TakeLiveMessages(stored->owner, stored->key);
    g_liveMessageEdits.push_back(stored);
    ReleaseSRWLockExclusive(&g_editsLock);
    Log("uilib: %s %s message %s of %s (%zu message%s)", stored->owner.c_str(), text ? "sets" : "drops", name.c_str(),
        stored->path.c_str(), stored->messages.size(), stored->messages.size() == 1 ? "" : "s");
    return true;
}

// Removes owner's edit of path (script, images or messages). False when there was none.
bool RemoveEdit(const char* owner, const char* path)
{
    std::string reason;
    XV2Files* files = Files(reason);
    if (!files) return false;
    AcquireSRWLockExclusive(&g_editsLock);
    TakeLive(owner, PathKey(path));
    TakeLiveImages(owner, PathKey(path));
    TakeLiveMessages(owner, PathKey(path));
    ReleaseSRWLockExclusive(&g_editsLock);
    return files->Remove(owner, path) != 0;
}

// F1 reload: every Lua script's edits go (the scripts register them again).
void ClearScriptEdits()
{
    std::vector<std::pair<std::string, std::string>> removed;
    AcquireSRWLockExclusive(&g_editsLock);
    for (auto it = g_liveEdits.begin(); it != g_liveEdits.end();) {
        if ((*it)->fromScript) {
            removed.emplace_back((*it)->owner, (*it)->path);
            it = g_liveEdits.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = g_liveImageEdits.begin(); it != g_liveImageEdits.end();) {
        if ((*it)->fromScript) {
            removed.emplace_back((*it)->owner, (*it)->path);
            it = g_liveImageEdits.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = g_liveMessageEdits.begin(); it != g_liveMessageEdits.end();) {
        if ((*it)->fromScript) {
            removed.emplace_back((*it)->owner, (*it)->path);
            it = g_liveMessageEdits.erase(it);
        } else {
            ++it;
        }
    }
    ReleaseSRWLockExclusive(&g_editsLock);
    std::string reason;
    if (XV2Files* files = Files(reason))
        for (const auto& edit : removed) files->Remove(edit.first.c_str(), edit.second.c_str());
}

// --- Lua ---------------------------------------------------------------------

// UTF-8 -> UTF-16 (file paths).
std::wstring Wide(const char* text)
{
    int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::wstring wide(length > 1 ? (size_t)length - 1 : 0, L'\0');
    if (length > 1) MultiByteToWideChar(CP_UTF8, 0, text, -1, &wide[0], length);
    return wide;
}

// The file argument at `index`, relative to the script's mod folder (the
// loader keeps it in the registry as "xv2_script_folder") unless absolute.
std::wstring ArgFilePath(lua_State* L, int index)
{
    std::wstring wide = Wide(luaL_checkstring(L, index));
    const bool absolute = (wide.size() > 1 && wide[1] == L':') || (!wide.empty() && (wide[0] == L'\\' || wide[0] == L'/'));
    if (absolute) return wide;
    lua_getfield(L, LUA_REGISTRYINDEX, "xv2_script_folder");
    const char* folder = lua_tostring(L, -1);
    std::wstring path = (folder ? Wide(folder) : std::wstring()) + L"\\" + wide;
    lua_pop(L, 1);
    return path;
}

// Reads a whole file; false when it can't or it is empty / too big.
bool ReadFileBytes(const std::wstring& path, Bytes& out)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") || !file) return false;
    uint8_t buffer[65536];
    size_t got = 0;
    while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0 && out.size() <= kMaxScriptBytes)
        out.insert(out.end(), buffer, buffer + got);
    fclose(file);
    return !out.empty() && out.size() <= kMaxScriptBytes;
}

// The script's name: its LUA_NAME global, "Lua" without one (as the loader's).
std::string ScriptName(lua_State* L)
{
    std::string name = "Lua";
    if (lua_getglobal(L, "LUA_NAME") == LUA_TSTRING) name = lua_tostring(L, -1);
    lua_pop(L, 1);
    return name;
}

// uilib.EditScript(path, abcFile [, baseFile [, gameFile [, order]]])
//   -> true | false, reason
//   path      the menu ("ui/iggy/CHARASELE.iggy")
//   abcFile   the whole edited ABC, relative to the script's mod folder
//   baseFile  what it was made from when not the game's script (an FFDec
//             rebuild, ClaudeTools\iggy\ffdec_base.py); nil = gameFile
//   gameFile  the game script baseFile comes from; nil = the menu's own
//   order     lower runs earlier (default 0)
// Called again with another gameFile, it adds a script for that version of
// the menu; with the same one, it replaces that script.
int UiEditScript(lua_State* L)
{
    const std::string path = luaL_checkstring(L, 1);
    const int order = (int)luaL_optinteger(L, 5, 0);
    ScriptVariant variant;
    std::string reason;
    if (!ReadFileBytes(ArgFilePath(L, 2), variant.mod)) reason = "can't read the ABC file";
    else if (!lua_isnoneornil(L, 3) && !ReadFileBytes(ArgFilePath(L, 3), variant.base)) reason = "can't read the base file";
    else if (!lua_isnoneornil(L, 4) && !ReadFileBytes(ArgFilePath(L, 4), variant.game)) reason = "can't read the game file";
    const bool registered = reason.empty() && Register(ScriptName(L), path, true, std::move(variant), order, reason);
    lua_pushboolean(L, registered);
    if (registered) return 1;
    lua_pushstring(L, reason.c_str());
    return 2;
}

// uilib.Remove(path) -> removed
int UiRemove(lua_State* L)
{
    lua_pushboolean(L, RemoveEdit(ScriptName(L).c_str(), luaL_checkstring(L, 1)));
    return 1;
}

// Pushes true, or false + reason (the uilib functions' result).
int PushResult(lua_State* L, bool done, const std::string& reason)
{
    lua_pushboolean(L, done);
    if (done) return 1;
    lua_pushstring(L, reason.c_str());
    return 2;
}

// uilib.SetImage(path, name, imageFile [, order]) -> true | false, reason
//   path       the texture file ("ui/iggy/CMNTITLE_en.iggytex", "ui/texture/CHARA01.emb")
//   name       iggytex/iggyted: the texture id (14 or "14"); emb: the entry's
//              name, or "#<index>" in an EMB without names
//   imageFile  the new image (a DDS for iggytex), relative to the mod folder
//   order      lower runs earlier (default 0); the file's first call sets it
// Replaces that image, or adds it when the file has none by that name.
int UiSetImage(lua_State* L)
{
    const std::string path = luaL_checkstring(L, 1);
    const std::string name = luaL_checkstring(L, 2);
    const int order = (int)luaL_optinteger(L, 4, 0);
    Bytes data;
    std::string reason;
    if (!ReadFileBytes(ArgFilePath(L, 3), data)) return PushResult(L, false, "can't read the image file");
    return PushResult(L, SetImage(ScriptName(L), path, true, name, &data, order, reason), reason);
}

// uilib.RemoveImage(path, name) -> true | false, reason
int UiRemoveImage(lua_State* L)
{
    std::string reason;
    const bool removed = SetImage(ScriptName(L), luaL_checkstring(L, 1), true, luaL_checkstring(L, 2), nullptr, 0, reason);
    return PushResult(L, removed, reason);
}

// uilib.SetMessage(path, name, text [, order]) -> true | false, reason
//   path   the msg file ("msg/lobby_text_en.msg"; one call per language)
//   name   the entry's name ("name_4293"; the game looks text up by name)
//   text   plain UTF-8; ' " & < > are stored as the game's entities
//   order  lower runs earlier (default 0); the file's first call sets it
// Replaces the entry's first line, or adds the entry when the file has none.
int UiSetMessage(lua_State* L)
{
    const std::string path = luaL_checkstring(L, 1);
    const std::string name = luaL_checkstring(L, 2);
    const char* text = luaL_checkstring(L, 3);
    const int order = (int)luaL_optinteger(L, 4, 0);
    std::string reason;
    return PushResult(L, SetMessage(ScriptName(L), path, true, name, text, order, reason), reason);
}

// uilib.RemoveMessage(path, name) -> true | false, reason
int UiRemoveMessage(lua_State* L)
{
    std::string reason;
    const bool removed = SetMessage(ScriptName(L), luaL_checkstring(L, 1), true, luaL_checkstring(L, 2), nullptr, 0, reason);
    return PushResult(L, removed, reason);
}

void RegisterTable(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {"EditScript",    UiEditScript},
        {"Remove",        UiRemove},
        {"SetImage",      UiSetImage},
        {"RemoveImage",   UiRemoveImage},
        {"SetMessage",    UiSetMessage},
        {"RemoveMessage", UiRemoveMessage},
        {nullptr, nullptr},
    };
    luaL_newlib(L, functions);
    lua_setglobal(L, "uilib");
}

// A copy of a DLL mod's buffer; empty for null / size 0.
Bytes Copy(const void* data, size_t size)
{
    if (!data || size == 0 || size > kMaxScriptBytes) return Bytes();
    return Bytes(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
}

}  // namespace

extern "C" __declspec(dllexport) bool ModLoaderLib_Init(const ModLoaderHost* host)
{
    if (!host || host->version < 1) return false;
    g_host = host;
    host->AddLuaRegistrar(RegisterTable);
    host->AddUnloadHandler(ClearScriptEdits);
    return true;
}

extern "C" __declspec(dllexport) int XV2UI_EditScript(const char* owner, const char* path, const void* abc,
                                                      size_t abcSize, const void* base, size_t baseSize,
                                                      const void* game, size_t gameSize, int order, char* reason,
                                                      int reasonBytes)
{
    std::string failure;
    if (!owner || !*owner || !path || !*path) {
        failure = "bad arguments (owner and path are required)";
    } else {
        try {
            ScriptVariant variant;
            variant.mod = Copy(abc, abcSize);
            variant.base = Copy(base, baseSize);
            variant.game = Copy(game, gameSize);
            if (Register(owner, path, false, std::move(variant), order, failure)) return 1;
        } catch (const std::exception& error) {
            failure = error.what();
        }
    }
    CopyOut(failure, reason, reasonBytes > 0 ? (size_t)reasonBytes : 0);
    return 0;
}

extern "C" __declspec(dllexport) int XV2UI_Remove(const char* owner, const char* path)
{
    return owner && path && RemoveEdit(owner, path) ? 1 : 0;
}

extern "C" __declspec(dllexport) int XV2UI_SetImage(const char* owner, const char* path, const char* name,
                                                    const void* data, size_t size, int order, char* reason,
                                                    int reasonBytes)
{
    std::string failure;
    if (!owner || !*owner || !path || !*path || !name || !data || size == 0) {
        failure = "bad arguments (owner, path, name and data are required)";
    } else {
        try {
            const Bytes image = Copy(data, size);
            if (image.empty()) failure = "image larger than 64 MB";
            else if (SetImage(owner, path, false, name, &image, order, failure)) return 1;
        } catch (const std::exception& error) {
            failure = error.what();
        }
    }
    CopyOut(failure, reason, reasonBytes > 0 ? (size_t)reasonBytes : 0);
    return 0;
}

extern "C" __declspec(dllexport) int XV2UI_RemoveImage(const char* owner, const char* path, const char* name)
{
    if (!owner || !path || !name) return 0;
    std::string reason;
    try {
        return SetImage(owner, path, false, name, nullptr, 0, reason) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

extern "C" __declspec(dllexport) int XV2UI_SetMessage(const char* owner, const char* path, const char* name,
                                                      const char* text, int order, char* reason, int reasonBytes)
{
    std::string failure;
    if (!owner || !*owner || !path || !*path || !name || !text) {
        failure = "bad arguments (owner, path, name and text are required)";
    } else {
        try {
            if (SetMessage(owner, path, false, name, text, order, failure)) return 1;
        } catch (const std::exception& error) {
            failure = error.what();
        }
    }
    CopyOut(failure, reason, reasonBytes > 0 ? (size_t)reasonBytes : 0);
    return 0;
}

extern "C" __declspec(dllexport) int XV2UI_RemoveMessage(const char* owner, const char* path, const char* name)
{
    if (!owner || !path || !name) return 0;
    std::string reason;
    try {
        return SetMessage(owner, path, false, name, nullptr, 0, reason) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}
