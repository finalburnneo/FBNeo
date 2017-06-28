#include "hexspinbox.h"
#include <QHBoxLayout>

HexSpinBox::HexSpinBox(QWidget *parent) :
    QSpinBox(parent)
{
    m_hexValidator = new QRegExpValidator(QRegExp("[0-9a-fA-F]{1,8}"), this);
}

int HexSpinBox::valueFromText(const QString &text) const
{
    return text.toInt(nullptr, 16);
}

QString HexSpinBox::textFromValue(int val) const
{
    return QString::number(val, 16).toUpper();
}

QValidator::State HexSpinBox::validate(QString &input, int &pos) const
{
    return m_hexValidator->validate(input, pos);
}

HexSpinDialog::HexSpinDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle("Edit hex value");
    m_editor = new HexSpinBox(this);
    m_editor->setRange(0, 255);
    m_closeButton = new QPushButton(tr("Close"), this);

    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget(m_editor);
    layout->addWidget(m_closeButton);
    layout->setStretch(0, 1);
    setLayout(layout);
    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(accept()));
}

int HexSpinDialog::value() const
{
    return m_editor->value();
}

void HexSpinDialog::setValue(int value)
{
    m_editor->setValue(value);
}
