#ifndef INPUTSETDIALOG_H
#define INPUTSETDIALOG_H

#include <QDialog>
#include <QTimer>

namespace Ui {
class InputSetDialog;
}

class InputSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InputSetDialog(QWidget *parent = 0);
    ~InputSetDialog();

    void setInput(int i);

    int exec();
public slots:
    void updateInputs();
private:
    int m_state;
    int m_lastFound;
    int m_input;
    Ui::InputSetDialog *ui;
    QTimer *m_timer;
};

#endif // INPUTSETDIALOG_H
