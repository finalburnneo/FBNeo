# FinalBurn Neo

## Background

FinalBurn Neo (also referred to as FBNeo or FBN) is a multi-system emulator (Arcade, consoles and computers) under active development. Unlike MAME it's more focused on playability and advanced features than preservation.
It is the follow-up of FinalBurn and FinalBurn Alpha emulators.
The libretro core provides wide compatibility with platforms and features supported by libretro.

For the most part, FBNeo is just as accurate as current MAME, occasionally we even find out a few games are more faithful to the pcb videos. The main difference with MAME is that FBNeo doesn't mind including "quality of life" hacks, while MAME is about absolute preservation. "Quality of life" hacks include things like improving original game's sound, having control alternatives that didn't exist on original cabinet, or dramatically reducing hardware requirements by cutting what we deem as unnecessary corners in the emulation code.

## License and changelog

It's distributed under non-commercial license, see [LICENSE.md](https://github.com/finalburnneo/FBNeo/blob/master/LICENSE.md) and [whatsnew.html](https://github.com/finalburnneo/FBNeo/blob/master/whatsnew.html).

## Extensions

zip, 7z

## Building this core manually

From the root of the repository, run 
```
make -j5 -C src/burner/libretro generate-files
make -j5 -C src/burner/libretro
```
Note : `-j5` is to optimize build time on cpus with 4 cores, you can rise or reduce that value to match your own, however a value too high will increase ram usage and might even cause your system to become instable.

Note : if you need additional parameters, they must be added to both commands.

## Building romsets for FBNeo

Arcade emulation won't work properly without the romsets matching the emulator. FBNeo being an emulator under active development, a given romset might change from time to time to stay in sync with the best dump available for that game. **All of this is to offer you the best gaming experience possible, because older bad dumps can prevent the game from working as it should**.

Don't expect things to work properly if you didn't build valid romsets, and don't report issues because your romsets are invalid.

### Step 1: Obtaining an XML DAT

