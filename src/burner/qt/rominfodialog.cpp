#include <QtWidgets>
#include "rominfodialog.h"
#include "ui_rominfodialog.h"
#include "burner.h"

RomInfoDialog::RomInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RomInfoDialog)
{
    ui->setupUi(this);
}

RomInfoDialog::~RomInfoDialog()
{
    delete ui;
}

void RomInfoDialog::setDriverNo(int no)
{
    clear();
    m_driverNo = no;

    int tmp = nBurnDrvActive;

    nBurnDrvActive = m_driverNo;
    setWindowTitle(QString(BurnDrvGetText(DRV_FULLNAME)));
    for (int i = 0; i < 0x100; i++) {
        BurnRomInfo info;
        char *romName = nullptr;
        memset(&info, 0, sizeof(info));
        BurnDrvGetRomInfo(&info, i);
        BurnDrvGetRomName(&romName, i, 0);

        if (info.nLen == 0 || info.nType & BRF_BIOS)
            continue;

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, QString(romName));
        item->setText(1, QString::number(info.nLen));


        item->setText(2, QString::number(info.nCrc, 16));

        QStringList type;
        if (info.nType & BRF_ESS)
            type << "Essential";
        if (info.nType & BRF_OPT)
            type << "Optional";
        if (info.nType & BRF_PRG)
            type << "Program";
        if (info.nType & BRF_GRA)
            type << "Graphics";
        if (info.nType & BRF_SND)
            type << "Sound";
        if (info.nType & BRF_BIOS)
            type << "BIOS";
        item->setText(3, type.join(", "));

        if (info.nType & BRF_NODUMP)
            item->setText(4, "No Dump");

        item->setTextAlignment(0, Qt::AlignLeft);
        item->setTextAlignment(1, Qt::AlignRight);
        item->setTextAlignment(2, Qt::AlignRight);
        ui->tvRoms->addTopLevelItem(item);
    }

    nBurnDrvActive = tmp;
}

void RomInfoDialog::clear()
{
    ui->tvRoms->clear();
}
