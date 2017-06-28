#include <cstring>
#include <cstdarg>
#include "logdialog.h"
#include "ui_logdialog.h"
#include "burner.h"

LogDialog *LogDialog::m_instance = nullptr;

LogDialog::LogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);
    setWindowTitle("Messages");
    bprintf = &LogDialog::qprintf;
}

LogDialog::~LogDialog()
{
    delete ui;
    m_instance = nullptr;
}

INT32 LogDialog::qprintf(INT32 level, TCHAR *fmt, ...)
{

    LogDialog *p = LogDialog::get();
    if (!p)
        return 0;

    static const QColor colorLevels[4] = {
        QColor( 0,   0,   0),
        QColor( 0,   0, 127),
        QColor( 0, 127,   0),
        QColor(127,  0,   0)
    };
    if (level >= 4)
        level = 0;

    const int maxBuffer = 1024;
    char strBuffer[maxBuffer];

    va_list args;
    va_start (args, fmt);
    ::vsnprintf(strBuffer, maxBuffer, fmt, args);
    va_end(args);

    p->ui->teLogger->setTextColor(colorLevels[level]);
    p->ui->teLogger->append(strBuffer);
    return 1;
}

LogDialog *LogDialog::get(QWidget *parent)
{
    if (!m_instance)
        m_instance = new LogDialog(parent);
    return m_instance;
}

