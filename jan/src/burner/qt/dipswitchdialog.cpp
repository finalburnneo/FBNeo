#include <QtWidgets>
#include "dipswitchdialog.h"
#include "ui_dipswitchdialog.h"
#include "burner.h"

DipswitchDialog::DipswitchDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DipswitchDialog)
{
    ui->setupUi(this);
    setWindowTitle("DIPSwitches");
    connect(ui->tvSettings, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
            this, SLOT(dipChange(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(ui->cbValues, SIGNAL(currentIndexChanged(int)), this, SLOT(dipValueChange(int)));
    connect(ui->btnDefault, SIGNAL(clicked()), this, SLOT(reset()));
    m_dipGroup = 0;
    m_dipChanging = false;
}

DipswitchDialog::~DipswitchDialog()
{
    delete ui;
}

int DipswitchDialog::exec()
{
    getDipOffset();
    makeList();
    int ret = QDialog::exec();
    return ret;
}

void DipswitchDialog::reset()
{
    int i = 0;
    BurnDIPInfo bdi;
    struct GameInp* pgi;

    getDipOffset();

    while (BurnDrvGetDIPInfo(&bdi, i) == 0) {
        if (bdi.nFlags == 0xFF) {
            pgi = GameInp + bdi.nInput + m_dipOffset;
            pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~bdi.nMask) | (bdi.nSetting & bdi.nMask);
        }
        i++;
    }

    makeList();
}

void DipswitchDialog::dipChange(QTreeWidgetItem *item, QTreeWidgetItem *prev)
{
    if (item == nullptr)
        return;

    m_dipGroup = item->data(0, Qt::UserRole).toInt();

    BurnDIPInfo bdiGroup;
    BurnDrvGetDIPInfo(&bdiGroup, m_dipGroup);

    qDebug() << "DIP Change" << bdiGroup.szText;
    m_dipChanging = true;
    ui->cbValues->clear();

    int nCurrentSetting = 0;
    for (int i = 0, j = 0; i < bdiGroup.nSetting; i++) {
        TCHAR szText[256];
        BurnDIPInfo bdi;

        do {
            BurnDrvGetDIPInfo(&bdi, m_dipGroup + 1 + j++);
        } while (bdi.nFlags == 0);

        if (bdiGroup.szText) {
            _stprintf(szText, _T("%hs: %hs"), bdiGroup.szText, bdi.szText);
        } else {
            _stprintf(szText, _T("%hs"), bdi.szText);
        }
        ui->cbValues->insertItem(i, szText);

        if (checkSetting(m_dipGroup + j)) {
            nCurrentSetting = i;
        }
    }
    ui->cbValues->setCurrentIndex(nCurrentSetting);
    m_dipChanging = false;
}

void DipswitchDialog::dipValueChange(int index)
{
    if (ui->cbValues->count() <= 0 || m_dipChanging)
        return;

    BurnDIPInfo bdi = {0, 0, 0, 0, NULL};
    struct GameInp *pgi;
    int j = 0, k = 0;
    for (int i = 0; i <= index; i++) {
        do {
            BurnDrvGetDIPInfo(&bdi, m_dipGroup + 1 + j++);
        } while (bdi.nFlags == 0);
    }
    pgi = GameInp + bdi.nInput + m_dipOffset;
    pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~bdi.nMask) | (bdi.nSetting & bdi.nMask);
    if (bdi.nFlags & 0x40) {
        while (BurnDrvGetDIPInfo(&bdi, m_dipGroup + 1 + j++) == 0) {
            if (bdi.nFlags == 0) {
                pgi = GameInp + bdi.nInput + m_dipOffset;
                pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~bdi.nMask) | (bdi.nSetting & bdi.nMask);
            } else {
                break;
            }
        }
    }

    QTreeWidgetItem *item = ui->tvSettings->currentItem();
    item->setText(1, bdi.szText);
}

bool DipswitchDialog::checkSetting(int i)
{
    BurnDIPInfo bdi;
    BurnDrvGetDIPInfo(&bdi, i);
    struct GameInp* pgi = GameInp + bdi.nInput + m_dipOffset;

    if ((pgi->Input.Constant.nConst & bdi.nMask) == bdi.nSetting) {
        unsigned char nFlags = bdi.nFlags;
        if ((nFlags & 0x0F) <= 1) {
            return true;
        } else {
            for (int j = 1; j < (nFlags & 0x0F); j++) {
                BurnDrvGetDIPInfo(&bdi, i + j);
                pgi = GameInp + bdi.nInput + m_dipOffset;
                if (nFlags & 0x80) {
                    if ((pgi->Input.Constant.nConst & bdi.nMask) == bdi.nSetting) {
                        return false;
                    }
                } else {
                    if ((pgi->Input.Constant.nConst & bdi.nMask) != bdi.nSetting) {
                        return false;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

void DipswitchDialog::getDipOffset()
{
    BurnDIPInfo bdi;
    m_dipOffset = 0;
    for (int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++) {
        if (bdi.nFlags == 0xF0) {
            m_dipOffset = bdi.nInput;
            break;
        }
    }
}

void DipswitchDialog::makeList()
{
    clearList();
    BurnDIPInfo bdi;
    unsigned int i = 0, j = 0, k = 0;
    char* pDIPGroup = NULL;
    while (BurnDrvGetDIPInfo(&bdi, i) == 0) {
        if ((bdi.nFlags & 0xF0) == 0xF0) {
            if (bdi.nFlags == 0xFE || bdi.nFlags == 0xFD) {
                pDIPGroup = bdi.szText;
                k = i;
            }
            i++;
        } else {
            if (checkSetting(i)) {
                QTreeWidgetItem *item = new QTreeWidgetItem();

                item->setText(0, pDIPGroup);
                item->setText(1, bdi.szText);
                item->setData(0, Qt::UserRole, k);
                item->setData(1, Qt::UserRole, k);
                ui->tvSettings->addTopLevelItem(item);
                j++;
            }
            i += (bdi.nFlags & 0x0F);
        }
    }

}

void DipswitchDialog::clearList()
{
    while (ui->tvSettings->topLevelItemCount() > 0)
        ui->tvSettings->takeTopLevelItem(0);
}
