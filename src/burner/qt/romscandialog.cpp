#include <QtWidgets>
#include "romscandialog.h"
#include "ui_romscandialog.h"
#include "burner.h"

RomScanDialog::RomScanDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RomScanDialog),
    m_analyzer(this)
{
    ui->setupUi(this);

    setWindowTitle(tr("Wait..."));
    setFixedSize(size());

    m_status.resize(nBurnDrvCount);
    for (int i = 0; i < nBurnDrvCount; i++)
        m_status[i] = 0;

    connect(&m_analyzer, SIGNAL(finished()), this, SLOT(accept()));
    connect(ui->btnCancel, SIGNAL(clicked()), &m_analyzer, SLOT(terminate()));

    connect(&m_analyzer, SIGNAL(setRange(int,int)),
            ui->progressBar, SLOT(setRange(int,int)));
    connect(&m_analyzer, SIGNAL(setValue(int)),
            ui->progressBar, SLOT(setValue(int)));
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

}

bool RomScanDialog::save()
{

}

RomAnalyzer::RomAnalyzer(RomScanDialog *parent) :
    m_scanDlg(parent)
{
}

void RomAnalyzer::run()
{
    QVector<char> &status = m_scanDlg->m_status;

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
    nBurnDrvActive = tmp;
}
