# Training QoL

**Version 1.1.0** · for Dragon Ball Xenoverse 2 **1.27** · needs [XV2 Mod Loader](../XV2ModLoader/README.md) 1.1.0 or newer

An XV2 Mod Loader mod for Training. Press **D-pad up** to take control of
the Training partner: your controller and the camera move to the partner,
and you play it as if you had picked it. Press it again to go back to your
own character. While you control the partner, your own character just
stands still.

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

## Settings

`training_qol.json`:

| Key | Default | What it does |
|---|---|---|
| `toggle_button` | `"dpad_up"` | Controller button that switches: `dpad_up`, `dpad_down`, `dpad_left`, `dpad_right`, `start`, `back`, `l3`, `r3`, `lb`, `rb`, `a`, `b`, `x`, `y`, or `""` for none |
| `toggle_key` | `""` | Keyboard key that switches: `F1`..`F12`, a letter or a digit, or `""` for none |
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