You can download the dat files for the latest version of the core from the [dats](https://github.com/libretro/FBNeo/tree/master/dats/) directory. Note that some devices (Nintendo 3DS) are running a "light" build with fewer supported games due to memory limitation, the dat files for that build are available from the [light](https://github.com/libretro/FBNeo/tree/master/dats/light/) subdirectory.

### Step 2: Gathering the ingredients

It mostly consists of latest dumps available for MAME.
The other romsets are usually a mix of hacks and homebrews, most of them can be found in HBMAME dumps.
Having an older FBAlpha/FBNeo set among your ingredients will also help a lot.

### Step 3: Building the romsets

Refer to a [clrmamepro tutorial](https://docs.libretro.com/guides/arcade-getting-started/#optional-clrmamepro-tutorial) for details on how to configure ClrMamePro to use your sources as "rebuild" folders.

## Features

| Feature           | Supported |
|-------------------|-----------|
| Saves             | ✔         |
| States            | ✔         |
| Rewind            | ✔         |
| Run-Ahead         | ✔         |
| Preemptive Frames | ✔         |
| Netplay           | ✔         |
| RetroAchievements | ✔         |
| RetroArch Cheats  | ✔         |
| Native Cheats     | ✔         |
| Controllers       | ✔         |
| Multi-Mouse       | ✔         |
| Rumble            | ✕         |
| Sensors           | ✕         |
| Camera            | ✕         |
| Location          | ✕         |
| Subsystem         | ✔         |

## Mapping

We don't have a convenient tool like the MAME OSD, instead we use the retroarch api to customize mappings, you can do that by going into `Quick Menu > Controls`.
For those who don't want to fully customize their mapping, there are 2 convenient presets you can apply by changing the "device type" for a player in this menu :
* **Classic** : it will apply the original neogeo layout from neogeo cd gamepads for neogeo games, and use L/R as 5th and 6th button for 6 buttons games like Street Fighter II.
* **Modern** : it will apply the modern neogeo layout from neogeo arcade stick pro and mini pad for neogeo games, and use R1/R2 as 5th and 6th button for 6 buttons games like Street Fighter II (because it's also their modern layout), this is really convenient with most arcade sticks.

The following "device type" also exist, but they won't be compatible with every games :
* **Mouse (ball only)** : it will use mouse/trackball for analog movements, buttons will stay on retropad
* **Mouse (full)** : same as above, but the buttons will be on the mouse
* **Pointer** : it will use "pointer" device (can be a mouse/trackball) to determine coordinates on screen, buttons will stay on retropad
* **Lightgun** : it will use lightgun to determine coordinates on screen, buttons will be on the lightgun too.

## Emulating consoles and computers

It also requires usage of specific romsets, meaning the rom must have the expected crc/size, and be packaged in an archive with a specific name (the instructions to build those romsets don't differ from arcade's).

You can use specific folder's name for detection, it's the easiest and recommended method, especially if you are using RetroArch playlists or if your device is not compatible with subsystems (android and consoles) :
* CBS ColecoVision : `coleco` | `colecovision`
* Fairchild ChannelF : `chf` | `channelf`
* MSX 1 : `msx` | `msx1`
* Nec PC-Engine : `pce` | `pcengine`
* Nec SuperGrafX : `sgx` | `supergrafx`
* Nec TurboGrafx-16 : `tg16`
* Nintendo Entertainment System : `nes`
* Nintendo Family Disk System : `fds`
* Sega GameGear : `gamegear`
* Sega Master System : `sms` | `mastersystem`
* Sega Megadrive : `megadriv` | `megadrive` | `genesis`
* Sega SG-1000 : `sg1000`
* SNK Neo-Geo Pocket : `ngp`
* ZX Spectrum : `spectrum` | `zxspectrum`

You can also emulate consoles by prefixing the name of the roms with `XXX_` and removing the `zip|7z` extension in the command line, or adding the `--subsystem XXX` argument, here is the list of available prefixes :
* CBS ColecoVision : `cv`
* Fairchild ChannelF : `chf`
* MSX 1 : `msx`
* Nec PC-Engine : `pce`
* Nec SuperGrafX : `sgx`
* Nec TurboGrafx-16 : `tg`
* Nintendo Entertainment System : `nes`
* Nintendo Family Disk System : `fds`
* Sega GameGear : `gg`
* Sega Master System : `sms`
* Sega Megadrive : `md`
* Sega SG-1000 : `sg1k`
* SNK Neo-Geo Pocket : `ngp`
* ZX Spectrum : `spec`

## BIOS

Bioses will be searched through 3 folders :
* the folder of the current romset
* the `SYSTEM_DIRECTORY/fbneo/` folder
* the `SYSTEM_DIRECTORY/` folder

The following bioses are required for some of the emulated systems :
* neogeo.zip (Neo Geo BIOS)
* neocdz.zip (Neo Geo CDZ System BIOS)
* decocass.zip (DECO Cassette System BIOS)
* isgsm.zip (ISG Selection Master Type 2006 System BIOS)
* midssio.zip (Midway SSIO Sound Board Internal ROM)
* nmk004.zip (NMK004 Internal ROM)
* pgm.zip (PGM System BIOS)
* skns.zip (Super Kaneko Nova System BIOS)
* ym2608.zip (YM2608 Internal ROM)
* cchip.zip (C-Chip Internal ROM)
* bubsys.zip (Bubble System BIOS)
* namcoc69.zip (Namco C69 BIOS)
* namcoc70.zip (Namco C70 BIOS)
* namcoc75.zip (Namco C75 BIOS)
* coleco.zip (ColecoVision System BIOS)
* fdsbios.zip (FDS System BIOS)
* msx.zip (MSX1 System BIOS)
* ngp.zip (NeoGeo Pocket BIOS)
* spectrum.zip (ZX Spectrum BIOS)
* spec128.zip (ZX Spectrum 128 BIOS)
* spec1282a.zip (ZX Spectrum 128 +2a BIOS)
* channelf.zip (Fairchild Channel F BIOS)

## Samples

Samples should be put under `SYSTEM_DIRECTORY/fbneo/samples`

## Hiscores

Copy [hiscore.dat](https://github.com/libretro/FBNeo/tree/master/metadata/hiscore.dat) to `SYSTEM_DIRECTORY/fbneo/` and have the hiscore core option enabled. It doesn't guarantee hiscores will work for a specific game though, sometimes a driver could just be missing the necessary support code for hiscores, or `hiscore.dat` might have a missing or broken entry for that romset. You can request support in the issue tracker. Runahead now works with hiscores, it'll require fairly recent version of the core AND RetroArch though (support was added after 1.10.3 and is still only available through nightlies as of 2022-09-05).

## Run Ahead input lag reduction

This core widely supports the RetroArch "Run Ahead" input latency reduction feature, with **single instance** being the recommended method. Support for `Second Instance` won't be guaranteed anymore as of [2022-06-25](https://github.com/libretro/FBNeo/commit/7ea5708565955658eeaf49da2be4a9905409bb35).

## RetroAchievements

This core provides support for RetroAchievements, and some were added for popular games.

## Dipswitches

They are either directly available from `Quick Menu > Core Options`, or from the service menu after setting its shortcut in the `Diagnostic Input` core option.

## Cheats

You can either use the RetroArch cheat feature, or download a pack of FBNeo native cheats from [here](https://github.com/finalburnneo/FBNeo-cheats/archive/master.zip) and uncompress them into the `SYSTEM_DIRECTORY/fbneo/cheats/` folder, then they'll become available through core options (`Quick Menu > Options`, **NOT** `Quick Menu > Cheats`).

## Frequently asked questions

### Where can i find the XXX roms ?
As far as we are concerned, you are supposed to dump your own games, so we can't help you with acquiring romsets.

### Game XXX is not launching, why ?
It is either not supported or you have a bad rom, your logs will give you more details. Build a valid romset with clrmamepro as said above.
There are also a few games marked as not working, try one of their clones.

### I patched game XXX and can't run it, why ?
Because it's considered a bad rom since the crcs won't match, however there is a method to use a patched romset, if you put the patched version of the romset into `SYSTEM_DIRECTORY/fbneo/patched` it will work (NB: you can strip it of any file that don't differ from non-patched romset if you want). **The romset you must launch is still the original non-patched romset though (its content will be overrided by the content of the patched one)**, you can disable that override by toggling off the `Allow patched romsets` core option.

### I bought unibios from http://unibios.free.fr/ and can't run it, why ?
Same answer as above.

### Game XXX has graphical glitches, why ?
Write a report with details on the issue and your platform.

### Game XXX runs slowly, why ?
Your hardware is probably too slow to run the game with your current settings. Try the following :
* Check if there is a speedhack dipswitch in the core options, set it to "yes".
* Try disabling rewind, runahead, pre-emptive frames, shaders, or any other retroarch setting known for increasing requirements.
* Try enabling "Threaded Video" in retroarch settings.
* Try changing "Max swapchain images" in retroarch settings (higher values gave a small performance benefit on my setup).
* Try setting a value for frameskip in core options (note : "Fixed" frameskip is recommended, the other methods don't seem to be nearly as reliable).
* Try lowering CPU clock in core options (note : some games don't support this feature).
* Try lowering audio settings in the core options.
* With m68k games (most boards from the late 80s and early 90s) on arm platforms, you can try enabling cyclone in core options, however this is really a last resort since some games won't work properly with this, furthermore it's causing savestates incompatibilities.
* If it is not enough, upgrade/overclock your hardware, or use another core.

We won't accept requests for "making the core faster", as far as we are concerned this core has a good balance between accuracy & speed, and for the most part will already run really well on low-end devices (rpi3, ...).

### Game XXX has choppy sound, why ?
Most likely for the same reason as above.

### Game XXX runs faster in MAME2003/MAME2010, why ?
Overall, FBNeo is slower than old MAME version, because it's more accurate, meaning graphics, sound and gameplay are more likely to be faithful to the real machine.
This libretro port also supports various features which are usually buggy or totally absent in MAME cores (runahead, netplay, rewind, retroachievements, ...), those features might use additional resources.

### Neogeo CD doesn't work, why ?
There are several things to know :
* You need a copy of neocdz.zip and neogeo.zip in your libretro system directory
* You need to add `--subsystem neocd` to the command line, or to place your games in a `neocd` folder
* Supported format are ccd/sub/img (trurip), and single file MODE1/2352 cue/bin (use utilities like "CDmage" to convert your iso if needed), **they must not be compressed**

You can convert your unsupported isos by following this tutorial :
* Get [CDMage 1.02.1 (beta)](https://www.videohelp.com/software/CDMage) (freeware & no ads). **Don't get CDMage 1.01.5, it doesn't have the "Save As" function**
* File > Open > select your iso (NB : for multi-track, select the .cue file, not the .iso file)
* File > Save As > write the name of your new file
* Make sure you select MODE1/2352 in the second drop-down
* Press OK, wait for the process to finish (a few seconds on my computer), and it’s done !

### Killer instinct won't work, why ?
That driver was disabled for now, it doesn't meet our quality criteria.

~~There are several things to know :~~
* ~~It is only running at playable speed on x86_64 (other arch will basically need a cpu at 4Ghz because they lack a mips3 dynarec), and the core needs to be built like this to enable this dynarec : `make -j5 -C src/burner/libretro USE_X64_DRC=1`~~
* ~~If your rom is at `ROM_DIRECTORY/kinst.zip`, you'll need the uncompressed disc image at `ROM_DIRECTORY/kinst/kinst.img`~~
* ~~To get the uncompressed disc image, you'll need to use the chdman tool from MAME on the chd from mame, the command looks like this : `chdman extracthd -i kinst.chd -o kinst.img`~~

### Vertical games don't work properly, why ?
2 settings are required when running vertical games in FBNeo :
* `Settings > Core > Allow rotation` must be enabled  (`video_allow_rotate = "true"` in `retroarch.cfg`)
* `Settings > Video > Scaling > Aspect Ratio` should be set to `Core Provided` (`aspect_ratio_index = "22"` in `retroarch.cfg`)

If you are wondering why this isn't required for the MAME core, you can find more information about it [here](https://github.com/libretro/mame/issues/261)

Additionally :
* If you are playing on a vertical screen, you'll want to use the `Vertical Mode` core option to rotate the display for your needs, while it's also possible to rotate display from `Settings > Video > Output > Video Rotation`, that method is not recommended because it doesn't rotate the aspect ratio.
* If you are using a bezel pack, it seems you need to enable `Settings > On-Screen Display > On-Screen Overlay > Auto-Scale Overlay` (`input_overlay_auto_scale = "true"` in `retroarch.cfg`)

### Music is high-pitched, too fast and/or different from FBNeo standalone, why ?
For better or worse, it was decided to use different default audio settings from standalone in the libretro port. By default standalone has 44100 samplerate and both interpolations off and that's what you should set in libretro's core options if you want the same audio output.

You might also want to make sure you are running the game at the correct speed, most crt games don't run at 60Hz and if you want the proper refresh rate to be emulated you'll need to make sure `Force 60Hz` isn't enabled in core options and `Settings > Video > Synchronization > Sync to Exact Content Framerate` is enabled (`vrr_runloop_enable = "true"` in `retroarch.cfg`). Please note that your screen might not handle well the correct refresh rate, in which case you'll have to make a choice between smoothness and correct refresh rate.

### I have a black screen in neogeo games, why ?
Most likely because you have an incomplete `neogeo` romset and you changed the bios in core options for some reason. `MVS Asia/Europe ver. 6 (1 slot)` is the default bios and the only one that will cause a "white screen of death" if missing from your `neogeo` romset. If you select another neogeo bios while not having the corresponding file, you won't get the "white screen of death", instead you'll have a black screen with some sound playing. This issue will only happen if you didn't follow the instructions about romsets.

## External Links

- [Official FBNeo forum](https://neo-source.com/)
- [Official FBNeo github repository](https://github.com/finalburnneo/FBNeo)
- [Libretro FBNeo github repository](https://github.com/libretro/FBNeo)
- [[GUIDE] Setting up RetroArch playlists with FBNeo](https://neo-source.com/index.php?topic=3725.0)
- [Gameplay Videos](https://www.youtube.com/playlist?list=PLRbgg4gk_0IfsAHeGqGD-DkRzI87q7V_Q)
