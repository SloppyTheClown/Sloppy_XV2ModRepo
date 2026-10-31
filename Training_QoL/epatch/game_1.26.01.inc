; Game 1.26.01 addresses for training_qol_cave.asm (Research_Notes\training-qol.md).
; make_epatch.py also reads CAVE_RVA, STATE_RVA and FN_TRAINING_APPLY from here.

CAVE_RVA            equ 0F4B400h    ; .text slack: .text ends at 0xf4b05c, next section 0xf4c000, all zero
STATE_RVA           equ 18A7F00h    ; .data slack: .data ends at 0x18a76d0, page ends 0x18a8000, all zero, RW

; exe imports (IAT slots, kernel32)
IAT_GetProcAddress  equ 0F4C128h
IAT_LoadLibraryW    equ 0F4C1B8h

; game globals and functions
BATTLE_MANAGER      equ 1554460h    ; Battle manager*
CAMERA_MANAGER      equ 156B408h    ; camera manager*
FN_TRAINING_APPLY   equ 192E00h     ; apply CPU Status (training, float deltaTime)
FN_SET_CONTROL_MODE equ 0D9870h     ; (mob, mode): 0 human, 1 CPU
FN_SET_CAMERA_TARGET equ 0B8CA0h    ; (cameraManager, mob)
FN_LOCAL_PLAYER_MOB equ 14D040h     ; (battleManager) -> Mob of the local player
FN_BATTLE_PAUSED    equ 1560C0h     ; (battleManager) -> nonzero while paused
FN_CAMERA_AT        equ 0B9EC0h     ; (cameraManager, index) -> camera
FN_SWITCH_CAMERA    equ 0B8C70h     ; (cameraManager, index <= 9, int *param = 0)
FN_SET_CHARA_INPUT  equ 0D98D0h     ; (mob, enable)
FN_SET_CAMERA_INPUT_OFF equ 7914C0h ; (camera, off)

; battle manager fields that differ between game versions
MGR_HUD_HIDDEN      equ 0D528h      ; HUD not drawn while nonzero (the patch removes its online-only check)
