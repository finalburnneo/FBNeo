#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QAction>
#include <QActionGroup>
#include <QShortcut>
#include <QMenu>
#include <QThread>
#include <QVector>
#include <QImage>
#include "selectdialog.h"
#include "qrubyviewport.h"
#include "qaudiointerface.h"
#include "supportdirsdialog.h"
#include "aboutdialog.h"
#include "dipswitchdialog.h"
#include "emuworker.h"
#include "burner.h"


class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int main(QApplication &app);
signals:
public slots:
    void loadGame();
    void exitEmulator();
    void closeGame();
    void setupInputInterface(QAction *action);
    void toogleMenu();
    void toogleFullscreen();
    void closeEvent(QCloseEvent *event);
    void resizeEvent(QResizeEvent *event);
private:
    void createDefaultDirs();
    void createMenus();
    void createControls();
    void createActions();
    void connectActions();
    void enableInGame();
    void disableInGame();
    void drawLogo();
    void setupRubyViewport();
    int m_game;
    bool m_isRunning;
    QRubyViewport *m_viewport;
    QAudioInterface *m_audio;
    EmuWorker *m_emulation;
    SelectDialog *m_selectDlg;
    SupportDirsDialog *m_supportPathDlg;
    AboutDialog *m_aboutDlg;
    DipswitchDialog *m_dipSwitchDlg;
    QMenu *m_menuGame;
    QMenu *m_menuMisc;
    QMenu *m_menuHelp;
    QMenu *m_menuInput;
    QMenu *m_menuInputPlugin;
    QAction *m_actionConfigureRomPaths;
    QAction *m_actionConfigureSupportPaths;
    QAction *m_actionAbout;
    QAction *m_actionLoadGame;
    QAction *m_actionExitEmulator;
    QAction *m_actionCloseGame;
    QAction *m_actionToogleMenu;
    QAction *m_actionDipswitch;
    QShortcut *m_scutToogleMenu;
    QShortcut *m_scutToogleFullscreen;
    QImage m_logo;

    void setupInputInterfaces();
    QVector<const InputInOut *> m_inputInterfaces;
    QActionGroup *m_actionInputPlugins;
    bool m_closeApp;
};

#endif // MAINWINDOW_H
