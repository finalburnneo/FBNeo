#include <QtWidgets>
#include "qinputinterface.h"
#include "burner.h"

QInputInterface *qInput = nullptr;

int QtKToFBAK(int key);
static char qKeyboardState[QINPUT_MAX_KEYS] = { 0 };
static bool bKeyboardRead = false;

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
    return 0;
}

static int ReadJoystick()
{
    return 0;
}

int QtInputJoyAxis(int i, int nAxis)
{
    return 0;
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
    return 0;
}

static int CheckMouseState(unsigned int nSubCode)
{
    return 0;
}

int QtInputState(int nCode)
{
    if (nCode < 0)
        return 0;

    if (nCode < 256) {
#if 1
        if (!bKeyboardRead)
            ReadKeyboard();
        return qKeyboardState[nCode & 0xFF];
#else
        return qInput->state(nCode);
#endif
    }
    return 0;
}

int QtInputFind(bool CreateBaseline)
{
    return -1;
}

int QtInputGetControlName(int nCode, TCHAR* pszDeviceName, TCHAR* pszControlName)
{
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
}

QInputInterface::~QInputInterface()
{
    if (m_isInitialized)
        uninstall();
    m_onlyInstance = nullptr;
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
        //qDebug() << m_timer.elapsed() << "pressed";
        int fbak = QtKToFBAK(keyEvent->key());
        if (fbak < QINPUT_MAX_KEYS)
            m_keys[fbak] = 1;
    }
    if (event->type() == QEvent::KeyRelease) {
        //qDebug() << m_timer.elapsed() << "released";
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
