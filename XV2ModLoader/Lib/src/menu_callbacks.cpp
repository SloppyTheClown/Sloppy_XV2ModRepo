// ---------------------------------------------------------------------------
//  Menu callbacks - a menu's ActionScript talking to a Lua script.
//
//  Every Iggy menu class (OggyBase subclasses: SelectCharacter, ItemList,
//  ...) has a vtable slot the menu's AS reaches with
//  m_callback.CallbackUserData("user", sendType, value) or
//  CallbackUserDataString(...): the game's handler switches on sendType. A
//  patched menu script (files.ReplaceIggyScript) can send sendTypes of its
//  own; menus.HookCallback chains into that slot and takes those messages
//  (they never reach the game, which has no such cases), every other call
//  goes on to whatever the slot held. The script answers by putting a
//  string into the menu's user data, which the AS reads with
//  m_callback.GetUserDataString(index).
//
//  Call record (Iggy, seen in SelectCharacter's handler 0x5e1630 on
//  1.26.01): argument count at +0x10, arguments 0x20 apart from +0x28 - the
//  sendType (a double) at +0x28, the value at +0x48 (a double, or a UTF-8
//  char* for a string). User data (OggyBase): a vector at +0x160 (begin) /
//  +0x168 (end) of 16-byte entries {int set, int type, value}.
//
//  Threads: the game calls the slot on its menu thread, Lua runs on the
//  render thread (not proven to be the same one), so nothing here calls
//  into Lua from the hook. Messages are queued for menus.Messages; an
//  answer set by menus.SetUserDataString is written by the hook itself at
//  the menu's next taken message, on the game's thread. One SRWLOCK guards
//  it all.
//
//  Research_Notes\chara-select-search.md ("The AS <-> mod channel").
// ---------------------------------------------------------------------------
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "lua.h"
#include "lauxlib.h"
#include "host.h"
#include "menu_callbacks.h"

