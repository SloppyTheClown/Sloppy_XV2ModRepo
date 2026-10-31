// ---------------------------------------------------------------------------
//  Training_QoL.dll - loaded by XV2 Mod Loader from
//  <game>\XV2ModLoader\mods\Training_QoL\
//
//  In Training, a button (D-pad up by default) hands your controller and
//  the camera to the Training partner, and back on the next press. The
//  partner is controlled as if you had picked it; your own character stands
//  still meanwhile. Stats, teams and the partner's settings are untouched.
//  Another button (D-pad down) turns on the game's photo camera (the
//  online battle photo mode's free camera) and holds both characters still;
//  the next press goes back to the battle camera. Offline Training only.
//
//  How (DBXV2.exe 1.27 RVAs; Research_Notes\training-qol.md):
//  * MenuTraining's per-frame update 0x192db0 calls the "apply CPU Status"
//    function 0x192ec0(training, deltaTime) at 0x192ea5. It forces the
//    partner back to CPU every frame, so a switch would not hold. That call
//    (and the two other calls of 0x192ec0, at 0x191fc3 on a settings change
//    and 0x192914 at menu setup) are redirected to this DLL; the per-frame
//    one is where the button is read and the switch happens, on the game's
//    own thread.
//  * The switch itself is what the game's Body Change (Ginyu, 0x158de0) does,
//    without its stat/team swap: swap the two slots' input bindings, set the
//    local player override to the partner's slot, point the camera at the
//    partner - plus set the partner to human control (0xd9930) and skip the
//    CPU Status apply while switched.
//  * The photo camera is what Battle::PhotoArtEvent (online only) does on
//    entry/exit (0x18e4e1 / 0x18f502): switch the camera manager to camera 9
//    (Battle::CameraPhoto), turn off both characters' pad input (0xd9990)
//    and turn on the photo camera's own pad input (0x79f430). The HUD is
//    hidden with the event's own flag (mgr+0xd528); the HUD draws 0x1637f0
//    and 0x163af0 only check it online, so that check is patched out.
//  * Both buttons are ignored while the battle is paused (the pause flag
//    0x156180, or the pause popup open), where the D-pad moves the menu's
//    cursor; pausing turns the photo camera off.
//
//  Reads training_qol.json next to the DLL; writes Training_QoL.log only
//  when "logging" is true. No network calls.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <Xinput.h>
#include "default_config.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

// ---- DBXV2.exe 1.27 RVAs ------------------------------------------------
// Battle manager singleton; +0xbc = 1 in Training, +0xc0 = online flag
// (0x156190 returns it).
constexpr uintptr_t kBattleManagerRva = 0x15681c0;
constexpr uintptr_t kCameraManagerRva = 0x157f168;       // [cam+8] director, +0x90 followed Mob
constexpr uintptr_t kTrainingApplyRva = 0x192ec0;         // apply CPU Status (training, float deltaTime)
constexpr uintptr_t kSetControlModeRva = 0xd9930;         // (mob, mode): 0 human, 1 CPU
constexpr uintptr_t kSetCameraTargetRva = 0xb8ca0;        // (cameraManager, mob); Body Change uses it at 0x15971c
constexpr uintptr_t kLocalPlayerMobRva = 0x14d100;        // (battleManager) -> Mob of the local player
// (battleManager) -> nonzero while the battle is paused/blocked (menu,
// manager busy flags); MenuTraining's HUD update 0x193330 skips itself on it.
// 1.0.1 guarded the toggle with it; it may also cover ultimates, so it is
// only logged now.
constexpr uintptr_t kBattleBlockedRva = 0x155ec0;
// (battleManager) -> mgr+0x24 bit 0, the pause flag: the pause controller
// reads it (0x16eca1, 0x16f011) and PhotoArtEvent enters photo only while
// it is set (0x18e4c6).
constexpr uintptr_t kBattlePausedRva = 0x156180;
// Photo camera (Battle::PhotoArtEvent's entry 0x18e4e1 / exit 0x18f502).
constexpr uintptr_t kCameraAtRva = 0xb9ec0;              // (cameraManager, index) -> camera
constexpr uintptr_t kSwitchCameraRva = 0xb8c70;          // (cameraManager, index <= 9, int *param = 0)
constexpr uintptr_t kSetCharacterInputRva = 0xd9990;     // (mob, enable): 0 sets Mob +0x78 bit 0x10, clears Command input
constexpr uintptr_t kSetCameraInputOffRva = 0x79f430;    // (camera, off): byte camera +0x654
constexpr int kBattleCameraIndex = 0;
constexpr int kPhotoCameraIndex = 9;                     // Battle::CameraPhoto (update 0x79e390)
// Camera vfunc the game checks on camera 0 before either switch (0x18e506,
// 0x18f523): nonzero = the battle camera can be switched now.
constexpr size_t kCameraCanSwitchSlot = 0xc8;
// The two battle HUD draw callbacks (0x1637f0, 0x163af0) skip drawing when
// mgr+0xd528 is set - but only online: "cmp [rcx+0xc0], 0; je draw" comes
// first. The je (74 0d) is turned into two nops so the flag works offline;
// only PhotoArtEvent (online, 0x157d20) and this DLL ever set it.
constexpr uintptr_t kHudOnlineCheckJumps[] = { 0x16381e, 0x163b1b };
constexpr unsigned char kHudOnlineCheckBytes[2] = { 0x74, 0x0D };
// (table, 0, index) -> the CPU Status entry (24 bytes: +0 status, +4 level)
// that 0x192ec0 applies, index = MenuTraining +0xac, table = +0xc8.
constexpr uintptr_t kCpuStatusEntryRva = 0x196940;

