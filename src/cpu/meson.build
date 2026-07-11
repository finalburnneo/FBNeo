# =============================================================================
# CPU emulation core selection & static library
#
# The parent meson.build provides a flat `enabled_components` list. This file
# intersects it with the known CPU catalog to derive `cpu_list`, then builds
# the enabled cores into a static library. Each core is pure data below
# (description + sources + include dir); m68000 keeps a small special block
# for its native opcode-table generator.
# =============================================================================

# ---------------------------------------------------------------------------
# Master catalog of every CPU core: name -> { desc, sources, inc }
# Source paths are relative to this directory.
# ---------------------------------------------------------------------------
all_cpu = {
    'adsp2100': {'desc': 'Analog Devices ADSP-2100 DSP', 'sources': ['adsp2100/adsp2100.cpp', 'adsp2100_intf.cpp'], 'inc': ['adsp2100']},
    'arm': {'desc': 'ARM (ARM2 / ARM6)', 'sources': ['arm/arm.cpp', 'arm_intf.cpp'], 'inc': ['arm']},
    'arm7': {'desc': 'ARM7 (ARM7TDMI)', 'sources': ['arm7/arm7.cpp', 'arm7_intf.cpp'], 'inc': ['arm7']},
    'arm9': {'desc': 'ARM9', 'sources': ['arm9/arm9.cpp', 'arm9_intf.cpp'], 'inc': ['arm9']},
    'e132xs': {'desc': 'NEC E132XS RISC', 'sources': ['e132xs/e132xs.cpp'], 'inc': ['e132xs']},
    'f8': {'desc': 'Fairchild F8', 'sources': ['f8/f8.cpp'], 'inc': ['f8']},
    'h6280': {'desc': 'Hudson H6280', 'sources': ['h6280/h6280.cpp', 'h6280_intf.cpp'], 'inc': ['h6280']},
    'hd6309': {'desc': 'Hitachi HD6309', 'sources': ['hd6309/hd6309.cpp', 'hd6309_intf.cpp'], 'inc': ['hd6309']},
    'hmcs40': {'desc': 'Hitachi HMCS40 4-bit MCU', 'sources': ['hmcs40/hmcs40.cpp'], 'inc': ['hmcs40']},
    'i386': {'desc': 'Intel 386', 'sources': ['i386/i386.cpp'], 'inc': ['i386']},
    'i8039': {'desc': 'Intel 8039 (MCS-48)', 'sources': ['i8039/i8039.cpp'], 'inc': ['i8039']},
    'i8051': {'desc': 'Intel 8051 (MCS-51)', 'sources': ['i8051/mcs51.cpp'], 'inc': ['i8051']},
    'i8085': {'desc': 'Intel 8085', 'sources': ['i8085/i8085.cpp'], 'inc': ['i8085']},
    'i8x41': {'desc': 'Intel 8041 / 8048 UPI', 'sources': ['i8x41/mcs48.cpp'], 'inc': ['i8x41']},
    'konami': {'desc': 'Konami custom CPU', 'sources': ['konami/konami.cpp', 'konami_intf.cpp'], 'inc': ['konami']},
    'm377': {'desc': 'Mitsubishi M37710', 'sources': ['m377/m37710.cpp'], 'inc': ['m377']},
    'm6502': {'desc': 'MOS 6502', 'sources': ['m6502/m6502.cpp', 'm6502_intf.cpp'], 'inc': ['m6502']},
    'm6800': {'desc': 'Motorola 6800', 'sources': ['m6800/m6800.cpp', 'm6800_intf.cpp'], 'inc': ['m6800']},
    'm68000': {'desc': 'Motorola 68000 (Musashi core)', 'sources': ['m68000_intf.cpp', 'm68k/m68kcpu.c', 'm68k/m68kdasm.c'], 'inc': ['m68k']},
    'm6805': {'desc': 'Motorola 6805', 'sources': ['m6805/m6805.cpp', 'm6805_intf.cpp'], 'inc': ['m6805']},
    'm6809': {'desc': 'Motorola 6809', 'sources': ['m6809/m6809.cpp', 'm6809_intf.cpp'], 'inc': ['m6809']},
    'mb88xx': {'desc': 'Fujitsu MB88xx', 'sources': ['mb88xx/mb88xx.cpp'], 'inc': ['mb88xx']},
    'mips3': {'desc': 'MIPS III', 'sources': ['mips3/cop0.cpp', 'mips3/cop1.cpp', 'mips3/mips3.cpp', 'mips3_intf.cpp'], 'inc': ['mips3']},
    'nec': {'desc': 'NEC V30 / V33', 'sources': ['nec/nec.cpp', 'nec/v25.cpp', 'nec_intf.cpp'], 'inc': ['nec']},
    'pic16c5x': {'desc': 'Microchip PIC16C5x', 'sources': ['pic16c5x/pic16c5x.cpp', 'pic16c5x_intf.cpp'], 'inc': ['pic16c5x']},
    's2650': {'desc': 'Signetics S2650', 'sources': ['s2650/s2650.cpp', 's2650_intf.cpp'], 'inc': ['s2650']},
    'sh2': {'desc': 'Hitachi SH-2', 'sources': ['sh2/sh2.cpp'], 'inc': ['sh2']},
    'sh4': {'desc': 'Hitachi SH-4', 'sources': ['sh4/sh4.cpp'], 'inc': ['sh4']},
    'tlcs90': {'desc': 'Toshiba TLCS-90', 'sources': ['tlcs90/tlcs90.cpp', 'tlcs90_intf.cpp'], 'inc': ['tlcs90']},
    'tlcs900': {'desc': 'Toshiba TLCS-900', 'sources': ['tlcs900/tlcs900.cpp'], 'inc': ['tlcs900']},
    'tms32010': {'desc': 'TI TMS32010 DSP', 'sources': ['tms32010/tms32010.cpp'], 'inc': ['tms32010']},
    'tms34010': {'desc': 'TI TMS34010 GSP', 'sources': ['tms34/tms34010.cpp', 'tms34_intf.cpp'], 'inc': ['tms34']},
    'upd7725': {'desc': 'NEC uPD7725 DSP', 'sources': ['upd7725/upd7725.cpp'], 'inc': ['upd7725']},
    'upd7810': {'desc': 'NEC uPD7810', 'sources': ['upd7810/upd7810.cpp'], 'inc': ['upd7810']},
    'v60': {'desc': 'NEC V60', 'sources': ['v60/v60.cpp'], 'inc': ['v60']},
    'z80': {'desc': 'Zilog Z80', 'sources': ['z80/z80.cpp', 'z80/z80ctc.cpp', 'z80/z80daisy.cpp', 'z80/z80pio.cpp', 'z80_intf.cpp'], 'inc': ['z80']},
    'z180': {'desc': 'Zilog Z180', 'sources': ['z180/z180.cpp', 'z180_intf.cpp'], 'inc': ['z180']},
}

