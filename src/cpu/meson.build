# =============================================================================
# CPU emulation core selection & static library
#
# The parent meson.build provides a `cpu_driver_list` array listing which
# CPUs should be compiled.  This file checks every known CPU name against
# that list, sets a `cpu_<name>_enable` boolean for each, accumulates
# source / include lists, and finally builds them as a static library.
# =============================================================================

cpu_sources = []
cpu_include_dirs = ['.']

# ---------------------------------------------------------------------------
# Master dictionary of every CPU core known to the project.
# Key  = cpu driver name (used as `cpu_<name>_enable` variable name)
# Value = human-readable description printed in the configure summary
# ---------------------------------------------------------------------------
all_cpu = {
    'adsp2100': 'Analog Devices ADSP-2100 DSP',
    'arm': 'ARM (ARM2 / ARM6)',
    'arm7': 'ARM7 (ARM7TDMI)',
    'arm9': 'ARM9',
    'e132xs': 'NEC E132XS RISC',
    'f8': 'Fairchild F8',
    'h6280': 'Hudson H6280',
    'hd6309': 'Hitachi HD6309',
    'hmcs40': 'Hitachi HMCS40 4-bit MCU',
    'i386': 'Intel 386',
    'i8039': 'Intel 8039 (MCS-48)',
    'i8051': 'Intel 8051 (MCS-51)',
    'i8085': 'Intel 8085',
    'i8x41': 'Intel 8041 / 8048 UPI',
    'konami': 'Konami custom CPU',
    'm377': 'Mitsubishi M37710',
    'm6502': 'MOS 6502',
    'm6800': 'Motorola 6800',
    'm68000': 'Motorola 68000 (Musashi core)',
    'm6805': 'Motorola 6805',
    'm6809': 'Motorola 6809',
    'mb88xx': 'Fujitsu MB88xx',
    'mips3': 'MIPS III',
    'nec': 'NEC V30 / V33',
    'pic16c5x': 'Microchip PIC16C5x',
    's2650': 'Signetics S2650',
    'sh2': 'Hitachi SH-2',
    'sh4': 'Hitachi SH-4',
    'tlcs90': 'Toshiba TLCS-90',
    'tlcs900': 'Toshiba TLCS-900',
    'tms32010': 'TI TMS32010 DSP',
    'tms34010': 'TI TMS34010 GSP',
    'upd7725': 'NEC uPD7725 DSP',
    'upd7810': 'NEC uPD7810',
    'v60': 'NEC V60',
    'z80': 'Zilog Z80',
    'z180': 'Zilog Z180',
}

# ---------------------------------------------------------------------------
#
# ---------------------------------------------------------------------------
all_cpu_names = all_cpu.keys()
foreach name : cpu_list
    assert(
        all_cpu_names.contains(name),
        'Missing cpu metabata for cpu_driver_list entry: ' + name,
    )
endforeach

# ---------------------------------------------------------------------------
# Derive per-CPU enable booleans from the parent's cpu_driver_list
# ---------------------------------------------------------------------------
cpu_status = {}
foreach name, desc : all_cpu
    enabled = cpu_list.contains(name)
    set_variable('cpu_@0@_enable'.format(name), enabled)
    cpu_status += {desc: enabled ? 'ON' : 'OFF'}
endforeach
summary(cpu_status, section: 'CPU cores', list_sep: '')

# =============================================================================
# Per-CPU source / include blocks — gated by cpu_<name>_enable
# =============================================================================

# ---------------------------------------------------------------------------
# ADSP-2100 – Analog Devices DSP
# ---------------------------------------------------------------------------
if cpu_adsp2100_enable
    cpu_sources += [
        'adsp2100/adsp2100.cpp',
        'adsp2100_intf.cpp',
    ]
    cpu_include_dirs += ['adsp2100']
endif

# ---------------------------------------------------------------------------
# ARM – ARM2 / ARM6
# ---------------------------------------------------------------------------
if cpu_arm_enable
    cpu_sources += [
        'arm/arm.cpp',
        'arm_intf.cpp',
    ]
    cpu_include_dirs += ['arm']
