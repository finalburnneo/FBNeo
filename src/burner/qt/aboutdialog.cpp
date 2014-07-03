#include <QtCore>
#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "qutil.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->teLicense->hide();
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSizeConstraint(QLayout::SetFixedSize);

    ui->teLicense->setText(util::loadText(tr(":/resource/license.txt")));
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
