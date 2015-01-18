#include <QtWidgets>
#ifdef Q_OS_MACX
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include "mainwindow.h"
#include "burner.h"
#include "selectdialog.h"
#include "version.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{

    createDefaultDirs();

    BurnLibInit();
    createActions();
    createMenus();
    createControls();

    setWindowTitle(QString("FBAlpha %1.%2.%3.%4")
                   .arg(VER_MAJOR)
                   .arg(VER_MINOR)
                   .arg(VER_BETA)
                   .arg(VER_ALPHA));

    connectActions();

    m_logo = QImage(":/resource/splash.bmp");

    m_isRunning = false;
    InputInit();

    show();

}

MainWindow::~MainWindow()
{
    BurnLibExit();
    AudSoundStop();
    AudSoundExit();
}

int MainWindow::main(QApplication &app)
{
    m_closeApp = false;
    while (!m_closeApp) {
        if (m_isRunning) {
            app.processEvents();
            m_emulation->run();
        } else {
            app.processEvents();
            QThread::msleep(10);
        }
    }
    return 0;
}

void MainWindow::loadGame()
{
    int ret = m_selectDlg->exec();
    if (ret != QDialog::Accepted) {
        return;
    }
    m_emulation->pause();
    m_emulation->setGame(m_selectDlg->selectedDriver());
    bool okay = m_emulation->init();

    if (!okay) {
        QStringList str;
        str << BzipText.szText << BzipDetail.szText;
        QMessageBox::critical(this, "Error", str.join("\n"));
        return;
    }
    m_isRunning = true;
    enableInGame();
    m_emulation->resume();
}


void MainWindow::exitEmulator()
{
    close();
}

void MainWindow::closeGame()
{
    if (bDrvOkay) {
        m_emulation->close();
        disableInGame();
        m_isRunning = false;
        drawLogo();
    }
}

void MainWindow::setupInputInterface(QAction *action)
{
    InputExit();
    nInputSelect = action->data().toInt();
    InputInit();
}

void MainWindow::toogleMenu()
{
    if (menuBar()->isHidden()) {
        menuBar()->show();
    }
    else menuBar()->hide();
    drawLogo();
}

void MainWindow::toogleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else showFullScreen();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_closeApp = true;
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    drawLogo();
}

void MainWindow::createDefaultDirs()
{
    QStringList dirs;
    dirs << "config" << "config/games" << "config/ips" << "config/localisation"
         << "config/presets" << "recordings" << "roms" << "savestates"
         << "screenshots" << "support" << "support/previews"
         << "support/titles" << "support/icons" << "support/cheats"
         << "support/hiscores" << "support/samples" << "support/ips"
         << "support/neocdz" << "neocdiso";
    QDir current = QDir::currentPath();
    foreach (QString dirName, dirs)
        current.mkpath(dirName);
}

void MainWindow::createMenus()
{
    m_menuGame = menuBar()->addMenu(tr("Game"));
    m_menuGame->addAction(m_actionLoadGame);
    m_menuGame->addAction(m_actionCloseGame);
    m_menuGame->addSeparator();
    m_menuGame->addAction(m_actionExitEmulator);

    m_menuInput = menuBar()->addMenu(tr("Input"));
    m_menuInputPlugin = new QMenu(tr("Plugin"), this);
    m_menuInput->addMenu(m_menuInputPlugin);
    setupInputInterfaces();

    m_menuInput->addAction(m_actionDipswitch);
    m_menuInput->addAction(m_actionMapGameInputs);

    m_menuMisc = menuBar()->addMenu(tr("Misc"));
    m_menuMisc->addAction(m_actionConfigureRomPaths);
    m_menuMisc->addAction(m_actionConfigureSupportPaths);
    m_menuMisc->addSeparator();
    m_menuMisc->addAction(m_actionToogleMenu);
    m_menuMisc->addAction(m_actionLogWindow);

    m_menuHelp = menuBar()->addMenu(tr("Help"));
    m_menuHelp->addAction(m_actionAbout);
}

void MainWindow::createControls()
{
    m_audio = QAudioInterface::get(this);
    m_viewport = new OGLViewport(nullptr);
    setCentralWidget(m_viewport);
    resize(640, 480);

    m_emulation = new EmuWorker();
    m_selectDlg = new SelectDialog(this);
    m_supportPathDlg = new SupportDirsDialog(this);
    m_aboutDlg = new AboutDialog(this);
    m_dipSwitchDlg = new DipswitchDialog(this);
    m_inputDlg = new InputDialog(this);
    m_logDlg = LogDialog::get(this);

    extern void ProgressSetParent(QWidget *);
    ProgressSetParent(this);
}

