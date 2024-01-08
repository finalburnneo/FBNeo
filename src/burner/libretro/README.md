# FinalBurn Neo

## Background

FinalBurn Neo (also referred to as FBNeo or FBN) is a multi-system emulator (Arcade, consoles and computers) under active development. Unlike MAME it's more focused on playability and advanced features than preservation.
It is the follow-up of the FinalBurn and FinalBurn Alpha emulators.
The libretro core provides wide compatibility with platforms and features supported by libretro.

## Difference from MAME

FBNeo strives for accuracy, just like MAME. There are some arcade boards where one or the other will be more accurate, but for the most part they should be equally accurate.
The main difference with MAME is that FBNeo doesn't mind including "quality of life" hacks, while MAME is about absolute preservation. "Quality of life" hacks include things like :

* improving original game's sound (some games like "Burger Time" have noise which was clearly unintended by their developpers, we are removing it)
* implementing alternative colors for games where the colors don't look right (sometimes there are controversies about which colors are right for an arcade board, like "Tropical Angel", we implement alternative colors as dipswitches)
* having control alternatives that didn't exist on original cabinet (play rotary stick games like twin-stick shooters, use lightguns in "Rambo 3", use simplified 8-way directional controls for "Battlezone", ...)
* improving the gaming experience by cutting what we deem as unnecessary aspect of emulation (you don't have to spend 20 minutes "installing" CPS-3 games, neither 100s loading Deco Cassette games)
* reducing hardware requirements by cutting what we deem as unnecessary corners in the emulation code
* supporting popular romhacks

Note: some of those "quality of life" hacks might be doable with programming skills and lua language on MAME

## License and changelog

It's distributed under a non-commercial license, see [LICENSE.md](https://github.com/finalburnneo/FBNeo/blob/master/LICENSE.md) and [whatsnew.html](https://github.com/finalburnneo/FBNeo/blob/master/whatsnew.html).

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

We don't have a convenient tool like the MAME OSD, instead we use the libretro api to announce buttons and let the frontend customize mapping, this is done through `Quick Menu > Controls`.

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
* SNK Neo-Geo CD : `neocd`
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
* SNK Neo-Geo CD : `neocd`
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

Samples should be put under `SYSTEM_DIRECTORY/fbneo/samples`.

Here is a list of samples currently in use :

* blockade.zip
* buckrog.zip
* carnival.zip
* cheekyms.zip
* congo.zip
* dkongjr.zip
* dkong.zip
* donpachi.zip
* elim2.zip
* fantasy.zip
* galaga.zip
* gaplus.zip
* gridlee.zip
* heiankyo.zip
* invaders.zip
* journey.zip
* mario.zip
* mmagic.zip
* natodef.zip
* nitedrvr.zip
* nsub.zip
* qbert.zip
* radarscp.zip
* rallyx.zip
* reactor.zip
* safarir.zip
* sasuke.zip
* sharkatt.zip
* spacefb.zip
* spacfury.zip
* stinger.zip
* subroc3d.zip
* thehand.zip
* thief.zip
* tr606drumkit.zip
* turbo.zip
* twotiger.zip
* vanguard.zip
* xevious.zip
* zaxxon.zip
* zektor.zip
* zerohour.zip

## Hiscores

Copy [hiscore.dat](https://github.com/libretro/FBNeo/raw/master/metadata/hiscore.dat) to `SYSTEM_DIRECTORY/fbneo/` and have the hiscore core option enabled.

It doesn't guarantee hiscores will work for a specific game though, sometimes a driver could just be missing the necessary support code for this feature, or `hiscore.dat` might have a missing or broken entry for that romset. You can request support in the issue tracker. 

Runahead now works with hiscores, it'll require fairly recent version of the core AND RetroArch though (support was added after 1.10.3).

## Input lag reduction

This core widely supports the RetroArch input latency reduction features, with **runahead single instance** and **preemptive frames** being the recommended methods. 

Proper support for **runahead second instance** is not guaranteed because it doesn't exist in standalone FBNeo unlike the other methods.

## RetroAchievements

This core provides support for RetroAchievements, and some were added for popular games.

## Dipswitches

They are either directly available from `Quick Menu > Core Options`, or from the service menu after setting its shortcut in the `Diagnostic Input` core option.

## Cheats

This core supports the RetroArch cheat feature with the `.cht` files. However it is recommended to use FBNeo's native cheat support instead :

* Download the pack of cheats from [here](https://github.com/finalburnneo/FBNeo-cheats/archive/master.zip)
* Uncompress **all of them** into the `SYSTEM_DIRECTORY/fbneo/cheats/` folder (which is **NOT** the same folder as the RetroArch feature with the `.cht` files)
* Cheats will become available through core options (`Quick Menu > Options`, **NOT** `Quick Menu > Cheats`) afterward.

## Frequently asked questions

### Where can i find the XXX roms ?

As far as we are concerned, you are supposed to dump your own games, so we can't help you with acquiring romsets.

### Why am i getting a white screen ?

Refer to [getting started with arcade emulation](https://docs.libretro.com/guides/arcade-getting-started/#step-3-use-the-correct-version-romsets-for-that-emulator) to understand how romsets work.

The white screen tells you if the romset is supported at all and which files are wrong or missing.
Exceptionally there might be a false positive due to your file being unreadable for some reason.

### How can i run that romhack i found ?

A lot of romhacks are supported natively, so your romhack might already be supported under a specific romset name.

For the unsupported romhacks, you can put the patched version of the romset into `SYSTEM_DIRECTORY/fbneo/patched` (NB: you can strip it of any file that don't differ from non-patched romset if you want), that method will only work if the sizes and names matches with the original romset. 
**The romset you must launch is still the original non-patched romset (its content will be overrided at runtime by the content of the patched one)**, you can disable that behavior by toggling off the `Allow patched romsets` core option.

### How can i run that unibios i bought from http://unibios.free.fr/ ?

Same answer as above.

### I think i found a glitch, how do i report it ?

Write a report [here](https://github.com/finalburnneo/FBNeo/issues) with details on the issue and your platform.
If the issue is not self-explanatory, it is important to provide a video of the PCB (meaning real hardware), any other material (remakes, other emulators, fpga, game rips, ...) will be ignored.
If the issue doesn't happen right from the beginning, please try to provide a savestate from right before the issue.

### Why does game XXX run slowly ?

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

### Why does game XXX have choppy sound ?

Most likely for the same reason as above.

### Why does game XXX run faster in MAME2003/MAME2010 ?

Overall, FBNeo is slower than old MAME version, because it's more accurate, meaning graphics, sound and gameplay are more likely to be faithful to the real machine.
This libretro port also supports various features which are usually buggy or totally missing in MAME cores (runahead, netplay, rewind, retroachievements, ...), those features might require additional resources.

### How do i launch a neogeo CD game ?

There are several things to know :

* You need to follow the instructions about [emulating consoles](#emulating-consoles-and-computers)
* You need a copy of the `neocdz.zip` and `neogeo.zip` bioses
* The supported format is single file MODE1/2352 cue/bin (use "CDmage" to convert your iso if needed), **they must not be compressed**

You can convert your unsupported cd images by following this tutorial :

* Get [CDMage 1.02.1 (beta)](https://www.videohelp.com/software/CDMage) (freeware & no ads). **Don't get CDMage 1.01.5, it doesn't have the "Save As" function**
* File > Open > select your iso (NB : for multi-track, select the .cue file, not the .iso file)
* File > Save As > write the name of your new file
* Make sure you select MODE1/2352 in the second drop-down
* Press OK, wait for the process to finish (a few seconds on my computer), and it’s done !

### Why can't i launch Killer instinct ? I heard it's supported.

That driver was disabled for now, it didn't meet our quality criteria.

### Why are vertical games not working properly ?

2 settings are required when running vertical games in FBNeo :

* `Settings > Core > Allow rotation` must be enabled  (`video_allow_rotate = "true"` in `retroarch.cfg`)
* `Settings > Video > Scaling > Aspect Ratio` should be set to `Core Provided` (`aspect_ratio_index = "22"` in `retroarch.cfg`)

If you are wondering why this isn't required for the MAME core, you can find more information about it [here](https://github.com/libretro/mame/issues/261)

Additionally :

* If you are playing on a vertical screen, you'll want to use the `Vertical Mode` core option to rotate the display for your needs, it should also be possible to rotate display from `Settings > Video > Output > Video Rotation` but that method might handle the aspect ratio incorrectly.
* If you are using a bezel pack, make sure it's compatible with FBNeo (apparently, some were written specifically to work with MAME's internal rotation) and to follow its official instructions. In some case it seems enabling `Settings > On-Screen Display > On-Screen Overlay > Auto-Scale Overlay` (`input_overlay_auto_scale = "true"` in `retroarch.cfg`) can help.

### Why is the music high-pitched, too fast and/or different from upstream ?

For better or worse, it was decided to use different default audio settings from standalone in the libretro port. 
By default standalone has 44100 samplerate and both interpolations off, and that's what you should set in core options if you want the same audio output.

You might also want to make sure you are running the game at the correct speed, most crt games don't run at 60Hz and if you want the proper refresh rate to be emulated you'll need to make sure `Force 60Hz` isn't enabled in core options and `Settings > Video > Synchronization > Sync to Exact Content Framerate` is enabled (`vrr_runloop_enable = "true"` in `retroarch.cfg`). Please note that your screen might not handle well the correct refresh rate, in which case you'll have to make a choice between smoothness and correct refresh rate.

### Why do i get a black screen and/or can't i change bios in neogeo games ?

The `neogeo` romset is a collection of neogeo bioses, and most of them are considered as optional so they won't cause a "white screen" when missing. Only `MVS Asia/Europe ver. 6 (1 slot)` is mandatory.

However, having an incomplete romset can still cause various issues :
* If you are using the "Use bios set in BIOS dipswitch" as "Neo-Geo mode" and the bios set in dipswitches is missing, you'll have a black screen where you can hear some sound playing.
* If you are using any of the other choices available in "Neo-Geo mode" and a corresponding bios can't be found, the core will fallback to one of the available bioses.

Obviously, none of this is supposed to ever happen if you followed the instructions about romsets as you are supposed to.

### Why do i get some weird transparent effects in game XXX ?

You probably installed some `.bld` files in the `SYSTEM_DIRECTORY/fbneo/blend` folder. Those files are meant to create such effects but some of them are very broken. I'd recommend removing them.

### Why is my favorite combo button not available ?

Libretro doesn't allow cores to declare more buttons and map them later, meaning the number of different "actions" available is limited by the number of buttons available on the retropad model.

Removing that limitation was asked in https://github.com/libretro/RetroArch/issues/6718, then again in https://github.com/libretro/RetroArch/issues/11273, it's not possible to add more macros as long as this limitation exists. If you want more macros, go support those issues, preferably the later.

The currently available neogeo combos were decided in https://github.com/libretro/FBNeo/issues/51, they won't be replaced, but they might totally disappear if users keep complaining about them.

Note that there was also a request to add a retroarch macro mapper in https://github.com/libretro/RetroArch/issues/8209.

### Why can't i enable hardcore mode in RetroAchievements ?

This feature doesn't accept achievements made with any kind of cheat, meaning unibios, cheats, and patched romsets must be disabled in core options.

### Why do i need to re-enable cheats every time i boot a game ?

It is common for arcade machines to execute self-tests at boot, and in many cases they won't boot if unexpected values have been injected into their memory, which is exactly what cheats do. Disabling cheats at boot is a safety mecanism to prevent those boot issues.

### Where is SYSTEM_DIRECTORY ?

Open your `retroarch.cfg` file and look for `system_directory`, or check `Settings > Directory > System/BIOS`.

## External Links

- [Official FBNeo forum](https://neo-source.com/)
- [Official FBNeo github repository](https://github.com/finalburnneo/FBNeo)
- [Libretro FBNeo github repository](https://github.com/libretro/FBNeo)
- [[GUIDE] Setting up RetroArch playlists with FBNeo](https://neo-source.com/index.php?topic=3725.0)
- [Gameplay Videos](https://www.youtube.com/playlist?list=PLRbgg4gk_0IfsAHeGqGD-DkRzI87q7V_Q)
