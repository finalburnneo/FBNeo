#===============================================================================
#                           FINAL BURN ALPHA QT
#===============================================================================
QT += widgets multimedia opengl
TARGET = fbaqt

linux:QT += x11extras

#===============================================================================
#                                DRIVERS
#===============================================================================
DRV_CAPCOM      = true
DRV_CAVE        = true
DRV_CPS3        = true
DRV_DATAEAST    = true
DRV_GALAXIAN    = true
DRV_IREM        = true
DRV_KONAMI      = true
DRV_SMS         = true
DRV_MEGADRIVE   = true
DRV_NEOGEO      = true
DRV_PCE         = true
DRV_PGM         = true
DRV_PRE90S      = true
DRV_PST90S      = true
DRV_SEGA        = true
DRV_SNES        = true
DRV_TAITO       = true
DRV_TOAPLAN     = true
DRV_PSIKYO      = true
DRV_MIDWAY      = true

#===============================================================================
#                             DEPENDENCIES
#===============================================================================

#-------------------------------------------------------------------------------
# for Linux, absolute paths
#-------------------------------------------------------------------------------
SRC = $$system(readlink -m $$PWD/../../src)
SCRIPTS = $$SRC/dep/scripts
GEN = $$SRC/dep/generated

# We need ld
FBA_LD = ld
DEFINES += FBA_DEBUG

#-------------------------------------------------------------------------------
# Dynamic recompilers
#-------------------------------------------------------------------------------
DRC_MIPS3_X64   = true

#-------------------------------------------------------------------------------
# Additional include paths
#-------------------------------------------------------------------------------
INCLUDEPATH += \
    $$GEN \
    $$SRC/dep/qtcreator \
    $$SRC/burn \
    $$SRC/burn/snd \
    $$SRC/burn/devices \
    $$SRC/burn/drv/capcom \
    $$SRC/burn/drv/cave \
    $$SRC/burn/drv/konami \
    $$SRC/burn/drv/neogeo \
    $$SRC/burn/drv/pgm \
    $$SRC/burn/drv/pre90s \
    $$SRC/burn/drv/psikyo \
    $$SRC/burn/drv/pst90s \
    $$SRC/burn/drv/sega \
    $$SRC/burn/drv/taito \
    $$SRC/burn/drv/toaplan \
    $$SRC/burner \
    $$SRC/burner/qt \
    $$SRC/cpu \
    $$SRC/cpu/i8039 \
    $$SRC/cpu/konami \
    $$SRC/cpu/m68k \
    $$SRC/intf \
    $$SRC/intf/video \
    $$SRC/intf/video/scalers \
    $$SRC/intf/input \
    $$SRC/intf/audio \
    $$SRC/intf/cd \
    $$SRC/dep/libs \
    $$SRC/dep/libs/libpng \
    $$SRC/dep/libs/zlib \

DEFINES += BUILD_QT \
    LSB_FIRST \
    "__fastcall=" \
    "_fastcall=" \
    WITH_QTCREATOR \
    INCLUDE_LIB_PNGH

linux: DEFINES += BUILD_QT_LINUX
macx: DEFINES += BUILD_QT_MACX

# no warnings...
QMAKE_CXXFLAGS += -w
QMAKE_CFLAGS += -w


#-------------------------------------------------------------------------------
# C++11
#-------------------------------------------------------------------------------
CONFIG += c++11

#-------------------------------------------------------------------------------
# src/dep/generated
#-------------------------------------------------------------------------------
GENERATED.target = $$GEN/empty
GENERATED.commands =                                \
    @echo "Creating generated directory...";        \
    cd $$SRC/dep;                                   \
    mkdir generated;                                \
    touch generated/empty

#-------------------------------------------------------------------------------
# src/dep/generated/ctv.h
#-------------------------------------------------------------------------------
CTV_HEADER.depends = GENERATED
CTV_HEADER.target = $$GEN/ctv.h

CTV_HEADER.commands =                               \
    @echo "Building ctv...";                        \
    cd $$SRC/burn/drv/capcom;                       \
    $$QMAKE_CXX ctv_make.cpp -o ctv_make;           \
    ./ctv_make > $$CTV_HEADER.target;         \
    rm ctv_make

#-------------------------------------------------------------------------------
# perl scripts
#-------------------------------------------------------------------------------

# cave_sprite_func.h
CAVE_SPRFUNC_HEADER.depends = GENERATED
CAVE_SPRFUNC_HEADER.target = $$GEN/cave_sprite_func.h
CAVE_SPRFUNC_HEADER.commands =                                          \
    @echo "Generating cave_sprite_func.h";                              \
    perl $$SCRIPTS/cave_sprite_func.pl -o $$CAVE_SPRFUNC_HEADER.target;

# cave_tile_func.h
CAVE_TILEFUNC_HEADER.depends = GENERATED
CAVE_TILEFUNC_HEADER.target = $$GEN/cave_tile_func.h
CAVE_TILEFUNC_HEADER.commands =                                         \
    @echo "Generating cave_tile_func.h";                                \
    perl $$SCRIPTS/cave_tile_func.pl -o $$CAVE_TILEFUNC_HEADER.target;

# neo_sprite_func.h
NEO_SPRFUNC_HEADER.depends = GENERATED
NEO_SPRFUNC_HEADER.target = $$GEN/neo_sprite_func.h
NEO_SPRFUNC_HEADER.commands =                                         \
    @echo "Generating neo_sprite_func.h";                                \
    perl $$SCRIPTS/neo_sprite_func.pl -o $$NEO_SPRFUNC_HEADER.target;

