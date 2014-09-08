#ifndef ROMDIRSDIALOG_H
#define ROMDIRSDIALOG_H

#include <QDialog>
#include <QButtonGroup>
#include "qutil.h"

namespace Ui {
class RomDirsDialog;
}

class RomDirsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RomDirsDialog(QWidget *parent = 0);
    ~RomDirsDialog();

public slots:
    int exec();
    void editPath(int no);
    bool load();
    bool save();
private:
    Ui::RomDirsDialog *ui;
    int m_activePath;
    QButtonGroup *m_group;

    util::PathHandler m_handlers[DIRS_MAX];

};

#endif // ROMDIRSDIALOG_H
