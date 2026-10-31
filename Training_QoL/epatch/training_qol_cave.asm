; Training QoL for XV2Patcher alone: the code cave written into DBXV2.exe by
; Training_QoL.xml (an Eternity patch). Same behaviour as src\dllmain.cpp,
; without the config, keyboard keys or log:
;   D-pad up   - hand the controller + camera to the Training partner and back
;   D-pad down - photo camera (camera 9) with both characters held still, HUD hidden
; Both are ignored while the battle is paused (pause leaves the photo camera).
;
; make_epatch.py assembles this with ml64 for one game version (/DGAME_1_25_02
; picks game_1.25.02.inc, /DGAME_1_27 game_1.27.inc, else game_1.26.01.inc), copies the code into the
; exe's slack (the main piece at CAVE_RVA, the CAVE2 piece where that version
; has room for it - see make_epatch.py GAMES), then points the three calls of
; Training's CPU Status apply at PerFrameApply / OtherApply. The code must
; stay position independent and relocation free (the script checks): exe
; addresses are [rbx + RVA] with rbx = exe base, worked out from where the
; cave runs.
;
; Cave-internal convention: every helper gets rbx = exe base and
; rdi = the state block, and leaves both as they were.
; Field offsets below are the same in 1.25.02, 1.26.01 and 1.27; everything that
; differs is in the game_*.inc files (Research_Notes\training-qol.md).

IFDEF GAME_1_25_02
include <game_1.25.02.inc>
ELSEIFDEF GAME_1_27
include <game_1.27.inc>
ELSE
include <game_1.26.01.inc>
ENDIF

SWITCH_BUTTON       equ 0001h       ; XINPUT_GAMEPAD_DPAD_UP
PHOTO_BUTTON        equ 0002h       ; XINPUT_GAMEPAD_DPAD_DOWN
PAD_SCAN_FRAMES     equ 60          ; pads not connected are asked again only this often (XInputGetState is slow for them)

BATTLE_CAMERA       equ 0
PHOTO_CAMERA        equ 9           ; Battle::CameraPhoto
CAMERA_CAN_SWITCH   equ 0C8h        ; camera vtable slot: the battle camera can be switched away from

; battle manager fields
MGR_TRAINING        equ 0BCh        ; 1 in Training
MGR_ONLINE          equ 0C0h        ; nonzero online
MGR_PAUSE_CONTROLLER equ 4A90h
PAUSE_POPUP         equ 68h         ; pause controller: PopupPause* while open
MGR_LOCAL_OVERRIDE  equ 1C00h       ; local player slot override, -1 = none
MGR_MOBS            equ 3A58h       ; Mob* per slot
MGR_BINDINGS        equ 3AD0h       ; input binding* per slot
SLOT_COUNT          equ 14

; Mob / Command / MenuTraining fields
MOB_SLOT            equ 44h
MOB_CONTROL_MODE    equ 50h
MOB_COMMAND         equ 4E8h
COMMAND_BINDING     equ 0D48h       ; the Mob's own copy of its binding*
TRAINING_PARTNER    equ 28h

; state block at STATE_RVA (starts zeroed with the exe)
S_XINPUT_GET_STATE  equ 00h         ; qword: XInputGetState, 0 = not found
S_SWITCH_ACTIVE     equ 08h         ; byte
S_PHOTO_ACTIVE      equ 09h         ; byte
S_SWITCH_HELD       equ 0Ah         ; byte: button down last frame
S_PHOTO_HELD        equ 0Bh         ; byte
S_XINPUT_TRIED      equ 0Ch         ; byte: XInput looked up already
S_SW_TRAINING       equ 10h         ; the MenuTraining the switch was made in
S_SW_MANAGER        equ 18h
S_SW_PLAYER         equ 20h         ; the character picked
S_SW_PARTNER        equ 28h
S_SW_PLAYER_SLOT    equ 30h         ; dword
S_SW_PARTNER_SLOT   equ 34h         ; dword
S_SW_PLAYER_BINDING equ 38h         ; the bindings before the swap
S_SW_PARTNER_BINDING equ 40h
S_SW_OVERRIDE       equ 48h         ; dword: local override before the switch
S_SW_PARTNER_MODE   equ 4Ch         ; dword: partner's control mode before the switch
S_PH_TRAINING       equ 50h         ; the MenuTraining the photo camera was turned on in
S_PH_MANAGER        equ 58h
S_PH_MOB0           equ 60h         ; the two characters held still
S_PH_MOB1           equ 68h
S_PH_SLOT0          equ 70h         ; dword
S_PH_SLOT1          equ 74h         ; dword
S_PAD_CONNECTED     equ 78h         ; 4 bytes, one per XInput pad
S_FRAMES_TO_SCAN    equ 7Ch         ; dword

