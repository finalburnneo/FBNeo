#===============================================================================
#                           FINAL BURN ALPHA QT
#===============================================================================
TARGET = fbasdl

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
DRV_TAITO       = true
DRV_TOAPLAN     = true
DRV_PSIKYO      = true

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
    $$SRC/burn/drv/taito \
    $$SRC/burn/drv/toaplan \
    $$SRC/burner \
    $$SRC/burner/sdl \
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
    $$SRC/dep/libs/libpng \
    $$SRC/dep/libs/zlib \

DEFINES += BUILD_SDL \
    LSB_FIRST \
    "__fastcall=" \
    "_fastcall=" \
    WITH_QTCREATOR \
    INCLUDE_LIB_PNGH

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
    $$QMAKE_CC $$SRC/cpu/m68k/m68kmake.c -o $$M68K_MAKE.target; \
    $$M68K_MAKE.target $$GEN/ $$SRC/cpu/m68k/m68k_in.c;

# objects
M68K_OPAC.target = $$GEN/m68kopac.o
M68K_OPAC.depends = M68K_MAKE
M68K_OPAC.commands = \
    @echo "Compiling Musashi MC680x0 core: m68kopac.c...";     \
    $$QMAKE_CC $$QMAKE_CFLAGS -I$$SRC/cpu/m68k $$GEN/m68kopac.c -c -o $$M68K_OPAC.target;

M68K_OPDM.target = $$GEN/m68kopdm.o
M68K_OPDM.depends = M68K_MAKE
M68K_OPDM.commands = \
    @echo "Compiling Musashi MC680x0 core: m68kopdm.c...";     \
    $$QMAKE_CC $$QMAKE_CFLAGS -I$$SRC/cpu/m68k $$GEN/m68kopdm.c -c -o $$M68K_OPDM.target;

M68K_OPNZ.target = $$GEN/m68kopnz.o
M68K_OPNZ.depends = M68K_MAKE
M68K_OPNZ.commands = \
    @echo "Compiling Musashi MC680x0 core: m68kopnz.c...";     \
    $$QMAKE_CC $$QMAKE_CFLAGS -I$$SRC/cpu/m68k $$GEN/m68kopnz.c -c -o $$M68K_OPNZ.target;

M68K_OPS.target = $$GEN/m68kops.o
M68K_OPS.depends = M68K_MAKE
M68K_OPS.commands = \
    @echo "Compiling Musashi MC680x0 core: m68kops.c...";     \
    $$QMAKE_CC $$QMAKE_CFLAGS -I$$SRC/cpu/m68k $$GEN/m68kops.c -c -o $$M68K_OPS.target;

M68K_OPS_HEADER.target = $$GEN/m68kops.h
M68K_OPS_HEADER.depends = M68K_MAKE
M68K_OPS_HEADER.commands = \
    $$M68K_MAKE.target $$GEN/ $$SRC/cpu/m68k/m68k_in.c;

M68K_LIB.target = $$GEN/libm68kops.o
M68K_LIB.depends = M68K_MAKE M68K_OPAC M68K_OPDM M68K_OPNZ M68K_OPS M68K_OPS_HEADER
M68K_LIB.commands = \
    @echo "Partially linking Musashi MC680x0 core: libm68kops.o...";  \
    $$FBA_LD -r $$M68K_OPAC.target $$M68K_OPDM.target \
                $$M68K_OPNZ.target $$M68K_OPS.target -o $$M68K_LIB.target


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

DRIVERLIST.commands = \
    @echo "Generating driverlist.h";                                \
    perl $$SCRIPTS/gamelist.pl -o $$DRIVERLIST.target $$DRIVERLIST_PATHS;

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

