#include <QtCore>
#include <QGraphicsEffect>
#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "qutil.h"
#include "version.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->teLicense->hide();
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSizeConstraint(QLayout::SetFixedSize);

    ui->teLicense->setText(util::loadText(tr(":/license.txt")));

    ui->lblVersion->setText(QString(" FBAlpha %1.%2.%3.%4")
                            .arg(VER_MAJOR)
                            .arg(VER_MINOR)
                            .arg(VER_BETA)
                            .arg(VER_ALPHA));
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setColor(QColor(255, 102, 0));
    shadow->setXOffset(0);
    shadow->setYOffset(0);
    shadow->setBlurRadius(10);
    ui->lblVersion->setFont(QFont("Monospace", 12));
    ui->lblVersion->setGraphicsEffect(shadow);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