; rbx = exe base, rdi = state block
SETUP_BASE macro
    lea rbx, CaveStart
    sub rbx, CAVE_RVA
    lea rdi, [rbx + STATE_RVA]
endm

.code

CaveStart:

; Replaces the per-frame call (0x192de5, MenuTraining update 0x192cf0).
; rcx = MenuTraining, xmm1 = deltaTime.
PerFrameApply PROC PUBLIC
    push rbx
    push rsi
    push rdi
    push r13
    push r14
    push r15
    sub rsp, 38h
    movss dword ptr [rsp + 20h], xmm1
    mov rsi, rcx
    SETUP_BASE

    ; drop a switch or photo camera left over from another battle
    cmp byte ptr [rdi + S_SWITCH_ACTIVE], 0
    je switch_checked
    mov rcx, rsi
    call SwitchStillValid
    test eax, eax
    jnz switch_checked
    mov byte ptr [rdi + S_SWITCH_ACTIVE], 0
switch_checked:
    cmp byte ptr [rdi + S_PHOTO_ACTIVE], 0
    je photo_checked
    mov rcx, rsi
    call PhotoStillValid
    test eax, eax
    jnz photo_checked
    mov rax, [rbx + BATTLE_MANAGER]
    cmp rax, [rdi + S_PH_MANAGER]
    jne photo_dropped
    mov dword ptr [rax + MGR_HUD_HIDDEN], 0
photo_dropped:
    mov byte ptr [rdi + S_PHOTO_ACTIVE], 0
photo_checked:

    ; r13d / r14d = switch / photo button went down this frame
    call PadButtonsHeld
    xor r13d, r13d
    xor r14d, r14d
    test eax, SWITCH_BUTTON
    setnz cl
    cmp byte ptr [rdi + S_SWITCH_HELD], 0
    mov [rdi + S_SWITCH_HELD], cl
    jne switch_edge_done
    movzx r13d, cl
switch_edge_done:
    test eax, PHOTO_BUTTON
    setnz cl
    cmp byte ptr [rdi + S_PHOTO_HELD], 0
    mov [rdi + S_PHOTO_HELD], cl
    jne photo_edge_done
    movzx r14d, cl
photo_edge_done:

    call BattlePaused
    test eax, eax
    jz not_paused
    cmp byte ptr [rdi + S_PHOTO_ACTIVE], 0
    je buttons_done
    call LeavePhoto
    jmp buttons_done
not_paused:
    test r14d, r14d
    jz check_switch
    cmp byte ptr [rdi + S_PHOTO_ACTIVE], 0
    je enter_photo
    call LeavePhoto
    jmp buttons_done
enter_photo:
    mov rcx, rsi
    call EnterPhoto
    jmp buttons_done
check_switch:
    test r13d, r13d
    jz buttons_done
    cmp byte ptr [rdi + S_PHOTO_ACTIVE], 0
    jne buttons_done
    cmp byte ptr [rdi + S_SWITCH_ACTIVE], 0
    je switch_to_partner
    call SwitchBack
    jmp buttons_done
switch_to_partner:
    mov rcx, rsi
    call SwitchToPartner
    test eax, eax
    jnz per_frame_done                  ; switched: the partner is not given its CPU Status this frame