// The three calls of 0x192ec0: "e8 rel32" each.
struct CallSite { uintptr_t rva; unsigned char bytes[5]; bool perFrame; };
constexpr CallSite kApplyCalls[] = {
    { 0x192ea5, { 0xE8, 0x16, 0x00, 0x00, 0x00 }, true },    // MenuTraining update 0x192db0, every frame
    { 0x191fc3, { 0xE8, 0xF8, 0x0E, 0x00, 0x00 }, false },   // vtable slot 9 0x191f00, a setting changed
    { 0x192914, { 0xE8, 0xA7, 0x05, 0x00, 0x00 }, false },   // 0x192830, menu setup
};

// Battle manager fields.
constexpr size_t kTrainingFlag = 0xbc;        // 1 in Training
constexpr size_t kOnlineFlag = 0xc0;          // nonzero online
constexpr size_t kPauseController = 0x4a90;   // made in 0x155570 (ctor 0x16e970)
constexpr size_t kPausePopup = 0x68;          // pause controller: PopupPause* while open (0x170760 / 0x16fcd0)
constexpr size_t kHudHidden = 0xd528;         // HUD not drawn while nonzero (setter 0x157d20, zeroed in the ctor 0x151c08)
constexpr size_t kLocalOverride = 0x1c00;     // local player slot override, -1 = none (Body Change 0x1591b1)
constexpr size_t kMobs = 0x3a58;              // Mob* per slot (14 slots)
constexpr size_t kBindings = 0x3ad0;          // input binding* per slot (Body Change 0x15964e)
constexpr int kSlotCount = 14;
// Mob fields.
constexpr size_t kMobSlot = 0x44;             // slot index
constexpr size_t kMobControlMode = 0x50;      // 0 human, 1 CPU (setter 0xd9930)
constexpr size_t kMobCommand = 0x4e8;         // Battle::Command*
constexpr size_t kCommandBinding = 0xd48;     // the Mob's own copy of its binding* (Body Change 0x159639)
// MenuTraining fields (read by 0x192ec0).
constexpr size_t kTrainingPartner = 0x28;     // the partner's Mob
constexpr size_t kTrainingCpuIndex = 0xac;    // CPU Status entry chosen in the menu (set by slot 9 0x191f00)
constexpr size_t kTrainingCpuTable = 0xc8;    // CPU Status table

using TrainingApplyFn = void (*)(void *training, float deltaTime);
using SetControlModeFn = void (*)(void *mob, int mode);
using SetCameraTargetFn = void (*)(void *cameraManager, void *mob);
using LocalPlayerMobFn = void *(*)(void *battleManager);
using BattleBlockedFn = int (*)(void *battleManager);
using CpuStatusEntryFn = const int *(*)(void *table, int tableIndex, int index);
using BattlePausedFn = int (*)(void *battleManager);
using CameraAtFn = void *(*)(void *cameraManager, int index);
using SwitchCameraFn = void (*)(void *cameraManager, int index, int *param);
using SetCharacterInputFn = void (*)(void *mob, int enable);
using SetCameraInputOffFn = void (*)(void *camera, int off);
using CameraCanSwitchFn = int (*)(void *camera);

