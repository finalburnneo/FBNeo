#ifndef DIPSWITCHDIALOG_H
#define DIPSWITCHDIALOG_H

#include <QDialog>
#include <QTreeWidgetItem>

namespace Ui {
class DipswitchDialog;
}

class DipswitchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DipswitchDialog(QWidget *parent = 0);
    ~DipswitchDialog();

public slots:
    int exec();
    void reset();
    void dipChange(QTreeWidgetItem * item, QTreeWidgetItem * prev);
    void dipValueChange(int index);
private:
    bool checkSetting(int i);
    void getDipOffset();
    void makeList();
    void clearList();
    Ui::DipswitchDialog *ui;
    unsigned m_dipOffset;
    unsigned m_dipGroup;
    bool m_dipChanging;
};

#endif // DIPSWITCHDIALOG_H
