project(
    'fbneo',
    ['c', 'cpp'],
    version: '1.0.0.3',
    default_options: ['c_std=c99', 'cpp_std=c++11'],
)

perl = find_program('perl', required: true)
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if cc.get_id() in ['gcc', 'clang']
    add_project_arguments('-D_GNU_SOURCE', language: 'c')
endif
if cpp.get_id() in ['gcc', 'clang']
    add_project_arguments('-D_GNU_SOURCE', language: 'cpp')
endif

if cpp.get_id() in ['gcc', 'clang']
    add_project_arguments('-Wno-write-strings', language: 'cpp')
elif cpp.get_id() == 'msvc'
    add_project_arguments('/Zc:strictStrings-', language: 'cpp')
endif

include_7z = get_option('include_7z')
frontend = get_option('frontend')
if frontend == 'auto'
    if host_machine.system() == 'windows'
        frontend = 'win32'
    elif host_machine.system() == 'darwin'
        frontend = 'macos'
    else
        frontend = 'sdl2'
    endif
endif

message('FBNeo frontend: ' + frontend)
force_pulse_audio = get_option('force_pulse_audio')
rom_verify = get_option('rom_verify')
fastcall_support = get_option('fastcall_support')

if fastcall_support
    fastcall_test = []
    warn_as_error_args = []

    if cc.get_id() in ['gcc', 'clang']
        fastcall_test = '''
        int __attribute__((fastcall)) test_fastcall(int a) { return a; }
        int main(void) { return test_fastcall(0); }
        '''

        warn_as_error_args = '-Werror'
    elif cc.get_id() == 'msvc'
        fastcall_test = '''
        int __fastcall test_fastcall(int a) { return a; }
        int main(void) { return test_fastcall(0); }
        '''

        warn_as_error_args = ['/WX']
    endif

    if not cc.compiles(
        fastcall_test,
        name: '__fastcall keyword (C)',
        args: warn_as_error_args,
    )
        add_project_arguments('-D__fastcall=', language: 'c')
    else
        add_project_arguments('-DFASTCALL', language: 'c')
    endif

    if not cpp.compiles(
        fastcall_test,
        name: '__fastcall keyword (C++)',
        args: warn_as_error_args,
    )
        add_project_arguments('-D__fastcall=', language: 'cpp')
    else
        add_project_arguments('-DFASTCALL', language: 'cpp')
    endif
endif

