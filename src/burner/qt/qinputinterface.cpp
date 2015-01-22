#include <QtWidgets>
#include <QVector>
#include "SDL.h"
#include "qinputinterface.h"
#include "burner.h"

QInputInterface *qInput = nullptr;

static QVector<SDL_Joystick*> qJoysticks;

int QtKToFBAK(int key);
static char qKeyboardState[QINPUT_MAX_KEYS] = { 0 };
static bool bKeyboardRead = false;
static bool bJoystickRead = false;

int QtInputSetCooperativeLevel(bool bExclusive, bool)
{
    qDebug() << __func__;
    return 0;
}

int QtInputExit()
{
    qDebug() << __func__;
    if (qInput) {
        delete qInput;
        qInput = nullptr;
    }

    return 0;
}

int QtInputInit()
{
    qDebug() << __func__;
    qInput = QInputInterface::get();
    qInput->install();
    memset(qKeyboardState, 0, QINPUT_MAX_KEYS);
    bKeyboardRead = false;

    return 0;
}

int QtInputStart()
{
    bKeyboardRead = false;
    bJoystickRead = false;
    return 0;
}

static int ReadJoystick()
{
    if (bJoystickRead)
        return 0;
    qInput->joystickUpdate();
    bJoystickRead = true;
    return 0;
}

int QtInputJoyAxis(int i, int nAxis)
{
    return qInput->joystickAxis(i, nAxis);
}

static int ReadKeyboard()
{
    if (bKeyboardRead)
        return 0;
    qInput->snapshot(qKeyboardState);
    bKeyboardRead = true;
    return 0;
}

static int ReadMouse()
{
    return 0;
}

int QtInputMouseAxis(int i, int nAxis)
{
    return 0;
}

static int JoystickState(int i, int nSubCode)
{
    if (i >= qInput->joystickCount() || nSubCode < 0)
        return 0;

    if (nSubCode < 0x10) {
        const int deadZone = 0x4000;

        int axis = nSubCode / 2;
        int value = qInput->joystickAxis(i, axis);

        if (nSubCode % 2)
            return value > deadZone;

        return value < -deadZone;
    }

    if (nSubCode < 0x20) {
        const static unsigned mask[4] = { SDL_HAT_LEFT,
                                          SDL_HAT_RIGHT,
                                          SDL_HAT_UP,
                                          SDL_HAT_DOWN };

        int hat = (nSubCode & 0x0F) >> 2;
        return qInput->joystickHat(i, hat) & mask[nSubCode & 3];
    }

    if (nSubCode < 0x80)
        return 0;

    int button = nSubCode - 0x80;

    return qInput->joystickButton(i, button);
}

static int CheckMouseState(unsigned int nSubCode)
{
    return 0;
}

int QtInputState(int nCode)
{
    if (nCode < 0)
        return 0;

    if (nCode < 0x100) {
        if (!bKeyboardRead)
            ReadKeyboard();
        return qKeyboardState[nCode & 0xFF];
    }

    if (nCode < 0x4000)
        return 0;

    if (nCode < 0x8000) {
        if (!bJoystickRead)
            ReadJoystick();

        int joystick = (nCode - 0x4000) >> 8;
        return JoystickState(joystick, nCode & 0xFF);
    }

    return 0;
}

int QtInputFind(bool CreateBaseline)
{
    if (!bKeyboardRead)
        ReadKeyboard();

    for (int i = 0; i < 256; i++)
        if (qKeyboardState[i])
            return i;

    return -1;
}

int QtInputGetControlName(int nCode, TCHAR* pszDeviceName, TCHAR* pszControlName)
{
    pszDeviceName[0] = '\0';
    pszControlName[0] = '\0';

    if (nCode < 256) {
        strcpy(pszControlName, QString("Key %1").arg(nCode).toLatin1().data());
        strcpy(pszDeviceName, "Keyboard");
        return 0;
    }

    return 0;
}

struct InputInOut InputInOutQt = { QtInputInit, QtInputExit,
                                   QtInputSetCooperativeLevel, QtInputStart,
                                   QtInputState, QtInputJoyAxis, QtInputMouseAxis,
                                   QtInputFind, QtInputGetControlName, NULL,
                                   ("Qt") };


