# Effect Texture Limit

An XV2 Mod Loader mod that lets an effect texture file (`*.ptcl.emb`, e.g.
`vfx\cmn\BTL_AURA.ptcl.emb`) hold up to **255** textures instead of 128.
Without it, the game draws particles that use texture 128 or higher without
their texture. Files with 128 textures or fewer look exactly as before.

Mods that add many auras need it, e.g. New Transformations on top of Revamp.
To install those, see `XV2_NewTransformationsBridge`.

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first. Close the game,
then put the `EffectTextureLimit` folder from the download in the game's
`XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            EffectTextureLimit\
                EffectTextureLimit.dll
                effect_texture_limit.json
```

Everyone you play online with needs it too. Without it, their game draws
auras that use the extra textures without their texture.

To remove it, delete the `EffectTextureLimit` folder.

## Building

Needs CMake and Visual Studio 2022.

## Credits

* The game's 128-texture limit was found by reverse engineering the game.
