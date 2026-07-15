win = import('windows')
proj = meson.project_source_root()

frontend_sources = files(
    '../../dep/kaillera/client/net.cpp',
    '../../intf/audio/win32/aud_dsound3.cpp',
    '../../intf/audio/win32/aud_xaudio2.cpp',
    '../../intf/audio/win32/dsound_core.cpp',
    '../../intf/cd/win32/cd_img.cpp',
    '../../intf/input/win32/dinput_core.cpp',
    '../../intf/input/win32/inp_dinput.cpp',
    '../../intf/perfcount/prf_interface.cpp',
    '../../intf/perfcount/win32/prf_performance_counter.cpp',
    '../../intf/video/scalers/2xpm.cpp',
    '../../intf/video/scalers/2xsai.cpp',
    '../../intf/video/scalers/crt.cpp',
    '../../intf/video/scalers/ddt3x.cpp',
    '../../intf/video/scalers/epx.cpp',
    '../../intf/video/scalers/hq2xs.cpp',
    '../../intf/video/scalers/hq2xs_16.cpp',
    '../../intf/video/scalers/hq3xs.cpp',
    '../../intf/video/scalers/hq_shared32.cpp',
    '../../intf/video/scalers/xbr.cpp',
    '../../intf/video/scalers/xbr32.cpp',
    '../../intf/video/vid_softfx.cpp',
    '../../intf/video/win32/ddraw_core.cpp',
    '../../intf/video/win32/directx9_core.cpp',
    '../../intf/video/win32/vid_d3d.cpp',
    '../../intf/video/win32/vid_ddraw.cpp',
    '../../intf/video/win32/vid_ddrawfx.cpp',
    '../../intf/video/win32/vid_directx9.cpp',
    '../../intf/video/win32/vid_directx_support.cpp',
    '../../intf/video/win32/vid_effect.cpp',
    '../luaengine.cpp',
    '../luasav.cpp',
    'about.cpp',
    'avi.cpp',
    'bzip.cpp',
    'choose_monitor.cpp',
    'cona.cpp',
    'd3dkmt_sync.cpp',
    'debugger.cpp',
    'drv.cpp',
    'fba_kaillera.cpp',
    'gameinfo.cpp',
    'image_win32.cpp',
    'inpc.cpp',
    'inpcheat.cpp',
    'inpd.cpp',
    'inpdipsw.cpp',
    'inps.cpp',
    'ips_manager.cpp',
    'localise.cpp',
    'localise_download.cpp',
    'localise_gamelist.cpp',
    'luaconsole.cpp',
    'main.cpp',
    'mbtwc.cpp',
    'media.cpp',
    'memcard.cpp',
    'menu.cpp',
    'misc_win32.cpp',
    'neocdlist.cpp',
    'neocdsel.cpp',
    'numdial.cpp',
    'paletteviewer.cpp',
    'placeholder.cpp',
    'popup_win32.cpp',
    'progress.cpp',
    'replay.cpp',
    'res.cpp',
    'romdata.cpp',
    'roms.cpp',
    'run.cpp',
    'scrn.cpp',
    'sel.cpp',
    'sfactd.cpp',
    'splash.cpp',
    'stated.cpp',
    'support_paths.cpp',
    'systeminfo.cpp',
    'wave.cpp',
)

license_rtf = custom_target(
    'license_rtf',
    input: proj / 'src/license.txt',
    output: 'license.rtf',
    command: [
        perl,
        proj / 'src/dep/scripts/license2rtf.pl',
        '@INPUT@',
        '-o', meson.current_build_dir() / 'license.rtf',
    ],
)

rc_sources_incs = ['.', '..', '../../burn', '../../intf/video/win32']
rc_sources_deps = [build_details_h, license_rtf]

if cpp.get_id() == 'msvc'
    rc_sources_incs += '../../dep/vc/include'
else
    app_gnuc_rc = custom_target(
        'app_gnuc_rc',
        input: 'app.rc',
        output: 'app_gnuc.rc',
        command: [
            perl,
            proj / 'src/dep/scripts/fixrc.pl',
            '@INPUT@',
            '-o', meson.current_build_dir() / 'app_gnuc.rc',
        ],
        depend_files: files(
            '../resource/about.bmp',
            '../resource/fba.ico',
            '../resource/misc.bmp',
            '../resource/splash.bmp',
            'app.rc',
            proj / 'src/dep/scripts/fixrc.pl',
        ),
        depends: license_rtf,
    )

    rc_sources_deps += app_gnuc_rc
endif

rc_args = []
foreach inc : rc_sources_incs
    rc_args += ['-I', meson.current_source_dir() / inc]
endforeach
rc_args += ['-DFINENAME=fbneo']
message('rc incs', rc_args)

rc_sources = win.compile_resources(
    'resource.rc',
    args: rc_args,
    depends: rc_sources_deps,
)

if cpp.get_id() == 'msvc'
    frontend_link_args = [
        'user32.lib',
        'gdi32.lib',
        'comdlg32.lib',
        'comctl32.lib',
        'shell32.lib',
        'winmm.lib',
        'shlwapi.lib',
        'advapi32.lib',
        'setupapi.lib',
        'ole32.lib',
        'uuid.lib',
        'wininet.lib',
        'vfw32.lib',
    ]
    frontend_win32_dep_inc = [
        '../../dep/vc/include',
        '../../dep/vc/include/xaudio2',
        '../../dep/mingw/include/directx9',
    ]
else
    frontend_link_args = [
        '-luser32',
        '-lgdi32',
        '-lcomdlg32',
        '-lcomctl32',
        '-lshell32',
        '-lwinmm',
        '-lshlwapi',
        '-ladvapi32',
        '-lsetupapi',
        '-lole32',
        '-luuid',
        '-lwininet',
        '-lvfw32',
    ]
    frontend_win32_dep_inc = [
        '../../dep/mingw/include',
        '../../dep/mingw/include/directx9',
        '../../dep/mingw/include/xaudio2',
    ]
endif

frontend_inc = [
    '.',
    '../../intf/audio/win32',
    '../../intf/input/win32',
    '../../intf/video/win32',
    '../../intf/cd/win32',
    '../../intf/perfcount/win32',
    '../../dep/kaillera/client',
    '../../dep/libs/lua',
    '../../dep/libs/zip',
] + frontend_win32_dep_inc

frontend_dep = declare_dependency(
    include_directories: include_directories(frontend_inc),
    compile_args: [
        '-DINCLUDE_AVI_RECORDING',
        '-D_USE_MATH_DEFINES',
    ],
    sources: rc_sources,
)
