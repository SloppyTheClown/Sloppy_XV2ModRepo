; Game 1.27 addresses for training_qol_cave.asm, ported from 1.26.01 with the
; same address map as the DLL (Research_Notes\exe-and-tooling.md "Porting to a
; new game build"). make_epatch.py also reads CAVE_RVA, STATE_RVA and
; FN_TRAINING_APPLY from here.

CAVE_RVA            equ 0F59600h    ; .text slack: .text ends at 0xf595ec, next section 0xf5a000, all zero
STATE_RVA           equ 18BBF00h    ; .data slack: .data ends at 0x18bb5b0, page ends 0x18bc000, all zero, RW

; exe imports (IAT slots, kernel32)
IAT_GetProcAddress  equ 0F5A128h
IAT_LoadLibraryW    equ 0F5A1B8h

; game globals and functions (1.26.01 address in brackets)
BATTLE_MANAGER      equ 15681C0h    ; (0x1554460) Battle manager*
CAMERA_MANAGER      equ 157F168h    ; (0x156b408) camera manager*
FN_TRAINING_APPLY   equ 192EC0h     ; (0x192e00) apply CPU Status (training, float deltaTime)
FN_SET_CONTROL_MODE equ 0D9930h     ; (0xd9870) (mob, mode): 0 human, 1 CPU
FN_SET_CAMERA_TARGET equ 0B8CA0h    ; (0xb8ca0) (cameraManager, mob)
FN_LOCAL_PLAYER_MOB equ 14D100h     ; (0x14d040) (battleManager) -> Mob of the local player
FN_BATTLE_PAUSED    equ 156180h     ; (0x1560c0) (battleManager) -> nonzero while paused
FN_CAMERA_AT        equ 0B9EC0h     ; (0xb9ec0) (cameraManager, index) -> camera
FN_SWITCH_CAMERA    equ 0B8C70h     ; (0xb8c70) (cameraManager, index <= 9, int *param = 0)
FN_SET_CHARA_INPUT  equ 0D9990h     ; (0xd98d0) (mob, enable)
FN_SET_CAMERA_INPUT_OFF equ 79F430h ; (0x7914c0) (camera, off)

; battle manager fields that differ between game versions
MGR_HUD_HIDDEN      equ 0D528h      ; (0xd528, unchanged) HUD not drawn while nonzero (the patch removes its online-only check)
