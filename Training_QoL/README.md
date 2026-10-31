# Training QoL

**Version 1.1.0** · for Dragon Ball Xenoverse 2 **1.27** · needs [XV2 Mod Loader](../XV2ModLoader/README.md) 1.1.0 or newer

An XV2 Mod Loader mod for Training. Press **D-pad up** to take control of
the Training partner: your controller and the camera move to the partner,
and you play it as if you had picked it. Press it again to go back to your
own character. While you control the partner, your own character just
stands still.

Press **D-pad down** for the photo camera: the game's free camera from the
online battle photo mode. Both characters hold still and the battle HUD is
hidden; press it again to go back. Pausing also turns it off.

Stats, teams and the partner's Training settings are not changed. It only
works in offline Training and does nothing anywhere else.

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first. Close the game,
then put the `Training_QoL` folder from the download in the game's
`XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            Training_QoL\
                Training_QoL.dll
                training_qol.json
```

To remove it, delete the `Training_QoL` folder.

### XV2Patcher-only version (no mod loader)

The `Training_QoL_Epatch_<game version>-<version>.zip` downloads hold the
same features as an XV2Patcher Epatch, for game 1.27, 1.26.01 or 1.25.02
(Revamp's version) - pick the one for your game. Extract it into the game
folder:

```
DB Xenoverse 2\
    XV2PATCHER\
        Epatches\
            Training_QoL.xml
```

The buttons are fixed to D-pad up / down (no settings). Use either this or
the mod loader version, not both. To remove it, delete
`XV2PATCHER\Epatches\Training_QoL.xml`.

## Settings

`training_qol.json`:

| Key | Default | What it does |
|---|---|---|
| `toggle_button` | `"dpad_up"` | Controller button that switches: `dpad_up`, `dpad_down`, `dpad_left`, `dpad_right`, `start`, `back`, `l3`, `r3`, `lb`, `rb`, `a`, `b`, `x`, `y`, or `""` for none |
| `toggle_key` | `""` | Keyboard key that switches: `F1`..`F12`, a letter or a digit, or `""` for none |
| `photo_button` | `"dpad_down"` | Controller button for the photo camera (same choices) |
| `photo_key` | `""` | Keyboard key for the photo camera |
| `photo_hide_hud` | `true` | `true` = hides the battle HUD while the photo camera is on |
| `logging` | `false` | `true` = writes `Training_QoL.log` next to the DLL |

Changing a Training setting while you control the partner switches you
back first.

Version 1.0.x used D-pad down for the switch. An existing
`training_qol.json` is never overwritten, so after updating change
`"toggle_button"` to `"dpad_up"` yourself.

## Building

Needs CMake and Visual Studio 2022. Run `Build_Install.bat` to build and
install, `Build_Release.bat` for the release zip.

## Credits

* How the game swaps control between characters (its Body Change) and how
  Training applies the partner's settings were found by reverse engineering
  the game.
* XInput (Microsoft, part of Windows): reads the controller button.
