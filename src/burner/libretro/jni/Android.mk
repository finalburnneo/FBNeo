LOCAL_PATH := $(call my-dir)

MAIN_FBNEO_DIR := $(LOCAL_PATH)/../../..

INCLUDE_CPLUSPLUS11_FILES := 0
INCLUDE_7Z_SUPPORT        := 1
EXTERNAL_ZLIB             := 0
BUILD_X64_EXE             := 0
WANT_NEOGEOCD             := 0
HAVE_NEON                 := 0
USE_CYCLONE               := 0

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  HAVE_NEON               := 1
  #USE_CYCLONE             := 1
endif

CFLAGS      :=
CXXFLAGS    :=
LDFLAGS     :=
SOURCES_C   :=
SOURCES_CXX :=
FBNEO_DEFINES :=

include $(LOCAL_PATH)/../Makefile.common

COMMON_FLAGS := -DUSE_SPEEDHACKS -D__LIBRETRO__ -DANDROID -Wno-write-strings -DLSB_FIRST $(FBNEO_DEFINES)

# Build shared library including static C module
include $(CLEAR_VARS)
LOCAL_MODULE       := retro
LOCAL_SRC_FILES    := $(SOURCES_C) $(SOURCES_S) $(SOURCES_CXX)
LOCAL_C_INCLUDES   := $(INCLUDE_DIRS)
LOCAL_CFLAGS       := $(CFLAGS) $(COMMON_FLAGS)
LOCAL_CPPFLAGS     := $(CXXFLAGS) $(COMMON_FLAGS)
LOCAL_LDFLAGS      := -Wl,-version-script=$(MAIN_FBNEO_DIR)/burner/libretro/link.T
LOCAL_LDLIBS       := $(LDFLAGS)
LOCAL_CPP_FEATURES := exceptions rtti
LOCAL_DISABLE_FORMAT_STRING_CHECKS := true
LOCAL_ARM_MODE := arm

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  LOCAL_ARM_MODE := arm
  LOCAL_ARM_NEON := true
endif

include $(BUILD_SHARED_LIBRARY)