buttons_done:
    cmp byte ptr [rdi + S_SWITCH_ACTIVE], 0
    jne keep_partner_human
    mov rcx, rsi
    movss xmm1, dword ptr [rsp + 20h]
    lea rax, [rbx + FN_TRAINING_APPLY]
    call rax
    jmp per_frame_done
keep_partner_human:
    ; the CPU Status apply is skipped while switched; keep the partner on human control
    mov rcx, [rdi + S_SW_PARTNER]
    cmp dword ptr [rcx + MOB_CONTROL_MODE], 0
    je per_frame_done
    xor edx, edx
    lea rax, [rbx + FN_SET_CONTROL_MODE]
    call rax
per_frame_done:
    add rsp, 38h
    pop r15
    pop r14
    pop r13
    pop rdi
    pop rsi
    pop rbx
    ret
PerFrameApply ENDP

; Replaces the other two calls (0x191f03 a setting changed, 0x192854 menu
; setup): switch back first so the new CPU Status lands on the partner.
; rcx = MenuTraining, xmm1 = deltaTime.
OtherApply PROC PUBLIC
    push rbx
    push rsi
    push rdi
    sub rsp, 30h
    movss dword ptr [rsp + 20h], xmm1
    mov rsi, rcx
    SETUP_BASE
    cmp byte ptr [rdi + S_SWITCH_ACTIVE], 0
    je other_apply
    mov rcx, rsi
    call SwitchStillValid
    test eax, eax
    jz other_drop
    call SwitchBack
other_drop:
    mov byte ptr [rdi + S_SWITCH_ACTIVE], 0
other_apply:
    mov rcx, rsi
    movss xmm1, dword ptr [rsp + 20h]
    lea rax, [rbx + FN_TRAINING_APPLY]
    call rax
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    ret
OtherApply ENDP

; -> eax = XInput buttons held on any pad (0 without XInput).
; XInput is looked up once: xinput1_3 is XV2Patcher itself (it forwards
; XInputGetState), then the system's other XInput versions.
PadButtonsHeld PROC
    push rsi
    push rbp
    push r12
    push r13
    sub rsp, 38h                        ; [rsp+20h] XINPUT_STATE
    cmp byte ptr [rdi + S_XINPUT_TRIED], 0
    jne xinput_ready
    mov byte ptr [rdi + S_XINPUT_TRIED], 1
    lea rsi, XInputNames
xinput_try_name:
    cmp word ptr [rsi], 0
    je xinput_ready
    mov rcx, rsi
    call qword ptr [rbx + IAT_LoadLibraryW]
    test rax, rax
    jz xinput_next_name
    mov rcx, rax
    lea rdx, XInputGetStateName
    call qword ptr [rbx + IAT_GetProcAddress]
    mov [rdi + S_XINPUT_GET_STATE], rax
    test rax, rax
    jnz xinput_ready
xinput_next_name:
    add rsi, 2                          ; skip to the next name (past its terminating zero)
    cmp word ptr [rsi - 2], 0
    jne xinput_next_name
    jmp xinput_try_name
xinput_ready:
    xor r12d, r12d                      ; buttons
    mov rbp, [rdi + S_XINPUT_GET_STATE]
    test rbp, rbp
    jz pads_done
    xor r13d, r13d                      ; ask the pads that were not connected too
    dec dword ptr [rdi + S_FRAMES_TO_SCAN]
    jg pads_scan_decided
    mov dword ptr [rdi + S_FRAMES_TO_SCAN], PAD_SCAN_FRAMES
    mov r13d, 1
pads_scan_decided:
    xor esi, esi                        ; pad index
pad_loop:
    test r13d, r13d
    jnz pad_query
    cmp byte ptr [rdi + rsi + S_PAD_CONNECTED], 0
    je pad_next
