LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH),arm)
LOCAL_CXXFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
LOCAL_C_FLAGS += -fuse-ld=gold
LOCAL_CPP_FLAGS+= -fuse-ld=gold
LOCAL_LDLIBS := -fuse-ld=gold
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CXXFLAGS +=  -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CXXFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

MAIN_FBA_DIR := ../../../src
FBA_BURN_DIR := $(MAIN_FBA_DIR)/burn
FBA_BURN_DRIVERS_DIR := $(MAIN_FBA_DIR)/burn/drv
FBA_BURNER_DIR := $(MAIN_FBA_DIR)/burner
LIBRETRO_DIR := $(FBA_BURNER_DIR)/libretro
FBA_CPU_DIR := $(MAIN_FBA_DIR)/cpu
FBA_LIB_DIR := $(MAIN_FBA_DIR)/dep/libs
FBA_INTERFACE_DIR := $(MAIN_FBA_DIR)/intf
FBA_GENERATED_DIR = $(MAIN_FBA_DIR)/dep/generated
FBA_SCRIPTS_DIR = $(MAIN_FBA_DIR)/dep/scripts

BURN_BLACKLIST := $(FBA_BURNER_DIR)/un7z.cpp \
	$(FBA_CPU_DIR)/arm7/arm7exec.c \
	$(FBA_CPU_DIR)/arm7/arm7core.c \
	$(FBA_CPU_DIR)/hd6309/6309tbl.c \
	$(FBA_CPU_DIR)/hd6309/6309ops.c \
	$(FBA_CPU_DIR)/konami/konamtbl.c \
	$(FBA_CPU_DIR)/konami/konamops.c \
	$(FBA_CPU_DIR)/m68k/m68k_in.c \
	$(FBA_CPU_DIR)/m6800/6800ops.c \
	$(FBA_CPU_DIR)/m6800/6800tbl.c \
	$(FBA_CPU_DIR)/m6805/6805ops.c \
	$(FBA_CPU_DIR)/m6809/6809ops.c \
	$(FBA_CPU_DIR)/m6809/6809tbl.c \
	$(FBA_CPU_DIR)/sh2/mksh2.cpp \
	$(FBA_CPU_DIR)/sh2/mksh2-x86.cpp \
	$(FBA_CPU_DIR)/m68k/m68kmake.c \
	$(FBA_BURNER_DIR)/wave_writer.cpp \
	$(FBA_CPU_DIR)/m68k/m68kdasm.c \
	$(FBA_LIBRETRO_DIR)/menu.cpp \
	$(FBA_CPU_DIR)/sh2/mksh2.cpp \
	$(FBA_BURNER_DIR)/sshot.cpp \
	$(FBA_BURNER_DIR)/conc.cpp \
	$(FBA_BURNER_DIR)/dat.cpp \
	$(FBA_BURNER_DIR)/cong.cpp \
	$(FBA_BURNER_DIR)/image.cpp \
	$(FBA_BURNER_DIR)/misc.cpp \
	$(FBA_CPU_DIR)/h6280/tblh6280.c \
	$(FBA_CPU_DIR)/m6502/t65sc02.c \
	$(FBA_CPU_DIR)/m6502/t65c02.c \
	$(FBA_CPU_DIR)/m6502/tdeco16.c \
	$(FBA_CPU_DIR)/m6502/tn2a03.c \
	$(FBA_CPU_DIR)/m6502/t6502.c \
	$(FBA_CPU_DIR)/nec/v25sfr.c \
	$(FBA_CPU_DIR)/nec/v25instr.c \
	$(FBA_CPU_DIR)/nec/necinstr.c \
	$(FBA_BURN_DIR)/drv/capcom/ctv_make.cpp

FBA_BURN_DIRS := $(FBA_BURN_DIR) \
	$(FBA_BURN_DIR)/devices \
	$(FBA_BURN_DIR)/snd \
	$(FBA_BURN_DRIVERS_DIR)/capcom \
	$(FBA_BURN_DRIVERS_DIR)/cave \
	$(FBA_BURN_DRIVERS_DIR)/cps3 \
	$(FBA_BURN_DRIVERS_DIR)/dataeast \
	$(FBA_BURN_DRIVERS_DIR)/galaxian \
	$(FBA_BURN_DRIVERS_DIR)/irem \
	$(FBA_BURN_DRIVERS_DIR)/konami \
	$(FBA_BURN_DRIVERS_DIR)/megadrive \
	$(FBA_BURN_DRIVERS_DIR)/neogeo \
	$(FBA_BURN_DRIVERS_DIR)/pce \
	$(FBA_BURN_DRIVERS_DIR)/pgm \
	$(FBA_BURN_DRIVERS_DIR)/pre90s \
	$(FBA_BURN_DRIVERS_DIR)/psikyo \
	$(FBA_BURN_DRIVERS_DIR)/pst90s \
	$(FBA_BURN_DRIVERS_DIR)/sega \
	$(FBA_BURN_DRIVERS_DIR)/snes \
	$(FBA_BURN_DRIVERS_DIR)/taito \
	$(FBA_BURN_DRIVERS_DIR)/toaplan

