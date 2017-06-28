#ifndef ROMINFODIALOG_H
#define ROMINFODIALOG_H

#include <QDialog>

namespace Ui {
class RomInfoDialog;
}

class RomInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RomInfoDialog(QWidget *parent = 0);
    ~RomInfoDialog();

    void setDriverNo(int no);
private:
    void clear();
    int m_driverNo;
    Ui::RomInfoDialog *ui;
};

#endif // ROMINFODIALOG_H
