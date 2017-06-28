#ifndef INPUTDIALOG_H
#define INPUTDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QTreeWidgetItem>
#include "widgets/hexspinbox.h"
#include "inputsetdialog.h"

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
    void inputEdit(QTreeWidgetItem *item, int column);
private:
    void makeList();
    void clear();
    Ui::InputDialog *ui;
    QTimer *m_timer;
    HexSpinDialog *m_hexEditor;
    InputSetDialog *m_inputSet;
};

#endif // INPUTDIALOG_H
