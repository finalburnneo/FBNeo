#include "burner.h"

static int MacOSinpJoystickInit(int i)
{
    return 0;
}

static int MacOSinpKeyboardInit()
{
    return 0;
}

static int MacOSinpMouseInit()
{
    return 0;
}

int MacOSinpSetCooperativeLevel(bool bExclusive, bool /*bForeGround*/)
{
    return 0;
}

int MacOSinpExit()
{
    return 0;
}

int MacOSinpInit()
{
    return 0;
}

int MacOSinpStart()
{
    return 0;
}

int MacOSinpJoyAxis(int i, int nAxis)
{
    return 0;
}

int MacOSinpMouseAxis(int i, int nAxis)
{
    return 0;
}

int MacOSinpState(int nCode)
{
    return 0;
}

int MacOSinpFind(bool CreateBaseline)
{
    return 0;
}

int MacOSinpGetControlName(int nCode, TCHAR* pszDeviceName, TCHAR* pszControlName)
{
    return 0;
}

struct InputInOut InputInOutMacOS = {
    MacOSinpInit,
    MacOSinpExit,
    MacOSinpSetCooperativeLevel,
    MacOSinpStart,
    MacOSinpState,
    MacOSinpJoyAxis,
    MacOSinpMouseAxis,
    MacOSinpFind,
    MacOSinpGetControlName,
    NULL,
    _T("MacOS input")
};