FBA_CPU_DIRS := $(FBA_CPU_DIR) \
	$(FBA_CPU_DIR)/arm \
	$(FBA_CPU_DIR)/arm7 \
	$(FBA_CPU_DIR)/h6280 \
	$(FBA_CPU_DIR)/hd6309 \
	$(FBA_CPU_DIR)/i8039 \
	$(FBA_CPU_DIR)/konami \
	$(FBA_CPU_DIR)/m68k \
	$(FBA_CPU_DIR)/m6502 \
	$(FBA_CPU_DIR)/m6800 \
	$(FBA_CPU_DIR)/m6805 \
	$(FBA_CPU_DIR)/m6809 \
	$(FBA_CPU_DIR)/nec \
	$(FBA_CPU_DIR)/s2650 \
	$(FBA_CPU_DIR)/sh2 \
	$(FBA_CPU_DIR)/z80

FBA_LIB_DIRS := $(FBA_LIB_DIR)/zlib

FBA_SRC_DIRS := $(FBA_BURNER_DIR) $(FBA_BURN_DIRS) $(FBA_CPU_DIRS) $(FBA_BURNER_DIRS) $(FBA_LIB_DIRS)

LOCAL_MODULE    := libretro

LOCAL_SRC_FILES := $(filter-out $(BURN_BLACKLIST),$(foreach dir,$(FBA_SRC_DIRS),$(wildcard $(dir)/*.cpp))) $(filter-out $(BURN_BLACKLIST),$(foreach dir,$(FBA_SRC_DIRS),$(wildcard $(dir)/*.c))) $(LIBRETRO_DIR)/libretro.cpp $(LIBRETRO_DIR)/neocdlist.cpp


LOCAL_CXXFLAGS += -O3 -fno-stack-protector -DUSE_SPEEDHACKS -DINLINE="static inline" -DSH2_INLINE="static inline" -D__LIBRETRO_OPTIMIZATIONS__ -DLSB_FIRST -D__LIBRETRO__ -Wno-write-strings -DUSE_FILE32API -DANDROID -DFRONTEND_SUPPORTS_RGB565
LOCAL_CFLAGS = -O3 -fno-stack-protector -DUSE_SPEEDHACKS -DINLINE="static inline" -DSH2_INLINE="static inline" -D__LIBRETRO_OPTIMIZATIONS__ -DLSB_FIRST -D__LIBRETRO__ -Wno-write-strings -DUSE_FILE32API -DANDROID -DFRONTEND_SUPPORTS_RGB565

LOCAL_C_INCLUDES = $(FBA_BURNER_DIR)/win32 \
	$(LIBRETRO_DIR) \
	$(LIBRETRO_DIR)/tchar \
	$(FBA_BURN_DIR) \
	$(MAIN_FBA_DIR)/cpu \
	$(FBA_BURN_DIR)/snd \
	$(FBA_BURN_DIR)/devices \
	$(FBA_INTERFACE_DIR) \
	$(FBA_INTERFACE_DIR)/input \
	$(FBA_INTERFACE_DIR)/cd \
	$(FBA_BURNER_DIR) \
	$(FBA_CPU_DIR) \
	$(FBA_CPU_DIR)/i8039 \
	$(FBA_LIB_DIR)/zlib \
	$(FBA_BURN_DIR)/drv/capcom \
	$(FBA_BURN_DIR)/drv/dataeast \
	$(FBA_BURN_DIR)/drv/cave \
	$(FBA_BURN_DIR)/drv/neogeo \
	$(FBA_BURN_DIR)/drv/psikyo \
	$(FBA_BURN_DIR)/drv/sega \
	$(FBA_BURN_DIR)/drv/toaplan \
	$(FBA_BURN_DIR)/drv/taito \
	$(FBA_GENERATED_DIR) \
	$(FBA_LIB_DIR)

include $(BUILD_SHARED_LIBRARY)
