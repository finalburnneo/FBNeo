#ifndef SELECTDIALOG_H
#define SELECTDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMap>
#include <QPixmap>
#include "rominfodialog.h"
#include "romscandialog.h"
#include "romdirsdialog.h"

namespace Ui {
class SelectDialog;
}

class TreeDriverItem : public QTreeWidgetItem
{
public:
    TreeDriverItem();

    int driverNo() const;
    void setDriverNo(int driverNo);

    const char *romName() const;
    void setRomName(const char *romName);
    const char *fullName() const;
    void setFullName(const char *fullName);

    bool isParent() const;
    void setIsParent(bool isParent);

private:
    int m_driverNo;
    const char *m_romName;
    const char *m_fullName;
    bool m_isParent;
};

class SelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SelectDialog(QWidget *parent = 0);
    ~SelectDialog();

    int selectedDriver() const;
public slots:
    void openRomInfo();
    void rescanRoms();
    void rescanRomset();
    void editRomPaths();
    void driverChange(QTreeWidgetItem * item, QTreeWidgetItem * prev);
    void driverSelect(QTreeWidgetItem * item, int column);
    void playGame();

    void itemShowAvaliable(bool state);
    void itemShowUnavaliable(bool state);
    void itemShowClones(bool state);
    void itemShowZipNames(bool state);
    void doSearch();

signals:
    void driverSelected(int no);
private:
    int m_selectedDriver;
    void updateTitleScreen();
    void updatePreview();

    void updateLabelCounter();
    void buildDriverTree();
    bool isFiltered(TreeDriverItem *driver);
    void filterDrivers();

    Ui::SelectDialog *ui;
    QIcon m_icoNotFound;
    QIcon m_icoNotFoundNonEssential;
    QIcon m_icoNotWorking;
    RomInfoDialog *m_romInfoDlg;
    RomScanDialog *m_romScanner;
    RomDirsDialog *m_romPathEditor;
    QMap<QString, TreeDriverItem*> m_parents;

    bool m_showAvailable;
    bool m_showUnavailable;
    bool m_showClones;
    bool m_showZipNames;
    int m_showCount;

    QPixmap m_defaultImage;
    QAction *m_actionScanThis;
};

#endif // SELECTDIALOG_H