// A controller button and/or keyboard key, and whether it was down last frame.
struct Trigger {
    WORD button = 0;    // XInput button bit; 0 = none
    int key = 0;        // virtual key; 0 = none
    bool held = false;
};

uintptr_t g_exe = 0;
std::wstring g_modDir;
bool g_logging = false;
Trigger g_switchTrigger = { XINPUT_GAMEPAD_DPAD_UP };    // "toggle_button" / "toggle_key"
Trigger g_photoTrigger = { XINPUT_GAMEPAD_DPAD_DOWN };   // "photo_button" / "photo_key"
bool g_photoHidesHud = true;    // "photo_hide_hud"
bool g_hudCheckPatched = false; // the online-only HUD checks are patched (Start)

void Log(const char *message)
{
    if (!g_logging) return;
    FILE *file = nullptr;
    // One session per log: the first write of a run moves the last run's log to <name>.prev.log.
    static volatile LONG rotated = 0;
    std::wstring path = g_modDir + L"Training_QoL.log";
    if (!InterlockedExchange(&rotated, 1)) MoveFileExW(path.c_str(), (g_modDir + L"Training_QoL.prev.log").c_str(), MOVEFILE_REPLACE_EXISTING);
    if (_wfopen_s(&file, path.c_str(), L"a, ccs=UTF-8") != 0 || !file) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    fwprintf(file, L"%02d:%02d:%02d %hs\n", now.wHour, now.wMinute, now.wSecond, message);
    fclose(file);
}

template <typename T> T &Field(void *object, size_t offset)
{
    return *reinterpret_cast<T *>(static_cast<char *>(object) + offset);
}

// The CPU Status entry 0x192ec0 applies to the partner, or null.
const int *CpuStatusEntry(void *training)
{
    void *table = Field<void *>(training, kTrainingCpuTable);
    int index = Field<int>(training, kTrainingCpuIndex);
    // 0x196940 checks the upper bound only (signed).
    if (!table || index < 0) return nullptr;
    return reinterpret_cast<CpuStatusEntryFn>(g_exe + kCpuStatusEntryRva)(table, 0, index);
}

// Log line with the partner's control mode and CPU Status entry, so a
// report can be matched with what the game applied.
void LogSwitchState(const char *event, void *training, void *partner)
{
    if (!g_logging) return;
    const int *entry = CpuStatusEntry(training);
    char line[200];
    snprintf(line, sizeof(line), "%s (partner mode %d, CPU Status entry %d: status %d level %d)", event,
             partner ? Field<int>(partner, kMobControlMode) : -1, Field<int>(training, kTrainingCpuIndex),
             entry ? entry[0] : -1, entry ? entry[1] : -1);
    Log(line);
}

// ---- Config ----------------------------------------------------------------

// The string value of "<key>" in the config text ("" when missing).
std::string ReadString(const char *text, const char *key)
{
    const char *found = strstr(text, key);
    if (!found) return "";
    const char *open = strchr(found + strlen(key), '"');
    const char *close = open ? strchr(open + 1, '"') : nullptr;
    return close ? std::string(open + 1, close) : "";
}

bool ReadBool(const char *text, const char *key)
{
    const char *found = strstr(text, key);
    if (!found) return false;
    const char *value = strchr(found, ':');
    return value && strncmp(value + 1 + strspn(value + 1, " \t\r\n"), "true", 4) == 0;
}

// "dpad_down" etc. -> XInput button bit; 0 for "" or an unknown name.
WORD ButtonFromName(const std::string &name)
{
    static const struct { const char *name; WORD bit; } kButtons[] = {
        { "dpad_up", XINPUT_GAMEPAD_DPAD_UP }, { "dpad_down", XINPUT_GAMEPAD_DPAD_DOWN },
        { "dpad_left", XINPUT_GAMEPAD_DPAD_LEFT }, { "dpad_right", XINPUT_GAMEPAD_DPAD_RIGHT },
        { "start", XINPUT_GAMEPAD_START }, { "back", XINPUT_GAMEPAD_BACK },
        { "l3", XINPUT_GAMEPAD_LEFT_THUMB }, { "r3", XINPUT_GAMEPAD_RIGHT_THUMB },
        { "lb", XINPUT_GAMEPAD_LEFT_SHOULDER }, { "rb", XINPUT_GAMEPAD_RIGHT_SHOULDER },
        { "a", XINPUT_GAMEPAD_A }, { "b", XINPUT_GAMEPAD_B }, { "x", XINPUT_GAMEPAD_X }, { "y", XINPUT_GAMEPAD_Y },
    };
    for (const auto &button : kButtons)
        if (_stricmp(name.c_str(), button.name) == 0) return button.bit;
    return 0;
}