QInputInterface *QInputInterface::m_onlyInstance = nullptr;
QInputInterface::QInputInterface(QObject *parent) :
    QObject(parent)
{
    memset(m_keys, 0, QINPUT_MAX_KEYS);
    m_isInitialized = false;
    joystickInit();
}

bool QInputInterface::joystickInit()
{
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        qDebug() << "SDL Joystick initialization error!" << SDL_GetError();
        return false;
    }

    int nJoys = SDL_NumJoysticks();

    m_joysticks.clear();
    m_joysticks.resize(nJoys);

    for (int i = 0; i < nJoys; i++) {
        SDL_Joystick *joystick = SDL_JoystickOpen(i);
        if (joystick == nullptr)
            qDebug() << "joystick error:" << i;
        else
            m_joysticks[i] = joystick;
    }

    SDL_JoystickEventState(SDL_IGNORE);

    return true;
}

void QInputInterface::joystickExit()
{
    foreach (SDL_Joystick *joystick, m_joysticks)
        SDL_JoystickClose(joystick);

    m_joysticks.clear();

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

void QInputInterface::joystickUpdate()
{
    SDL_JoystickUpdate();
}

int QInputInterface::joystickHat(int joy, int hat)
{
    if (m_joysticks.empty())
        return 0;

    if (joy >= m_joysticks.size() || joy < 0)
        return 0;

    if (hat > SDL_JoystickNumHats(m_joysticks[joy]))
        return 0;

    return SDL_JoystickGetHat(m_joysticks[joy], hat);
}

int QInputInterface::joystickAxis(int joy, int axis)
{
    if (m_joysticks.empty())
        return 0;

    if (joy >= m_joysticks.size() || joy < 0)
        return 0;

    if (axis > SDL_JoystickNumAxes(m_joysticks[joy]))
        return 0;

    return SDL_JoystickGetAxis(m_joysticks[joy], axis);
}

int QInputInterface::joystickButton(int joy, int button)
{
    if (m_joysticks.empty())
        return 0;

    if (joy >= m_joysticks.size() || joy < 0)
        return 0;

    if (button > SDL_JoystickNumButtons(m_joysticks[joy]))
        return 0;

    return SDL_JoystickGetButton(m_joysticks[joy], button);
}

QInputInterface::~QInputInterface()
{
    if (m_isInitialized)
        uninstall();
    m_onlyInstance = nullptr;
    joystickExit();
}

QInputInterface *QInputInterface::get(QObject *parent)
{
    if (m_onlyInstance != nullptr)
        return m_onlyInstance;
    m_onlyInstance = new QInputInterface(parent);
    return m_onlyInstance;
}

void QInputInterface::install()
{
    qApp->installEventFilter(this);
    m_isInitialized = true;
}

void QInputInterface::uninstall()
{
    qApp->removeEventFilter(this);
    m_isInitialized = false;
}

int QInputInterface::joystickCount()
{
    return m_joysticks.size();
}

void QInputInterface::snapshot(char *buffer, int keys)
{
    if (keys >= QINPUT_MAX_KEYS)
        keys = QINPUT_MAX_KEYS;
    memcpy(buffer, m_keys, keys);
}

int QInputInterface::state(int key)
{
    if (key >= 0 && key < 256)
        return m_keys[key];
    return 0;
}

bool QInputInterface::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        int fbak = QtKToFBAK(keyEvent->key());
        if (fbak < QINPUT_MAX_KEYS)
            m_keys[fbak] = 1;
    }
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        int fbak = QtKToFBAK(keyEvent->key());
        if (fbak < QINPUT_MAX_KEYS)
            m_keys[fbak] = 0;
    }
    return false;
}