QMAKE_CLEAN += $$GEN/*
#===============================================================================
#                              CAPCOM DRIVERS
#===============================================================================
$$DRV_CAPCOM {
	message("Capcom drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/capcom/cps.h \
	    ../../src/burn/drv/capcom/ctv_do.h
	
	SOURCES += \
	    ../../src/burn/drv/capcom/cps_config.cpp \
	    ../../src/burn/drv/capcom/cps_draw.cpp \
	    ../../src/burn/drv/capcom/cps_mem.cpp \
	    ../../src/burn/drv/capcom/cps_obj.cpp \
	    ../../src/burn/drv/capcom/cps_pal.cpp \
	    ../../src/burn/drv/capcom/cps_run.cpp \
	    ../../src/burn/drv/capcom/cps_rw.cpp \
	    ../../src/burn/drv/capcom/cps_scr.cpp \
	    ../../src/burn/drv/capcom/cps.cpp \
	    ../../src/burn/drv/capcom/cps2_crpt.cpp \
	    ../../src/burn/drv/capcom/cpsr.cpp \
	    ../../src/burn/drv/capcom/cpsrd.cpp \
	    ../../src/burn/drv/capcom/cpst.cpp \
	    ../../src/burn/drv/capcom/ctv.cpp \
	    ../../src/burn/drv/capcom/d_cps1.cpp \
	    ../../src/burn/drv/capcom/d_cps2.cpp \
	    ../../src/burn/drv/capcom/fcrash_snd.cpp \
	    ../../src/burn/drv/capcom/kabuki.cpp \
	    ../../src/burn/drv/capcom/ps_m.cpp \
	    ../../src/burn/drv/capcom/ps_z.cpp \
	    ../../src/burn/drv/capcom/ps.cpp \
	    ../../src/burn/drv/capcom/qs_c.cpp \
	    ../../src/burn/drv/capcom/qs_z.cpp \
	    ../../src/burn/drv/capcom/qs.cpp \
	    ../../src/burn/drv/capcom/sf2mdt_snd.cpp
}

#===============================================================================
#                                CAVE DRIVERS
#===============================================================================
$$DRV_CAVE {
	message("Cave drivers enabled")
	HEADERS += \
	    ../../src/burn/drv/cave/cave_sprite_render_zoom.h \
	    ../../src/burn/drv/cave/cave_sprite_render.h \
	    ../../src/burn/drv/cave/cave_tile_render.h \
	    ../../src/burn/drv/cave/cave.h
	
	SOURCES += \
	    ../../src/burn/drv/cave/cave_palette.cpp \
	    ../../src/burn/drv/cave/cave_sprite.cpp \
	    ../../src/burn/drv/cave/cave_tile.cpp \
	    ../../src/burn/drv/cave/cave.cpp \
	    ../../src/burn/drv/cave/d_dodonpachi.cpp \
	    ../../src/burn/drv/cave/d_donpachi.cpp \
	    ../../src/burn/drv/cave/d_esprade.cpp \
	    ../../src/burn/drv/cave/d_feversos.cpp \
	    ../../src/burn/drv/cave/d_gaia.cpp \
	    ../../src/burn/drv/cave/d_guwange.cpp \
	    ../../src/burn/drv/cave/d_hotdogst.cpp \
	    ../../src/burn/drv/cave/d_korokoro.cpp \
	    ../../src/burn/drv/cave/d_mazinger.cpp \
	    ../../src/burn/drv/cave/d_metmqstr.cpp \
	    ../../src/burn/drv/cave/d_pwrinst2.cpp \
	    ../../src/burn/drv/cave/d_sailormn.cpp \
	    ../../src/burn/drv/cave/d_tjumpman.cpp \
	    ../../src/burn/drv/cave/d_uopoko.cpp
}

#===============================================================================
#                                CPS3 DRIVERS
#===============================================================================
$$DRV_CPS3 {
	message("CPS3 drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/cps3/cps3.h
	
	SOURCES += \
	    ../../src/burn/drv/cps3/cps3run.cpp \
	    ../../src/burn/drv/cps3/cps3snd.cpp \
	    ../../src/burn/drv/cps3/d_cps3.cpp
}

#===============================================================================
#                              DATAEAST DRIVERS
#===============================================================================
$$DRV_DATAEAST {
	message("Dataeast drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/dataeast/deco16ic.h
	
	SOURCES += \
	    ../../src/burn/drv/dataeast/d_actfancr.cpp \
	    ../../src/burn/drv/dataeast/d_backfire.cpp \
	    ../../src/burn/drv/dataeast/d_boogwing.cpp \
	    ../../src/burn/drv/dataeast/d_cbuster.cpp \
	    ../../src/burn/drv/dataeast/d_cninja.cpp \
	    ../../src/burn/drv/dataeast/d_darkseal.cpp \
	    ../../src/burn/drv/dataeast/d_dassault.cpp \
	    ../../src/burn/drv/dataeast/d_dec0.cpp \
	    ../../src/burn/drv/dataeast/d_dec8.cpp \
	    ../../src/burn/drv/dataeast/d_dietgogo.cpp \
	    ../../src/burn/drv/dataeast/d_funkyjet.cpp \
	    ../../src/burn/drv/dataeast/d_karnov.cpp \
	    ../../src/burn/drv/dataeast/d_lemmings.cpp \
	    ../../src/burn/drv/dataeast/d_pktgaldx.cpp \
	    ../../src/burn/drv/dataeast/d_rohga.cpp \
	    ../../src/burn/drv/dataeast/d_sidepckt.cpp \
	    ../../src/burn/drv/dataeast/d_simpl156.cpp \
	    ../../src/burn/drv/dataeast/d_supbtime.cpp \
	    ../../src/burn/drv/dataeast/d_tumblep.cpp \
	    ../../src/burn/drv/dataeast/d_vaportra.cpp \
	    ../../src/burn/drv/dataeast/deco16ic.cpp
}

#===============================================================================
#                              GALAXIAN DRIVERS
#===============================================================================
$$DRV_GALAXIAN {
	message("Galaxian drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/galaxian/gal.h
	
	SOURCES += \
	    ../../src/burn/drv/galaxian/d_galaxian.cpp \
	    ../../src/burn/drv/galaxian/gal_gfx.cpp \
	    ../../src/burn/drv/galaxian/gal_run.cpp \
	    ../../src/burn/drv/galaxian/gal_sound.cpp \
	    ../../src/burn/drv/galaxian/gal_stars.cpp
}

#===============================================================================
#                                IREM DRIVERS
#===============================================================================
$$DRV_IREM {
	message("Irem drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/irem/irem_cpu.h
	
	SOURCES += \
	    ../../src/burn/drv/irem/d_m62.cpp \
	    ../../src/burn/drv/irem/d_m63.cpp \
	    ../../src/burn/drv/irem/d_m72.cpp \
	    ../../src/burn/drv/irem/d_m90.cpp \
	    ../../src/burn/drv/irem/d_m92.cpp \
	    ../../src/burn/drv/irem/d_m107.cpp \
	    ../../src/burn/drv/irem/d_vigilant.cpp \
	    ../../src/burn/drv/irem/irem_cpu.cpp
}

#===============================================================================
#                              KONAMI DRIVERS
#===============================================================================
$$DRV_KONAMI {
	message("Konami drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/konami/konamiic.h
	
	SOURCES += \
	    ../../src/burn/drv/konami/d_88games.cpp \
	    ../../src/burn/drv/konami/d_ajax.cpp \
	    ../../src/burn/drv/konami/d_aliens.cpp \
	    ../../src/burn/drv/konami/d_blockhl.cpp \
	    ../../src/burn/drv/konami/d_bottom9.cpp \
	    ../../src/burn/drv/konami/d_contra.cpp \
	    ../../src/burn/drv/konami/d_crimfght.cpp \
	    ../../src/burn/drv/konami/d_gberet.cpp \
	    ../../src/burn/drv/konami/d_gbusters.cpp \
	    ../../src/burn/drv/konami/d_gradius3.cpp \
	    ../../src/burn/drv/konami/d_gyruss.cpp \
	    ../../src/burn/drv/konami/d_hcastle.cpp \
	    ../../src/burn/drv/konami/d_hexion.cpp \
	    ../../src/burn/drv/konami/d_kontest.cpp \
	    ../../src/burn/drv/konami/d_mainevt.cpp \
	    ../../src/burn/drv/konami/d_mikie.cpp \
	    ../../src/burn/drv/konami/d_mogura.cpp \
	    ../../src/burn/drv/konami/d_nemesis.cpp \
	    ../../src/burn/drv/konami/d_parodius.cpp \
	    ../../src/burn/drv/konami/d_pooyan.cpp \
	    ../../src/burn/drv/konami/d_rollerg.cpp \
	    ../../src/burn/drv/konami/d_scotrsht.cpp \
	    ../../src/burn/drv/konami/d_simpsons.cpp \
	    ../../src/burn/drv/konami/d_spy.cpp \
	    ../../src/burn/drv/konami/d_surpratk.cpp \
	    ../../src/burn/drv/konami/d_thunderx.cpp \
	    ../../src/burn/drv/konami/d_tmnt.cpp \
	    ../../src/burn/drv/konami/d_twin16.cpp \
	    ../../src/burn/drv/konami/d_ultraman.cpp \
	    ../../src/burn/drv/konami/d_vendetta.cpp \
	    ../../src/burn/drv/konami/d_xmen.cpp \
	    ../../src/burn/drv/konami/k051316.cpp \
	    ../../src/burn/drv/konami/k051733.cpp \
	    ../../src/burn/drv/konami/k051960.cpp \
	    ../../src/burn/drv/konami/k052109.cpp \
	    ../../src/burn/drv/konami/k053245.cpp \
	    ../../src/burn/drv/konami/k053247.cpp \
	    ../../src/burn/drv/konami/k053251.cpp \
	    ../../src/burn/drv/konami/k053936.cpp \
	    ../../src/burn/drv/konami/k054000.cpp \
	    ../../src/burn/drv/konami/konamiic.cpp
}

#===============================================================================
#                              MEGADRIVE DRIVERS
#===============================================================================
$$DRV_MEGADRIVE {
	message("Megadrive drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/megadrive/megadrive.h
	
	SOURCES += \
	    ../../src/burn/drv/megadrive/d_megadrive.cpp \
	    ../../src/burn/drv/megadrive/megadrive.cpp
}

#===============================================================================
#                               NEOGEO DRIVERS
#===============================================================================
$$DRV_NEOGEO {
	message("Neogeo drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/neogeo/neo_sprite_render.h \
	    ../../src/burn/drv/neogeo/neo_text_render.h \
	    ../../src/burn/drv/neogeo/neogeo.h
	
	SOURCES += \
	    ../../src/burn/drv/neogeo/d_neogeo.cpp \
	    ../../src/burn/drv/neogeo/neo_decrypt.cpp \
	    ../../src/burn/drv/neogeo/neo_palette.cpp \
	    ../../src/burn/drv/neogeo/neo_run.cpp \
	    ../../src/burn/drv/neogeo/neo_sprite.cpp \
	    ../../src/burn/drv/neogeo/neo_text.cpp \
	    ../../src/burn/drv/neogeo/neo_upd4990a.cpp \
	    ../../src/burn/drv/neogeo/neogeo.cpp
}

#===============================================================================
#                              PC-ENGINE DRIVERS
#===============================================================================
$$DRV_PCE {
	message("PC-Engine drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/pce/pce.h
	
	SOURCES += \
	    ../../src/burn/drv/pce/d_pce.cpp \
	    ../../src/burn/drv/pce/pce.cpp
}

#===============================================================================
#                                PGM DRIVERS
#===============================================================================
$$DRV_PGM {
	message("PGM drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/pgm/pgm.h
	
	SOURCES += \
	    ../../src/burn/drv/pgm/d_pgm.cpp \
	    ../../src/burn/drv/pgm/pgm_crypt.cpp \
	    ../../src/burn/drv/pgm/pgm_draw.cpp \
	    ../../src/burn/drv/pgm/pgm_prot.cpp \
	    ../../src/burn/drv/pgm/pgm_run.cpp
}

#===============================================================================
#                                PRE90S DRIVERS
#===============================================================================
$$DRV_PRE90S {
	message("Pre90s drivers enabled")

	SOURCES += \
	    ../../src/burn/drv/pre90s/d_4enraya.cpp \
	    ../../src/burn/drv/pre90s/d_1942.cpp \
	    ../../src/burn/drv/pre90s/d_1943.cpp \
	    ../../src/burn/drv/pre90s/d_ambush.cpp \
	    ../../src/burn/drv/pre90s/d_arabian.cpp \
	    ../../src/burn/drv/pre90s/d_armedf.cpp \
	    ../../src/burn/drv/pre90s/d_atetris.cpp \
	    ../../src/burn/drv/pre90s/d_aztarac.cpp \
	    ../../src/burn/drv/pre90s/d_baraduke.cpp \
	    ../../src/burn/drv/pre90s/d_bionicc.cpp \
	    ../../src/burn/drv/pre90s/d_blktiger.cpp \
	    ../../src/burn/drv/pre90s/d_blockout.cpp \
	    ../../src/burn/drv/pre90s/d_blueprnt.cpp \
	    ../../src/burn/drv/pre90s/d_bombjack.cpp \
	    ../../src/burn/drv/pre90s/d_capbowl.cpp \
	    ../../src/burn/drv/pre90s/d_commando.cpp \
	    ../../src/burn/drv/pre90s/d_cybertnk.cpp \
	    ../../src/burn/drv/pre90s/d_ddragon.cpp \
	    ../../src/burn/drv/pre90s/d_dkong.cpp \
	    ../../src/burn/drv/pre90s/d_dynduke.cpp \
	    ../../src/burn/drv/pre90s/d_epos.cpp \
	    ../../src/burn/drv/pre90s/d_exedexes.cpp \
	    ../../src/burn/drv/pre90s/d_funkybee.cpp \
	    ../../src/burn/drv/pre90s/d_galaga.cpp \
	    ../../src/burn/drv/pre90s/d_gauntlet.cpp \
	    ../../src/burn/drv/pre90s/d_ginganin.cpp \
	    ../../src/burn/drv/pre90s/d_gng.cpp \
	    ../../src/burn/drv/pre90s/d_gunsmoke.cpp \
	    ../../src/burn/drv/pre90s/d_higemaru.cpp \
	    ../../src/burn/drv/pre90s/d_ikki.cpp \
	    ../../src/burn/drv/pre90s/d_jack.cpp \
	    ../../src/burn/drv/pre90s/d_kangaroo.cpp \
	    ../../src/burn/drv/pre90s/d_kyugo.cpp \
	    ../../src/burn/drv/pre90s/d_ladybug.cpp \
	    ../../src/burn/drv/pre90s/d_lwings.cpp \
	    ../../src/burn/drv/pre90s/d_madgear.cpp \
	    ../../src/burn/drv/pre90s/d_marineb.cpp \
	    ../../src/burn/drv/pre90s/d_markham.cpp \
	    ../../src/burn/drv/pre90s/d_meijinsn.cpp \
	    ../../src/burn/drv/pre90s/d_mitchell.cpp \
	    ../../src/burn/drv/pre90s/d_mole.cpp \
	    ../../src/burn/drv/pre90s/d_momoko.cpp \
	    ../../src/burn/drv/pre90s/d_mrdo.cpp \
	    ../../src/burn/drv/pre90s/d_mrflea.cpp \
	    ../../src/burn/drv/pre90s/d_mustache.cpp \
	    ../../src/burn/drv/pre90s/d_mystston.cpp \
	    ../../src/burn/drv/pre90s/d_pac2650.cpp \
	    ../../src/burn/drv/pre90s/d_pacland.cpp \
	    ../../src/burn/drv/pre90s/d_pacman.cpp \
	    ../../src/burn/drv/pre90s/d_pkunwar.cpp \
	    ../../src/burn/drv/pre90s/d_prehisle.cpp \
	    ../../src/burn/drv/pre90s/d_punchout.cpp \
	    ../../src/burn/drv/pre90s/d_quizo.cpp \
	    ../../src/burn/drv/pre90s/d_rallyx.cpp \
	    ../../src/burn/drv/pre90s/d_renegade.cpp \
	    ../../src/burn/drv/pre90s/d_route16.cpp \
	    ../../src/burn/drv/pre90s/d_rpunch.cpp \
	    ../../src/burn/drv/pre90s/d_scregg.cpp \
	    ../../src/burn/drv/pre90s/d_sf.cpp \
	    ../../src/burn/drv/pre90s/d_skyfox.cpp \
	    ../../src/burn/drv/pre90s/d_skykid.cpp \
	    ../../src/burn/drv/pre90s/d_snk68.cpp \
	    ../../src/burn/drv/pre90s/d_solomon.cpp \
	    ../../src/burn/drv/pre90s/d_sonson.cpp \
	    ../../src/burn/drv/pre90s/d_srumbler.cpp \
	    ../../src/burn/drv/pre90s/d_tail2nose.cpp \
	    ../../src/burn/drv/pre90s/d_tecmo.cpp \
	    ../../src/burn/drv/pre90s/d_tehkanwc.cpp \
	    ../../src/burn/drv/pre90s/d_terracre.cpp \
	    ../../src/burn/drv/pre90s/d_tigeroad.cpp \
	    ../../src/burn/drv/pre90s/d_toki.cpp \
	    ../../src/burn/drv/pre90s/d_vulgus.cpp \
	    ../../src/burn/drv/pre90s/d_wallc.cpp \
	    ../../src/burn/drv/pre90s/d_wc90.cpp \
	    ../../src/burn/drv/pre90s/d_wc90b.cpp \
	    ../../src/burn/drv/pre90s/d_wwfsstar.cpp \
	    ../../src/burn/drv/pre90s/d_xain.cpp
}


#===============================================================================
#                              PSIKYO DRIVERS
#===============================================================================
$$DRV_PSIKYO {
	message("Psikyo drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/psikyo/psikyo_render.h \
	    ../../src/burn/drv/psikyo/psikyo_sprite_func.h \
	    ../../src/burn/drv/psikyo/psikyo.h \
	    ../../src/burn/drv/psikyo/psikyosh_render.h
	
	SOURCES += \
	    ../../src/burn/drv/psikyo/d_psikyo.cpp \
	    ../../src/burn/drv/psikyo/d_psikyo4.cpp \
	    ../../src/burn/drv/psikyo/d_psikyosh.cpp \
	    ../../src/burn/drv/psikyo/psikyo_palette.cpp \
	    ../../src/burn/drv/psikyo/psikyo_sprite.cpp \
	    ../../src/burn/drv/psikyo/psikyo_tile.cpp \
	    ../../src/burn/drv/psikyo/psikyosh_render.cpp
}


#===============================================================================
#                                PST90S DRIVERS
#===============================================================================
$$DRV_PST90S {
	message("Pst90s drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/pst90s/kanekotb.h \
	    ../../src/burn/drv/pst90s/nmk004.h
	
	SOURCES += \
	    ../../src/burn/drv/pst90s/d_1945kiii.cpp \
	    ../../src/burn/drv/pst90s/d_aerofgt.cpp \
	    ../../src/burn/drv/pst90s/d_airbustr.cpp \
	    ../../src/burn/drv/pst90s/d_aquarium.cpp \
	    ../../src/burn/drv/pst90s/d_blmbycar.cpp \
	    ../../src/burn/drv/pst90s/d_bloodbro.cpp \
	    ../../src/burn/drv/pst90s/d_crospang.cpp \
	    ../../src/burn/drv/pst90s/d_crshrace.cpp \
	    ../../src/burn/drv/pst90s/d_dcon.cpp \
	    ../../src/burn/drv/pst90s/d_ddragon3.cpp \
	    ../../src/burn/drv/pst90s/d_deniam.cpp \
	    ../../src/burn/drv/pst90s/d_diverboy.cpp \
	    ../../src/burn/drv/pst90s/d_drgnmst.cpp \
	    ../../src/burn/drv/pst90s/d_drtomy.cpp \
	    ../../src/burn/drv/pst90s/d_egghunt.cpp \
	    ../../src/burn/drv/pst90s/d_esd16.cpp \
	    ../../src/burn/drv/pst90s/d_f1gp.cpp \
	    ../../src/burn/drv/pst90s/d_fstarfrc.cpp \
	    ../../src/burn/drv/pst90s/d_funybubl.cpp \
	    ../../src/burn/drv/pst90s/d_fuukifg3.cpp \
	    ../../src/burn/drv/pst90s/d_gaelco.cpp \
	    ../../src/burn/drv/pst90s/d_gaiden.cpp \
	    ../../src/burn/drv/pst90s/d_galpanic.cpp \
	    ../../src/burn/drv/pst90s/d_galspnbl.cpp \
	    ../../src/burn/drv/pst90s/d_gotcha.cpp \
	    ../../src/burn/drv/pst90s/d_gumbo.cpp \
	    ../../src/burn/drv/pst90s/d_hyperpac.cpp \
	    ../../src/burn/drv/pst90s/d_jchan.cpp \
	    ../../src/burn/drv/pst90s/d_kaneko16.cpp \
	    ../../src/burn/drv/pst90s/d_lordgun.cpp \
	    ../../src/burn/drv/pst90s/d_mcatadv.cpp \
	    ../../src/burn/drv/pst90s/d_midas.cpp \
	    ../../src/burn/drv/pst90s/d_mugsmash.cpp \
	    ../../src/burn/drv/pst90s/d_news.cpp \
	    ../../src/burn/drv/pst90s/d_nmg5.cpp \
	    ../../src/burn/drv/pst90s/d_nmk16.cpp \
	    ../../src/burn/drv/pst90s/d_ohmygod.cpp \
	    ../../src/burn/drv/pst90s/d_pass.cpp \
	    ../../src/burn/drv/pst90s/d_pirates.cpp \
	    ../../src/burn/drv/pst90s/d_playmark.cpp \
	    ../../src/burn/drv/pst90s/d_powerins.cpp \
	    ../../src/burn/drv/pst90s/d_pushman.cpp \
	    ../../src/burn/drv/pst90s/d_raiden.cpp \
	    ../../src/burn/drv/pst90s/d_seta.cpp \
	    ../../src/burn/drv/pst90s/d_seta2.cpp \
	    ../../src/burn/drv/pst90s/d_shadfrce.cpp \
	    ../../src/burn/drv/pst90s/d_silkroad.cpp \
	    ../../src/burn/drv/pst90s/d_silvmil.cpp \
	    ../../src/burn/drv/pst90s/d_speedspn.cpp \
	    ../../src/burn/drv/pst90s/d_suna16.cpp \
	    ../../src/burn/drv/pst90s/d_suprnova.cpp \
	    ../../src/burn/drv/pst90s/d_taotaido.cpp \
	    ../../src/burn/drv/pst90s/d_tecmosys.cpp \
	    ../../src/burn/drv/pst90s/d_tumbleb.cpp \
	    ../../src/burn/drv/pst90s/d_unico.cpp \
	    ../../src/burn/drv/pst90s/d_welltris.cpp \
	    ../../src/burn/drv/pst90s/d_wwfwfest.cpp \
	    ../../src/burn/drv/pst90s/d_xorworld.cpp \
	    ../../src/burn/drv/pst90s/d_yunsun16.cpp \
	    ../../src/burn/drv/pst90s/d_zerozone.cpp \
	    ../../src/burn/drv/pst90s/nmk004.cpp
}

#===============================================================================
#                                SEGA DRIVERS
#===============================================================================
$$DRV_SEGA {
	message("Sega drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/sega/fd1094.h \
	    ../../src/burn/drv/sega/genesis_vid.h \
	    ../../src/burn/drv/sega/mc8123.h \
	    ../../src/burn/drv/sega/sys16.h
	
	SOURCES += \
	    ../../src/burn/drv/sega/d_angelkds.cpp \
	    ../../src/burn/drv/sega/d_bankp.cpp \
	    ../../src/burn/drv/sega/d_dotrikun.cpp \
	    ../../src/burn/drv/sega/d_hangon.cpp \
	    ../../src/burn/drv/sega/d_outrun.cpp \
	    ../../src/burn/drv/sega/d_suprloco.cpp \
	    ../../src/burn/drv/sega/d_sys1.cpp \
	    ../../src/burn/drv/sega/d_sys16a.cpp \
	    ../../src/burn/drv/sega/d_sys16b.cpp \
	    ../../src/burn/drv/sega/d_sys18.cpp \
	    ../../src/burn/drv/sega/d_xbrd.cpp \
	    ../../src/burn/drv/sega/d_ybrd.cpp \
	    ../../src/burn/drv/sega/fd1089.cpp \
	    ../../src/burn/drv/sega/fd1094.cpp \
	    ../../src/burn/drv/sega/genesis_vid.cpp \
	    ../../src/burn/drv/sega/mc8123.cpp \
	    ../../src/burn/drv/sega/sys16_fd1094.cpp \
	    ../../src/burn/drv/sega/sys16_gfx.cpp \
	    ../../src/burn/drv/sega/sys16_run.cpp
}

#===============================================================================
#                            MASTERSYSTEM DRIVERS
#===============================================================================
$$DRV_SMS {
	message("SMS drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/sms/sms.h
	
	SOURCES += \
	    ../../src/burn/drv/sms/d_sms.cpp \
	    ../../src/burn/drv/sms/sms.cpp
}


#===============================================================================
#                                TAITO DRIVERS
#===============================================================================
$$DRV_TAITO {
	message("Taito drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/taito/taito_ic.h \
	    ../../src/burn/drv/taito/taito_m68705.h \
	    ../../src/burn/drv/taito/taito.h \
	    ../../src/burn/drv/taito/tnzs_prot.h
	
	SOURCES += \
	    ../../src/burn/drv/taito/cchip.cpp \
	    ../../src/burn/drv/taito/d_arkanoid.cpp \
	    ../../src/burn/drv/taito/d_ashnojoe.cpp \
	    ../../src/burn/drv/taito/d_asuka.cpp \
	    ../../src/burn/drv/taito/d_bublbobl.cpp \
	    ../../src/burn/drv/taito/d_chaknpop.cpp \
	    ../../src/burn/drv/taito/d_darius2.cpp \
	    ../../src/burn/drv/taito/d_darkmist.cpp \
            ../../src/burn/drv/taito/d_exzisus.cpp \
	    ../../src/burn/drv/taito/d_flstory.cpp \
	    ../../src/burn/drv/taito/d_lkage.cpp \
	    ../../src/burn/drv/taito/d_minivdr.cpp \
	    ../../src/burn/drv/taito/d_othunder.cpp \
	    ../../src/burn/drv/taito/d_retofinv.cpp \
	    ../../src/burn/drv/taito/d_slapshot.cpp \
	    ../../src/burn/drv/taito/d_superchs.cpp \
	    ../../src/burn/drv/taito/d_taitob.cpp \
	    ../../src/burn/drv/taito/d_taitof2.cpp \
	    ../../src/burn/drv/taito/d_taitomisc.cpp \
	    ../../src/burn/drv/taito/d_taitox.cpp \
	    ../../src/burn/drv/taito/d_taitoz.cpp \
	    ../../src/burn/drv/taito/d_tnzs.cpp \
	    ../../src/burn/drv/taito/d_wyvernf0.cpp \
	    ../../src/burn/drv/taito/pc080sn.cpp \
	    ../../src/burn/drv/taito/pc090oj.cpp \
	    ../../src/burn/drv/taito/taito_ic.cpp \
	    ../../src/burn/drv/taito/taito_m68705.cpp \
	    ../../src/burn/drv/taito/taito.cpp \
	    ../../src/burn/drv/taito/tc0100scn.cpp \
	    ../../src/burn/drv/taito/tc0110pcr.cpp \
	    ../../src/burn/drv/taito/tc0140syt.cpp \
	    ../../src/burn/drv/taito/tc0150rod.cpp \
	    ../../src/burn/drv/taito/tc0180vcu.cpp \
	    ../../src/burn/drv/taito/tc0220ioc.cpp \
	    ../../src/burn/drv/taito/tc0280grd.cpp \
	    ../../src/burn/drv/taito/tc0360pri.cpp \
	    ../../src/burn/drv/taito/tc0480scp.cpp \
	    ../../src/burn/drv/taito/tc0510nio.cpp \
	    ../../src/burn/drv/taito/tc0640fio.cpp \
	    ../../src/burn/drv/taito/tnzs_prot.cpp
}


#===============================================================================
#                               TOAPLAN DRIVERS
#===============================================================================
$$DRV_TOAPLAN {
	message("Toaplan drivers enabled")

	HEADERS += \
	    ../../src/burn/drv/toaplan/toa_extratext.h \
	    ../../src/burn/drv/toaplan/toa_gp9001_render.h \
	    ../../src/burn/drv/toaplan/toaplan.h
	
	SOURCES += \
	    ../../src/burn/drv/toaplan/d_batrider.cpp \
	    ../../src/burn/drv/toaplan/d_batsugun.cpp \
	    ../../src/burn/drv/toaplan/d_battleg.cpp \
	    ../../src/burn/drv/toaplan/d_bbakraid.cpp \
	    ../../src/burn/drv/toaplan/d_demonwld.cpp \
	    ../../src/burn/drv/toaplan/d_dogyuun.cpp \
	    ../../src/burn/drv/toaplan/d_fixeight.cpp \
	    ../../src/burn/drv/toaplan/d_ghox.cpp \
	    ../../src/burn/drv/toaplan/d_hellfire.cpp \
	    ../../src/burn/drv/toaplan/d_kbash.cpp \
	    ../../src/burn/drv/toaplan/d_kbash2.cpp \
	    ../../src/burn/drv/toaplan/d_mahoudai.cpp \
	    ../../src/burn/drv/toaplan/d_outzone.cpp \
	    ../../src/burn/drv/toaplan/d_pipibibs.cpp \
	    ../../src/burn/drv/toaplan/d_rallybik.cpp \
	    ../../src/burn/drv/toaplan/d_samesame.cpp \
	    ../../src/burn/drv/toaplan/d_shippumd.cpp \
	    ../../src/burn/drv/toaplan/d_snowbro2.cpp \
	    ../../src/burn/drv/toaplan/d_tekipaki.cpp \
	    ../../src/burn/drv/toaplan/d_tigerheli.cpp \
	    ../../src/burn/drv/toaplan/d_truxton.cpp \
	    ../../src/burn/drv/toaplan/d_truxton2.cpp \
	    ../../src/burn/drv/toaplan/d_vfive.cpp \
	    ../../src/burn/drv/toaplan/d_vimana.cpp \
	    ../../src/burn/drv/toaplan/d_zerowing.cpp \
	    ../../src/burn/drv/toaplan/toa_bcu2.cpp \
	    ../../src/burn/drv/toaplan/toa_extratext.cpp \
	    ../../src/burn/drv/toaplan/toa_gp9001.cpp \
	    ../../src/burn/drv/toaplan/toa_palette.cpp \
	    ../../src/burn/drv/toaplan/toaplan.cpp \
	    ../../src/burn/drv/toaplan/toaplan1.cpp
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
    ../../src/burner/sdl/bzip.cpp \
    ../../src/burner/sdl/config.cpp \
    ../../src/burner/sdl/drv.cpp \
    ../../src/burner/sdl/inpdipsw.cpp \
    ../../src/burner/sdl/main.cpp \
    ../../src/burner/sdl/run.cpp \
    ../../src/burner/sdl/stated.cpp \
    ../../src/burner/sdl/stringset.cpp \
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
    ../../src/intf/interface.cpp \
    ../../src/intf/audio/aud_dsp.cpp \
    ../../src/intf/audio/aud_interface.cpp \
    ../../src/intf/audio/lowpass2.cpp \
    ../../src/intf/audio/sdl/aud_sdl.cpp \
    ../../src/intf/cd/cd_interface.cpp \
    ../../src/intf/input/inp_interface.cpp \
    ../../src/intf/input/sdl/inp_sdl.cpp \
    ../../src/intf/video/vid_interface.cpp \
    ../../src/intf/video/vid_support.cpp \
    ../../src/intf/video/sdl/vid_sdlfx.cpp \
    ../../src/intf/video/sdl/vid_sdlopengl.cpp \
    ../../src/cpu/z80_intf.cpp \
    ../../src/burner/libretro/neocdlist.cpp \
    ../../src/intf/video/vid_softfx.cpp \
    ../../src/intf/video/scalers/2xpm.cpp \
    ../../src/intf/video/scalers/2xsai.cpp \
    ../../src/intf/video/scalers/ddt3x.cpp \
    ../../src/intf/video/scalers/epx.cpp \
    ../../src/intf/video/scalers/hq2xs_16.cpp \
    ../../src/intf/video/scalers/hq2xs.cpp \
    ../../src/intf/video/scalers/xbr.cpp

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
    ../../src/burner/sdl/tchar.h \
    ../../src/dep/libs/libpng/png.h \
    ../../src/dep/libs/libpng/pngconf.h \
    ../../src/dep/libs/libpng/pngdebug.h \
    ../../src/dep/libs/libpng/pnginfo.h \
    ../../src/dep/libs/libpng/pnglibconf.h \
    ../../src/dep/libs/libpng/pngpriv.h \
    ../../src/dep/libs/libpng/pngstruct.h \
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
    ../../src/intf/interface.h \
    ../../src/intf/audio/aud_dsp.h \
    ../../src/intf/audio/lowpass2.h \
    ../../src/intf/cd/cd_interface.h \
    ../../src/intf/input/inp_keys.h \
    ../../src/intf/input/sdl/inp_sdl_keys.h \
    ../../src/intf/video/vid_support.h \
    ../../src/burner/sdl/burner_sdl.h \
    ../../src/cpu/sh2_intf.h \
    ../../src/cpu/z80_intf.h \
    ../../src/intf/video/vid_softfx.h \
    ../../src/intf/video/scalers/hq_shared32.h \
    ../../src/intf/video/scalers/hq2xs.h \
    ../../src/intf/video/scalers/hq3xs.h \
    ../../src/intf/video/scalers/interp.h \
    ../../src/intf/video/scalers/scale2x_vc.h \
    ../../src/intf/video/scalers/scale2x.h \
    ../../src/intf/video/scalers/scale3x.h \
    ../../src/intf/video/scalers/xbr.h

OTHER_FILES +=
