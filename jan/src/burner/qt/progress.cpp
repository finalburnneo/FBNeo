#include <QProgressDialog>
#include <QDebug>
#include <QApplication>
#include "burner.h"

static QProgressDialog *dlgProgress = nullptr;
static QWidget *dlgProgressParent = nullptr;

static int nProgressPosBurn = 0;
static int nProgressPosBurner = 0;
static int nProgressMin = 0;
static int nProgressMax = 0;

static int ProgressSetRangeBurn(double dProgressRange)
{

    nProgressMin = -(int)((double)nProgressMax * dProgressRange);
    dlgProgress->setRange(nProgressMin, nProgressMax);
    return 0;
}

static int ProgressUpdateBurn(double dProgress, const TCHAR *pszText, bool bAbs)
{

    if (pszText) {
        dlgProgress->setLabelText(pszText);
    }

    if (bAbs) {
        nProgressPosBurn = (int)((double)(-nProgressMin) * dProgress);
        if (nProgressPosBurn > -nProgressMin) {
            nProgressPosBurn = -nProgressMin;
        }
        dlgProgress->setValue(nProgressMin + nProgressPosBurn + nProgressPosBurner);
    } else {
        if (dProgress) {
            nProgressPosBurn += (int)((double)(-nProgressMin) * dProgress);
            if (nProgressPosBurn > -nProgressMin) {
                nProgressPosBurn = -nProgressMin;
            }
            dlgProgress->setValue(nProgressMin + nProgressPosBurn + nProgressPosBurner);
        }
    }
    QApplication::processEvents();
    return 0;
}

void ProgressSetParent(QWidget *parent)
{

    dlgProgressParent = parent;
}

void ProgressCreate()
{
    nProgressMin = 0;
    nProgressMax = 1 << 30;

    nProgressPosBurn = 0;
    nProgressPosBurner = 0;

    BurnExtProgressRangeCallback = ProgressSetRangeBurn;
    BurnExtProgressUpdateCallback = ProgressUpdateBurn;

    if (dlgProgress == nullptr) {
        dlgProgress = new QProgressDialog(dlgProgressParent);
        dlgProgress->setWindowModality(Qt::WindowModal);
        dlgProgress->setRange(nProgressMin, nProgressMax);
        dlgProgress->setWindowTitle(QObject::tr("Working..."));
        dlgProgress->setAutoClose(true);
        dlgProgress->setAutoReset(true);
        dlgProgress->setMinimumDuration(500);
    }
    dlgProgress->setValue(0);
}

void ProgressDestroy()
{
    dlgProgress->setValue(nProgressMax);
    dlgProgress->close();
    QApplication::processEvents();
    BurnExtProgressRangeCallback = NULL;
    BurnExtProgressUpdateCallback = NULL;
}

int ProgressUpdateBurner(double dProgress, const TCHAR *pszText, bool bAbs)
{
    if (pszText) {
        dlgProgress->setLabelText(pszText);
    }

    if (bAbs) {
        nProgressPosBurner = (int)((double)nProgressMax * dProgress);
        if (nProgressPosBurner > nProgressMax) {
            nProgressPosBurner = nProgressMax;
        }
        dlgProgress->setValue(nProgressMin + nProgressPosBurn + nProgressPosBurner);
    } else {
        if (dProgress) {
            if (nProgressPosBurner > nProgressMax) {
                nProgressPosBurner = nProgressMax;
            }
            nProgressPosBurner += (int)((double)nProgressMax * dProgress);
            dlgProgress->setValue(nProgressMin + nProgressPosBurn + nProgressPosBurner);
        }
    }
    return 0;
}

bool ProgressIsRunning()
{
    if (dlgProgress != nullptr) {
        if (dlgProgress->isVisible())
            return true;
    }
    return false;
}
