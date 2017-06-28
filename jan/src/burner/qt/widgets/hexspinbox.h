#ifndef HEXSPINBOX_H
#define HEXSPINBOX_H

#include <QSpinBox>
#include <QRegExpValidator>
#include <QPushButton>
#include <QDialog>

class HexSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    explicit HexSpinBox(QWidget *parent = 0);

protected:
    int valueFromText(const QString &text) const;
    QString textFromValue(int val) const;
    QValidator::State validate(QString &input, int &pos) const;

private:
    QRegExpValidator *m_hexValidator;

};

class HexSpinDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HexSpinDialog(QWidget *parent = 0);
public slots:
    int value() const;
    void setValue(int value);
private:
    QPushButton *m_closeButton;
    HexSpinBox *m_editor;
};


#endif // HEXSPINBOX_H
