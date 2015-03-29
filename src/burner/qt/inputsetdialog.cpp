#include <QtWidgets>
#include "inputsetdialog.h"
#include "ui_inputsetdialog.h"
#include "burner.h"


InputSetDialog::InputSetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InputSetDialog)
{
    ui->setupUi(this);

    m_timer = new QTimer(this);
    m_timer->setInterval(10);
    m_state = 0;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(updateInputs()));
}

InputSetDialog::~InputSetDialog()
{
    delete ui;
}

void InputSetDialog::setInput(int i)
{
    m_input = i;

    struct BurnInputInfo bii;
    BurnDrvGetInputInfo(&bii, i);

    ui->txtInput->setText(bii.szName);
}

int InputSetDialog::exec()
{
    m_lastFound = 0;
    ui->lblInfo->setText("");
    m_timer->start();
    int ret = QDialog::exec();
    m_timer->stop();
    return ret;
}

void InputSetDialog::updateInputs()
{
    struct GameInp *pgi = GameInp;
    pgi += m_input;

    int i = InputFind(4);

    if (i != m_lastFound) {

        if (i != -1) {
            struct GameInp mpgi;

            memcpy(&mpgi, pgi, sizeof(struct GameInp));
            mpgi.Input.Constant.nConst = i;

            auto str = InpToDesc(&mpgi);
            ui->lblInfo->setText(str);
        }
        else
        {
            ui->lblInfo->setText("");
        }
        m_lastFound = i;
    }
}
