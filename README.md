#Fightcade-fbneo
================

Fightcade emulator built with FBNeo and GGPO.

https://www.fightcade.com

# FinalBurn Neo
Official Forum: https://neo-source.com

# How to Compile
NOTE: Only the Visual Studio 2015 project is able to be compiled.
All the others will be broken due to bad C++ include paths.

1) Download, install and run Visual Studio 2019 IDE Community Edition from https://visualstudio.microsoft.com/.

2) Download and install the 2010 Direct-X SDK from https://www.microsoft.com/en-gb/download/details.aspx?id=6812).
   Make sure that you install it to the default location. The install may also error out at the end, but just ignore it.

3) Go to 'projectfiles\visualstudio-2015' and open 'fbneo_vs2015.sln' in VS2019.
4) Select development with C++ if you are asked.
5) If you are asked to change the C++ toolchain, download and install the one it is asking for.

6) Install the PERL interpreter for Windows. We reccomend ActivePerl (https://www.activestate.com/products/activeperl/downloads/).
   Make sure that you have enabled the checkbox to add PERL to the PATH variable list.
   
7) Install NASM for the assembly files from https://www.nasm.us/.
   You need to put NASM.EXE somewhere in your PATH.
   You can put it in your Windows directory (IE: C:\Windows), but I do not recommend this for a number of reasons.
   
8) Make the files "detector_buffers.h" and "detector_loaders.h" (just empty files) and put them in "src\intf\video\win32".
9) Make sure the build settings are set to "Release, x86".
10) Attempt to compile the project by pressing F6 (or by selecting 'Build' from menu).
11) Install Fightcade from https://www.fightcade.com/.
12) Copy all the DLL's from the Fightcade FBNeo path (IE: 'emulator\fbneo') to the 'Release' directory where 'fcadefbneo.exe' was just compiled to.
13) Try to run 'fcadefbneo.exe' that was compiled in the 'build' directory.