endif

# ---------------------------------------------------------------------------
# ARM7 – ARM7TDMI
# ---------------------------------------------------------------------------
if cpu_arm7_enable
    cpu_sources += [
        'arm7/arm7.cpp',
        'arm7_intf.cpp',
    ]
    cpu_include_dirs += ['arm7']
endif

# ---------------------------------------------------------------------------
# ARM9
# ---------------------------------------------------------------------------
if cpu_arm9_enable
    cpu_sources += [
        'arm9/arm9.cpp',
        'arm9_intf.cpp',
    ]
    cpu_include_dirs += ['arm9']
endif

# ---------------------------------------------------------------------------
# E132XS – NEC RISC
# ---------------------------------------------------------------------------
if cpu_e132xs_enable
    cpu_sources += ['e132xs/e132xs.cpp']
    cpu_include_dirs += ['e132xs']
endif

# ---------------------------------------------------------------------------
# F8 – Fairchild F8
# ---------------------------------------------------------------------------
if cpu_f8_enable
    cpu_sources += ['f8/f8.cpp']
    cpu_include_dirs += ['f8']
endif

# ---------------------------------------------------------------------------
# H6280 – Hudson
# ---------------------------------------------------------------------------
if cpu_h6280_enable
    cpu_sources += [
        'h6280/h6280.cpp',
        'h6280_intf.cpp',
    ]
    cpu_include_dirs += ['h6280']
endif

# ---------------------------------------------------------------------------
# HD6309 – Hitachi
# ---------------------------------------------------------------------------
if cpu_hd6309_enable
    cpu_sources += [
        'hd6309/hd6309.cpp',
        'hd6309_intf.cpp',
    ]
    cpu_include_dirs += ['hd6309']
endif

# ---------------------------------------------------------------------------
# HMCS40 – Hitachi 4-bit MCU
# ---------------------------------------------------------------------------
if cpu_hmcs40_enable
    cpu_sources += ['hmcs40/hmcs40.cpp']
    cpu_include_dirs += ['hmcs40']
endif

# ---------------------------------------------------------------------------
# I386 – Intel 386
# ---------------------------------------------------------------------------
if cpu_i386_enable
    cpu_sources += ['i386/i386.cpp']
    cpu_include_dirs += ['i386']
endif

# ---------------------------------------------------------------------------
# I8039 – Intel MCS-48
# ---------------------------------------------------------------------------
if cpu_i8039_enable
    cpu_sources += ['i8039/i8039.cpp']
    cpu_include_dirs += ['i8039']
endif

# ---------------------------------------------------------------------------
# I8051 – Intel MCS-51
# ---------------------------------------------------------------------------
if cpu_i8051_enable
    cpu_sources += ['i8051/mcs51.cpp']
    cpu_include_dirs += ['i8051']
endif

# ---------------------------------------------------------------------------
# I8085 – Intel 8085
# ---------------------------------------------------------------------------
if cpu_i8085_enable
    cpu_sources += ['i8085/i8085.cpp']
    cpu_include_dirs += ['i8085']
endif

# ---------------------------------------------------------------------------
# I8X41 – Intel UPI (8041 / 8048)
# ---------------------------------------------------------------------------
if cpu_i8x41_enable
    cpu_sources += ['i8x41/mcs48.cpp']
    cpu_include_dirs += ['i8x41']
endif

# ---------------------------------------------------------------------------
# Konami – custom CPU
# ---------------------------------------------------------------------------
if cpu_konami_enable
    cpu_sources += [
        'konami/konami.cpp',
        'konami_intf.cpp',
    ]
    cpu_include_dirs += ['konami']
endif

# ---------------------------------------------------------------------------
# M377 – Mitsubishi M37710
# ---------------------------------------------------------------------------
if cpu_m377_enable
    cpu_sources += ['m377/m37710.cpp']
    cpu_include_dirs += ['m377']
endif

# ---------------------------------------------------------------------------
# M6502 – MOS 6502
# ---------------------------------------------------------------------------
if cpu_m6502_enable
    cpu_sources += [
        'm6502/m6502.cpp',
        'm6502_intf.cpp',
    ]
    cpu_include_dirs += ['m6502']