# psikyo_tile_func.h
PSIKYO_TILEFUNC_HEADER.depends = GENERATED
PSIKYO_TILEFUNC_HEADER.target = $$GEN/psikyo_tile_func.h
PSIKYO_TILEFUNC_HEADER.commands =                                         \
    @echo "Generating psikyo_tile_func.h";                                \
    perl $$SCRIPTS/psikyo_tile_func.pl -o $$PSIKYO_TILEFUNC_HEADER.target;

# toa_gp9001_func
TOA_GP9001_FUNC_HEADER.depends = GENERATED
TOA_GP9001_FUNC_HEADER.target = $$GEN/toa_gp9001_func.h
TOA_GP9001_FUNC_HEADER.commands =                                         \
    @echo "Generating toa_gp9001_func.h";                                \
    perl $$SCRIPTS/toa_gp9001_func.pl -o $$TOA_GP9001_FUNC_HEADER.target;

#-------------------------------------------------------------------------------
# pgm_sprite.h
#-------------------------------------------------------------------------------
PGM_SPRITE_CREATE.depends = GENERATED
PGM_SPRITE_CREATE.target = $$GEN/pgm_sprite_create
PGM_SPRITE_CREATE.commands =\
    @echo "Compiling pgm_sprite_create.cpp";     \
    $$QMAKE_CXX $$SRC/burn/drv/pgm/pgm_sprite_create.cpp -o $$PGM_SPRITE_CREATE.target;

PGM_SPRITE_HEADER.depends = PGM_SPRITE_CREATE
PGM_SPRITE_HEADER.target = $$GEN/pgm_sprite.h
PGM_SPRITE_HEADER.commands =\
    @echo "Generating pgm_sprite.h";     \
    $$PGM_SPRITE_CREATE.target > $$PGM_SPRITE_HEADER.target



#-------------------------------------------------------------------------------
# Musashi68k
#-------------------------------------------------------------------------------
M68K_MAKE.target = $$GEN/m68kmake
M68K_MAKE.depends = $$SRC/cpu/m68k/m68kmake.c GENERATED
M68K_MAKE.commands = \
    @echo "Compiling Musashi MC680x0 core: m68kmake.c...";     \
    $$QMAKE_CC $$SRC/cpu/m68k/m68kmake.c -I$$SRC/burn -o $$M68K_MAKE.target; \
    $$M68K_MAKE.target $$GEN/ $$SRC/cpu/m68k/m68k_in.c;

# objects

M68K_OPS.target = $$GEN/m68kops.o
M68K_OPS.depends = M68K_MAKE
M68K_OPS.commands = \
    @echo "Compiling Musashi MC680x0 core: m68kops.c...";     \
    $$QMAKE_CC $$QMAKE_CFLAGS -I$$SRC/cpu/m68k -I$$SRC/burn $$GEN/m68kops.c -c -o $$M68K_OPS.target;

M68K_OPS_HEADER.target = $$GEN/m68kops.h
M68K_OPS_HEADER.depends = M68K_MAKE
M68K_OPS_HEADER.commands = \
    $$M68K_MAKE.target $$GEN/ $$SRC/cpu/m68k/m68k_in.c;

M68K_LIB.target = $$GEN/libm68kops.o
M68K_LIB.depends = M68K_MAKE M68K_OPS M68K_OPS_HEADER
M68K_LIB.commands = \
    @echo "Partially linking Musashi MC680x0 core: libm68kops.o...";  \
    $$FBA_LD -r $$M68K_OPS.target -o $$M68K_LIB.target


OBJECTS += $$M68K_LIB.target

#-------------------------------------------------------------------------------
# Gamelist
#-------------------------------------------------------------------------------
DRIVERLIST.depends = GENERATED
DRIVERLIST.target = $$GEN/driverlist.h

DRIVERLIST_PATHS =
$$DRV_SMS:DRIVERLIST_PATHS += $$SRC/burn/drv/sms
$$DRV_PCE:DRIVERLIST_PATHS += $$SRC/burn/drv/pce
$$DRV_PGM:DRIVERLIST_PATHS += $$SRC/burn/drv/pgm
$$DRV_CAVE:DRIVERLIST_PATHS += $$SRC/burn/drv/cave
$$DRV_CPS3:DRIVERLIST_PATHS += $$SRC/burn/drv/cps3
$$DRV_IREM:DRIVERLIST_PATHS += $$SRC/burn/drv/irem
$$DRV_SEGA:DRIVERLIST_PATHS += $$SRC/burn/drv/sega
$$DRV_SNES:DRIVERLIST_PATHS += $$SRC/burn/drv/snes
$$DRV_TAITO:DRIVERLIST_PATHS += $$SRC/burn/drv/taito
$$DRV_CAPCOM:DRIVERLIST_PATHS += $$SRC/burn/drv/capcom
$$DRV_NEOGEO:DRIVERLIST_PATHS += $$SRC/burn/drv/neogeo
$$DRV_KONAMI:DRIVERLIST_PATHS += $$SRC/burn/drv/konami
$$DRV_PRE90S:DRIVERLIST_PATHS += $$SRC/burn/drv/pre90s
$$DRV_PST90S:DRIVERLIST_PATHS += $$SRC/burn/drv/pst90s
$$DRV_PSIKYO:DRIVERLIST_PATHS += $$SRC/burn/drv/psikyo
$$DRV_TOAPLAN:DRIVERLIST_PATHS += $$SRC/burn/drv/toaplan
$$DRV_DATAEAST:DRIVERLIST_PATHS += $$SRC/burn/drv/dataeast
$$DRV_GALAXIAN:DRIVERLIST_PATHS += $$SRC/burn/drv/galaxian
$$DRV_MEGADRIVE:DRIVERLIST_PATHS += $$SRC/burn/drv/megadrive
$$DRV_MIDWAY:DRIVERLIST_PATHS += $$SRC/burn/drv/midway