// "F5", "G", "7" -> virtual key; 0 for "" or anything else.
int KeyFromName(const std::string &name)
{
    if (name.size() == 1 && isalnum(static_cast<unsigned char>(name[0])))
        return toupper(static_cast<unsigned char>(name[0]));
    if (name.size() >= 2 && (name[0] == 'F' || name[0] == 'f')) {
        int number = atoi(name.c_str() + 1);
        if (number >= 1 && number <= 12) return VK_F1 + number - 1;
    }
    return 0;
}

// Reads the config; a missing one is written from the default compiled in.
void ReadConfig()
{
    std::wstring path = g_modDir + L"training_qol.json";
    const char *text = kDefaultConfig;
    char fileText[4096] = {};
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") == 0 && file) {
        fread(fileText, 1, sizeof(fileText) - 1, file);
        fclose(file);
        text = fileText;
    } else if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file) {
        fputs(kDefaultConfig, file);
        fclose(file);
    }
    g_logging = ReadBool(text, "\"logging\"");
    g_switchTrigger.button = ButtonFromName(ReadString(text, "\"toggle_button\""));
    g_switchTrigger.key = KeyFromName(ReadString(text, "\"toggle_key\""));
    // A config from 1.0.x has no photo keys: keep the default button then,
    // unless it is the one the switch uses.
    if (strstr(text, "\"photo_button\"")) g_photoTrigger.button = ButtonFromName(ReadString(text, "\"photo_button\""));
    else if (g_photoTrigger.button == g_switchTrigger.button) g_photoTrigger.button = 0;
    g_photoTrigger.key = KeyFromName(ReadString(text, "\"photo_key\""));
    // Missing in a 1.1.0 test config: on.
    g_photoHidesHud = !strstr(text, "\"photo_hide_hud\"") || ReadBool(text, "\"photo_hide_hud\"");
}

// ---- Input -----------------------------------------------------------------
// Read once per frame from the training update. XInputGetState on an empty
// port is slow, so ports that are not connected are only re-checked once a
// second (60 frames).

using XInputGetStateFn = DWORD (WINAPI *)(DWORD, XINPUT_STATE *);
XInputGetStateFn g_xinputGetState = nullptr;
bool g_padConnected[XUSER_MAX_COUNT] = {};
int g_framesUntilPadScan = 0;

void LoadXInput()
{
    // The game imports xinput1_3; fall back to the system's newer copies.
    for (const wchar_t *name : { L"xinput1_3.dll", L"xinput1_4.dll", L"xinput9_1_0.dll" }) {
        HMODULE module = GetModuleHandleW(name);
        if (!module) module = LoadLibraryW(name);
        if (module && (g_xinputGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(module, "XInputGetState"))))
            return;
    }
    Log("XInput not found - only the keyboard key works");
}

bool GameHasFocus()
{
    DWORD processId = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &processId);
    return processId == GetCurrentProcessId();
}

// The buttons held on any connected controller.
WORD PadButtonsHeld()
{
    if (!g_xinputGetState || !(g_switchTrigger.button | g_photoTrigger.button)) return 0;
    WORD buttons = 0;
    bool scan = --g_framesUntilPadScan <= 0;
    if (scan) g_framesUntilPadScan = 60;
    for (DWORD pad = 0; pad < XUSER_MAX_COUNT; pad++) {
        if (!g_padConnected[pad] && !scan) continue;
        XINPUT_STATE state = {};
        g_padConnected[pad] = g_xinputGetState(pad, &state) == ERROR_SUCCESS;
        if (g_padConnected[pad]) buttons |= state.Gamepad.wButtons;
    }
    return buttons;
}

