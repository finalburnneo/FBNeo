#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include "burner.h"

namespace Ui {
class LogDialog;
}

class LogDialog : public QDialog
{
    Q_OBJECT
    explicit LogDialog(QWidget *parent = 0);
    static LogDialog *m_instance;
public:
    ~LogDialog();

    static LogDialog *get(QWidget *parent = 0);
    static INT32 qprintf(INT32 level, TCHAR *fmt, ...);
private:
    Ui::LogDialog *ui;
};


#endif // LOGDIALOG_H
