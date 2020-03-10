# FBNeo-libretro

## How to build

From the root of the repository, run `make -j5 -C src/burner/libretro`

## Roms

Use clrmamepro (which runs fine on linux with wine and on mac with crossover) to build valid romsets with dats from the [dats](/dats/) directory.
Don't report issues if you didn't build a valid romset.
Also, i don't provide a "parent-only" dat file, because i don't recommend using only parent roms (some don't work, and some clones are really different from their parent)

## Emulating consoles

You can emulate consoles (with specific romsets, dats are also in the [dats](/dats/) directory) by prefixing the name of the roms with `XXX_` and removing the `zip|7z` extension, or using the `--subsystem XXX` argument in the command line, here is the list of available prefixes :
* CBS ColecoVision : `cv`
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
* ZX Spectrum : `spec`

It's also possible to use folder's name for detection (this second method was added because some devices aren't compatible with subsystems) :
* CBS ColecoVision : `coleco`
* MSX 1 : `msx`
* Nec PC-Engine : `pce`
* Nec SuperGrafX : `sgx`
* Nec TurboGrafx-16 : `tg16`
* Nintendo Entertainment System : `nes`
* Nintendo Family Disk System : `fds`
* Sega GameGear : `gamegear`
* Sega Master System : `sms`
* Sega Megadrive : `megadriv`
* Sega SG-1000 : `sg1000`
* ZX Spectrum : `spectrum`

## Samples

Samples should be put under `SYSTEM_DIRECTORY/fbneo/samples`

## hiscore.dat

Copy [hiscore.dat](/metadata/hiscore.dat) to `SYSTEM_DIRECTORY/fbneo/`

## Mapping

We don't have a convenient tool like the MAME OSD, instead we use the retroarch api to customize mappings, you can do that by going into `Quick menu > Controls`.
For those who don't want to fully customize their mapping, there are 2 convenient presets you can apply by changing the "device type" for a player in this menu :
* **Classic** : it will apply the original neogeo cd "square" mapping in neogeo games, and use L/R as 5th and 6th button for 6 buttons games like Street Fighter II.
* **Modern** : it will apply the King of fighters mapping from Playstation 1 and above in neogeo fighting games, and it will use R1/R2 as 5th and 6th button for 6 buttons games like Street Fighter II (for the same reason as the neogeo games), this is really convenient for most arcade sticks.

The following "device type" also exist, but they won't be compatible with every games :
* **Mouse (ball only)** : it will use mouse/trackball for analog movements, buttons will stay on retropad
* **Mouse (full)** : same as above, but the buttons will be on the mouse
* **Pointer** : it will use "pointer" device (can be a mouse/trackball) to determine coordinates on screen, buttons will stay on retropad
* **Lightgun** : it will use lightgun to determine coordinates on screen, buttons will be on the lightgun too.

## Frequently asked questions

### Where can i find the XXX roms ?
We don't provide links for roms. Google is your friend.

### Game XXX is not launching, why ?
It is either not supported or you have a bad rom. Build a valid romset with clrmamepro as said above.
There is also a few games marked as not working, try one of their clones.

### Game XXX has graphical glitches, why ?
Write a report with details on the issue and your platform.

### Game XXX runs slowly, why ?
Your hardware is probably too slow to run the game with normal settings. Try the following :
* Check if there is a speedhack dipswitch in the core options, set it to "yes".
* Try setting a value for frameskip in core options.
* Try lowering CPU clock in core options
* Try disabling rewind, runahead, or any other retroarch setting known for increasing overhead.
* Try lowering audio settings in the core options.
* If it is not enough, upgrade/overclock your hardware, or use another core.

We won't accept requests for "making the core faster", as far as we are concerned this core has a good balance between accuracy & speed, and for the most part will already run really well on cheap arm socs (rpi3, ...).

### Game XXX has choppy sound, why ?
Most likely for the same reason as above.

### Game XXX runs faster in MAME2003/MAME2010, why ?
This is not MAME, we are generally using a more "up-to-date" code. 
Overall, FB Alpha is slower than old MAME version but more accurate and less buggy.
This libretro port also support various features which are usually buggy or absent in MAME cores (netplay, rewind, retroachievements, ...). It takes some resources.

### Cheat code doesn't work, why ?
There should be partial support through the new API relying on main ram exposition.

### Neogeo CD doesn't work, why ?
There are several things to know :
* You need a copy of neocdz.zip and neogeo.zip in your libretro system directory
* You need to add `--subsystem neocd` to the command line, or to place your games in a `neocd` folder
* Supported format are ccd/sub/img (trurip), and single file MODE1/2352 bin/cue (use utilities like "CDmage" to convert your iso if needed), **they must not be compressed**

You can convert your unsupported isos by following this tutorial :
* Get [CDMage 1.02.1](https://www.videohelp.com/software/CDMage) (freeware & no ads)
* File > Open > select your iso (NB : for multi-track, select the .cue file, not the .iso file)
* File > Save As > write the name of your new file
* Make sure you select MODE1/2352 in the second drop-down
* Press OK, wait for the process to finish (a few seconds on my computer), and it’s done !

### Killer instinct won't work, why ?
There are several things to know :
* It is only running at playable speed on x86_64 (other arch will basically need a cpu at 4Ghz because they lack a mips3 dynarec), and the core needs to be built like this to enable this dynarec : `make -j5 -C src/burner/libretro USE_X64_DRC=1`
* If your rom is at `ROM_DIRECTORY/kinst.zip`, you'll need the uncompressed disc image at `ROM_DIRECTORY/kinst/kinst.img`
* To get the uncompressed disc image, you'll need to use the chdman tool from MAME on the chd from mame, the command looks like this : `chdman extracthd -i kinst.chd -o kinst.img`

### Hiscore won't work, why ?
Having hiscore.dat and the core option enabled doesn't guarantee hiscores will work for a specific game, sometimes a driver will be missing the necessary code. You can request support in the issue tracker as long as the request is reasonable (i.e avoid making a list of several dozens/hundreds of games)

### On my android device, game XXX is closing my app at launch, why ?
Android OS has one of the worst features ever created : it kills an app if it considers it's taking too long to display something, no question asked ! Furthermore 2 devices will have 2 different time limit (how the time limit is calculated is a mystery). There is basically nothing we can do about this : some android devices won't be able to run the bigger romsets because loading the zip into memory is taking too long for them.