// True on the frame the trigger's button or key goes down.
bool Pressed(Trigger &trigger, WORD padButtons)
{
    bool held = (trigger.button && (padButtons & trigger.button))
             || (trigger.key && (GetAsyncKeyState(trigger.key) & 0x8000) && GameHasFocus());
    bool pressed = held && !trigger.held;
    trigger.held = held;
    return pressed;
}

// ---- The switch ------------------------------------------------------------

struct Switch {
    bool active = false;
    void *training = nullptr;       // the MenuTraining it was made in
    void *manager = nullptr;
    void *playerMob = nullptr;      // the character picked
    void *partnerMob = nullptr;
    int playerSlot = -1, partnerSlot = -1;
    void *playerBinding = nullptr;  // the bindings before the swap
    void *partnerBinding = nullptr;
    int savedOverride = -1;         // local override before the switch
    int savedPartnerMode = 1;       // partner's control mode before the switch
};
Switch g_switch;

void *BattleManager() { return *reinterpret_cast<void **>(g_exe + kBattleManagerRva); }
void *MobInSlot(void *manager, int slot) { return Field<void *>(manager, kMobs + slot * 8); }

void SetControlMode(void *mob, int mode) { reinterpret_cast<SetControlModeFn>(g_exe + kSetControlModeRva)(mob, mode); }

void PointCameraAt(void *mob)
{
    void *cameraManager = *reinterpret_cast<void **>(g_exe + kCameraManagerRva);
    if (cameraManager) reinterpret_cast<SetCameraTargetFn>(g_exe + kSetCameraTargetRva)(cameraManager, mob);
}

// Puts each binding pointer where the game keeps it: the manager's per-slot
// array and the Mob's own Battle::Command copy.
void AssignBinding(void *manager, int slot, void *mob, void *binding)
{
    Field<void *>(manager, kBindings + slot * 8) = binding;
    if (void *command = Field<void *>(mob, kMobCommand)) Field<void *>(command, kCommandBinding) = binding;
}

bool SlotHoldsMob(void *manager, int slot, void *mob)
{
    return slot >= 0 && slot < kSlotCount && MobInSlot(manager, slot) == mob;
}

// Hands control and camera to the partner. False when the battle is not
// offline Training or the characters are not where expected.
bool SwitchToPartner(void *training)
{
    void *manager = BattleManager();
    if (!manager || Field<int>(manager, kTrainingFlag) != 1 || Field<int>(manager, kOnlineFlag) != 0) return false;
    void *partner = Field<void *>(training, kTrainingPartner);
    void *player = reinterpret_cast<LocalPlayerMobFn>(g_exe + kLocalPlayerMobRva)(manager);
    if (!partner || !player || partner == player) return false;
    int playerSlot = Field<int>(player, kMobSlot), partnerSlot = Field<int>(partner, kMobSlot);
    if (!SlotHoldsMob(manager, playerSlot, player) || !SlotHoldsMob(manager, partnerSlot, partner)) return false;
    void *playerBinding = Field<void *>(manager, kBindings + playerSlot * 8);
    void *partnerBinding = Field<void *>(manager, kBindings + partnerSlot * 8);
    if (!playerBinding || !partnerBinding) return false;

    LogSwitchState("switching to the partner", training, partner);
    g_switch = { true, training, manager, player, partner, playerSlot, partnerSlot,
                 playerBinding, partnerBinding, Field<int>(manager, kLocalOverride),
                 Field<int>(partner, kMobControlMode) };
    AssignBinding(manager, playerSlot, player, partnerBinding);
    AssignBinding(manager, partnerSlot, partner, playerBinding);
    Field<int>(manager, kLocalOverride) = partnerSlot;
    SetControlMode(partner, 0);
    PointCameraAt(partner);
    return true;
}

// Still the same battle and characters the switch was made in?
bool SwitchStillValid(void *training)
{
    void *manager = BattleManager();
    return training == g_switch.training && manager == g_switch.manager
        && SlotHoldsMob(manager, g_switch.playerSlot, g_switch.playerMob)
        && SlotHoldsMob(manager, g_switch.partnerSlot, g_switch.partnerMob)
        && Field<void *>(manager, kBindings + g_switch.playerSlot * 8) == g_switch.partnerBinding;
}