burn_driver_list = get_option('driver_list')
burn_driver_dependencies = {
    'atari': {
        'cpus': ['adsp2100', 'm6502', 'm68000', 'm6809', 'tms34010'],
        'devices': [
            'atariic',
            'atarijsa',
            'atarimo',
            'atarirle',
            'atarivad',
            'avgdvg',
            'eeprom',
            'slapstic',
            'tms34061',
            'watchdog',
        ],
        'sounds': [
            'asteroids',
            'ay8910',
            'bzone',
            'dac',
            'llander',
            'pokey',
            'redbaron',
            'samples',
            'ym2151',
        ],
    },
    'capcom': {
        'cpus': ['m68000', 'z80'],
        'devices': ['eeprom', 'resnet', 'watchdog'],
        'sounds': [
            'ay8910',
            'burn_ym2151',
            'burn_ym2203',
            'burn_ym2413',
            'burn_ym2151',
            'burn_ym2610',
            'burn_ym2612',
            'burn_ym3526',
            'burn_ym3812',
            'burn_y8950',
            'dac',
            'fm',
            'fmopl',
            'msm5205',
            'msm6295',
            'samples',
            'ym2151',
            'ym2413',
            'ym2612',
        ],
    },
    'cave': {
        'cpus': ['m68000', 'sh2', 'sh4', 'z80'],
        'devices': ['eeprom', 'watchdog'],
        'sounds': ['burn_ym2151', 'burn_ym2203', 'burn_ym2610', 'msm6295', 'ym2151'],
    },
    'channelf': {
        'cpus': ['f8'],
        'devices': [],
        'sounds': ['dac'],
    },
    'coleco': {
        'cpus': ['z80'],
        'devices': [],
        'sounds': ['sn76496'],
    },
    'cps3': {
        'cpus': ['sh2'],
        'devices': ['eeprom', 'intelfsh'],
        'sounds': ['dac'],
    },
    'dataeast': {
        'cpus': [
            'arm',
            'h6280',
            'i8051',
            'i8x41',
            'm6502',
            'm68000',
            'm6809',
            'tms32010',
            'z80',
        ],
        'devices': ['decobsmt', 'eeprom'],
        'sounds': [
            'ay8910',
            'burn_ym2151',
            'burn_ym2203',
            'burn_ym3526',
            'burn_ym3812',
            'dac',
            'fm',
            'fmopl',
            'msm5205',
            'msm6295',
            'ym2151',
        ],
    },
    'galaxian': {
        'cpus': ['m6809', 's2650', 'z80'],
        'devices': ['resnet', 'watchdog'],
        'sounds': ['ay8910', 'samples', 'sn76477', 'sn76496'],
    },
    'irem': {
        'cpus': ['i8039', 'i8051', 'm68000', 'm6809', 'nec', 'z80'],
        'devices': ['eeprom', 'watchdog'],
        'sounds': [
            'ay8910',
            'burn_ym2151',
            'burn_ym2203',
            'burn_ym2151',
            'dac',
            'iremga20',
            'msm5205',
            'msm6295',
            'ym2151',
        ],
    },
    'konami': {
        'cpus': [
            'hd6309',
            'i8039',
            'i8x41',
            'konami',
            'm6800',
            'm68000',
            'm6805',
            'm6809',
            'z80',
        ],
        'devices': ['eeprom', 'watchdog'],
        'sounds': [
            'ay8910',
            'burn_ym2151',
            'burn_ym2203',
            'burn_ym3812',
            'dac',
            'k005289',
            'k007232',
            'k051649',
            'k053260',
            'k054539',
            'samples',
            'sn76496',
            'upd7759',
            'vlm5030',
            'ym2151',
        ],
    },
    'megadrive': {
        'cpus': ['m68000', 'z80'],
        'devices': ['eeprom'],
        'sounds': ['burn_md2612', 'burn_ym2612', 'dac', 'sn76496', 'ym2612'],
    },
    'midway': {
        'cpus': [
            'adsp2100',
            'i386',
            'm6502',
            'm68000',
            'm6809',
            'mips3',
            'tms34010',
        ],
        'devices': [
            '6821pia',
            '6840ptm',
            '8255ppi',
            'eeprom',
            'midcsd',
            'midcvsd',
            'midsat',
            'midsg',
            'midssio',
            'midtcs',
            'pic8259',
            'pit8253',
            'tms34061',
            'watchdog',
        ],
        'sounds': ['ay8910', 'dac', 'hc55516', 'msm6295', 'samples', 'ym2151'],
    },
    'msx': {
        'cpus': ['z80'],
        'devices': ['tms9928a'],
        'sounds': ['ay8910', 'dac', 'ym2413'],
    },
    'neogeo': {
        'cpus': ['m68000', 'z80'],
        'devices': ['eeprom', 'watchdog'],
        'sounds': ['burn_ym2610', 'dac', 'ym2612', 'ymdeltat'],
    },
    'nes': {
        'cpus': ['m6502'],
        'devices': [],
        'sounds': ['nes_apu'],
    },
    'pce': {
        'cpus': ['h6280'],
        'devices': ['vdc'],
        'sounds': ['c6280'],
    },
    'pgm': {
        'cpus': ['arm7', 'm68000', 'z80'],
        'devices': ['eeprom', 'i2ceeprom', 'watchdog'],
        'sounds': ['ics2115', 'msm6295'],
    },
    'pgm2': {
        'cpus': ['arm7'],
        'devices': ['eeprom', 'i2ceeprom'],
        'sounds': ['ics2115'],
    },
    'pre90s': {
        'cpus': [
            'adsp2100',
            'h6280',
            'hd6309',
            'hmcs40',
            'i8039',
            'i8051',
            'i8085',
            'i8x41',
            'konami',
            'm6502',
            'm6800',
            'm6805',
            'm6809',
            'm68000',
            'mb88xx',
            'nec',
            'pic16c5x',
            's2650',
            'tms32010',
            'tms34010',
            'z80',
        ],
        'devices': [
            '6821pia',
            '6840ptm',
            '74259',
            '8255ppi',
            '8257dma',
            'ad59mc07',
            'alpha8201',
            'avgdvg',
            'bsmt2000',
            'ds2404',
            'dtimer',
            'earom',
            'eeprom',
            'i8155',
            'mathbox',
            'namcoio',
            'nb1414m4',
            'nb1414m4_8bit',
            'nmk004',
            'pandora',
            'poly',
            'resnet',
            't5182',
            'timekpr',
            'tlc34076',
            'tms34061',
            'tms9928a',
            'v3021',
            'watchdog',
            'x2212',
        ],
        'sounds': [
            'asteroids',
            'astrocde',
            'ay8910',
            'beep',
            'bzone',
            'dac',
            'digitalk',
            'exidy440',
            'flower',
            'flt_rc',
            'fm',
            'fmopl',
            'hc55516',
            'k005289',
            'k007232',
            'llander',
            'msm5205',
            'msm5232',
            'msm6295',
            'namco_snd',
            'phoenixsound',
            'pleiadssound',
            'pokey',
            'redbaron',
            's14001a',
            'saa1099',
            'samples',
            'sn76477',
            'sn76496',
            'snk6502_sound',
            'sp0250',
            'sp0256',
            't6w28',
            'tiamc1',
            'tms36xx',
            'tms5110',
            'tms5220',
            'upd7759',
            'vlm5030',
            'votrax',
            'wiping',
            'x1010',
            'ym2151',
            'ym2413',
            'ym2612',
            'ymdeltat',
        ],
    },
    'psikyo': {
        'cpus': ['m68000', 'sh2', 'z80'],
        'devices': ['eeprom'],
        'sounds': ['burn_ym2610', 'ym2151'],
    },
    'pst90s': {
        'cpus': [
            'arm7',
            'arm9',
            'e132xs',
            'i386',
            'i8051',
            'm377',
            'm6502',
            'm68000',
            'm6805',
            'mips3',
            'nec',
            'sh2',
            'sh4',
            'tlcs90',
            'tlcs900',
            'tms32010',
            'upd7725',
            'upd7810',
            'v60',
            'z80',
            'z180',
        ],
        'devices': [
            'c169',
            'eeprom',
            'epic12',
            'gaelco_crypt',
            'i2ceeprom',
            'i4x00',
            'intelfsh',
            'k1ge',
            'kaneko_hit',
            'kaneko_tmap',
            'mb87078',
            'mermaid',
            'namco_c45',
            'nmk112',
            'qs1000',
            'rtc9701',
            'seibucop',
            'seibusnd',
            'serflash',
            'sknsspr',
            'st0020',
            'watchdog',
        ],
        'sounds': [
            'ay8910',
            'burn_ym2151',
            'burn_ym2203',
            'burn_ym2413',
            'burn_ym2608',
            'burn_ym2610',
            'burn_ym2612',
            'burn_ym3526',
            'burn_ym3812',
            'burn_ymf262',
            'burn_ymf271',
            'burn_ymf278b',
            'c140',
            'c352',
            'dac',
            'es5506',
            'es8712',
            'gaelco',
            'i5000',
            'ics2115',
            'mpeg_audio',
            'msm5205',
            'msm6295',
            'multipcm',
            'rf5c68',
            'samples',
            'segapcm',
            'ym2151',
            'ym2413',
            'ym2612',
            'ymdeltat',
            'ymf262',
            'ymf271',
            'ymf278b',
            'ymz280b',
            'ymz770',
        ],
    },
    'sega': {
        'cpus': [
            'i8039',
            'i8051',
            'i8x41',
            'm6502',
            'm68000',
            'm6809',
            'nec',
            'sh2',
            'v60',
            'z80',
        ],
        'devices': ['eeprom', 'seibusnd', 'watchdog'],
        'sounds': [
            'ay8910',
            'burn_ym2151',
            'burn_ym2203',
            'burn_ym2413',
            'burn_ym2610',
            'burn_ym2612',
            'dac',
            'multipcm',
            'samples',
            'segapcm',
            'sn76496',
            'upd7759',
            'ym2151',
            'ym2413',
            'ym2612',
        ],
    },
    'sg1000': {
        'cpus': ['z80'],
        'devices': ['tms9928a'],
        'sounds': ['sn76496'],
    },
    'sms': {
        'cpus': ['z80'],
        'devices': ['tms9928a'],
        'sounds': ['sn76496', 'ym2413'],
    },
    'snes': {
        'cpus': ['m6502', 'upd7725'],
        'devices': [],
        'sounds': ['dac'],
    },
    'spectrum': {
        'cpus': ['z80'],
        'devices': [],
        'sounds': ['ay8910', 'beep'],
    },
    'taito': {
        'cpus': [
            'hd6309',
            'i8051',
            'i8x41',
            'm6800',
            'm68000',
            'm6805',
            'm6809',
            'upd7810',
            'z80',
        ],
        'devices': ['eeprom', 'watchdog'],
        'sounds': [
            'ay8910',
            'burn_ym2151',
            'burn_ym2203',
            'burn_ym2610',
            'dac',
            'msm5205',
            'msm6295',
            'samples',
            'ym2151',
        ],
    },
    'toaplan': {
        'cpus': [
            'i8x41',
            'm68000',
            'm6805',
            'tms32010',
            'z180',
            'z80',
        ],
        'devices': ['eeprom', 'watchdog'],
        'sounds': ['burn_ym2151', 'burn_ym3812', 'dac', 'msm6295', 'ym2151'],
    },
}

