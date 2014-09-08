#include <QFileDialog>
#include <QDebug>
#include "qutil.h"

namespace util {

const QString appConfigName("config/fbaqt.ini");

void fixPath(QString &path)
{
    if (!path.isEmpty()) {
        QChar lastChar = path.at(path.size() - 1);
        if (lastChar != QChar('\\') && lastChar != QChar('/'))
            path.append('/');
    }
}

void PathHandler::editorToString()
{
    if (str == nullptr || edit == nullptr)
        return;
    QByteArray bytes;
    QString path = edit->text();
    fixPath(path);
    bytes = path.toLocal8Bit();
    _sntprintf(str, MAX_PATH, bytes.data());
}

void PathHandler::stringToEditor()
{
    if (edit == nullptr)
        return;
    edit->setText(QString(str));
}

void PathHandler::browse(QWidget *parent)
{
    QString path = QFileDialog::getExistingDirectory(parent);
    if (path.isEmpty())
        return;
    if (edit == nullptr)
        return;
    edit->setText(path);
}

QString loadText(const QString &fileName)
{
    QFile file(fileName);
    if (file.exists()) {
        file.open(QFile::ReadOnly | QFile::Text);
        return QString(file.readAll());
    }
    return QString();
}

}
