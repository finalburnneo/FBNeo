qt5 = import('qt5')

qt_sources = qt5.preprocess(
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
    'dipswitchdialog.cpp',
    'driver.cpp',
    'emuworker.cpp',
    'inputdialog.cpp',
    'inputsetdialog.cpp',
    'logdialog.cpp',
    'main.cpp',
    'mainwindow.cpp',
    'neocdlist.cpp',
    'oglviewport.cpp',
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
    include_directories: include_directories('.', '../../dep/qtcreator'),
    dependencies: [
        dependency('qt5', modules: ['Widgets', 'Multimedia', 'OpenGL']),
        dependency('opengl'),
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
