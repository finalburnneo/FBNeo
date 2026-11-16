qt5 = import('qt5')

qt_sources = qt5.preprocess(
    moc_headers: files(
        'aboutdialog.h',
        'dipswitchdialog.h',
        'emuworker.h',
        'inputdialog.h',
        'inputsetdialog.h',
        'logdialog.h',
        'mainwindow.h',
        'oglviewport.h',
        'qaudiointerface.h',
        'qinputinterface.h',
        'romdirsdialog.h',
        'rominfodialog.h',
        'romscandialog.h',
        'selectdialog.h',
        'supportdirsdialog.h',
        'widgets/hexspinbox.h',
    ),
    ui_files: files(
        'aboutdialog.ui',
        'dipswitchdialog.ui',
        'inputdialog.ui',
        'inputsetdialog.ui',
        'logdialog.ui',
        'romdirsdialog.ui',
        'rominfodialog.ui',
        'romscandialog.ui',
        'selectdialog.ui',
        'supportdirsdialog.ui',
    ),
    qresources: files('rscr.qrc'),
)

frontend_sources = files(
    'aboutdialog.cpp',
    'bzip.cpp',
    'config.cpp',
    'dipswitchdialog.cpp',
    'driver.cpp',
    'emuworker.cpp',
    'inputdialog.cpp',
    'inputsetdialog.cpp',
    'logdialog.cpp',
    'main.cpp',
    '../sdl/romdata.cpp',
    'mainwindow.cpp',
    '../cdlist.cpp',
    '../../intf/cd/cd_img.cpp',
    '../../intf/video/opengl/vid_opengl.cpp',
    '../../dep/libs/glad/src/glad.c',
    '../../intf/video/opengl/shader.cpp',
    '../../burn/snd/burn_ost.cpp',
    '../../burn/snd/samples.cpp',
    'oglviewport.cpp',
    '../../burn/burn_hdd.cpp',
    'progress.cpp',
    'qaudiointerface.cpp',
    'qinputinterface.cpp',
    'qutil.cpp',
    'romdirsdialog.cpp',
    'rominfodialog.cpp',
    'romscandialog.cpp',
    'selectdialog.cpp',
    'stringset.cpp',
    'supportdirsdialog.cpp',
    'widgets/hexspinbox.cpp',
)

frontend_sources += qt_sources

frontend_dep = declare_dependency(
    include_directories: include_directories(
        '.',
        '../../dep/qtcreator',
        '../../intf/video/opengl',
        '../../dep/libs/glad/include',
        '../../dep/libs/dr_libs',
    ),
    dependencies: [
        dependency('qt5', modules: ['Widgets', 'Multimedia', 'OpenGL']),
        dependency('opengl'),
        dependency('sdl2'),
    ],
    compile_args: [
        '-DBUILD_QT',
        '-DWITH_QTCREATOR',
        '-DINCLUDE_LIB_PNGH',
        '-D__fastcall=',
        '-D_fastcall=',
    ],
)

frontend_link_args = []