int QtKToFBAK(int key)
{
    /*
     * TODO:
     * FBK_CAPITAL         0x3A
     * RIGHT-(SHIFT/CONTROL/ALT)
     * NUMPADS
     */
    switch (key) {
    case Qt::Key_Escape: return FBK_ESCAPE;
    case Qt::Key_0: return FBK_0;
    case Qt::Key_1: return FBK_1;
    case Qt::Key_2: return FBK_2;
    case Qt::Key_3: return FBK_3;
    case Qt::Key_4: return FBK_4;
    case Qt::Key_5: return FBK_5;
    case Qt::Key_6: return FBK_6;
    case Qt::Key_7: return FBK_7;
    case Qt::Key_8: return FBK_8;
    case Qt::Key_9: return FBK_9;
    case Qt::Key_Minus: return FBK_MINUS;
    case Qt::Key_Equal: return FBK_EQUALS;
    case Qt::Key_Back: return FBK_BACK;
    case Qt::Key_Tab: return FBK_TAB;
    case Qt::Key_Q: return FBK_Q;
    case Qt::Key_W: return FBK_W;
    case Qt::Key_E: return FBK_E;
    case Qt::Key_R: return FBK_R;
    case Qt::Key_T: return FBK_T;
    case Qt::Key_Y: return FBK_Y;
    case Qt::Key_U: return FBK_U;
    case Qt::Key_I: return FBK_I;
    case Qt::Key_O: return FBK_O;
    case Qt::Key_P: return FBK_P;
    case Qt::Key_BracketLeft: return FBK_LBRACKET;
    case Qt::Key_BracketRight: return FBK_RBRACKET;
    case Qt::Key_Return: return FBK_RETURN;
    case Qt::Key_Control: return FBK_LCONTROL;
    case Qt::Key_A: return FBK_A;
    case Qt::Key_S: return FBK_S;
    case Qt::Key_D: return FBK_D;
    case Qt::Key_F: return FBK_F;
    case Qt::Key_G: return FBK_G;
    case Qt::Key_H: return FBK_H;
    case Qt::Key_J: return FBK_J;
    case Qt::Key_K: return FBK_K;
    case Qt::Key_L: return FBK_L;
    case Qt::Key_Semicolon: return FBK_SEMICOLON;
    case Qt::Key_Apostrophe: return FBK_APOSTROPHE;
    case Qt::Key_Dead_Grave: return FBK_GRAVE;
    case Qt::Key_Shift: return FBK_LSHIFT;
    case Qt::Key_Backslash: return FBK_BACKSLASH;
    case Qt::Key_Z: return FBK_Z;
    case Qt::Key_X: return FBK_X;
    case Qt::Key_C: return FBK_C;
    case Qt::Key_V: return FBK_V;
    case Qt::Key_B: return FBK_B;
    case Qt::Key_N: return FBK_N;
    case Qt::Key_M: return FBK_M;
    case Qt::Key_Comma: return FBK_COMMA;
    case Qt::Key_Period: return FBK_PERIOD;
    case Qt::Key_Slash: return FBK_SLASH;
    case Qt::Key_multiply: return FBK_MULTIPLY;
    case Qt::Key_Alt: return FBK_LALT;
    case Qt::Key_Space: return FBK_SPACE;
    case Qt::Key_F1: return FBK_F1;
    case Qt::Key_F2: return FBK_F2;
    case Qt::Key_F3: return FBK_F3;
    case Qt::Key_F4: return FBK_F4;
    case Qt::Key_F5: return FBK_F5;
    case Qt::Key_F6: return FBK_F6;
    case Qt::Key_F7: return FBK_F7;
    case Qt::Key_F8: return FBK_F8;
    case Qt::Key_F9: return FBK_F9;
    case Qt::Key_F10: return FBK_F10;
    case Qt::Key_NumLock: return FBK_NUMLOCK;
    case Qt::Key_ScrollLock: return FBK_SCROLL;
    case Qt::Key_Pause: return FBK_PAUSE;
    case Qt::Key_Home: return FBK_HOME;
    case Qt::Key_Up: return FBK_UPARROW;
    case Qt::Key_PageUp: return FBK_PRIOR;
    case Qt::Key_Left: return FBK_LEFTARROW;
    case Qt::Key_Right: return FBK_RIGHTARROW;
    case Qt::Key_End: return FBK_END;
    case Qt::Key_Down: return FBK_DOWNARROW;
    case Qt::Key_PageDown: return FBK_NEXT;
    case Qt::Key_Insert: return FBK_INSERT;
    case Qt::Key_Delete: return FBK_DELETE;
    case Qt::Key_F11: return FBK_F11;
    case Qt::Key_F12: return FBK_F12;
    case Qt::Key_F13: return FBK_F13;
    case Qt::Key_F14: return FBK_F14;
    case Qt::Key_F15: return FBK_F15;
    default:
        return 0;
    }
    return 0;
}
