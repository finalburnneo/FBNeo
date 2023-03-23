# FBNeo-libretro

## How to build

From the root of the repository, run 
```
make -j5 -C src/burner/libretro generate-files
make -j5 -C src/burner/libretro
```
Note : if you need additional parameters, they must be added to both commands.

## Roms

Use clrmamepro (which runs fine on linux with wine and on mac with crossover) to build valid romsets with dats from the [dats](/dats/) directory.
Don't report issues if you didn't build a valid romset.
Also, i don't provide a "parent-only" dat file, because i don't recommend using only parent roms (some don't work, and some clones are really different from their parent)

## Emulating consoles

You can emulate consoles (with specific romsets, dats are also in the [dats](/dats/) directory) by prefixing the name of the roms with `XXX_` and removing the `zip|7z` extension, or using the `--subsystem XXX` argument in the command line, here is the list of available prefixes :
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

It's also possible to use folder's name for detection (this second method was added because some devices aren't compatible with subsystems) :
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

## Samples

Samples should be put under `SYSTEM_DIRECTORY/fbneo/samples`

## Mapping

We don't have a convenient tool like the MAME OSD, instead we use the retroarch api to customize mappings, you can do that by going into `Quick menu > Controls`.
For those who don't want to fully customize their mapping, there are 2 convenient presets you can apply by changing the "device type" for a player in this menu :
* **Classic** : it will apply the original neogeo layout from neogeo cd gamepads for neogeo games, and use L/R as 5th and 6th button for 6 buttons games like Street Fighter II.
* **Modern** : it will apply the modern neogeo layout from neogeo arcade stick pro and mini pad for neogeo games, and use R1/R2 as 5th and 6th button for 6 buttons games like Street Fighter II (because it's also their modern layout), this is really convenient with most arcade sticks.

The following "device type" also exist, but they won't be compatible with every games :
* **Mouse (ball only)** : it will use mouse/trackball for analog movements, buttons will stay on retropad
* **Mouse (full)** : same as above, but the buttons will be on the mouse
* **Pointer** : it will use "pointer" device (can be a mouse/trackball) to determine coordinates on screen, buttons will stay on retropad
* **Lightgun** : it will use lightgun to determine coordinates on screen, buttons will be on the lightgun too.

## Frequently asked questions

### Where can i find the XXX roms ?
As far as we are concerned, you are supposed to dump your own games, so we can't help you with acquiring romsets.

### Game XXX is not launching, why ?
It is either not supported or you have a bad rom, your logs will give you more details. Build a valid romset with clrmamepro as said above.
There is also a few games marked as not working, try one of their clones.

### I patched game XXX and can't run it, why ?
Because it's considered a bad rom since the crcs won't match, however there is a method to use a patched romset, if you put the patched version of the romset into `SYSTEM_DIRECTORY/fbneo/patched` it will work (NB: you can strip it of any file that don't differ from non-patched romset if you want). **The romset you must launch is still the original non-patched romset though (its content will be overrided by the content of the patched one)**, you can disable that override by toggling off the `Allow patched romsets` core option.

### I bought unibios from http://unibios.free.fr/ and can't run it, why ?
Same as above.

### Game XXX has graphical glitches, why ?
Write a report with details on the issue and your platform.

### Game XXX runs slowly, why ?
Your hardware is probably too slow to run the game with normal settings. Try the following :
* Check if there is a speedhack dipswitch in the core options, set it to "yes".
* Try disabling rewind, runahead, or any other retroarch setting known for increasing overhead.
* Try enabling "Threaded Video" in retroarch settings.
* Try changing "Max swapchain images" in retroarch settings (higher values gave a small performance benefit on my setup).
* Try setting a value for frameskip in core options (note : "Fixed" frameskip is recommended, the other methods don't seem to be nearly as reliable).
* Try lowering CPU clock in core options (note : some games don't support this feature).
* Try lowering audio settings in the core options.
* A last resort on arm platforms would be to enable cyclone in core options, however keep in mind some games will stop working from this, so it's recommended to only enable it per-game if it's actually helping with performance on that specific game.
* If it is not enough, upgrade/overclock your hardware, or use another core.

We won't accept requests for "making the core faster", as far as we are concerned this core has a good balance between accuracy & speed, and for the most part will already run really well on cheap arm socs (rpi3, ...).

### Game XXX has choppy sound, why ?
Most likely for the same reason as above.

### Game XXX runs faster in MAME2003/MAME2010, why ?
Overall, FBNeo is slower than old MAME version, because it's more accurate, meaning graphics, sound and gameplay are more likely to be faithful to the real machine.
This libretro port also supports various features which are usually buggy or totally absent in MAME cores (runahead, netplay, rewind, retroachievements, ...), those features might use additional resources.

### How do i use cheat code ?
From a libretro point of view, there should be partial support through the API relying on main ram exposition. 
Otherwise if you put native FBNeo cheat .ini files (there is a pack available [here](https://github.com/finalburnneo/FBNeo-cheats/archive/master.zip)) into `SYSTEM_DIRECTORY/fbneo/cheats`, they will be translated to core options, using a MAME `cheat.dat` file should also work.

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

### Hiscore won't save, why ?
You need to copy [hiscore.dat](/metadata/hiscore.dat) to `SYSTEM_DIRECTORY/fbneo/` and to have the hiscore core option enabled. It doesn't guarantee hiscores will work for a specific game though, sometimes a driver could just be missing the necessary support code for hiscores (or hiscore.dat might not be listing the romset). You can request support in the issue tracker as long as the request is reasonable (avoid making a list of several dozens/hundreds of games if you don't want to be ignored). There were also issues with libretro features preventing hiscores from working properly (like runahead), it should now work properly if your libretro frontend handles `RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT` (you'll get a warning in logs if it doesn't).

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

You might also want to make sure you are running the game at the correct speed, many arcade machines don't run at 60Hz and if you want the proper refresh rate to be emulated you'll need to make sure `Force 60Hz` isn't enabled in core options and `Settings > Video > Synchronization > Sync to Exact Content Framerate` is enabled (`vrr_runloop_enable = "true"` in `retroarch.cfg`). Please note that your screen might not handle well the correct refresh rate, in which case you'll have to make a choice between smoothness and correct refresh rate.

### My favorite combo button is not available, why ?
Retroarch doesn't allow cores to declare buttons and map them later, meaning the number of different "actions" available is limited by the number of buttons available on the retropad model.

Removing that limitation was asked in https://github.com/libretro/RetroArch/issues/6718, then again in https://github.com/libretro/RetroArch/issues/11273, i won't add more macros as long as this limitation hasn't been removed. If you want more macros, go spam those issues, preferably the later.

The currently available neogeo combos were decided in https://github.com/libretro/FBNeo/issues/51, i won't replace them, but i'll seriously consider removing all of them if i keep having users complaining about them every month.

Note that there was also a request to add a retroarch macro mapper in https://github.com/libretro/RetroArch/issues/8209.