pad_query:
    xorps xmm0, xmm0
    movups [rsp + 20h], xmm0
    mov ecx, esi
    lea rdx, [rsp + 20h]
    call rbp
    test eax, eax                       ; ERROR_SUCCESS = connected
    setz byte ptr [rdi + rsi + S_PAD_CONNECTED]
    jnz pad_next
    movzx eax, word ptr [rsp + 24h]     ; XINPUT_STATE.Gamepad.wButtons
    or r12d, eax
pad_next:
    inc esi
    cmp esi, 4
    jb pad_loop
pads_done:
    mov eax, r12d
    add rsp, 38h
    pop r13
    pop r12
    pop rbp
    pop rsi
    ret
PadButtonsHeld ENDP

; -> eax = 1 while the battle is paused (pause flag or the pause popup open)
BattlePaused PROC
    sub rsp, 28h
    xor eax, eax
    mov rcx, [rbx + BATTLE_MANAGER]
    test rcx, rcx
    jz paused_done
    mov rax, [rcx + MGR_PAUSE_CONTROLLER]
    test rax, rax
    jz paused_ask
    cmp qword ptr [rax + PAUSE_POPUP], 0
    mov eax, 1
    jne paused_done
paused_ask:
    lea rax, [rbx + FN_BATTLE_PAUSED]
    call rax
    test eax, eax
    setnz al
    movzx eax, al
paused_done:
    add rsp, 28h
    ret
BattlePaused ENDP

; rcx = MenuTraining -> eax = 1 if the switch still belongs to this battle
SwitchStillValid PROC
    mov rax, [rbx + BATTLE_MANAGER]
    cmp rcx, [rdi + S_SW_TRAINING]
    jne switch_invalid
    cmp rax, [rdi + S_SW_MANAGER]
    jne switch_invalid
    movsxd rdx, dword ptr [rdi + S_SW_PLAYER_SLOT]
    cmp edx, SLOT_COUNT
    jae switch_invalid
    mov r8, [rax + rdx * 8 + MGR_MOBS]
    cmp r8, [rdi + S_SW_PLAYER]
    jne switch_invalid
    mov r8, [rax + rdx * 8 + MGR_BINDINGS]
    cmp r8, [rdi + S_SW_PARTNER_BINDING]
    jne switch_invalid
    movsxd rdx, dword ptr [rdi + S_SW_PARTNER_SLOT]
    cmp edx, SLOT_COUNT
    jae switch_invalid
    mov r8, [rax + rdx * 8 + MGR_MOBS]
    cmp r8, [rdi + S_SW_PARTNER]
    jne switch_invalid
    mov eax, 1
    ret
switch_invalid:
    xor eax, eax
    ret
SwitchStillValid ENDP

; rcx = MenuTraining -> eax = 1 if the photo camera still belongs to this battle
PhotoStillValid PROC
    mov rax, [rbx + BATTLE_MANAGER]
    cmp rcx, [rdi + S_PH_TRAINING]
    jne photo_invalid
    cmp rax, [rdi + S_PH_MANAGER]
    jne photo_invalid
    cmp qword ptr [rbx + CAMERA_MANAGER], 0
    je photo_invalid
    movsxd rdx, dword ptr [rdi + S_PH_SLOT0]
    cmp edx, SLOT_COUNT
    jae photo_invalid
    mov r8, [rax + rdx * 8 + MGR_MOBS]
    cmp r8, [rdi + S_PH_MOB0]
    jne photo_invalid
    movsxd rdx, dword ptr [rdi + S_PH_SLOT1]
    cmp edx, SLOT_COUNT
    jae photo_invalid
    mov r8, [rax + rdx * 8 + MGR_MOBS]
    cmp r8, [rdi + S_PH_MOB1]
    jne photo_invalid
    mov eax, 1
    ret
photo_invalid:
    xor eax, eax
    ret
PhotoStillValid ENDP