cpu_list = []
burn_device_list = []
burn_sound_list = []
foreach name : burn_driver_list
    deps = burn_driver_dependencies[name]
    foreach cpu_name : deps['cpus']
        if not cpu_list.contains(cpu_name)
            cpu_list += cpu_name
        endif
    endforeach
    foreach device_name : deps['devices']
        if not burn_device_list.contains(device_name)
            burn_device_list += device_name
        endif
    endforeach
    foreach sound_name : deps['sounds']
        if not burn_sound_list.contains(sound_name)
            burn_sound_list += sound_name
        endif
    endforeach
endforeach

subdir('src/burn')
subdir('src/cpu')
subdir('src/burn/devices')
subdir('src/burn/snd')
subdir('src/burn/drv')

subdir('src/dep/libs/zlib')
subdir('src/dep/libs/lua')
subdir('src/dep/libs/libchdr')
if include_7z
    subdir('src/dep/libs/lib7z')
endif

subdir('src/burner')

fbneo_sources = []
build_details_exe = executable(
    'build_details',
    'src/dep/scripts/build_details.cpp',
    cpp_args: ['-D_UNICODE']
    + (host_machine.cpu_family() == 'x86_64' ? ['-DBUILD_X64_EXE'] : []),
    native: true,
)
build_details_h = custom_target(
    'build_details_h',
    output: 'build_details.h',
    command: [build_details_exe],
    capture: true,
)
fbneo_sources += build_details_h

