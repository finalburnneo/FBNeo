#	Main Makefile for FBNeo, execute an appropriate system-specific makefile

export

#
#	Declare variables
#

# Make a special build, pass the quoted text as comment (use FORCE_UPDATE declaration below to force recompilation of resources)
# SPECIALBUILD = "This text will appear in the property sheet of the .exe file"



#
#	Flags. Uncomment any of these declarations to enable their function.
#

# Include Unicode support
UNICODE = 1

# Build A68K ASM 68000 core
#BUILD_A68K = 1

# Include x86 Assembly routines
BUILD_X86_ASM = 1

# Include GCC optmisations for your CPU e.g use -march=native. WARNING: This might mean that the generated binaries will not run on other peoples (older) machines!
#BUILD_NATIVE = 1

# Build for x64 targets (MinGW64 and MSVC only, this will undefine BUILD_A68K and BUILD_X86_ASM)
#BUILD_X64_EXE = 1

# Build for Windows XP target (for use with Visual Studio 2012-15)
#BUILD_VS_XP_TARGET = 1

# Include 7-zip support
INCLUDE_7Z_SUPPORT = 1

# Include AVI recording support (uses Video For Windows)
INCLUDE_AVI_RECORDING = 1

# Include symbols and other debug information in the executable
#SYMBOL = 1

# Include features for debugging drivers unless we are doing a release build
ifndef RELEASEBUILD
DEBUG	= 1
endif

# Include rom set verifying features (comment this for release builds)
#ROM_VERIFY = 1

# Force recompilation of files that need it (i.e. use __TIME__, __DATE__, SPECIALBUILD).
FORCE_UPDATE = 1

# Use the __fastcall calling convention when interfacing with A68K/Musashi/Doze
FASTCALL = 1

# Compress executable with upx (the DEBUG option ignores this)
# COMPRESS = 1

# Perl is available
PERL = 1

# Endianness
LSB_FIRST = 1

# Include png.h from burner.h
INCLUDE_LIB_PNGH = 1

# Enable CRT resolution switching
# INCLUDE_SWITCHRES = 1

#
#	execute an appropriate system-specific makefile
#

mingw345: FORCE
	@$(MAKE) -s -f makefile.mingw GCC345=1

mingw452: FORCE
	@$(MAKE) -s -f makefile.mingw GCC452=1

mingw471: FORCE
	@$(MAKE) -s -f makefile.mingw GCC471=1

mingw510: FORCE
	@$(MAKE) -s -f makefile.mingw GCC510=1
#cross compile in linux only 32 bit for now
mingwcc: FORCE
	@$(MAKE) -s -f makefile.mingw GCC510=1 CC=i686-w64-mingw32-gcc CXX=i686-w64-mingw32-g++-win32  WINDRES=i686-w64-mingw32-windres

mamemingw: FORCE
	@$(MAKE) -s -f makefile.mamemingw

sdl: FORCE
	@$(MAKE) -s -f makefile.sdl

sdl2: FORCE
	@$(MAKE) -s -f makefile.sdl2

vc: FORCE
	@$(MAKE) -s -f makefile.vc

pi: FORCE
	@$(MAKE) -s -f makefile.pi

FORCE:
