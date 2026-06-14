frontend_sources = files(
    'config.cpp',
    'drv.cpp',
    'font.c',
    'inpdipsw.cpp',
    'main.cpp',
    'pg.c',
    'run.cpp',
    'select.cpp',
    'stated.cpp',
    '../../intf/audio/psp/aud_psp.cpp',
    '../../intf/input/psp/inp_psp.cpp',
    '../../intf/video/psp/vid_psp.c',
    '../../intf/video/psp/vid_pspfx.cpp',
)

frontend_dep = declare_dependency(
    include_directories: include_directories(
        '.',
        '../../intf/audio/psp',
        '../../intf/input/psp',
        '../../intf/video/psp',
    ),
    compile_args: ['-DBUILD_PSP'],
)

frontend_link_args = []