DRIVERLIST.commands = \
    @echo "Generating driverlist.h";                                \
    perl $$SCRIPTS/gamelist.pl -o $$DRIVERLIST.target $$DRIVERLIST_PATHS;

#===============================================================================
#                              OPENGL DEPS
#===============================================================================

LINUX_VIDEO_GLX = true
MACX_VIDEO_CGL = true

linux {

    $${LINUX_VIDEO_GLX} {
        LIBS += -lX11 -lXext -lGLEW -lGL
    }
}

macx {
}


#-------------------------------------------------------------------------------
#-------------------------------------------------------------------------------
QMAKE_EXTRA_TARGETS +=      \
    GENERATED               \
    CTV_HEADER              \
    CAVE_SPRFUNC_HEADER     \
    CAVE_TILEFUNC_HEADER    \
    NEO_SPRFUNC_HEADER      \
    PSIKYO_TILEFUNC_HEADER  \
    PGM_SPRITE_CREATE       \
    PGM_SPRITE_HEADER       \
    TOA_GP9001_FUNC_HEADER  \
    DRIVERLIST              \
    M68K_MAKE               \
    M68K_OPAC               \
    M68K_OPDM               \
    M68K_OPNZ               \
    M68K_OPS                \
    M68K_OPS_HEADER         \
    M68K_LIB

PRE_TARGETDEPS +=                               \
    $$CTV_HEADER.target                         \
    $$CAVE_SPRFUNC_HEADER.target                \
    $$CAVE_TILEFUNC_HEADER.target               \
    $$NEO_SPRFUNC_HEADER.target                 \
    $$PSIKYO_TILEFUNC_HEADER.target             \
    $$PGM_SPRITE_HEADER.target                  \
    $$TOA_GP9001_FUNC_HEADER.target             \
    $$DRIVERLIST.target                         \
    $$M68K_LIB.target                           \

LIBS += -lSDL
linux:LIBS += -lpulse-simple

