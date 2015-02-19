#ifndef INPUTSETDIALOG_H
#define INPUTSETDIALOG_H

#include <QDialog>

namespace Ui {
class InputSetDialog;
}

class InputSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InputSetDialog(QWidget *parent = 0);
    ~InputSetDialog();

private:
    Ui::InputSetDialog *ui;
};

#endif // INPUTSETDIALOG_H