// Gives control and camera back to the character picked and puts the
// partner's control mode back as it was, so the CPU Status apply the caller
// runs next starts from the game's own state: a "CPU AI" entry (status 3)
// sets the mode only when its level changes (cached at MenuTraining +0xbc),
// so without this the partner stayed in human mode after a switch back.
void SwitchBack(const char *reason)
{
    if (!g_switch.active) return;
    AssignBinding(g_switch.manager, g_switch.playerSlot, g_switch.playerMob, g_switch.playerBinding);
    AssignBinding(g_switch.manager, g_switch.partnerSlot, g_switch.partnerMob, g_switch.partnerBinding);
    Field<int>(g_switch.manager, kLocalOverride) = g_switch.savedOverride;
    SetControlMode(g_switch.partnerMob, g_switch.savedPartnerMode);
    PointCameraAt(g_switch.playerMob);
    g_switch.active = false;
    LogSwitchState(reason, g_switch.training, g_switch.partnerMob);
}

TrainingApplyFn OriginalApply() { return reinterpret_cast<TrainingApplyFn>(g_exe + kTrainingApplyRva); }

// True while the battle is paused (pause flag, or the pause popup open - the
// Training menu included). Logs each change, and each change of the broader
// 0x155ec0 check beside it, to see in game what each covers (1.0.1 used
// 0x155ec0, which may also hold during ultimates).
bool BattlePaused()
{
    static bool wasPaused = false, wasBlocked = false;
    void *manager = BattleManager();
    bool paused = false, blocked = false;
    if (manager) {
        void *pauseController = Field<void *>(manager, kPauseController);
        paused = reinterpret_cast<BattlePausedFn>(g_exe + kBattlePausedRva)(manager) != 0
              || (pauseController && Field<void *>(pauseController, kPausePopup));
        blocked = reinterpret_cast<BattleBlockedFn>(g_exe + kBattleBlockedRva)(manager) != 0;
    }
    if (paused != wasPaused) Log(paused ? "battle paused - buttons off" : "battle resumed - buttons on");
    if (blocked != wasBlocked) Log(blocked ? "battle busy (0x155ec0)" : "battle no longer busy (0x155ec0)");
    wasPaused = paused;
    wasBlocked = blocked;
    return paused;
}

// ---- The photo camera --------------------------------------------------------

struct Photo {
    bool active = false;
    void *training = nullptr;      // the MenuTraining it was turned on in
    void *manager = nullptr;
    void *mobs[2] = {};            // the two characters held still
    int slots[2] = { -1, -1 };
};
Photo g_photo;

void *CameraManager() { return *reinterpret_cast<void **>(g_exe + kCameraManagerRva); }

void *CameraAt(void *cameraManager, int index)
{
    return reinterpret_cast<CameraAtFn>(g_exe + kCameraAtRva)(cameraManager, index);
}

// The check the game makes on camera 0 before switching to or from the
// photo camera.
bool CameraCanSwitch(void *cameraManager)
{
    void *battleCamera = CameraAt(cameraManager, kBattleCameraIndex);
    if (!battleCamera) return false;
    void **vtable = *reinterpret_cast<void ***>(battleCamera);
    return reinterpret_cast<CameraCanSwitchFn>(vtable[kCameraCanSwitchSlot / 8])(battleCamera) != 0;
}

void SwitchCamera(void *cameraManager, int index)
{
    int param = 0;   // the game passes a zeroed int (0x18e525)
    reinterpret_cast<SwitchCameraFn>(g_exe + kSwitchCameraRva)(cameraManager, index, &param);
}

void SetCharacterInput(void *mob, bool enable)
{
    reinterpret_cast<SetCharacterInputFn>(g_exe + kSetCharacterInputRva)(mob, enable ? 1 : 0);
}

void SetPhotoCameraInput(void *cameraManager, bool enable)
{
    if (void *photoCamera = CameraAt(cameraManager, kPhotoCameraIndex))
        reinterpret_cast<SetCameraInputOffFn>(g_exe + kSetCameraInputOffRva)(photoCamera, enable ? 0 : 1);
}

void SetHudHidden(void *manager, bool hidden)
{
    if (g_hudCheckPatched && (g_photoHidesHud || !hidden)) Field<int>(manager, kHudHidden) = hidden ? 1 : 0;
}

