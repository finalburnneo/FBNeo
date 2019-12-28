# FinalBurn Neo
Official Forum: https://neo-source.com

This is the official repository of FinalBurn Neo, an Emulator for Arcade Games & Select Consoles. It is based on the emulators FinalBurn and old versions of [MAME](https://www.mamedev.org)

Use of this program and its source code is subject to the license conditions provided in the [license.txt](/src/license.txt) file in the src folder.

# Work in Progress builds
If the below build status badge is green, you can download the latest builds from [this repository](https://github.com/finalburnneo/FBNeo-WIP-Storage-Facility/releases/tag/appveyor-build). Please note that if the below build status badge is not green then the build will be out of date. As this build is of the last commit occasionally you might run into incomplete code, crashes or other issues that [official releases](https://github.com/finalburnneo/FBNeo/releases) will not have.

[![Build status](https://ci.appveyor.com/api/projects/status/8rkefxtvxd3cllag/branch/master?svg=true)](https://ci.appveyor.com/project/tmaul/fbneo-kbhgd/branch/master)

# Ports

Raspberry Pi [build instructions](README-PI.md).

macOS [build instructions](README-macOS.md) and [releases](https://github.com/fbn-mac/FBNeo/releases).

[LibRetro port](https://github.com/libretro/FBNeo) with builds availble via retroarch for most platforms.

For SDL1.2 builds just type `make sdl` (requires SDL1.2 and GCC, make, perl and nasm)

For SDL2 builds just type `make sdl2` (requires SDL2, SDL2_image, gcc, make, perl and nasm)

# Reporting Issues

Please raise an issue on the [project GitHub](https://github.com/finalburnneo/FBNeo) or report on the forums at [Neosource](https://neo-source.com)

# What about FB Alpha?

Many of the developers of this project also worked on FB Alpha. Due to a [controversy](https://www.google.com/search?q=capcom+home+arcade+illegal&oq=capcom+home+arcade+illegal), we no longer do, and recommend that everyone use this emulator instead.

# Contributing

We welcome pull requests and other submissions from anyone. We maintain a list of known bugs and features that would be nice to add on the [issue tracker](https://github.com/finalburnneo/FBNeo/issues), some of which would be a good starting point for new contributors. 

One of the focuses of FBNeo is ensuring that the codebase is compilable on older systems. This is for many reasons, not least because older hardware still has a use outside of landfill or being stored in a recycling center, but also it can be a lot of fun porting and running FBNeo on PC hardware. Currently, this means we will always aim for [C++03 compliance](https://en.wikipedia.org/wiki/C%2B%2B03) as a minimum. Any pull requests should keep this in mind!

## notes on contributions

In the root of the source tree there is an [.editorconfig](https://editorconfig.org/) that mandates:

* tabs for indentation
* tabs use 4 columns

Please see the following function for some ideas on how naming, brackets and braces should be


```
void FunctionName(UINT8 var1, UINT16 var2)
{
	UINT64 result;
	if (var1 * var2 >= 10) {
		result = var1 * var2;
	} else {
		result = var1;
	}
}

```
## porting

In the main source tree, you will see in the intf directory various implementations for different platforms. You should look in here when porting to new platforms. We also encourage new ports, and are happy to have them merged in to the main sourcetree. There is probably a project there for someone to re-implement some of the older ports using the intf standard, should they want to.


For portability we define the following types
```
unsigned char   UINT8;
signed char     INT8;
unsigned short	UINT16;
signed short	INT16;
unsigned int	UINT32;
signed int      INT32;
signed int64	INT64;
unsigned int64  UINT64;

```

