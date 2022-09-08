## Fightcade-fbneo
Fightcade emulator built with FBNeo and GGPO
Public Release: https://www.fightcade.com

## FinalBurn Neo
Official Forum: https://neo-source.com

## How to Compile
NOTE: You will need at least VS2015 to build FightcadeFBNeo.

**1)** Download, install and run Visual Studio IDE Community Edition from https://visualstudio.microsoft.com/.

**2)** Download and install the 2010 DirectX SDK from https://www.microsoft.com/en-gb/download/details.aspx?id=6812). Make sure that you install it to the default location. The installer may also error out at the end, you can ignore it.

**3)** Go to '*projectfiles\visualstudio-2015*' and open '*fbneo_vs2015.sln*' in Visual Studio.

**4)** Select development with C++ if you are asked.

**5)** If you are asked to change the C++ toolchain, download and install the one it is asking for.

**6)** Install the PERL interpreter for Windows. We reccomend ActivePerl (https://www.activestate.com/products/activeperl/downloads/).
Make sure that you have enabled the checkbox to add PERL to the PATH variable list.

**7)** Install NASM for the assembly files from https://www.nasm.us/.
You need to put NASM.EXE somewhere in your PATH. You can put it in your Windows directory (IE: *'C:\Windows*'), but I do not recommend this for a number of reasons.

**8)** Open a command line shell and run the '*games.bat*' script. This will populate the games list, do this if you add a new game and want it to appear on the internal list and test it.

**9)** Make sure the build settings are set to '*Release, x86*'. Debug, x86 configuration also builds correctly, but is intended for developers and debugging sessions.

**10)** Build using '*Build / Build Solution*' Menu (or using keyboard shortcuts, depending on your setup, F6, F7 or Ctrl+Shift+B).

**11)** Install Fightcade from https://www.fightcade.com/.

**12)** Copy all the DLL's from the Fightcade FBNeo path (IE: '*emulator\fbneo*') to the '*build*' directory where '*fcadefbneo.exe*' was just compiled to.

**13)** Run '*fcadefbneo.exe*' that was compiled in the 'build' directory, or you can use '*Debug*' it from Visual Studio, it will launch it from that folder (debug version is '*fcadefbneod.exe*'

**14)** If you want to test a new detector, put it in '*build\detector*'.