// Turns the photo camera on and holds both characters still. False when not
// in offline Training or the battle camera can't be switched right now.
bool EnterPhoto(void *training)
{
    void *manager = BattleManager();
    void *cameraManager = CameraManager();
    if (!manager || !cameraManager || Field<int>(manager, kTrainingFlag) != 1 || Field<int>(manager, kOnlineFlag) != 0)
        return false;
    // The two Training characters (the switch keeps its own copy of both).
    void *player = g_switch.active ? g_switch.playerMob : reinterpret_cast<LocalPlayerMobFn>(g_exe + kLocalPlayerMobRva)(manager);
    void *partner = g_switch.active ? g_switch.partnerMob : Field<void *>(training, kTrainingPartner);
    if (!player || !partner || !CameraAt(cameraManager, kPhotoCameraIndex)) return false;
    if (!CameraCanSwitch(cameraManager)) { Log("photo camera: the battle camera is busy - not switched"); return false; }

    g_photo = { true, training, manager, { player, partner }, { Field<int>(player, kMobSlot), Field<int>(partner, kMobSlot) } };
    SwitchCamera(cameraManager, kPhotoCameraIndex);
    SetCharacterInput(player, false);
    SetCharacterInput(partner, false);
    SetPhotoCameraInput(cameraManager, true);
    SetHudHidden(manager, true);
    Log("photo camera on");
    return true;
}

bool PhotoStillValid(void *training)
{
    void *manager = BattleManager();
    return training == g_photo.training && manager == g_photo.manager && CameraManager()
        && SlotHoldsMob(manager, g_photo.slots[0], g_photo.mobs[0])
        && SlotHoldsMob(manager, g_photo.slots[1], g_photo.mobs[1]);
}

// Back to the battle camera and the characters' own input.
void LeavePhoto(const char *reason)
{
    if (!g_photo.active) return;
    void *cameraManager = CameraManager();
    if (CameraCanSwitch(cameraManager)) SwitchCamera(cameraManager, kBattleCameraIndex);
    else Log("photo camera: the battle camera is busy - camera left as it is");
    SetCharacterInput(g_photo.mobs[0], true);
    SetCharacterInput(g_photo.mobs[1], true);
    SetPhotoCameraInput(cameraManager, false);
    SetHudHidden(g_photo.manager, false);
    g_photo.active = false;
    Log(reason);
}

// Replaces the per-frame call at 0x192ea5. The game runs it while the menu
// is open too, so the buttons are ignored while paused (the D-pad moves the
// menu cursor there).
void PerFrameApply(void *training, float deltaTime)
{
    if (g_switch.active && !SwitchStillValid(training)) {
        // The battle or the menu changed under the switch (e.g. a restart):
        // the game rebuilt its own state, so nothing to restore.
        g_switch.active = false;
        Log("battle changed while switched - switch dropped");
    }
    if (g_photo.active && !PhotoStillValid(training)) {
        // A new battle manager starts with the HUD shown; the same one gets
        // it back.
        if (BattleManager() == g_photo.manager) SetHudHidden(g_photo.manager, false);
        g_photo.active = false;
        Log("battle changed with the photo camera on - dropped");
    }
    // Read the buttons every frame so a press held over the end of a pause
    // does not count as a new one.
    WORD padButtons = PadButtonsHeld();
    bool switchPressed = Pressed(g_switchTrigger, padButtons);
    bool photoPressed = Pressed(g_photoTrigger, padButtons);
    bool paused = BattlePaused();
    if (paused && g_photo.active) LeavePhoto("battle paused - photo camera off");
    if (!paused && photoPressed) {
        if (g_photo.active) LeavePhoto("photo camera off");
        else EnterPhoto(training);
    } else if (!paused && switchPressed && !g_photo.active) {
        if (g_switch.active) SwitchBack("switched back");
        else if (SwitchToPartner(training)) return;
    }
    if (!g_switch.active) { OriginalApply()(training, deltaTime); return; }
    // Switched: skip the apply, and keep the partner human if something
    // else changed it.
    if (Field<int>(g_switch.partnerMob, kMobControlMode) != 0) SetControlMode(g_switch.partnerMob, 0);
}

// Replaces the other two calls (a CPU Status entry picked in the menu, menu
// setup): switch back first so the setting applies to the partner as usual.
void OtherApply(void *training, float deltaTime)
{
    bool wasSwitched = g_switch.active;
    if (g_switch.active && SwitchStillValid(training)) SwitchBack("CPU Status changed - switched back");
    else if (g_switch.active) Log("CPU Status changed in another battle - switch dropped");
    g_switch.active = false;
    OriginalApply()(training, deltaTime);
    if (wasSwitched) LogSwitchState("CPU Status applied", training, Field<void *>(training, kTrainingPartner));
}

