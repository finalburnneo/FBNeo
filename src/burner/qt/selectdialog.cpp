#include <QtWidgets>
#include <QtAlgorithms>
#include <QMap>
#include "selectdialog.h"
#include "ui_selectdialog.h"
#include "burner.h"
#include "rominfodialog.h"
#include "qutil.h"

SelectDialog::SelectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SelectDialog)
{
    ui->setupUi(this);

    m_icoNotFound = QIcon(tr(":/resource/tv-not-found.ico"));
    m_icoNotFoundNonEssential = QIcon(tr(":/resource/tv-not-found-non-essential.ico"));
    m_icoNotWorking = QIcon(tr(":/resource/tv-not-working.ico"));

    m_defaultImage = QPixmap(tr(":/resource/splash.bmp"));

    m_romInfoDlg = new RomInfoDialog(this);
    m_romScanner = new RomScanDialog(this);
    m_romPathEditor =  new RomDirsDialog(this);

    setWindowTitle(tr("Select Game"));
    buildDriverTree();

    connect(ui->tvDrivers, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
            this, SLOT(driverChange(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(ui->tvDrivers, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(driverSelect(QTreeWidgetItem*,int)));
    connect(ui->btnRomInfo, SIGNAL(clicked()), this, SLOT(openRomInfo()));
    connect(ui->btnScanRoms, SIGNAL(clicked()), this, SLOT(rescanRoms()));
    connect(ui->btnRomDirs, SIGNAL(clicked()), this, SLOT(editRomPaths()));

    connect(ui->btnPlay, SIGNAL(clicked()), this, SLOT(playGame()));

    connect(ui->ckbShowAvaliable, SIGNAL(toggled(bool)), this, SLOT(itemShowAvaliable(bool)));
    connect(ui->ckbShowUnavaliable, SIGNAL(toggled(bool)), this, SLOT(itemShowUnavaliable(bool)));
    connect(ui->ckbShowClones, SIGNAL(toggled(bool)), this, SLOT(itemShowClones(bool)));
    connect(ui->ckbUseZipnames, SIGNAL(toggled(bool)), this, SLOT(itemShowZipNames(bool)));

    m_selectedDriver = 0;

    m_showAvailable = true;
    m_showUnavailable = true;
    m_showClones = true;
    m_showZipNames = false;
    m_showCount = 0;

    QMenu *contextMenu = new QMenu(ui->tvDrivers);
    m_actionScanThis = new QAction("Rescan this romset", contextMenu);
    ui->tvDrivers->setContextMenuPolicy(Qt::ActionsContextMenu);
    ui->tvDrivers->addAction(m_actionScanThis);
    connect(m_actionScanThis, SIGNAL(triggered()), this, SLOT(rescanRomset()));

    connect(ui->btnSearch, SIGNAL(clicked()), this, SLOT(doSearch()));

    // hide zip name column
    ui->tvDrivers->setColumnHidden(1, true);
    filterDrivers();
}

SelectDialog::~SelectDialog()
{
    delete ui;
}

void SelectDialog::driverChange(QTreeWidgetItem *item, QTreeWidgetItem *prev)
{
    TreeDriverItem *driver = static_cast<TreeDriverItem*>(item);
    int tmp = nBurnDrvActive;
    int flags = DRV_ASCIIONLY;

    m_selectedDriver = driver->driverNo();

    // atualiza as informações sobre o driver
    nBurnDrvActive = driver->driverNo();
    ui->leGameInfo->setText(BurnDrvGetText(flags | DRV_FULLNAME));
    updateTitleScreen();
    updatePreview();
    {
        QString manufacturer = BurnDrvGetTextA(DRV_MANUFACTURER) ?
                    BurnDrvGetText(flags | DRV_MANUFACTURER) : tr("Unknown");
        QString date = BurnDrvGetTextA(DRV_DATE);
        QString system = BurnDrvGetText(flags | DRV_SYSTEM);
        QString prefix = (BurnDrvGetHardwareCode() & HARDWARE_PREFIX_CARTRIDGE)
                ? tr("Cartridge") : tr("Hardware");
        QString releaseInfo = QString("%0 (%1, %2 %3)").arg(manufacturer,
                                                            date,
                                                            system,
                                                            prefix);
        ui->leReleasedBy->setText(releaseInfo);
    }
    ui->leRomName->setText(driver->romName());
    ui->leGenre->setText(util::decorateGenre());
    ui->leRomInfo->setText(util::decorateRomInfo());


    QString comments(BurnDrvGetText(flags | DRV_COMMENT));
    if (BurnDrvGetFlags() & BDF_HISCORE_SUPPORTED)
        comments += tr(", hiscores supported");
    ui->leNotes->setText(comments);


    nBurnDrvActive = tmp;
}

void SelectDialog::driverSelect(QTreeWidgetItem *item, int column)
{
    TreeDriverItem *driver = static_cast<TreeDriverItem*>(item);
    m_selectedDriver = driver->driverNo();
    accept();
}

void SelectDialog::playGame()
{
    TreeDriverItem *driver = static_cast<TreeDriverItem*>(ui->tvDrivers->currentItem());
    if (driver != nullptr) {
        m_selectedDriver = driver->driverNo();
        accept();
    }
}



void SelectDialog::itemShowAvaliable(bool state)
{
    if (state == m_showAvailable)
        return;
    m_showAvailable = state;
    filterDrivers();
}

void SelectDialog::itemShowUnavaliable(bool state)
{
    if (state == m_showUnavailable)
        return;
    m_showUnavailable = state;
    filterDrivers();
}

void SelectDialog::itemShowClones(bool state)
{
    if (state == m_showClones)
        return;
    m_showClones = state;
    filterDrivers();
}

void SelectDialog::itemShowZipNames(bool state)
{
    if (state == m_showZipNames)
        return;
    m_showZipNames = state;

    if (m_showZipNames)
        ui->tvDrivers->setColumnHidden(1, false);
    else
        ui->tvDrivers->setColumnHidden(1, true);
}

void SelectDialog::doSearch()
{
    const QString criteria(ui->leSearch->text());
    if (criteria.isEmpty()) {
        filterDrivers();
        return;
    }

    QList<QTreeWidgetItem *> found =
            ui->tvDrivers->findItems(criteria, Qt::MatchContains | Qt::MatchRecursive);

    // hide all drivers
    foreach (TreeDriverItem *driver, m_parents.values()) {
        if (driver == nullptr)
            continue;
        driver->setHidden(true);
        for (int idx = 0; idx < driver->childCount(); idx++) {
            driver->child(idx)->setHidden(true);
        }
    }

    foreach (QTreeWidgetItem *item, found) {
        TreeDriverItem *driver = static_cast<TreeDriverItem *>(item);
        if (!driver->isParent()) {
            driver->parent()->setHidden(false);
        }
        driver->setHidden(false);
    }
}

void SelectDialog::updateTitleScreen()
{
    QString drv = BurnDrvGetTextA(DRV_NAME);
    QString path = QString(szAppTitlesPath);
    path += QString("%0.png").arg(drv);
    if (QFile(path).exists()) {
        QPixmap p(path);
        ui->imgTitleScreen->setPixmap(p);
    } else {
        ui->imgTitleScreen->setPixmap(m_defaultImage);
    }
}

void SelectDialog::updatePreview()
{
    QString drv = BurnDrvGetTextA(DRV_NAME);
    QString path = QString(szAppPreviewsPath);
    path += QString("%0.png").arg(drv);
    if (QFile(path).exists()) {
        QPixmap p(path);
        ui->imgPreview->setPixmap(p);
    } else {
        ui->imgPreview->setPixmap(m_defaultImage);
    }
}

void SelectDialog::updateLabelCounter()
{
    QString text(tr("Showing %0 of %1 sets").
                 arg(m_showCount).arg(nBurnDrvCount));
    ui->lblCounter->setText(text);
}

int SelectDialog::selectedDriver() const
{
    return m_selectedDriver;
}

void SelectDialog::openRomInfo()
{
    m_romInfoDlg->setDriverNo(m_selectedDriver);
    m_romInfoDlg->exec();
}

void SelectDialog::rescanRoms()
{
    if (m_romScanner->exec() == QDialog::Accepted) {
        filterDrivers();
    }
}

void SelectDialog::rescanRomset()
{
    int tmp = nBurnDrvActive;
    nBurnDrvActive = m_selectedDriver;
    int stat = BzipOpen(1);
    switch (stat) {
    case 0:
        m_romScanner->setStatus(m_selectedDriver, 3);
        break;
    case 2:
        m_romScanner->setStatus(m_selectedDriver, 1);
        break;
    case 1:
    default:
        m_romScanner->setStatus(m_selectedDriver, 0);
        break;
    }
    BzipClose();
    nBurnDrvActive = tmp;
    filterDrivers();
}

void SelectDialog::editRomPaths()
{
    m_romPathEditor->exec();
}

void SelectDialog::buildDriverTree()
{
    int nTmpDrv = nBurnDrvActive;

    // build parent list
    for (int i = 0; i < nBurnDrvCount; i++) {
        nBurnDrvActive = i;
        if (BurnDrvGetFlags() & BDF_BOARDROM)
            continue;

        // skip clones
        if (BurnDrvGetText(DRV_PARENT) != NULL && (BurnDrvGetFlags() & BDF_CLONE))
            continue;

        TreeDriverItem *ditem = new TreeDriverItem();
        ditem->setIcon(0, m_icoNotFound);
        ditem->setFullName(BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME));
        ditem->setText(0, ditem->fullName());
        ditem->setRomName(BurnDrvGetTextA(DRV_NAME));
        ditem->setText(1, ditem->romName());
        ditem->setDriverNo(i);
        ditem->setIsParent(true);
        ui->tvDrivers->addTopLevelItem(ditem);
        m_parents[tr(ditem->romName())] = ditem;
    }

    // build clones tree
    for (int i = 0; i < nBurnDrvCount; i++) {
        nBurnDrvActive = i;

        if (BurnDrvGetFlags() & BDF_BOARDROM)
            continue;

        // skip parents
        if (BurnDrvGetTextA(DRV_PARENT) == NULL || !(BurnDrvGetFlags() & BDF_CLONE))
            continue;

        TreeDriverItem *itemParent = m_parents[tr(BurnDrvGetTextA(DRV_PARENT))];
        if (itemParent) {
            TreeDriverItem *ditem = new TreeDriverItem();
            ditem->setIcon(0, m_icoNotFound);
            ditem->setFullName(BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME));
            ditem->setText(0, ditem->fullName());
            ditem->setRomName(BurnDrvGetTextA(DRV_NAME));
            ditem->setText(1, ditem->romName());
            ditem->setDriverNo(i);
            ditem->setIsParent(false);
            //ditem->setBackgroundColor(0, QColor(200, 230, 255));
            //ditem->setBackgroundColor(1, QColor(210, 240, 255));
            itemParent->addChild(ditem);
        }
    }
    nBurnDrvActive = nTmpDrv;
}

bool SelectDialog::isFiltered(TreeDriverItem *driver)
{
    bool status = m_romScanner->status(driver->driverNo()) ? true : false;

    if (!m_showUnavailable)
        if (!(m_showAvailable && status))
            return false;
    return true;
}

void SelectDialog::filterDrivers()
{
    auto setupIcon = [&](int stat, TreeDriverItem *item) -> void {
        switch (stat) {
        case 0: item->setIcon(0, m_icoNotFound); break;
        case 2:
        case 3: item->setIcon(0, m_icoNotFoundNonEssential); break;
        case 1: item->setIcon(0, m_icoNotWorking); break;
        }
    };

    m_showCount = 0;
    foreach (TreeDriverItem *driver, m_parents.values()) {
        // skip it
        if (driver == nullptr)
            continue;

        // show all
        driver->setHidden(true);

        // setup icon
        int stat = m_romScanner->status(driver->driverNo());
        setupIcon(stat, driver);

        if (!isFiltered(driver))
            continue;

        driver->setHidden(false);

        for (int idx = 0; idx < driver->childCount(); idx++) {
            TreeDriverItem *clone = static_cast<TreeDriverItem *>(driver->child(idx));
            clone->setHidden(true);

            int cstat = m_romScanner->status(driver->driverNo());
            setupIcon(cstat, clone);

            if (m_showClones) {
                if (!isFiltered(clone))
                    continue;
                clone->setHidden(false);
                m_showCount++;
            }
        }
        m_showCount++;
    }
    updateLabelCounter();
}

TreeDriverItem::TreeDriverItem() : QTreeWidgetItem()
{

}

int TreeDriverItem::driverNo() const
{
    return m_driverNo;
}

void TreeDriverItem::setDriverNo(int driverNo)
{
    m_driverNo = driverNo;
}

const char *TreeDriverItem::romName() const
{
    return m_romName;
}

void TreeDriverItem::setRomName(const char *romName)
{
    m_romName = romName;
}

const char *TreeDriverItem::fullName() const
{
    return m_fullName;
}

void TreeDriverItem::setFullName(const char *fullName)
{
    m_fullName = fullName;
}

bool TreeDriverItem::isParent() const
{
    return m_isParent;
}

void TreeDriverItem::setIsParent(bool isParent)
{
    m_isParent = isParent;
}
