# FinalBurn Neo for macOS

## Compatibility
Emulator has been tested successfully on High Sierra and above, though it's very
likely that it may run on earlier OS's (perhaps as low as Mountain Lion). If you
test successfully on an earlier version of macOS (OS X?), please let me know.

## Use
Launch any ROM set by dropping it on the app's icon or the window. You can also
use `File/Open` and `File/Open Recent`. ROMs can reside anywhere, though if you
load a game that requires other sets (e.g. Neo-Geo), the supplementary ROMs are
expected to be in the same directory. Supported archives are `zip` and `7z`.

Error display is fairly rudimentary at the moment - if you're wondering why a
set failed to load, launch the macOS Console app and filter by app name.

## Input
Only input device currently supported is the keyboard, using standard FinalBurn layout.

## Known issues
* CPS3 titles - and perhaps others - will not render at the moment. There's sound
  and the keys work, but no video. This must be fixed.
* Joysticks/mice are currently unsupported
* Error display is exceptionally spartan at the moment, and needs improvement