endif

# ---------------------------------------------------------------------------
# M6800 – Motorola 6800
# ---------------------------------------------------------------------------
if cpu_m6800_enable
    cpu_sources += [
        'm6800/m6800.cpp',
        'm6800_intf.cpp',
    ]
    cpu_include_dirs += ['m6800']
endif

# ---------------------------------------------------------------------------
# M68000 – Motorola 68000 (Musashi core + generated opcode table)
# ---------------------------------------------------------------------------
if cpu_m68000_enable
    cpu_sources += [
        'm68000_intf.cpp',
        'm68k/m68kcpu.c',
        'm68k/m68kdasm.c',
    ]
    cpu_include_dirs += ['m68k']

    if get_option('buildtype').contains('debug')
        cpu_sources += 'm68k/m68kdasm.c'
    endif

    # Native helper to generate the 68000 opcode table
    m68kmake = executable('m68kmake', 'm68k/m68kmake.c', native: true)
    m68kops = custom_target(
        'm68kops',
        output: ['m68kops.c', 'm68kops.h'],
        command: [
            m68kmake,
            meson.current_build_dir() + '/',
            meson.current_source_dir() / 'm68k' / 'm68k_in.c',
        ],
    )
    cpu_sources += m68kops
else
    # Stub so the parent build file can always reference m68kops.
    m68kops = []
endif

# ---------------------------------------------------------------------------
# M6805 – Motorola 6805
# ---------------------------------------------------------------------------
if cpu_m6805_enable
    cpu_sources += [
        'm6805/m6805.cpp',
        'm6805_intf.cpp',
    ]
    cpu_include_dirs += ['m6805']
endif

# ---------------------------------------------------------------------------
# M6809 – Motorola 6809
# ---------------------------------------------------------------------------
if cpu_m6809_enable
    cpu_sources += [
        'm6809/m6809.cpp',
        'm6809_intf.cpp',
    ]
    cpu_include_dirs += ['m6809']
endif

# ---------------------------------------------------------------------------
# MB88XX – Fujitsu MB88xx
# ---------------------------------------------------------------------------
if cpu_mb88xx_enable
    cpu_sources += ['mb88xx/mb88xx.cpp']
    cpu_include_dirs += ['mb88xx']
endif

# ---------------------------------------------------------------------------
# MIPS3 – MIPS III
# ---------------------------------------------------------------------------
if cpu_mips3_enable
    cpu_sources += [
        'mips3/cop0.cpp',
        'mips3/cop1.cpp',
        'mips3/mips3.cpp',
        'mips3_intf.cpp',
    ]
    cpu_include_dirs += ['mips3']
endif

# ---------------------------------------------------------------------------
# NEC – V30 / V33
# ---------------------------------------------------------------------------
if cpu_nec_enable
    cpu_sources += [
        'nec/nec.cpp',
        'nec/v25.cpp',
        'nec_intf.cpp',
    ]
    cpu_include_dirs += ['nec']
endif

# ---------------------------------------------------------------------------
# PIC16C5X – Microchip PIC
# ---------------------------------------------------------------------------
if cpu_pic16c5x_enable
    cpu_sources += [
        'pic16c5x/pic16c5x.cpp',
        'pic16c5x_intf.cpp',
    ]
    cpu_include_dirs += ['pic16c5x']
endif

# ---------------------------------------------------------------------------
# S2650 – Signetics
# ---------------------------------------------------------------------------
if cpu_s2650_enable
    cpu_sources += [
        's2650/s2650.cpp',
        's2650_intf.cpp',
    ]
    cpu_include_dirs += ['s2650']
endif

# ---------------------------------------------------------------------------
# SH-2 – Hitachi
# ---------------------------------------------------------------------------
if cpu_sh2_enable
    cpu_sources += ['sh2/sh2.cpp']
    cpu_include_dirs += ['sh2']
endif

