#ifndef SUPPORTDIRSDIALOG_H
#define SUPPORTDIRSDIALOG_H

#include <QDialog>
#include <QButtonGroup>
#include <QLineEdit>
#include <QSettings>
#include "qutil.h"

namespace Ui {
class SupportDirsDialog;
}

class SupportDirsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SupportDirsDialog(QWidget *parent = 0);
    ~SupportDirsDialog();

public slots:
    void editPath(int no);
    int exec();
    bool load();
    bool save();

private:
    void loadPath(QSettings &settings, QString name, int i);
    Ui::SupportDirsDialog *ui;
    QButtonGroup *m_group;
    enum {
        PATH_PREVIEWS = 0,
        PATH_TITLES,
        PATH_HISCORES,
        PATH_SAMPLES,
        PATH_CHEATS,
        PATH_MAX_HANDLERS
    };

    util::PathHandler m_handlers[PATH_MAX_HANDLERS];
};

#endif // SUPPORTDIRSDIALOG_H