namespace {

typedef void(__fastcall* UserCallbackFn)(void* menu, const uint8_t* call);

// The Iggy call record (see the header comment).
const size_t kCallArgCount = 0x10, kCallSendType = 0x28, kCallValue = 0x48;
// OggyBase user data. SetUserDataStr (0x2c8240) stores type 6 and a
// wchar_t pointer, sets "set" and menu +0x54 bit 0 (user data changed).
// It is not called: xv2patcher hooks it and stops the game ("ResolveCode:
// Cannot map value") for slots it doesn't know - chara-select-search.md.
const size_t kUserDataBegin = 0x160, kUserDataEnd = 0x168, kUserDataEntrySize = 16;
const size_t kUserDataType = 4, kUserDataValue = 8;
const int kUserDataTypeWideString = 6;
const size_t kUserDataChangedFlags = 0x54;

const size_t kMaxTextBytes = 1024 * 1024;   // longest string message copied (CHARASELE's list: ~20 KB)
const size_t kMaxQueued = 256;              // messages kept between two menus.Messages calls
const int kHookedSlotMax = 8;               // one hook function per chained slot (Hook<0..7>)
const int kAnswerRing = 16;                 // answers kept alive (the game keeps the pointer)

// A vtable slot this library chained into.
struct HookedSlot {
    uintptr_t vtableRva = 0;
    int slot = 0;
    UserCallbackFn original = nullptr;
};

struct Message {
    int sendType = 0;
    bool isText = false;
    double number = 0;
    std::string text;
};

// A string to put in the menu's user data at its next taken message.
struct Answer {
    int index = 0;              // entry; negative = from the end (-1 = last)
    std::wstring text;
};

// One menus.HookCallback name. Never freed: a hook may be running in it
// on the game's thread while a script is reloaded.
struct Registration {
    std::string name;
    std::string owner;          // the script's LUA_NAME
    int hookedSlot = -1;        // index in g_slots
    bool active = false;        // false after menus.Unhook or an F1 reload: calls pass on
    std::map<int, bool> takes;  // sendType -> its value is a string
    std::deque<Message> queue;
    size_t dropped = 0;         // messages lost to a full queue
    std::map<int, ULONGLONG> lastSeen;  // sendType -> GetTickCount64 of its last call
    std::vector<Answer> pending;
    std::wstring written[kAnswerRing];
    int writtenNext = 0;
    std::string lastWriteProblem;
};

SRWLOCK g_lock = SRWLOCK_INIT;
HookedSlot g_slots[kHookedSlotMax];
int g_slotCount = 0;
std::vector<Registration*> g_registrations;

template <typename T> T& Field(void* object, size_t offset) { return *(T*)((uint8_t*)object + offset); }

// Copies a string the AS passed (at most capacity - 1 bytes); false when
// the pointer can't be read. No C++ objects here: __try needs that.
bool CopyAsString(const char* text, char* out, size_t capacity)
{
    __try {
        size_t length = strnlen(text, capacity - 1);
        memcpy(out, text, length);
        out[length] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::wstring Wide(const std::string& text)
{
    int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    std::wstring wide((size_t)(length > 0 ? length : 0), L'\0');
    if (length > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), &wide[0], length);
    return wide;
}

// Writes the registration's pending answers into `menu`'s user data, as
// SetUserDataStr would (call with g_lock held, on the game's thread).
void WriteAnswers(Registration& registration, void* menu)
{
    uint8_t* begin = Field<uint8_t*>(menu, kUserDataBegin);
    uint8_t* end = Field<uint8_t*>(menu, kUserDataEnd);
    const ptrdiff_t entries = begin && end > begin ? (end - begin) / (ptrdiff_t)kUserDataEntrySize : 0;
    for (Answer& answer : registration.pending) {
        const ptrdiff_t index = answer.index < 0 ? entries + answer.index : answer.index;
        if (index < 0 || index >= entries) {
            registration.lastWriteProblem = "user data entry " + std::to_string(answer.index) + " out of range (" +
                                            std::to_string(entries) + " entries)";
            continue;
        }
        std::wstring& kept = registration.written[registration.writtenNext];
        registration.writtenNext = (registration.writtenNext + 1) % kAnswerRing;
        kept = std::move(answer.text);
        uint8_t* entry = begin + index * kUserDataEntrySize;
        *(const wchar_t**)(entry + kUserDataValue) = kept.c_str();
        *(int*)(entry + kUserDataType) = kUserDataTypeWideString;
        *(int*)entry = 1;
        Field<uint32_t>(menu, kUserDataChangedFlags) |= 1;
    }
    registration.pending.clear();
}

// The hooked slot's work: true when a script took the call (not passed on).
bool HandleCall(int hookedSlot, void* menu, const uint8_t* call)
{
    if (!call || *(const int*)(call + kCallArgCount) < 2) return false;
    const int sendType = (int)*(const double*)(call + kCallSendType);
    AcquireSRWLockShared(&g_lock);
    bool wanted = false, isText = false;
    for (Registration* registration : g_registrations) {
        if (!registration->active || registration->hookedSlot != hookedSlot) continue;
        auto taken = registration->takes.find(sendType);
        if (taken == registration->takes.end()) continue;
        wanted = true;
        isText = taken->second;
        break;
    }
    ReleaseSRWLockShared(&g_lock);
    if (!wanted) return false;

    // read outside the lock (the AS string may be long)
    Message message;
    message.sendType = sendType;
    message.isText = isText;
    if (isText) {
        const char* text = *(const char* const*)(call + kCallValue);
        std::vector<char> copy(kMaxTextBytes);
        if (text && CopyAsString(text, copy.data(), copy.size())) message.text = copy.data();
    } else {
        message.number = *(const double*)(call + kCallValue);
    }

    AcquireSRWLockExclusive(&g_lock);
    bool taken = false;
    for (Registration* registration : g_registrations) {
        if (!registration->active || registration->hookedSlot != hookedSlot || !registration->takes.count(sendType))
            continue;
        taken = true;
        std::deque<Message>& queue = registration->queue;
        // a number sent every frame keeps only its latest value in a row
        if (!isText && !queue.empty() && !queue.back().isText && queue.back().sendType == sendType)
            queue.back().number = message.number;
        else
            queue.push_back(message);
        while (queue.size() > kMaxQueued) {
            queue.pop_front();
            ++registration->dropped;
        }
        registration->lastSeen[sendType] = GetTickCount64();
        if (!registration->pending.empty()) WriteAnswers(*registration, menu);
        break;
    }
    ReleaseSRWLockExclusive(&g_lock);
    return taken;
}

// One hook function per chained slot, so each knows its original.
template <int Index> void __fastcall Hook(void* menu, const uint8_t* call)
{
    if (!HandleCall(Index, menu, call)) g_slots[Index].original(menu, call);
}
const UserCallbackFn kHooks[kHookedSlotMax] = {Hook<0>, Hook<1>, Hook<2>, Hook<3>,
                                               Hook<4>, Hook<5>, Hook<6>, Hook<7>};

// The g_slots index chained into (vtableRva, slot), chaining it first when
// needed (under `mutex` when given: other mods that hook the same slot take
// it too). -1 with the reason when it can't. Call with g_lock held.
int EnsureHookedSlot(uintptr_t vtableRva, int slot, const std::string& mutex, std::string& reason)
{
    for (int index = 0; index < g_slotCount; ++index)
        if (g_slots[index].vtableRva == vtableRva && g_slots[index].slot == slot) return index;
    if (g_slotCount >= kHookedSlotMax) {
        reason = "too many hooked menu slots (" + std::to_string(kHookedSlotMax) + ")";
        return -1;
    }
    const uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    const uint8_t* image = (const uint8_t*)base;
    const uint32_t imageSize = *(const uint32_t*)(image + *(const uint32_t*)(image + 0x3C) + 24 + 56);  // SizeOfImage
    uintptr_t* address = (uintptr_t*)(base + vtableRva) + slot;
    if (slot < 0 || vtableRva + (slot + 1) * sizeof(uintptr_t) > imageSize) {
        reason = "vtable slot outside DBXV2.exe";
        return -1;
    }
    HANDLE lock = mutex.empty() ? nullptr : CreateMutexA(nullptr, FALSE, mutex.c_str());
    if (lock) WaitForSingleObject(lock, 10000);
    int index = -1;
    DWORD oldProtect;
    if (!*address) {
        reason = "the vtable slot is empty";
    } else if (VirtualProtect(address, sizeof(*address), PAGE_READWRITE, &oldProtect)) {
        index = g_slotCount++;
        g_slots[index].vtableRva = vtableRva;
        g_slots[index].slot = slot;
        g_slots[index].original = (UserCallbackFn)*address;
        *address = (uintptr_t)kHooks[index];
        VirtualProtect(address, sizeof(*address), oldProtect, &oldProtect);
        Log("menu callback: vtable 0x%llx slot %d chained (it held 0x%llx)", (unsigned long long)vtableRva, slot,
            (unsigned long long)((uintptr_t)g_slots[index].original - base));
    } else {
        reason = "could not patch the vtable";
    }
    if (lock) { ReleaseMutex(lock); CloseHandle(lock); }
    return index;
}

// The registration called `name`, or null (call with g_lock held).
Registration* Find(const std::string& name)
{
    for (Registration* registration : g_registrations)
        if (registration->name == name) return registration;
    return nullptr;
}

// The caller's active registration called `name` (argument 1), or null.
Registration* FindOwn(lua_State* L)
{
    Registration* registration = Find(luaL_checkstring(L, 1));
    return registration && registration->active && registration->owner == ScriptName(L) ? registration : nullptr;
}

// menus.HookCallback(name, vtable, slot, messages [, mutex]) -> true | false, reason
int MenusHookCallback(lua_State* L)
{
    const std::string name = luaL_checkstring(L, 1);
    const lua_Integer vtableRva = luaL_checkinteger(L, 2);
    const lua_Integer slot = luaL_checkinteger(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    const std::string mutex = luaL_optstring(L, 5, "");
    luaL_argcheck(L, vtableRva > 0 && vtableRva < 0x10000000, 2, "not a vtable address in DBXV2.exe");
    luaL_argcheck(L, slot >= 0 && slot < 4096, 3, "not a vtable slot");
    std::map<int, bool> takes;
    lua_pushnil(L);
    while (lua_next(L, 4)) {
        const char* kind = lua_tostring(L, -1);
        luaL_argcheck(L, lua_isinteger(L, -2) && kind && (!strcmp(kind, "number") || !strcmp(kind, "string")), 4,
                      "expected {[sendType] = \"number\" | \"string\"}");
        takes[(int)lua_tointeger(L, -2)] = !strcmp(kind, "string");
        lua_pop(L, 1);
    }
    const std::string owner = ScriptName(L);
    std::string reason;
    if (!IsGame12601()) reason = "needs DBXV2.exe 1.26.01";
    else if (takes.empty()) reason = "no sendTypes";
    if (!reason.empty()) {
        lua_pushboolean(L, false);
        lua_pushstring(L, reason.c_str());
        return 2;
    }
    AcquireSRWLockExclusive(&g_lock);
    Registration* registration = Find(name);
    if (registration && registration->active && registration->owner != owner) {
        reason = "already hooked by " + registration->owner;
    } else {
        const int hookedSlot = EnsureHookedSlot((uintptr_t)vtableRva, (int)slot, mutex, reason);
        if (hookedSlot >= 0) {
            if (!registration) {
                registration = new Registration;
                registration->name = name;
                g_registrations.push_back(registration);
            }
            registration->owner = owner;
            registration->hookedSlot = hookedSlot;
            registration->takes = takes;
            registration->queue.clear();
            registration->pending.clear();
            registration->lastSeen.clear();
            registration->active = true;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    lua_pushboolean(L, reason.empty());
    if (reason.empty()) return 1;
    lua_pushstring(L, reason.c_str());
    return 2;
}

// menus.Messages(name) -> {{type = sendType, value = number | string}, ...}
int MenusMessages(lua_State* L)
{
    std::deque<Message> messages;
    AcquireSRWLockExclusive(&g_lock);
    if (Registration* registration = FindOwn(L)) messages.swap(registration->queue);
    ReleaseSRWLockExclusive(&g_lock);
    lua_createtable(L, (int)messages.size(), 0);
    int position = 0;
    for (const Message& message : messages) {
        lua_createtable(L, 0, 2);
        lua_pushinteger(L, message.sendType);
        lua_setfield(L, -2, "type");
        if (message.isText) lua_pushlstring(L, message.text.data(), message.text.size());
        else lua_pushnumber(L, message.number);
        lua_setfield(L, -2, "value");
        lua_rawseti(L, -2, ++position);
    }
    return 1;
}

// menus.LastSeen(name, sendType) -> seconds | nil
int MenusLastSeen(lua_State* L)
{
    const int sendType = (int)luaL_checkinteger(L, 2);
    bool seen = false;
    ULONGLONG at = 0;
    AcquireSRWLockShared(&g_lock);
    if (Registration* registration = FindOwn(L)) {
        auto found = registration->lastSeen.find(sendType);
        if (found != registration->lastSeen.end()) { seen = true; at = found->second; }
    }
    ReleaseSRWLockShared(&g_lock);
    if (seen) lua_pushnumber(L, (double)(GetTickCount64() - at) / 1000.0);
    else lua_pushnil(L);
    return 1;
}

// menus.SetUserDataString(name, index, text) -> true | false
int MenusSetUserDataString(lua_State* L)
{
    const lua_Integer index = luaL_checkinteger(L, 2);
    size_t length = 0;
    const char* text = luaL_checklstring(L, 3, &length);
    luaL_argcheck(L, index >= -65536 && index < 65536, 2, "not a user data entry");
    Answer answer;
    answer.index = (int)index;
    answer.text = Wide(std::string(text, length));
    AcquireSRWLockExclusive(&g_lock);
    Registration* registration = FindOwn(L);
    if (registration) {
        // a newer answer for the same entry replaces one not written yet
        for (auto it = registration->pending.begin(); it != registration->pending.end();)
            it = it->index == answer.index ? registration->pending.erase(it) : it + 1;
        registration->pending.push_back(std::move(answer));
    }
    ReleaseSRWLockExclusive(&g_lock);
    lua_pushboolean(L, registration != nullptr);
    return 1;
}

// menus.Status(name) -> text | nil
int MenusStatus(lua_State* L)
{
    std::string status;
    AcquireSRWLockShared(&g_lock);
    if (Registration* registration = FindOwn(L)) {
        const HookedSlot& slot = g_slots[registration->hookedSlot];
        char line[160];
        snprintf(line, sizeof(line), "vtable 0x%llx slot %d, %zu message(s) waiting, %zu dropped",
                 (unsigned long long)slot.vtableRva, slot.slot, registration->queue.size(), registration->dropped);
        status = line;
        if (!registration->lastWriteProblem.empty()) status += "; " + registration->lastWriteProblem;
    }
    ReleaseSRWLockShared(&g_lock);
    if (status.empty()) lua_pushnil(L);
    else lua_pushstring(L, status.c_str());
    return 1;
}

// menus.Unhook(name) -> true | false
int MenusUnhook(lua_State* L)
{
    AcquireSRWLockExclusive(&g_lock);
    Registration* registration = FindOwn(L);
    if (registration) {
        registration->active = false;
        registration->queue.clear();
        registration->pending.clear();
    }
    ReleaseSRWLockExclusive(&g_lock);
    lua_pushboolean(L, registration != nullptr);
    return 1;
}

}  // namespace

void RegisterLuaMenus(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {"HookCallback",      MenusHookCallback},
        {"Messages",          MenusMessages},
        {"LastSeen",          MenusLastSeen},
        {"SetUserDataString", MenusSetUserDataString},
        {"Status",            MenusStatus},
        {"Unhook",            MenusUnhook},
        {nullptr, nullptr},
    };
    luaL_newlib(L, functions);
    lua_setglobal(L, "menus");
}

void ClearScriptMenuCallbacks()
{
    AcquireSRWLockExclusive(&g_lock);
    for (Registration* registration : g_registrations) {
        registration->active = false;
        registration->queue.clear();
        registration->pending.clear();
    }
    ReleaseSRWLockExclusive(&g_lock);
}
