#ifndef QINPUTINTERFACE_H
#define QINPUTINTERFACE_H

#include <QObject>
#include <QVector>
#include "SDL.h"

#define QINPUT_MAX_KEYS 256

class QInputInterface : public QObject
{
    Q_OBJECT
    explicit QInputInterface(QObject *parent = 0);
    static QInputInterface *m_onlyInstance;
    char m_keys[QINPUT_MAX_KEYS];
    bool m_isInitialized;
    QVector<SDL_Joystick*> m_joysticks;
    bool joystickInit();
    void joystickExit();
public:
    ~QInputInterface();
    static QInputInterface *get(QObject *parent=nullptr);
    void install();
    void uninstall();
    int joystickCount();
    void joystickUpdate();
    int joystickHat(int joy, int hat);
    int joystickAxis(int joy, int axis);
    int joystickButton(int joy, int button);
    void snapshot(char *buffer, int keys=QINPUT_MAX_KEYS);
    int state(int key);
protected:
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // QINPUTINTERFACE_H