; rcx = manager, edx = slot, r8 = mob, r9 = binding: give the slot this
; binding in both places the game keeps it (Body Change 0x15958e / 0x159579)
AssignBinding PROC
    movsxd rdx, edx
    mov [rcx + rdx * 8 + MGR_BINDINGS], r9
    mov rax, [r8 + MOB_COMMAND]
    test rax, rax
    jz assign_done
    mov [rax + COMMAND_BINDING], r9
assign_done:
    ret
AssignBinding ENDP

; rcx = mob, edx = mode
SetControlMode PROC
    lea rax, [rbx + FN_SET_CONTROL_MODE]
    jmp rax
SetControlMode ENDP

; rcx = mob: the battle camera follows it
PointCameraAt PROC
    sub rsp, 28h
    mov rdx, rcx
    mov rcx, [rbx + CAMERA_MANAGER]
    test rcx, rcx
    jz point_done
    lea rax, [rbx + FN_SET_CAMERA_TARGET]
    call rax
point_done:
    add rsp, 28h
    ret
PointCameraAt ENDP

; rcx = MenuTraining -> eax = 1 if the controller and camera went to the partner
SwitchToPartner PROC
    push rsi
    push rbp
    push r12
    push r13
    push r14
    push r15
    sub rsp, 28h
    mov rsi, rcx
    mov r12, [rbx + BATTLE_MANAGER]
    test r12, r12
    jz to_partner_failed
    cmp dword ptr [r12 + MGR_TRAINING], 1
    jne to_partner_failed
    cmp dword ptr [r12 + MGR_ONLINE], 0
    jne to_partner_failed
    mov r13, [rsi + TRAINING_PARTNER]
    test r13, r13
    jz to_partner_failed
    mov rcx, r12
    lea rax, [rbx + FN_LOCAL_PLAYER_MOB]
    call rax
    mov r14, rax
    test r14, r14
    jz to_partner_failed
    cmp r14, r13
    je to_partner_failed
    movsxd rbp, dword ptr [r14 + MOB_SLOT]      ; player slot
    movsxd r15, dword ptr [r13 + MOB_SLOT]      ; partner slot
    cmp ebp, SLOT_COUNT
    jae to_partner_failed
    cmp [r12 + rbp * 8 + MGR_MOBS], r14
    jne to_partner_failed
    cmp r15d, SLOT_COUNT
    jae to_partner_failed
    cmp [r12 + r15 * 8 + MGR_MOBS], r13
    jne to_partner_failed
    mov rax, [r12 + rbp * 8 + MGR_BINDINGS]
    test rax, rax
    jz to_partner_failed
    mov [rdi + S_SW_PLAYER_BINDING], rax
    mov rax, [r12 + r15 * 8 + MGR_BINDINGS]
    test rax, rax
    jz to_partner_failed
    mov [rdi + S_SW_PARTNER_BINDING], rax

    mov [rdi + S_SW_TRAINING], rsi
    mov [rdi + S_SW_MANAGER], r12
    mov [rdi + S_SW_PLAYER], r14
    mov [rdi + S_SW_PARTNER], r13
    mov [rdi + S_SW_PLAYER_SLOT], ebp
    mov [rdi + S_SW_PARTNER_SLOT], r15d
    mov eax, [r12 + MGR_LOCAL_OVERRIDE]
    mov [rdi + S_SW_OVERRIDE], eax
    mov eax, [r13 + MOB_CONTROL_MODE]
    mov [rdi + S_SW_PARTNER_MODE], eax
    mov byte ptr [rdi + S_SWITCH_ACTIVE], 1

    mov rcx, r12                                ; player slot gets the partner's binding
    mov edx, ebp
    mov r8, r14
    mov r9, [rdi + S_SW_PARTNER_BINDING]
    call AssignBinding
    mov rcx, r12                                ; and the other way round
    mov edx, r15d
    mov r8, r13
    mov r9, [rdi + S_SW_PLAYER_BINDING]
    call AssignBinding
    mov [r12 + MGR_LOCAL_OVERRIDE], r15d
    mov rcx, r13
    xor edx, edx
    call SetControlMode
    mov rcx, r13
    call PointCameraAt
    mov eax, 1
    jmp to_partner_done