QMAKE_CLEAN += $$GEN/*
#===============================================================================
#                              CAPCOM DRIVERS
#===============================================================================
$$DRV_CAPCOM {
	message("Capcom drivers enabled")

        HEADERS += $$files(../../src/burn/drv/capcom/*.h)
        SOURCES += $$files(../../src/burn/drv/capcom/*.cpp)
        SOURCES -= ../../src/burn/drv/capcom/ctv_make.cpp
}

#===============================================================================
#                                CAVE DRIVERS
#===============================================================================
$$DRV_CAVE {
	message("Cave drivers enabled")
        HEADERS += $$files(../../src/burn/drv/cave/*.h)
        SOURCES += $$files(../../src/burn/drv/cave/*.cpp)
}

#===============================================================================
#                                CPS3 DRIVERS
#===============================================================================
$$DRV_CPS3 {
	message("CPS3 drivers enabled")

        HEADERS += $$files(../../src/burn/drv/cps3/*.h)
        SOURCES += $$files(../../src/burn/drv/cps3/*.cpp)
}

#===============================================================================
#                              DATAEAST DRIVERS
#===============================================================================
$$DRV_DATAEAST {
	message("Dataeast drivers enabled")

        HEADERS += $$files(../../src/burn/drv/dataeast/*.h)
        SOURCES += $$files(../../src/burn/drv/dataeast/*.cpp)
}

#===============================================================================
#                              GALAXIAN DRIVERS
#===============================================================================
$$DRV_GALAXIAN {
	message("Galaxian drivers enabled")

        HEADERS += $$files(../../src/burn/drv/galaxian/*.h)
        SOURCES += $$files(../../src/burn/drv/galaxian/*.cpp)
}

#===============================================================================
#                                IREM DRIVERS
#===============================================================================
$$DRV_IREM {
	message("Irem drivers enabled")

        HEADERS += $$files(../../src/burn/drv/irem/*.h)
        SOURCES += $$files(../../src/burn/drv/irem/*.cpp)
}

#===============================================================================
#                              KONAMI DRIVERS
#===============================================================================
$$DRV_KONAMI {
	message("Konami drivers enabled")

        HEADERS += $$files(../../src/burn/drv/konami/*.h)
        SOURCES += $$files(../../src/burn/drv/konami/*.cpp)
}

#===============================================================================
#                              MEGADRIVE DRIVERS
#===============================================================================
$$DRV_MEGADRIVE {
	message("Megadrive drivers enabled")

        HEADERS += $$files(../../src/burn/drv/megadrive/*.h)
        SOURCES += $$files(../../src/burn/drv/megadrive/*.cpp)
}

#===============================================================================
#                               NEOGEO DRIVERS
#===============================================================================
$$DRV_NEOGEO {
	message("Neogeo drivers enabled")

        HEADERS += $$files(../../src/burn/drv/neogeo/*.h)
        SOURCES += $$files(../../src/burn/drv/neogeo/*.cpp)
        SOURCES *= ../../src/burner/qt/neocdlist.cpp
}

#===============================================================================
#                              PC-ENGINE DRIVERS
#===============================================================================
$$DRV_PCE {
	message("PC-Engine drivers enabled")

        HEADERS += $$files(../../src/burn/drv/pce/*.h)
        SOURCES += $$files(../../src/burn/drv/pce/*.cpp)
}

#===============================================================================
#                                PGM DRIVERS
#===============================================================================
$$DRV_PGM {
	message("PGM drivers enabled")

        HEADERS += $$files(../../src/burn/drv/pgm/*.h)
        SOURCES += $$files(../../src/burn/drv/pgm/*.cpp)
        SOURCES -= ../../src/burn/drv/pgm/pgm_sprite_create.cpp
}

#===============================================================================
#                                PRE90S DRIVERS
#===============================================================================
$$DRV_PRE90S {
	message("Pre90s drivers enabled")

        HEADERS += $$files(../../src/burn/drv/pre90s/*.h)
        SOURCES += $$files(../../src/burn/drv/pre90s/*.cpp)

        HEADERS *= ../../src/burn/drv/sega/mc8123.h
        SOURCES *= ../../src/burn/drv/sega/mc8123.cpp
        # CAPCOM deps...
        HEADERS *= $$files(../../src/burn/drv/capcom/*.h)
        SOURCES *= $$files(../../src/burn/drv/capcom/*.cpp)
        SOURCES -= ../../src/burn/drv/capcom/ctv_make.cpp
        # KONAMI deps...
        HEADERS *= $$files(../../src/burn/drv/konami/*.h)
        SOURCES *= $$files(../../src/burn/drv/konami/k*.cpp)

}


#===============================================================================
#                              PSIKYO DRIVERS
#===============================================================================
$$DRV_PSIKYO {
	message("Psikyo drivers enabled")

        HEADERS += $$files(../../src/burn/drv/psikyo/*.h)
        SOURCES += $$files(../../src/burn/drv/psikyo/*.cpp)
}


#===============================================================================
#                                PST90S DRIVERS
#===============================================================================
$$DRV_PST90S {
	message("Pst90s drivers enabled")
        HEADERS += $$files(../../src/burn/drv/pst90s/*.h)
        SOURCES += $$files(../../src/burn/drv/pst90s/*.cpp)
}

#===============================================================================
#                                SEGA DRIVERS
#===============================================================================
$$DRV_SEGA {
	message("Sega drivers enabled")

        HEADERS *= $$files(../../src/burn/drv/sega/*.h)
        SOURCES *= $$files(../../src/burn/drv/sega/*.cpp)
}

#===============================================================================
#                            MASTERSYSTEM DRIVERS
#===============================================================================
$$DRV_SMS {
	message("SMS drivers enabled")

        HEADERS += $$files(../../src/burn/drv/sms/*.h)
        SOURCES += $$files(../../src/burn/drv/sms/*.cpp)
}

#===============================================================================
#                               SNES DRIVERS
#===============================================================================
$$DRV_SNES {
	message("SNES drivers enabled")

        HEADERS += $$files(../../src/burn/drv/snes/*.h)
        SOURCES += $$files(../../src/burn/drv/snes/*.cpp)
}


#===============================================================================
#                                TAITO DRIVERS
#===============================================================================
$$DRV_TAITO {
	message("Taito drivers enabled")

        HEADERS += $$files(../../src/burn/drv/taito/*.h)
        SOURCES += $$files(../../src/burn/drv/taito/*.cpp)
}


#===============================================================================
#                               TOAPLAN DRIVERS
#===============================================================================
$$DRV_TOAPLAN {
	message("Toaplan drivers enabled")

        HEADERS += $$files(../../src/burn/drv/toaplan/*.h)
        SOURCES += $$files(../../src/burn/drv/toaplan/*.cpp)
}

#===============================================================================
#                                MIDWAY DRIVERS
#===============================================================================
$$DRV_MIDWAY {
        message("Midway drivers enabled")
        HEADERS += $$files(../../src/burn/drv/midway/*.h)
        SOURCES += $$files(../../src/burn/drv/midway/*.cpp)
}

SOURCES += \
    ../../src/burn/devices/8255ppi.cpp \
    ../../src/burn/devices/8257dma.cpp \
    ../../src/burn/devices/eeprom.cpp \
    ../../src/burn/devices/pandora.cpp \
    ../../src/burn/devices/seibusnd.cpp \
    ../../src/burn/devices/sknsspr.cpp \
    ../../src/burn/devices/slapstic.cpp \
    ../../src/burn/devices/t5182.cpp \
    ../../src/burn/devices/timekpr.cpp \
    ../../src/burn/devices/tms34061.cpp \
    ../../src/burn/devices/v3021.cpp \
    ../../src/burn/devices/vdc.cpp \
    ../../src/burn/snd/burn_y8950.cpp \
    ../../src/burn/snd/burn_ym2151.cpp \
    ../../src/burn/snd/burn_ym2203.cpp \
    ../../src/burn/snd/burn_ym2413.cpp \
    ../../src/burn/snd/burn_ym2608.cpp \
    ../../src/burn/snd/burn_ym2610.cpp \
    ../../src/burn/snd/burn_ym2612.cpp \
    ../../src/burn/snd/burn_ym3526.cpp \
    ../../src/burn/snd/burn_ym3812.cpp \
    ../../src/burn/snd/burn_ymf278b.cpp \
    ../../src/burn/snd/c6280.cpp \
    ../../src/burn/snd/dac.cpp \
    ../../src/burn/snd/es5506.cpp \
    ../../src/burn/snd/es8712.cpp \
    ../../src/burn/snd/flt_rc.cpp \
    ../../src/burn/snd/ics2115.cpp \
    ../../src/burn/snd/iremga20.cpp \
    ../../src/burn/snd/k005289.cpp \
    ../../src/burn/snd/k007232.cpp \
    ../../src/burn/snd/k051649.cpp \
    ../../src/burn/snd/k053260.cpp \
    ../../src/burn/snd/k054539.cpp \
    ../../src/burn/snd/msm5205.cpp \
    ../../src/burn/snd/msm5232.cpp \
    ../../src/burn/snd/msm6295.cpp \
    ../../src/burn/snd/namco_snd.cpp \
    ../../src/burn/snd/nes_apu.cpp \
    ../../src/burn/snd/rf5c68.cpp \
    ../../src/burn/snd/saa1099.cpp \
    ../../src/burn/snd/samples.cpp \
    ../../src/burn/snd/segapcm.cpp \
    ../../src/burn/snd/sn76496.cpp \
    ../../src/burn/snd/upd7759.cpp \
    ../../src/burn/snd/vlm5030.cpp \
    ../../src/burn/snd/x1010.cpp \
    ../../src/burn/snd/ymz280b.cpp \
    ../../src/burn/snd/ay8910.c \
    ../../src/burn/snd/fm.c \
    ../../src/burn/snd/fmopl.c \
    ../../src/burn/snd/ym2151.c \
    ../../src/burn/snd/ym2413.c \
    ../../src/burn/snd/ymdeltat.c \
    ../../src/burn/snd/ymf278b.c \
    ../../src/burn/snd/pokey.cpp \
    ../../src/burn/burn_sound.cpp \
    ../../src/burn/burn.cpp \
    ../../src/burn/cheat.cpp \
    ../../src/burn/debug_track.cpp \
    ../../src/burn/hiscore.cpp \
    ../../src/burn/load.cpp \
    ../../src/burn/tiles_generic.cpp \
    ../../src/burn/timer.cpp \
    ../../src/burn/vector.cpp \
    ../../src/burn/burn_sound_c.cpp \
    ../../src/burn/burn_memory.cpp \
    ../../src/burn/burn_led.cpp \
    ../../src/burn/burn_gun.cpp \
    ../../src/cpu/hd6309_intf.cpp \
    ../../src/cpu/konami_intf.cpp \
    ../../src/cpu/m6502_intf.cpp \
    ../../src/cpu/m6800_intf.cpp \
    ../../src/cpu/m6805_intf.cpp \
    ../../src/cpu/m6809_intf.cpp \
    ../../src/cpu/m68000_intf.cpp \
    ../../src/cpu/nec_intf.cpp \
    ../../src/cpu/pic16c5x_intf.cpp \
    ../../src/cpu/s2650_intf.cpp \
    ../../src/cpu/h6280_intf.cpp \
    ../../src/cpu/arm7_intf.cpp \
    ../../src/cpu/arm_intf.cpp \
    ../../src/cpu/arm/arm.cpp \
    ../../src/cpu/arm7/arm7.cpp \
    ../../src/cpu/arm7/arm7core.c \
    ../../src/cpu/arm7/arm7exec.c \
    ../../src/cpu/h6280/h6280.cpp \
    ../../src/cpu/h6280/tblh6280.c \
    ../../src/cpu/hd6309/hd6309.cpp \
    ../../src/cpu/hd6309/6309ops.c \
    ../../src/cpu/hd6309/6309tbl.c \
    ../../src/cpu/i8039/i8039.cpp \
    ../../src/cpu/konami/konami.cpp \
    ../../src/cpu/konami/konamops.c \
    ../../src/cpu/konami/konamtbl.c \
    ../../src/cpu/m6502/m6502.cpp \
    ../../src/cpu/m6502/t65c02.c \
    ../../src/cpu/m6502/t65sc02.c \
    ../../src/cpu/m6502/t6502.c \
    ../../src/cpu/m6502/tdeco16.c \
    ../../src/cpu/m6502/tn2a03.c \
    ../../src/cpu/m6800/m6800.cpp \
    ../../src/cpu/m6800/6800ops.c \
    ../../src/cpu/m6800/6800tbl.c \
    ../../src/cpu/m6805/m6805.cpp \
    ../../src/cpu/m6805/6805ops.c \
    ../../src/cpu/m6809/m6809.cpp \
    ../../src/cpu/m6809/6809ops.c \
    ../../src/cpu/m6809/6809tbl.c \
    ../../src/cpu/nec/nec.cpp \
    ../../src/cpu/nec/v25.cpp \
    ../../src/cpu/nec/necinstr.c \
    ../../src/cpu/nec/v25instr.c \
    ../../src/cpu/nec/v25sfr.c \
    ../../src/cpu/pic16c5x/pic16c5x.cpp \
    ../../src/cpu/s2650/s2650.cpp \
    ../../src/cpu/sh2/sh2.cpp \
    ../../src/cpu/z80/z80.cpp \
    ../../src/cpu/z80/z80daisy.cpp \
    ../../src/cpu/m68k/m68kcpu.c \
    ../../src/burner/conc.cpp \
    ../../src/burner/cong.cpp \
    ../../src/burner/dat.cpp \
    ../../src/burner/gamc.cpp \
    ../../src/burner/gami.cpp \
    ../../src/burner/image.cpp \
    ../../src/burner/misc.cpp \
    ../../src/burner/sshot.cpp \
    ../../src/burner/state.cpp \
    ../../src/burner/statec.cpp \
    ../../src/burner/zipfn.cpp \
    ../../src/burner/ioapi.c \
    ../../src/burner/unzip.c \
    ../../src/dep/libs/libpng/png.c \
    ../../src/dep/libs/libpng/pngerror.c \
    ../../src/dep/libs/libpng/pngget.c \
    ../../src/dep/libs/libpng/pngmem.c \
    ../../src/dep/libs/libpng/pngpread.c \
    ../../src/dep/libs/libpng/pngread.c \
    ../../src/dep/libs/libpng/pngrio.c \
    ../../src/dep/libs/libpng/pngrtran.c \
    ../../src/dep/libs/libpng/pngrutil.c \
    ../../src/dep/libs/libpng/pngset.c \
    ../../src/dep/libs/libpng/pngtrans.c \
    ../../src/dep/libs/libpng/pngwio.c \
    ../../src/dep/libs/libpng/pngwrite.c \
    ../../src/dep/libs/libpng/pngwtran.c \
    ../../src/dep/libs/libpng/pngwutil.c \
    ../../src/intf/interface.cpp \
    ../../src/intf/audio/aud_dsp.cpp \
    ../../src/intf/audio/aud_interface.cpp \
    ../../src/intf/audio/lowpass2.cpp \
    ../../src/intf/cd/cd_interface.cpp \
    ../../src/intf/input/inp_interface.cpp \
    ../../src/intf/video/vid_interface.cpp \
    ../../src/intf/video/vid_support.cpp \
    ../../src/cpu/z80_intf.cpp \
    ../../src/burner/qt/main.cpp \
    ../../src/burner/qt/aboutdialog.cpp \
    ../../src/burner/qt/bzip.cpp \
    ../../src/burner/qt/dipswitchdialog.cpp \
    ../../src/burner/qt/driver.cpp \
    ../../src/burner/qt/emuworker.cpp \
    ../../src/burner/qt/mainwindow.cpp \
    ../../src/burner/qt/progress.cpp \
    ../../src/burner/qt/qaudiointerface.cpp \
    ../../src/burner/qt/qinputinterface.cpp \
    ../../src/burner/qt/qutil.cpp \
    ../../src/burner/qt/romdirsdialog.cpp \
    ../../src/burner/qt/rominfodialog.cpp \
    ../../src/burner/qt/romscandialog.cpp \
    ../../src/burner/qt/selectdialog.cpp \
    ../../src/burner/qt/stringset.cpp \
    ../../src/burner/qt/supportdirsdialog.cpp \
    ../../src/dep/libs/zlib/adler32.c \
    ../../src/dep/libs/zlib/compress.c \
    ../../src/dep/libs/zlib/crc32.c \
    ../../src/dep/libs/zlib/deflate.c \
    ../../src/dep/libs/zlib/gzclose.c \
    ../../src/dep/libs/zlib/gzlib.c \
    ../../src/dep/libs/zlib/gzread.c \
    ../../src/dep/libs/zlib/gzwrite.c \
    ../../src/dep/libs/zlib/infback.c \
    ../../src/dep/libs/zlib/inffast.c \
    ../../src/dep/libs/zlib/inflate.c \
    ../../src/dep/libs/zlib/inftrees.c \
    ../../src/dep/libs/zlib/trees.c \
    ../../src/dep/libs/zlib/uncompr.c \
    ../../src/dep/libs/zlib/zutil.c \
    ../../src/burn/devices/nmk004.cpp \
    ../../src/cpu/tlcs90/tlcs90.cpp \
    ../../src/cpu/tlcs90_intf.cpp \
    ../../src/burn/devices/kaneko_tmap.cpp \
    ../../src/burner/qt/inputdialog.cpp \
    ../../src/burner/qt/widgets/hexspinbox.cpp \
    ../../src/burner/qt/logdialog.cpp \
    ../../src/burner/qt/inputsetdialog.cpp \
    ../../src/burner/qt/oglviewport.cpp \
    ../../src/intf/video/opengl/vid_opengl.cpp \
    ../../src/intf/video/opengl/shader.cpp  \
    ../../src/cpu/mips3/cop0.cpp \
    ../../src/cpu/mips3/cop1.cpp \
    ../../src/cpu/mips3/mips3_dasm.cpp \
    ../../src/cpu/mips3/mips3.cpp \
    ../../src/cpu/mips3_intf.cpp \
    ../../src/cpu/adsp2100_intf.cpp \
    ../../src/cpu/adsp2100/adsp2100.cpp \
    ../../src/cpu/adsp2100/2100dasm.cpp



HEADERS += \
    ../../src/burn/devices/8255ppi.h \
    ../../src/burn/devices/8257dma.h \
    ../../src/burn/devices/eeprom.h \
    ../../src/burn/devices/pandora.h \
    ../../src/burn/devices/seibusnd.h \
    ../../src/burn/devices/sknsspr.h \
    ../../src/burn/devices/slapstic.h \
    ../../src/burn/devices/t5182.h \
    ../../src/burn/devices/timekpr.h \
    ../../src/burn/devices/tms34061.h \
    ../../src/burn/devices/v3021.h \
    ../../src/burn/devices/vdc.h \
    ../../src/burn/snd/ay8910.h \
    ../../src/burn/snd/burn_y8950.h \
    ../../src/burn/snd/burn_ym2151.h \
    ../../src/burn/snd/burn_ym2203.h \
    ../../src/burn/snd/burn_ym2413.h \
    ../../src/burn/snd/burn_ym2608.h \
    ../../src/burn/snd/burn_ym2610.h \
    ../../src/burn/snd/burn_ym2612.h \
    ../../src/burn/snd/burn_ym3526.h \
    ../../src/burn/snd/burn_ym3812.h \
    ../../src/burn/snd/burn_ymf278b.h \
    ../../src/burn/snd/c6280.h \
    ../../src/burn/snd/dac.h \
    ../../src/burn/snd/es5506.h \
    ../../src/burn/snd/es8712.h \
    ../../src/burn/snd/flt_rc.h \
    ../../src/burn/snd/fm.h \
    ../../src/burn/snd/fmopl.h \
    ../../src/burn/snd/ics2115.h \
    ../../src/burn/snd/iremga20.h \
    ../../src/burn/snd/k005289.h \
    ../../src/burn/snd/k007232.h \
    ../../src/burn/snd/k051649.h \
    ../../src/burn/snd/k053260.h \
    ../../src/burn/snd/k054539.h \
    ../../src/burn/snd/msm5205.h \
    ../../src/burn/snd/msm5232.h \
    ../../src/burn/snd/msm6295.h \
    ../../src/burn/snd/namco_snd.h \
    ../../src/burn/snd/nes_apu.h \
    ../../src/burn/snd/nes_defs.h \
    ../../src/burn/snd/rescap.h \
    ../../src/burn/snd/rf5c68.h \
    ../../src/burn/snd/saa1099.h \
    ../../src/burn/snd/samples.h \
    ../../src/burn/snd/segapcm.h \
    ../../src/burn/snd/sn76496.h \
    ../../src/burn/snd/upd7759.h \
    ../../src/burn/snd/vlm5030.h \
    ../../src/burn/snd/x1010.h \
    ../../src/burn/snd/ym2151.h \
    ../../src/burn/snd/ym2413.h \
    ../../src/burn/snd/ymdeltat.h \
    ../../src/burn/snd/ymf278b.h \
    ../../src/burn/snd/ymz280b.h \
    ../../src/burn/snd/pokey.h \
    ../../src/burn/burn_sound.h \
    ../../src/burn/burn.h \
    ../../src/burn/burnint.h \
    ../../src/burn/cheat.h \
    ../../src/burn/driver.h \
    ../../src/burn/hiscore.h \
    ../../src/burn/state.h \
    ../../src/burn/stdfunc.h \
    ../../src/burn/tiles_generic.h \
    ../../src/burn/timer.h \
    ../../src/burn/vector.h \
    ../../src/burn/version.h \
    ../../src/burn/burn_led.h \
    ../../src/burn/burn_gun.h \
    ../../src/burn/bitswap.h \
    ../../src/cpu/h6280_intf.h \
    ../../src/cpu/hd6309_intf.h \
    ../../src/cpu/konami_intf.h \
    ../../src/cpu/m6502_intf.h \
    ../../src/cpu/m6800_intf.h \
    ../../src/cpu/m6805_intf.h \
    ../../src/cpu/m6809_intf.h \
    ../../src/cpu/m68000_debug.h \
    ../../src/cpu/m68000_intf.h \
    ../../src/cpu/nec_intf.h \
    ../../src/cpu/pic16c5x_intf.h \
    ../../src/cpu/s2650_intf.h \
    ../../src/cpu/arm7_intf.h \
    ../../src/cpu/arm_intf.h \
    ../../src/cpu/arm7/arm7core.h \
    ../../src/cpu/h6280/h6280.h \
    ../../src/cpu/h6280/h6280ops.h \
    ../../src/cpu/hd6309/hd6309.h \
    ../../src/cpu/i8039/i8039.h \
    ../../src/cpu/konami/konami.h \
    ../../src/cpu/m6502/ill02.h \
    ../../src/cpu/m6502/m6502.h \
    ../../src/cpu/m6502/ops02.h \
    ../../src/cpu/m6502/opsc02.h \
    ../../src/cpu/m6502/opsn2a03.h \
    ../../src/cpu/m6800/m6800.h \
    ../../src/cpu/m6805/m6805.h \
    ../../src/cpu/m6809/m6809.h \
    ../../src/cpu/nec/nec.h \
    ../../src/cpu/nec/necea.h \
    ../../src/cpu/nec/necinstr.h \
    ../../src/cpu/nec/necmacro.h \
    ../../src/cpu/nec/necmodrm.h \
    ../../src/cpu/nec/necpriv.h \
    ../../src/cpu/nec/v25instr.h \
    ../../src/cpu/nec/v25priv.h \
    ../../src/cpu/pic16c5x/pic16c5x.h \
    ../../src/cpu/s2650/s2650.h \
    ../../src/cpu/z80/z80.h \
    ../../src/cpu/z80/z80daisy.h \
    ../../src/cpu/m68k/m68kcpu.h \
    ../../src/cpu/m68k/m68kconf.h \
    ../../src/burner/burner.h \
    ../../src/burner/gameinp.h \
    ../../src/burner/ioapi.h \
    ../../src/burner/neocdlist.h \
    ../../src/burner/title.h \
    ../../src/burner/unzip.h \
    ../../src/dep/libs/libpng/png.h \
    ../../src/dep/libs/libpng/pngconf.h \
    ../../src/dep/libs/libpng/pngdebug.h \
    ../../src/dep/libs/libpng/pnginfo.h \
    ../../src/dep/libs/libpng/pnglibconf.h \
    ../../src/dep/libs/libpng/pngpriv.h \
    ../../src/dep/libs/libpng/pngstruct.h \
    ../../src/intf/interface.h \
    ../../src/intf/audio/aud_dsp.h \
    ../../src/intf/audio/lowpass2.h \
    ../../src/intf/cd/cd_interface.h \
    ../../src/intf/input/inp_keys.h \
    ../../src/intf/video/vid_support.h \
    ../../src/cpu/sh2_intf.h \
    ../../src/cpu/z80_intf.h \
    ../../src/burner/qt/aboutdialog.h \
    ../../src/burner/qt/burner_qt.h \
    ../../src/burner/qt/dipswitchdialog.h \
    ../../src/burner/qt/emuworker.h \
    ../../src/burner/qt/mainwindow.h \
    ../../src/burner/qt/qaudiointerface.h \
    ../../src/burner/qt/qinputinterface.h \
    ../../src/burner/qt/qutil.h \
    ../../src/burner/qt/romdirsdialog.h \
    ../../src/burner/qt/rominfodialog.h \
    ../../src/burner/qt/romscandialog.h \
    ../../src/burner/qt/selectdialog.h \
    ../../src/burner/qt/supportdirsdialog.h \
    ../../src/burner/qt/tchar.h \
    ../../src/dep/libs/zlib/crc32.h \
    ../../src/dep/libs/zlib/deflate.h \
    ../../src/dep/libs/zlib/gzguts.h \
    ../../src/dep/libs/zlib/inffast.h \
    ../../src/dep/libs/zlib/inffixed.h \
    ../../src/dep/libs/zlib/inflate.h \
    ../../src/dep/libs/zlib/inftrees.h \
    ../../src/dep/libs/zlib/trees.h \
    ../../src/dep/libs/zlib/zconf.h \
    ../../src/dep/libs/zlib/zconf.h.in \
    ../../src/dep/libs/zlib/zlib.h \
    ../../src/dep/libs/zlib/zutil.h \
    ../../src/burn/devices/nmk004.h \
    ../../src/burn/devices/kaneko_tmap.h \
    ../../src/burner/qt/inputdialog.h \
    ../../src/burner/qt/widgets/hexspinbox.h \
    ../../src/burner/qt/logdialog.h \
    ../../src/burner/qt/inputsetdialog.h \
    ../../src/burner/qt/oglviewport.h \
    ../../src/intf/video/opengl/shader.h \
    ../../src/cpu/mips3/mips3_common.h \
    ../../src/cpu/mips3/mips3_memory.h \
    ../../src/cpu/mips3/mips3.h \
    ../../src/cpu/mips3/mips3_arithm.h \
    ../../src/cpu/mips3/mips3_bitops.h \
    ../../src/cpu/mips3/mips3_branch.h \
    ../../src/cpu/mips3/mips3_misc.h \
    ../../src/cpu/mips3/mips3_rw.h \
    ../../src/cpu/mips3/mips3_shift.h \
    ../../src/cpu/mips3/mipsdef.h \
    ../../src/cpu/mips3_intf.h \
    ../../src/cpu/adsp2100_intf.h \
    ../../src/cpu/adsp2100/adsp2100.h \
    ../../src/cpu/adsp2100/adsp2100_defs.h \
    ../../src/cpu/adsp2100/cpuintrf.h


#-------------------------------------------------------------------------------
# MIPS3 x64 recompiler
#-------------------------------------------------------------------------------

$$DRC_MIPS3_X64 {
        message("MIPS3 x64 dynarec enabled")
        DEFINES += \
            XBYAK_NO_OP_NAMES \
            MIPS3_X64_DRC

        HEADERS += \
            ../../src/cpu/mips3/x64/mips3_x64.h \
            ../../src/cpu/mips3/x64/mips3_x64_arithm.h \
            ../../src/cpu/mips3/x64/mips3_x64_bitops.h \
            ../../src/cpu/mips3/x64/mips3_x64_branch.h \
            ../../src/cpu/mips3/x64/mips3_x64_cop0.h \
            ../../src/cpu/mips3/x64/mips3_x64_cop1.h \
            ../../src/cpu/mips3/x64/mips3_x64_defs.h \
            ../../src/cpu/mips3/x64/mips3_x64_misc.h \
            ../../src/cpu/mips3/x64/mips3_x64_rw.h \
            ../../src/cpu/mips3/x64/mips3_x64_shift.h \
            ../../src/cpu/mips3/x64/xbyak/xbyak.h \
            ../../src/cpu/mips3/x64/xbyak/xbyak_bin2hex.h \
            ../../src/cpu/mips3/x64/xbyak/xbyak_mnemonic.h \
            ../../src/cpu/mips3/x64/xbyak/xbyak_util.h

        SOURCES += \
            ../../src/cpu/mips3/x64/mips3_x64.cpp
}


#-------------------------------------------------------------------------------
# Linux only drivers
#-------------------------------------------------------------------------------
linux: HEADERS += ../../src/intf/audio/linux/ringbuffer.h
linux: SOURCES += ../../src/intf/audio/linux/aud_pulse_simple.cpp

OTHER_FILES +=

RESOURCES += \
    ../../src/burner/qt/rscr.qrc

FORMS += \
    ../../src/burner/qt/aboutdialog.ui \
    ../../src/burner/qt/dipswitchdialog.ui \
    ../../src/burner/qt/romdirsdialog.ui \
    ../../src/burner/qt/rominfodialog.ui \
    ../../src/burner/qt/romscandialog.ui \
    ../../src/burner/qt/selectdialog.ui \
    ../../src/burner/qt/supportdirsdialog.ui \
    ../../src/burner/qt/inputdialog.ui \
    ../../src/burner/qt/logdialog.ui \
    ../../src/burner/qt/inputsetdialog.ui \
