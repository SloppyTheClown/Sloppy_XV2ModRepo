# Effect Texture Limit

**Version 1.1.1** · for Dragon Ball Xenoverse 2 **1.27** · needs [XV2 Mod Loader](../XV2ModLoader/README.md) 1.1.0 or newer

An XV2 Mod Loader mod that lets an effect texture file (`*.ptcl.emb`, e.g.
`vfx\cmn\BTL_AURA.ptcl.emb`) hold up to **255** textures instead of 128.
Without it, the game draws particles that use texture 128 or higher without
their texture. Files with 128 textures or fewer look exactly as before.

**It is off by default.** LB Mod Installer, EEPK Organiser and other effect
tools treat a file with more than 128 textures as corrupt. The better fix
for a full aura file is to merge its textures and stay under 128: EEPK
Organiser's Tools > Optimize Textures (see "Optimize Your Auras!" in the
credits). `XV2_NewTransformationsBridge` does that automatically. Turn this
mod on only if you need more than 128 textures anyway.

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

To turn it on, set `"raise_limit": true` in `effect_texture_limit.json`.
Everyone you play online with needs it on too. Without it, their game draws
auras that use the extra textures without their texture.

To remove it, delete the `EffectTextureLimit` folder.

## Settings

`effect_texture_limit.json`:

| Key | Default | What it does |
|---|---|---|
| `raise_limit` | `false` | `true` = textures 128..254 are drawn (limit 255) |
| `logging` | `false` | `true` = writes `EffectTextureLimit.log` next to the DLL |

## Building

Needs CMake and Visual Studio 2022.

## Credits

* The game's 128-texture limit was found by reverse engineering the game.
* "Optimize Your Auras!" (community guide, Google Docs):
  https://docs.google.com/document/d/13erQ3iCNZPjsIZfEQhxbGgVKw5hu4kVhg7aL8g4IbZw
  - the texture merge recommended above instead of raising the limit.
* EEPK Organiser by LazyBone (XV2-Tools,
  https://github.com/LazyBone152/XV2-Tools, MIT): its Optimize Textures tool.
