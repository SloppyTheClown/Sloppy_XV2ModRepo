; Game 1.25.02 addresses for training_qol_cave.asm, ported from 1.26.01 by
; matching code between decrypted images (Research_Notes\training-qol.md
; "1.25.02 port"). make_epatch.py also reads CAVE_RVA, STATE_RVA and
; FN_TRAINING_APPLY from here.

; .text slack is only 0x264 bytes here (.text ends at 0xfd8d9c, next section
; 0xfd9000), so the main piece goes into .bind slack (.bind ends at 0x1b77448,
; page ends 0x1b78000, executable), after Revamp 5.1.1's caves at
; 0x1b77500..0x1b77960 (RevampExtra.xml + RevampDynamicPartsets.xml); the
; CAVE2 piece goes into .text slack at 0xfd8da0.
CAVE_RVA            equ 1B77960h
STATE_RVA           equ 19D7F00h    ; .data slack: .data ends at 0x19d7850, page ends 0x19d8000, all zero, RW

; exe imports (IAT slots, kernel32)
IAT_GetProcAddress  equ 0FD9130h
IAT_LoadLibraryW    equ 0FD91B8h

; game globals and functions (1.26.01 address in brackets)
BATTLE_MANAGER      equ 1681C20h    ; (0x1554460) Battle manager*
CAMERA_MANAGER      equ 1698BC8h    ; (0x156b408) camera manager*
FN_TRAINING_APPLY   equ 1961E0h     ; (0x192e00) apply CPU Status (training, float deltaTime)
FN_SET_CONTROL_MODE equ 0D4210h     ; (0xd9870) (mob, mode): 0 human, 1 CPU
FN_SET_CAMERA_TARGET equ 0B5240h    ; (0xb8ca0) (cameraManager, mob)
FN_LOCAL_PLAYER_MOB equ 50560h      ; (0x14d040) (battleManager) -> Mob of the local player
FN_BATTLE_PAUSED    equ 159A30h     ; (0x1560c0) (battleManager) -> nonzero while paused
FN_CAMERA_AT        equ 0B6230h     ; (0xb9ec0) (cameraManager, index) -> camera
FN_SWITCH_CAMERA    equ 0B5210h     ; (0xb8c70) (cameraManager, index <= 9, int *param = 0)
FN_SET_CHARA_INPUT  equ 0D4270h     ; (0xd98d0) (mob, enable)
FN_SET_CAMERA_INPUT_OFF equ 7ED610h ; (0x7914c0) (camera, off)

; battle manager fields that differ between game versions
MGR_HUD_HIDDEN      equ 0D524h      ; (0xd528) HUD not drawn while nonzero (the patch removes its online-only check)