# ---------------------------------------------------------------------------
# SH-4 – Hitachi
# ---------------------------------------------------------------------------
if cpu_sh4_enable
    cpu_sources += ['sh4/sh4.cpp']
    cpu_include_dirs += ['sh4']
endif

# ---------------------------------------------------------------------------
# TLCS-90 – Toshiba
# ---------------------------------------------------------------------------
if cpu_tlcs90_enable
    cpu_sources += [
        'tlcs90/tlcs90.cpp',
        'tlcs90_intf.cpp',
    ]
    cpu_include_dirs += ['tlcs90']
endif

# ---------------------------------------------------------------------------
# TLCS-900 – Toshiba
# ---------------------------------------------------------------------------
if cpu_tlcs900_enable
    cpu_sources += ['tlcs900/tlcs900.cpp']
    cpu_include_dirs += ['tlcs900']
endif

# ---------------------------------------------------------------------------
# TMS32010 – TI DSP
# ---------------------------------------------------------------------------
if cpu_tms32010_enable
    cpu_sources += ['tms32010/tms32010.cpp']
    cpu_include_dirs += ['tms32010']
endif

# ---------------------------------------------------------------------------
# TMS34010 – TI Graphics System Processor
# ---------------------------------------------------------------------------
if cpu_tms34010_enable
    cpu_sources += [
        'tms34/tms34010.cpp',
        'tms34_intf.cpp',
    ]
    cpu_include_dirs += ['tms34']
endif

# ---------------------------------------------------------------------------
# uPD7725 – NEC DSP
# ---------------------------------------------------------------------------
if cpu_upd7725_enable
    cpu_sources += ['upd7725/upd7725.cpp']
    cpu_include_dirs += ['upd7725']
endif

# ---------------------------------------------------------------------------
# uPD7810 – NEC
# ---------------------------------------------------------------------------
if cpu_upd7810_enable
    cpu_sources += ['upd7810/upd7810.cpp']
    cpu_include_dirs += ['upd7810']
endif

# ---------------------------------------------------------------------------
# V60 – NEC
# ---------------------------------------------------------------------------
if cpu_v60_enable
    cpu_sources += ['v60/v60.cpp']
    cpu_include_dirs += ['v60']
endif

# ---------------------------------------------------------------------------
# Z80 – Zilog Z80
# ---------------------------------------------------------------------------
if cpu_z80_enable
    cpu_sources += [
        'z80/z80.cpp',
        'z80/z80ctc.cpp',
        'z80/z80daisy.cpp',
        'z80/z80pio.cpp',
        'z80_intf.cpp',
    ]
    cpu_include_dirs += ['z80']
endif

# ---------------------------------------------------------------------------
# Z180 – Zilog Z180
# ---------------------------------------------------------------------------
if cpu_z180_enable
    cpu_sources += [
        'z180/z180.cpp',
        'z180_intf.cpp',
    ]
    cpu_include_dirs += ['z180']
endif

# =============================================================================
# Static library — all enabled CPU core objects go here
# =============================================================================

cpu_lib_inc = ['..', '../burn'] + cpu_include_dirs

cpu_defs = []
if host_machine.system() == 'windows'
    cpu_defs += '-D_USE_MATH_DEFINES'
endif

cpu_cpp_warning_args = []
if cpp.get_id() in ['gcc', 'clang']
    cpu_cpp_warning_args = [
        '-Wno-sequence-point',
        '-Wno-sign-compare',
        '-Wno-unused-but-set-variable',
        '-Wno-unused-parameter',
        '-Wno-unused-value',
        '-Wno-unused-variable',
    ]
endif

cpu_cpp_args = cpu_defs + cpu_cpp_warning_args
cpu_lib = static_library(
    'cpu',
    cpu_sources,
    include_directories: include_directories(cpu_lib_inc),
    dependencies: burn_dep,
    cpp_args: cpu_cpp_args,
    c_args: cpu_defs,
)

# Dependency object — consumers get the library + all CPU public and
# internal include directories so they can include <hmcs40.h> etc.
cpu_dep = declare_dependency(
    link_with: cpu_lib,
    include_directories: include_directories(cpu_include_dirs),
)