void MainWindow::createActions()
{
    m_actionLoadGame = new QAction(tr("Load Game"), this);
    m_actionLoadGame->setShortcut(QKeySequence("F6"));
    m_actionCloseGame = new QAction(tr("Close Game"), this);
    m_actionCloseGame->setEnabled(false);
    m_actionConfigureRomPaths = new QAction(tr("Configure ROM paths..."), this);
    m_actionConfigureSupportPaths = new QAction(tr("Configure support paths..."), this);
    m_actionExitEmulator = new QAction(tr("Exit emulator"), this);
    m_actionToogleMenu = new QAction(tr("Toogle Menu"), this);
    m_scutToogleMenu = new QShortcut(QKeySequence(tr("F12")), this);
    m_scutToogleFullscreen = new QShortcut(QKeySequence(tr("Alt+Return")), this);
    m_scutToogleMenu->setContext(Qt::ApplicationShortcut);
    m_actionAbout = new QAction(tr("About FBA"), this);
    m_actionDipswitch = new QAction(tr("Configure DIPs"), this);
    m_actionDipswitch->setEnabled(false);
    m_actionMapGameInputs = new QAction(tr("Map game inputs"), this);
    m_actionMapGameInputs->setEnabled(false);
    m_actionLogWindow = new QAction(tr("Log window"), this);
}

void MainWindow::connectActions()
{
    connect(m_actionLoadGame, SIGNAL(triggered()), this, SLOT(loadGame()));
    connect(m_actionCloseGame, SIGNAL(triggered()), this, SLOT(closeGame()));
    connect(m_actionConfigureRomPaths, SIGNAL(triggered()), m_selectDlg, SLOT(editRomPaths()));
    connect(m_actionExitEmulator, SIGNAL(triggered()), this, SLOT(exitEmulator()));
    connect(m_actionConfigureSupportPaths, SIGNAL(triggered()), m_supportPathDlg, SLOT(exec()));
    connect(m_actionAbout, SIGNAL(triggered()), m_aboutDlg, SLOT(exec()));
    connect(m_actionToogleMenu, SIGNAL(triggered()), this, SLOT(toogleMenu()));
    connect(m_scutToogleMenu, SIGNAL(activated()), this, SLOT(toogleMenu()));
    connect(m_scutToogleFullscreen, SIGNAL(activated()), this, SLOT(toogleFullscreen()));
    connect(m_actionDipswitch, SIGNAL(triggered()), m_dipSwitchDlg, SLOT(exec()));
    connect(m_actionMapGameInputs, SIGNAL(triggered()), m_inputDlg, SLOT(exec()));
    connect(m_actionLogWindow, SIGNAL(triggered()), m_logDlg, SLOT(show()));
}

void MainWindow::enableInGame()
{
    m_actionCloseGame->setEnabled(true);
    m_actionDipswitch->setEnabled(true);
    m_actionMapGameInputs->setEnabled(true);
}

void MainWindow::disableInGame()
{
    m_actionCloseGame->setEnabled(false);
    m_actionDipswitch->setEnabled(false);
    m_actionMapGameInputs->setEnabled(false);

}

void MainWindow::drawLogo()
{
}


void MainWindow::setupInputInterfaces()
{
    m_actionInputPlugins = new QActionGroup(this);
    m_actionInputPlugins->setExclusive(true);
    m_inputInterfaces = QVector<const InputInOut *>::fromStdVector(InputGetInterfaces());

    for (int i = 0; i < m_inputInterfaces.size(); i++) {
        const InputInOut *intf = m_inputInterfaces[i];
        QAction *action = new QAction(QString(intf->szModuleName), this);
        action->setCheckable(true);
        action->setChecked(i ? false : true);
        action->setData(QVariant(i));
        m_actionInputPlugins->addAction(action);
    }

    m_menuInputPlugin->addActions(m_actionInputPlugins->actions());
    connect(m_actionInputPlugins, SIGNAL(triggered(QAction*)),
            this, SLOT(setupInputInterface(QAction*)));
}
