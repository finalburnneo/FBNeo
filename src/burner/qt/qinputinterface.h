#ifndef QINPUTINTERFACE_H
#define QINPUTINTERFACE_H

#include <QObject>
#include <QElapsedTimer>

#define QINPUT_MAX_KEYS 256

class QInputInterface : public QObject
{
    Q_OBJECT
    explicit QInputInterface(QObject *parent = 0);
    static QInputInterface *m_onlyInstance;
    char m_keys[QINPUT_MAX_KEYS];
    QElapsedTimer m_timer;
    bool m_isInitialized;
public:
    ~QInputInterface();
    static QInputInterface *get(QObject *parent=nullptr);
    void install();
    void uninstall();
    void snapshot(char *buffer, int keys=QINPUT_MAX_KEYS);
    int state(int key);
signals:
public slots:
protected:
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // QINPUTINTERFACE_H
