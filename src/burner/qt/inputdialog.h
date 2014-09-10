#ifndef INPUTDIALOG_H
#define INPUTDIALOG_H

#include <QDialog>
#include <QTimer>

namespace Ui {
class InputDialog;
}

class InputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InputDialog(QWidget *parent = 0);
    ~InputDialog();

public slots:
    int exec();
    void updateInputs();

private:
    void makeList();
    void clear();
    Ui::InputDialog *ui;
    QTimer *m_timer;
};

#endif // INPUTDIALOG_H
