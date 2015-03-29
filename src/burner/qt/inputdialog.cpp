#include <QtWidgets>
#include "inputdialog.h"
#include "ui_inputdialog.h"
#include "burner.h"

InputDialog::InputDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InputDialog)
{
    ui->setupUi(this);
    ui->tvInputs->setColumnWidth(0, 200);
    ui->tvInputs->setColumnWidth(1, 200);

    setWindowTitle("Map Game Inputs");

    m_hexEditor = new HexSpinDialog(this);
    m_inputSet = new InputSetDialog(this);

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(updateInputs()));
    m_timer->setInterval(30);

    connect(ui->tvInputs, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(inputEdit(QTreeWidgetItem*,int)));
}

InputDialog::~InputDialog()
{
    delete ui;
}

int InputDialog::exec()
{
    makeList();
    m_timer->start();
    QDialog::exec();
    m_timer->stop();
}

void InputDialog::updateInputs()
{
    InputMake(true);

    for (int i = 0; i < ui->tvInputs->topLevelItemCount(); i++) {
        QTreeWidgetItem *item = ui->tvInputs->topLevelItem(i);
        struct GameInp *pgi = GameInp;

        pgi += item->data(0, Qt::UserRole).toInt();

        if (pgi->nType == 0)
            continue;

        int value = pgi->Input.nVal;
        int lastValue = item->data(2, Qt::UserRole).toInt();

        if (value == lastValue)
            continue;

        switch (pgi->nType) {
        case BIT_DIGITAL:
            if (value)
                item->setText(2, "ON");
            else
                item->setText(2, "");
            break;
        }
        item->setData(2, Qt::UserRole, value);
    }
}

void InputDialog::inputEdit(QTreeWidgetItem *item, int column)
{
    struct GameInp *pgi = GameInp;

    pgi += item->data(0, Qt::UserRole).toInt();

    switch (pgi->nType) {
    case BIT_DIPSWITCH: {
        if (pgi->nInput & BIT_GROUP_CONSTANT)
            break;

        m_hexEditor->setValue(pgi->Input.nVal);
        m_hexEditor->exec();

        pgi->nInput = GIT_CONSTANT;
        pgi->Input.Constant.nConst = (m_hexEditor->value() & 0xFF);
        auto value = InpToDesc(pgi);
        item->setText(1, value);
        break;
    }
    default: {
        m_inputSet->setInput(item->data(0, Qt::UserRole).toInt());
        m_inputSet->exec();
        break;
    }
    }
}

void InputDialog::makeList()
{
    static const QColor dipColor(255, 255, 210);
    clear();

    struct GameInp *pgi = GameInp;

    // Add all the input names to the list
    for (unsigned int i = 0; i < nGameInpCount; i++) {
        struct BurnInputInfo bii;

        // Get the name of the input
        bii.szName = nullptr;
        BurnDrvGetInputInfo(&bii, i);

        // skip unused inputs
        if (bii.pVal == nullptr)
            continue;

        if (bii.szName == nullptr)
            bii.szName = "";

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setData(0, Qt::UserRole, i);
        item->setData(2, Qt::UserRole, 0);

        if (pgi->nType == BIT_DIPSWITCH) {
            for (int j = 0; j < ui->tvInputs->columnCount(); j++)
                item->setBackgroundColor(j, dipColor);
        }

        item->setText(0, bii.szName);

        if (pgi->Input.pVal != nullptr) {
            auto value = InpToDesc(pgi);
            item->setText(1, value);
        }

        ui->tvInputs->addTopLevelItem(item);
        pgi++;
    }
}

void InputDialog::clear()
{
    while (ui->tvInputs->topLevelItemCount() > 0)
        ui->tvInputs->takeTopLevelItem(0);
}