to_partner_failed:
    xor eax, eax
to_partner_done:
    add rsp, 28h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rsi
    ret
SwitchToPartner ENDP

; give the controller and camera back to the player (no-op when not switched)
SwitchBack PROC
    sub rsp, 28h
    cmp byte ptr [rdi + S_SWITCH_ACTIVE], 0
    je back_done
    mov rcx, [rdi + S_SW_MANAGER]
    mov edx, [rdi + S_SW_PLAYER_SLOT]
    mov r8, [rdi + S_SW_PLAYER]
    mov r9, [rdi + S_SW_PLAYER_BINDING]
    call AssignBinding
    mov rcx, [rdi + S_SW_MANAGER]
    mov edx, [rdi + S_SW_PARTNER_SLOT]
    mov r8, [rdi + S_SW_PARTNER]
    mov r9, [rdi + S_SW_PARTNER_BINDING]
    call AssignBinding
    mov rax, [rdi + S_SW_MANAGER]
    mov ecx, [rdi + S_SW_OVERRIDE]
    mov [rax + MGR_LOCAL_OVERRIDE], ecx
    mov rcx, [rdi + S_SW_PARTNER]
    mov edx, [rdi + S_SW_PARTNER_MODE]
    call SetControlMode
    mov rcx, [rdi + S_SW_PLAYER]
    call PointCameraAt
    mov byte ptr [rdi + S_SWITCH_ACTIVE], 0
back_done:
    add rsp, 28h
    ret
SwitchBack ENDP

; rcx = camera manager, edx = index -> rax = camera
CameraAt PROC
    lea rax, [rbx + FN_CAMERA_AT]
    jmp rax
CameraAt ENDP

; rcx = camera manager -> eax = 1 if the battle camera can be switched away from
CameraCanSwitch PROC
    sub rsp, 28h
    xor edx, edx                                ; BATTLE_CAMERA
    call CameraAt
    test rax, rax
    jz can_switch_done                          ; eax = 0
    mov rcx, rax
    mov rax, [rax]
    call qword ptr [rax + CAMERA_CAN_SWITCH]
    test eax, eax
    setnz al
    movzx eax, al
can_switch_done:
    add rsp, 28h
    ret
CameraCanSwitch ENDP

; rcx = camera manager, edx = index
SwitchCamera PROC
    sub rsp, 38h
    mov dword ptr [rsp + 20h], 0                ; the game passes a zeroed int (0x18e465)
    lea r8, [rsp + 20h]
    lea rax, [rbx + FN_SWITCH_CAMERA]
    call rax
    add rsp, 38h
    ret
SwitchCamera ENDP

; The second piece: helpers of the photo camera and the XInput names. In
; 1.26.01 and 1.27 it follows the main piece; in 1.25.02 it goes elsewhere (not enough
; room in one place), so the main piece only reaches it by rip-relative
; calls/leas, which make_epatch.py resolves for where each piece lands.
CAVE2 SEGMENT PARA 'CODE'

; rcx = mob, edx = enable
SetCharacterInput PROC
    lea rax, [rbx + FN_SET_CHARA_INPUT]
    jmp rax
SetCharacterInput ENDP

; rcx = camera manager, edx = off: the photo camera's own stick/button input
SetPhotoCameraInputOff PROC
    push rsi
    sub rsp, 20h
    mov esi, edx
    mov edx, PHOTO_CAMERA
    call CameraAt
    test rax, rax
    jz photo_input_done
    mov rcx, rax
    mov edx, esi
    lea rax, [rbx + FN_SET_CAMERA_INPUT_OFF]
    call rax
photo_input_done:
    add rsp, 20h
    pop rsi
    ret
SetPhotoCameraInputOff ENDP