// ---- Hooking ---------------------------------------------------------------

// A "mov rax, target; jmp rax" stub within rel32 reach of the exe, so a
// 5-byte call can be redirected to this DLL.
void *MakeJumpStub(uintptr_t nearAddress, void *target)
{
    static unsigned char *page = nullptr;
    static size_t used = 0;
    if (!page) {
        // Look for free memory below the exe in 64 KB steps (bounded: 1 GB).
        for (uintptr_t address = (nearAddress & ~uintptr_t(0xffff)) - 0x10000; address > nearAddress - 0x40000000; address -= 0x10000) {
            page = static_cast<unsigned char *>(VirtualAlloc(reinterpret_cast<void *>(address), 0x1000,
                                                             MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
            if (page) break;
        }
        if (!page) return nullptr;
    }
    unsigned char *stub = page + used;
    stub[0] = 0x48; stub[1] = 0xB8;                       // mov rax, imm64
    memcpy(stub + 2, &target, 8);
    stub[10] = 0xFF; stub[11] = 0xE0;                     // jmp rax
    used += 16;
    return stub;
}

void RedirectCall(unsigned char *site, void *stub)
{
    int32_t relative = static_cast<int32_t>(reinterpret_cast<intptr_t>(stub) - reinterpret_cast<intptr_t>(site + 5));
    DWORD oldProtect = 0;
    VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(site + 1, &relative, 4);
    VirtualProtect(site, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
}

// Lets the HUD-hidden flag work offline (kHudOnlineCheckJumps); both jumps
// must match or neither is touched.
void PatchHudCheck()
{
    for (uintptr_t rva : kHudOnlineCheckJumps)
        if (memcmp(reinterpret_cast<void *>(g_exe + rva), kHudOnlineCheckBytes, 2) != 0) {
            Log("the HUD draw code doesn't match game 1.27 - the photo camera keeps the HUD");
            return;
        }
    for (uintptr_t rva : kHudOnlineCheckJumps) {
        unsigned char *jump = reinterpret_cast<unsigned char *>(g_exe + rva);
        DWORD oldProtect = 0;
        VirtualProtect(jump, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
        jump[0] = 0x90; jump[1] = 0x90;
        VirtualProtect(jump, 2, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), jump, 2);
    }
    g_hudCheckPatched = true;
    Log("patched the HUD draw checks (photo camera hides the HUD)");
}

// Steam DRM decrypts .text some time after start, so wait (bounded: 2
// minutes) until all three calls read as expected, then redirect them.
DWORD WINAPI Start(LPVOID)
{
    ReadConfig();
    Log("---- Training QoL v" MOD_VERSION " starting");
    if (!g_switchTrigger.button && !g_switchTrigger.key && !g_photoTrigger.button && !g_photoTrigger.key) {
        Log("no toggle or photo button/key set - nothing to do");
        return 0;
    }
    g_exe = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    for (int attempt = 0; attempt < 1200; attempt++, Sleep(100)) {
        bool ready = true;
        for (const auto &call : kApplyCalls)
            if (memcmp(reinterpret_cast<void *>(g_exe + call.rva), call.bytes, 5) != 0) ready = false;
        if (!ready) continue;
        LoadXInput();
        void *perFrameStub = MakeJumpStub(g_exe, reinterpret_cast<void *>(&PerFrameApply));
        void *otherStub = MakeJumpStub(g_exe, reinterpret_cast<void *>(&OtherApply));
        if (!perFrameStub || !otherStub) { Log("no memory near the exe for the hook stubs - nothing hooked"); return 0; }
        for (const auto &call : kApplyCalls)
            RedirectCall(reinterpret_cast<unsigned char *>(g_exe + call.rva), call.perFrame ? perFrameStub : otherStub);
        Log("hooked the Training CPU Status apply (0x192ec0 calls)");
        PatchHudCheck();
        return 0;
    }
    Log("the Training update code never matched game 1.27 - nothing hooked");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        wchar_t path[MAX_PATH];
        DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        g_modDir.assign(path, length);
        g_modDir.resize(g_modDir.find_last_of(L"\\/") + 1);
        // Off the loader lock: the wait for the DRM must not block DllMain.
        HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
