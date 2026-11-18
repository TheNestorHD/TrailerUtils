<h1 align="center">
  SCS-TrailerUtils
</h1>

<p align="center">
  This ETS2 and ATS plugin allows you to custom steer the trailer using either the menu or another input device!
  <br>
  <img src="https://i.imgur.com/aWMkaI7.png" alt="Image">
</p>

# Credits:
- [Baldy09](<https://github.com/Baldywaldy09>)

## ETS2 1.62 experimental port

This branch targets the 1.62 layout, not older versions. The core signature and offsets are based on the documented findings of [suagrsu/TrailerUtils](https://github.com/suagrsu/TrailerUtils/blob/main/ETS2-TrailerUtil/game_profiles.h) (ETS2 1.62.3.3). Steering and joint offsets were reported verified there; the slave pointer offset is inferred and needs testing with multi-trailer combinations. Suspension adjustment is disabled until its new offset is established. The original 1.60 source is preserved on main. No compiled DLL is redistributed.

### Build on Windows

- Visual Studio 2022 / v143, Desktop development with C++, Windows SDK.
- Vulkan SDK installed with VULKAN_SDK set; GLEW x64 static library at C:\\OpenGL_SDK\\glew (or edit the project paths).
- Open ETS2-TrailerUtil.sln, select Release | x64 and build. Copy only the resulting DLL manually into the game's bin\\win_x64\\plugins directory. No automatic deployment is performed.
- Start ETS2 in DirectX 11 single-player. Check game.log.txt for 'Matched game profile: ETS2 1.62 (experimental)'. If a signature is missing/ambiguous or the hook fails, steering is disabled. Back up your profile and use a disposable save for initial testing.

### Validation pending

Compilation and runtime tests were not performed for this fork. Check menu rendering, no-trailer state, manual slider/axis/button steering, attach/detach, multiple trailers, and joint lock/unlock (PhysX only). Remove the DLL if the game becomes unstable. Do not use in TruckersMP.
