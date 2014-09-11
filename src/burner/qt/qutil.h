#ifndef QUTIL_H
#define QUTIL_H

#include <QString>
#include <QChar>
#include <QLineEdit>
#include "burner.h"

namespace util {

struct PathHandler {
    TCHAR *str;
    QLineEdit *edit;
    int number;
    PathHandler(TCHAR *s=nullptr, QLineEdit *e=nullptr, int n=-1) {
        str = s;
        edit = e;
        number = n;
    }
    void editorToString();
    void stringToEditor();
    void browse(QWidget *parent=nullptr);
};

void fixPath(QString &path);
QString loadText(const QString &fileName);

extern const QString appConfigName;
QString decorateGenre();
QString decorateRomInfo();

}

#endif // QUTIL_H
