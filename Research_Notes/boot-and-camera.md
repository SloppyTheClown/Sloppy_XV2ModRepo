# Boot sequence and the camera

Covers: logos, intro movie and title (what General_QoL skips), the
renderer's camera, which game code sets it per mode, and everything that
reads the camera back (culling, markers) - found for General_QoL's camera
zoom. Game 1.26.01; xgcore.dll / render_pass_x64.dll named where relevant.
Found statically in a decrypted image (`ClaudeTools\exe\`).

## Boot
* Order: logos (Menu::Advertise) -> CMNWIN notice -> Menu::WaitInstall
  (intro demo A00_01) -> TITLE_us. Option `log_files` logs every loaded path.
* Logos: Menu::Advertise (ADVERTISE_us.emb, 8 slides 4/3/2x6 s, table
  0x14a79f8, [obj+0x18] slide index, 999 = done, IsDone 0x4dcb20), driven by
  Menu::Logo step 0x57e620; `test eax,eax; je 0x57e690` at 0x57e63d = the
  wait. skip_logos NOPs that je.
* Intro: WaitInstall (vtable 0x10cae30; Event::WaitInstall 0xfcd808 wraps it:
  slot 1 Start 0x6284e0, slot 3 IsDone = [obj+0x60], slot 4 Stop 0x628180)
  loads and plays demo "A00_01/A00_01" (Revamp's rvp.cpk replaces A00_01.dem
  + movie/REVAMPINTRO.usm). States 0x628480 -> 0x628400 (load) -> 0x628230
  (play; skip on finished 0x2b0e90 or a button) -> 0x628210 -> Stop. Only Stop
  sets +0x60. skip_intro: Start `jmp` Stop (bytes same in plain and
  Revamp-patched images). movie/PV_%02d.usm (0x2fb610) are other movies.
* Title music / volumes: sound.md.

## The renderer's camera
* xgcore `XG::Eva::EmCamera` (0x100 bytes): +0x10 view matrix, +0x50
  projection, +0x90 position, +0xa0 interest, +0xb0 up, +0xc0 roll, +0xec
  dirty bits (1 = view). Set* store and mark dirty; `calcViewMatrix` (export
  `?calcViewMatrix@EmCamera@Eva@XG@@IEBAXXZ`, xgcore 0xf650, starts
  `40 53 48 83 EC 40 4C 8D 81 A0 00 00 00`) = LookAt(position, interest, up) +
  roll; `Calculate` runs the dirty ones.
* Game camera classes (Battle::CameraBase / Lobby::CameraBase families) own an
  EmCamera at +8 and call Set* on it (~300 sites).
* render_pass `Scene` draws with its OWN EmCamera: `SetMainCamera(scene, cam)`
  copies position/interest/up/roll/fov/clip/aspect into
  [scene+0x2390+view*0x430] and calculates it (0x3f6f0); `SetMainPrevCamera`
  the same into +8 (previous frame, motion blur) unless `IsPrevCameraManaged`
  ([scene+0x3f0]). `GetMainCamera` returns the copy.
* SetMainCamera callers (import slot 0xf4d650; prev 0xf4d700), all
  `call [rip+x]`, by mode: battle 0x15d650 / 0x166470 / 0x16cf10
  (Battle::CameraManager [0x156b408]: 0xb90a0 current, 0xb90e0 previous,
  0xb9180) and 0xe4080 (Mob); lobby 0x3f4db0 (Lobby JobCallbackUpdateTail),
  0x457250 (MatchingLobby), both Lobby::CameraManager [0x15712e8] +0x20 (+0x28
  = second EmCamera); menu 0x665bf0 (Menu::Camera, char select/creation),
  0x6648c0 (restores a saved scene camera); cutscenes 0x2b05a0..0x2b2370
  (battle demo) and 0x3759f0 / 0x377540 (demo system [0x1554480]); loading
  screens LoadQuest 0x562270/0x5623f0, LoadVs 0x5668d0/0x5672d0, LoadVsGBB
  0x56d190/0x56dbb0, TtlLoadVsCore 0x876210/0x876c20.
* Battle's SetMainCamera source is the director's fixed output EmCamera:
  Battle::CameraManager +8 -> 0xad310 = [director+0] (or the demo camera while
  [0x1554480] plays one); 0xb90a0(idx) uses +0x18/+0x20 in split screen
  (+0x28 && +0x2c). 0x269f70 stores it at [obj+0x88] (0x3477b0) for the draw
  pass (0x339050 -> 0x338e20).
* GetMainCamera readers: 0x2d4ff0, 0x2fe974, 0x384210, 0x384e48, 0x385110,
  Menu 0x664f30 (saves the pointer, later passes it to SetMainCamera again).

## Who reads the SOURCE camera (must be zoomed too)
Zooming only the scene copy broke these (user tests 1-3, 2026-10-03):
* Lobby character culling 0x3c3070 (Lobby::CameraManager +0x20 view/proj) and
  battle world->screen 0xba15e (name plates, lock-on) -> fix: hook
  calcViewMatrix; for the camera last passed to SetMainCamera compute on a
  stack copy with the position zoomed, copy back only the view matrix.
* GetProjectionMatrix users (import 0xf4e870): 0xba15e, 0x15e230, 0x165e03,
  0x1c66d0, 0x3c3070, 0x3d5960, 0x449050, 0x483aa0, 0x4903e0, 0x4a7380.
* Map object culling 0x33b610 (breakable props; callers battle 0x26aa10 /
  0x26ab10, lobby 0x3f4db0 / 0x457250, scene camera 0x384210 / 0x385110,
  0x3477b0): world planes from GetPosition + GetFrustum + GetViewMatrix (eye
  in 0x1573a40) -> hooked, gets a zoomed stack copy, keeps no pointer.
* Behind-the-camera test 0x31dd70 (from 0x338e20, camera in rdx;
  dot(center - eye, dir) + radius < 0) and 0x31d980 (r9d < 0 path) -> same.
* 0x15e230 uses the view direction's yaw for an effect (not culling).
* Unused, for reference: data/system/camera.cam, CameraLimitValue.cml (per
  character, XV2-Tools CML_File), vs_load_cam.vlc.

## Text input (no typed chat in XV2)
Menu::TextInputPopup (vtable 0x10cf908, 0x2a0 bytes; made by
Textfield::InputTextFieldWindow slot 1 0x2f56a0 into +0x38, also by
FlowAccountSearch; slot 4 0x2f5700 opens, slot 6 0x2f5750 updates). Generic
menu Open 0x2c7460 registers an update job in [menu+0x60], Close 0x2c76b0
zeroes it. Keyboard = raw input in xgcore (Keybd::GetKeyState), no IME.
Loader overlay text fields: the game's keyboard is a **poll**, not messages:
xgcore 0xddf90 loops over 256 keys (index -> virtual key via vt+0xb8) and
calls USER32 GetKeyState (call at 0xde01a; `& 0x8000` -> byte per key).
GetKeyState follows the message queue, so swallowing WM_KEYDOWN/WM_INPUT in
the window procedure changes nothing. xgcore's GetRawInputData (0xdf30e) is
used only for the mouse (WM_INPUT dwType 0 + WM_MOUSEMOVE). Dead ends: a
WndProc filter on keyboard WM_INPUT (never sees any), a GetRawInputData
import filter (mouse only). Fix in overlay.cpp: xgcore's GetKeyState import
-> KeyStateForGame; while typing every key reads up, and keys down while
typing or when typing ends (Enter: ImGui polls GetAsyncKeyState on the render
thread and can end typing first) read up until really released. Confirmed
in game (Item Search box: no key reaches the game, Enter only ends typing).