; rcx = MenuTraining -> eax = 1 if the photo camera went on
EnterPhoto PROC
    push rsi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 20h
    mov rsi, rcx
    mov r12, [rbx + BATTLE_MANAGER]
    mov r15, [rbx + CAMERA_MANAGER]
    test r12, r12
    jz enter_failed
    test r15, r15
    jz enter_failed
    cmp dword ptr [r12 + MGR_TRAINING], 1
    jne enter_failed
    cmp dword ptr [r12 + MGR_ONLINE], 0
    jne enter_failed
    cmp byte ptr [rdi + S_SWITCH_ACTIVE], 0
    je enter_unswitched
    mov r13, [rdi + S_SW_PLAYER]
    mov r14, [rdi + S_SW_PARTNER]
    jmp enter_have_mobs
enter_unswitched:
    mov rcx, r12
    lea rax, [rbx + FN_LOCAL_PLAYER_MOB]
    call rax
    mov r13, rax
    mov r14, [rsi + TRAINING_PARTNER]
enter_have_mobs:
    test r13, r13
    jz enter_failed
    test r14, r14
    jz enter_failed
    mov rcx, r15
    mov edx, PHOTO_CAMERA
    call CameraAt
    test rax, rax
    jz enter_failed
    mov rcx, r15
    call CameraCanSwitch
    test eax, eax
    jz enter_failed

    mov [rdi + S_PH_TRAINING], rsi
    mov [rdi + S_PH_MANAGER], r12
    mov [rdi + S_PH_MOB0], r13
    mov [rdi + S_PH_MOB1], r14
    mov eax, [r13 + MOB_SLOT]
    mov [rdi + S_PH_SLOT0], eax
    mov eax, [r14 + MOB_SLOT]
    mov [rdi + S_PH_SLOT1], eax
    mov byte ptr [rdi + S_PHOTO_ACTIVE], 1

    mov rcx, r15
    mov edx, PHOTO_CAMERA
    call SwitchCamera
    mov rcx, r13
    xor edx, edx
    call SetCharacterInput
    mov rcx, r14
    xor edx, edx
    call SetCharacterInput
    mov rcx, r15
    xor edx, edx                                ; photo camera input on
    call SetPhotoCameraInputOff
    mov dword ptr [r12 + MGR_HUD_HIDDEN], 1
    mov eax, 1
    jmp enter_done
enter_failed:
    xor eax, eax
enter_done:
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    ret
EnterPhoto ENDP

; back to the battle camera, characters and HUD back on (no-op when off)
LeavePhoto PROC
    push rsi
    sub rsp, 20h
    cmp byte ptr [rdi + S_PHOTO_ACTIVE], 0
    je leave_done
    mov rsi, [rbx + CAMERA_MANAGER]
    test rsi, rsi
    jz leave_characters
    mov rcx, rsi
    call CameraCanSwitch
    test eax, eax
    jz leave_camera_input                       ; battle camera busy: camera left as it is
    mov rcx, rsi
    xor edx, edx                                ; BATTLE_CAMERA
    call SwitchCamera
leave_camera_input:
    mov rcx, rsi
    mov edx, 1
    call SetPhotoCameraInputOff
leave_characters:
    mov rcx, [rdi + S_PH_MOB0]
    mov edx, 1
    call SetCharacterInput
    mov rcx, [rdi + S_PH_MOB1]
    mov edx, 1
    call SetCharacterInput
    mov rax, [rdi + S_PH_MANAGER]
    mov dword ptr [rax + MGR_HUD_HIDDEN], 0
    mov byte ptr [rdi + S_PHOTO_ACTIVE], 0
leave_done:
    add rsp, 20h
    pop rsi
    ret
LeavePhoto ENDP

; names tried in order, each zero-terminated, list ends with an empty name
XInputNames:
    dw 'x','i','n','p','u','t','1','_','3','.','d','l','l',0
    dw 'x','i','n','p','u','t','1','_','4','.','d','l','l',0
    dw 'x','i','n','p','u','t','9','_','1','_','0','.','d','l','l',0
    dw 0
XInputGetStateName:
    db 'XInputGetState',0

CAVE2 ENDS

END
