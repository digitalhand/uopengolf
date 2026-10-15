# Range assets

## Playing and performance

The public range includes a grass material, optional short 3D grass, a dimpled
golf ball, and a red target flagstick. Open the project, press Play, and press
**R** for a replay shot. The imported `.uasset` files are included; players need
neither a network connection nor Blender to use them.

Use the Unreal console (**~**) during play:

| Command | Effect |
| --- | --- |
| `uopengolf.Grass3D 0` | Clears all decorative grass instances |
| `uopengolf.Grass3D 1` | Recreates the same deterministic grass population (default) |

Grass covers a 20 m radius around the tee, clipped at the range's back edge and
excluding the 5 m tee paint. Clumps contain 32 small triangles, use no collision
or shadows, and cull between 12 and 22 m. The toggle is checked every 0.25 s.
The fairway texture, mowing stripes, and physical ground remain when grass is off.
The flag is static; it does not simulate cloth.

The fairway and grass-blade materials expose `GrassDarkColor` and
`GrassLightColor` parameters, set to sRGB **#406B1F** and **#4D7E20** respectively.
These are explicit base colors converted to linear RGB for Unreal, rather than
multipliers on the source texture. The mowing stripes and subtle texture/blade
variation blend between these endpoints; the tee paint uses #406B1F. Lighting
and tone mapping still affect the final on-screen pixels. The supplied golf photograph was used
as a visual color reference only and is not distributed as a game asset.

## Sources and conversions

See [third-party notices](../THIRD_PARTY_NOTICES.md) for asset licenses and authors.
All third-party inputs below use CC0 1.0 Universal. Converted assets live under
`Content/UOpenGolf/Art/`; original material graphs and grass geometry use MIT.

| Input | Conversion |
| --- | --- |
| ambientCG `Grass004_2K-JPG.zip` | Import Color as sRGB, NormalDX as a normal map without flipping green, and Roughness as linear masks. Planar world-space UVs repeat every 140 cm; world-space normals match the flat ground. Add modest stripes and broad color variation in the material. |
| jpsweber `golf_0.zip`, `Golf_ball.obj` | Center and normalize to a 100 cm diameter. Use smooth normals and an ivory material. UE generates LODs at 100%, 25%, and 5% of source triangles, with screen sizes 1.0, 0.1, and 0.025. Source LOD has 61,672 triangles. |
| mess110 `game-golf.tar_6.gz`, `bin/golf-flag.obj` | Convert Y-up to Z-up, scale pole top to 240 cm, orient flag toward the tee, and assign pole/flag materials by mesh region. Retain the small below-ground pole section. Other models and textures are omitted. |
| Original grass clump | Deterministically generate 32 blades over a 20 cm square, 1.2–2 cm tall, with a two-sided material and per-instance color variation. |

Archive SHA-256 checksums, which pin the downloaded versions:

| Archive | SHA-256 |
| --- | --- |
| `Grass004_2K-JPG.zip` | `d7d3f08e8bb1e59aed0c3dbb85ea0b46d5308481f89b6a3fd62c0ec9e862245b` |
| `golf_0.zip` | `33358f1d9549e3b56f8826b65982dc1b5bafa701321425e7970971b7e4388d2e` |
| `game-golf.tar.gz` (downloaded as `game-golf.tar_6.gz`) | `fd2ca23a434ec9c1131d2a8758ea2d24710fcc6c4c00eb73b9f193b45ea2e303` |

## Regeneration

Close Unreal Editor and run from the repository root. Set the local Blender and
Unreal paths for your machine; no machine-specific paths are stored in the assets'
game references. Conversion was run with Blender 5.0 and Unreal 5.8.3 on Windows.

```powershell
python Scripts/PrepareRangeAssets.py
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' --background --python Scripts/PrepareRangeAssets.py
& "$env:UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PWD/UOpenGolf.uproject" `
  "-ExecutePythonScript=$PWD/Scripts/ImportRangeAssets.py" `
  -unattended -nopause -nosplash -nullrhi -DDC=UOpenGolfSmoke
```

The preparation script downloads missing archives, verifies their hashes, and
extracts only explicitly selected members into ignored `Saved/AssetImport/Prepared/`.
Running the script in Blender additionally exports FBX meshes. The import utility
refuses to overwrite assets without its `Generator` metadata tag. It reimports
owned meshes and textures and preserves existing material graphs; edit those
graphs explicitly in Unreal if their design needs changing. Use
`-ExecutePythonScript`, since the Python commandlet does not initialize the editor
subsystems needed for LOD generation.

`Scripts/RangeGrassAppearance.py` sets the two grass-color parameters without
reimporting textures or replacing material graphs. Run it through Unreal Python
to reset the palette to these defaults. The importer applies the palette to new
materials and preserves existing palette edits on subsequent imports.

## Validation

Build and run Smoke, `UOpenGolf.Physics`, and `UOpenGolf.Range` with the project
scripts. `UOpenGolf.Range.Presentation` checks the imported ball's source diameter,
non-colliding visuals, and grass disable/re-enable behavior.

For rendered inspection and a same-camera grass comparison:

```powershell
& "$env:UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PWD/UOpenGolf.uproject" `
  -unattended -nosound -RenderOffscreen -ResX=1280 -ResY=720 -DDC=UOpenGolfSmoke `
  '-ExecCmds=Automation RunTests UOpenGolf.Visual.RangeAssets' `
  '-TestExit=Automation Test Queue Empty' `
  "-ReportExportPath=$PWD/Saved/Automation/RangeAssets"
```

Captures are written under `Saved/Screenshots/RangeAssets/`. The test records mean
editor frame intervals with grass on and off after a settling period. These
include editor and automation overhead and are not standalone-game benchmarks.
The first rendered run using the project-local cache may spend several minutes
compiling shaders.
Record actual test outcomes and remaining platform gaps in
[phase status](PhasesStatus.md).
