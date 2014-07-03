#include <QProgressDialog>
#include <QDebug>
#include <QApplication>
#include "burner.h"

QProgressDialog *dlgProgress = nullptr;

void ProgressCreate()
{
    if (dlgProgress == nullptr) {
        dlgProgress = new QProgressDialog();
        dlgProgress->setModal(true);
        dlgProgress->setRange(0, 100);
        dlgProgress->setWindowTitle(QObject::tr("Working..."));
    }
    dlgProgress->setValue(0);
    dlgProgress->show();
    QApplication::processEvents();
}

void ProgressDestroy()
{
    dlgProgress->close();
}

int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs)
{
    if (dlgProgress != nullptr && dlgProgress->isVisible()) {
        dlgProgress->setValue(dlgProgress->value() + dProgress * 100);
        dlgProgress->setLabelText(pszText);
        QApplication::processEvents();
    }
}

bool ProgressIsRunning()
{
    if (dlgProgress != nullptr) {
        if (dlgProgress->isVisible())
            return true;
    }
    return false;
}