if frontend == 'sdl2'
    subdir('src/burner/sdl')
elif frontend == 'win32'
    subdir('src/burner/win32')
elif frontend == 'qt'
    subdir('src/burner/qt')
elif frontend == 'macos'
    subdir('src/burner/macos')
elif frontend == 'pi'
    subdir('src/burner/pi')
elif frontend == 'psp'
    subdir('src/burner/psp')
endif

fbneo_sources += frontend_sources

defs = []
extra_deps = []
if include_7z
    fbneo_sources += ['src/burner/un7z.cpp']
    defs += ['-DINCLUDE_7Z_SUPPORT']
    extra_deps += lib7z_dep
endif
if force_pulse_audio
    defs += ['-DFORCE_PULSE_AUDIO']
endif
if rom_verify
    defs += ['-DROM_VERIFY']
endif
fbneo_includes = include_directories(
    [
        'src/intf',
        'src/intf/video',
        'src/intf/video/scalers',
        'src/intf/audio',
        'src/intf/input',
        'src/intf/cd',
        'src/intf/perfcount',
    ],
)

deps = [
    dependency('threads'),
]
if force_pulse_audio
    deps += [dependency('pulse-simple')]
endif

c_warning_args = []
cpp_warning_args = []
if cc.get_id() in ['gcc', 'clang']
    c_warning_args = [
        '-Wno-long-long',
        '-Wno-sign-compare',
        '-Wno-uninitialized',
        '-Wno-unused',
        '-Wno-conversion',
        '-Wno-attributes',
        '-Wno-unused-parameter',
        '-Wno-unused-value',
    ]
endif
if cpp.get_id() in ['gcc', 'clang']
    cpp_warning_args = [
        '-Wconversion',
        '-Winline',
        '-Wno-attributes',
        '-Wno-conversion',
        '-Wno-long-long',
        '-Wno-missing-braces',
        '-Wno-multichar',
        '-Wno-narrowing',
        '-Wno-sign-compare',
        '-Wno-unused-parameter',
        '-Wno-unused-value',
        '-Wno-write-strings',
        '-Wpointer-arith',
        '-Wundef',
        '-Wuninitialized',
        '-Wunknown-pragmas',
    ]
endif

link_args = []
if host_machine.system() != 'windows'
    link_args += ['-lm']
endif
link_args += frontend_link_args

executable(
    'fbneo',
    fbneo_sources,
    include_directories: [fbneo_includes],
    dependencies: deps
    + [
        cpu_dep,
        burn_dep,
        burn_devices_dep,
        burn_snd_dep,
        burn_drv_dep,
        burner_dep,
        frontend_dep,
    ]
    + extra_deps,
    c_args: defs + c_warning_args,
    cpp_args: defs + cpp_warning_args,
    link_args: link_args,
    win_subsystem: frontend == 'win32' ? 'windows' : 'console',
)