# ---------------------------------------------------------------------------
# Derive the enabled CPU list from the parent's flat component set, then
# accumulate sources / include dirs from the catalog (data-driven).
# ---------------------------------------------------------------------------
cpu_sources = []
cpu_include_dirs = ['.']
cpu_status = {}
foreach name, info : all_cpu
    enabled = enabled_components.contains(name)
    cpu_status += {info['desc']: enabled ? 'ON' : 'OFF'}
    if enabled
        cpu_sources += info['sources']
        cpu_include_dirs += info['inc']
    endif
endforeach
summary(cpu_status, section: 'CPU cores', list_sep: '')

# ---------------------------------------------------------------------------
# M68000 special case: native helper generates the opcode table. The plain
# sources/include come from the catalog above; only the generated table and
# its native generator live here.
# ---------------------------------------------------------------------------
if enabled_components.contains('m68000')
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
    # Stub so anything referencing m68kops still resolves.
    m68kops = []
endif

# =============================================================================
# Static library — all enabled CPU core objects go here
# =============================================================================

cpu_lib_inc = ['..', '../burn'] + cpu_include_dirs

cpu_defs = platform_math_defs
cpu_cpp_args = cpu_defs + common_cpp_warning_args
cpu_lib = static_library(
    'cpu',
    cpu_sources,
    include_directories: [include_directories(cpu_lib_inc), global_inc],
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
