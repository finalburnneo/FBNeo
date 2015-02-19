#include <QtWidgets>
#include "inputsetdialog.h"
#include "ui_inputsetdialog.h"
#include "burner.h"

InputSetDialog::InputSetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InputSetDialog)
{
    ui->setupUi(this);
}

InputSetDialog::~InputSetDialog()
{
    delete ui;
}
