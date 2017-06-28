#include <QtWidgets>
#include "romscandialog.h"
#include "ui_romscandialog.h"
#include "burner.h"
#include "version.h"

RomScanDialog::RomScanDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RomScanDialog),
    m_analyzer(this),
    m_settingsName("config/fbaqt.roms.ini")
{
    ui->setupUi(this);

    setWindowTitle(tr("Wait..."));
    setFixedSize(size());

    if (!load()) {
        m_status.resize(nBurnDrvCount);
        for (int i = 0; i < nBurnDrvCount; i++)
            m_status[i] = 0;
    }

    connect(&m_analyzer, SIGNAL(finished()), this, SLOT(accept()));
    connect(ui->btnCancel, SIGNAL(clicked()), &m_analyzer, SLOT(terminate()));

    connect(&m_analyzer, SIGNAL(setRange(int,int)),
            ui->progressBar, SLOT(setRange(int,int)));
    connect(&m_analyzer, SIGNAL(setValue(int)),
            ui->progressBar, SLOT(setValue(int)));
    connect(&m_analyzer, SIGNAL(done()), this, SLOT(save()));
    connect(this, SIGNAL(rejected()), &m_analyzer, SLOT(terminate()));
}

RomScanDialog::~RomScanDialog()
{
    delete ui;
}

int RomScanDialog::status(int drvNo)
{
    if (drvNo < 0 && drvNo >= nBurnDrvCount)
        return 0;
    return m_status[drvNo];
}

void RomScanDialog::setStatus(int drvNo, char stat)
{
    if (drvNo < 0 && drvNo >= nBurnDrvCount)
        return;
    m_status[drvNo] = stat;
}

void RomScanDialog::cancel()
{
    close();
}

void RomScanDialog::showEvent(QShowEvent *event)
{
    ui->progressBar->setValue(0);
    m_analyzer.start();
}

bool RomScanDialog::load()
{
    QSettings settings(m_settingsName, QSettings::IniFormat);
    if (settings.status() == QSettings::AccessError)
        return false;

    if (settings.value("version") != BURN_VERSION)
        return false;

    if (settings.value("drivers") != nBurnDrvCount)
        return false;

    m_status = settings.value("status").toByteArray();
    return true;
}

bool RomScanDialog::save()
{
    QSettings settings(m_settingsName, QSettings::IniFormat);
    if (!settings.isWritable())
        return false;
    settings.setValue("version", BURN_VERSION);
    settings.setValue("drivers", nBurnDrvCount);
    settings.setValue("status", m_status);
    settings.sync();
    return true;
}

RomAnalyzer::RomAnalyzer(RomScanDialog *parent) :
    m_scanDlg(parent)
{
}

void RomAnalyzer::run()
{
    QByteArray &status = m_scanDlg->m_status;

    if (status.size() != nBurnDrvCount)
        status.resize(nBurnDrvCount);

    int tmp = nBurnDrvActive;

    emit setRange(0, nBurnDrvCount - 1);

    for (int i = 0; i < nBurnDrvCount; i++) {
        nBurnDrvActive = i;

        emit setValue(i);

        int stat = BzipOpen(1);
        switch (stat) {
        case 0:
            status[i] = 3;
            break;
        case 2:
            status[i] = 1;
            break;
        case 1:
            status[i] = 0;
            break;
        default:
            break;
        }
        BzipClose();
    }
    msleep(100);
    emit done();
    nBurnDrvActive = tmp;
}
