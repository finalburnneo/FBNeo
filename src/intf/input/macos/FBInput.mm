// Copyright (c) Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "FBInput.h"

#import "AppDelegate.h"

#include "burner.h"

static const int keyCodeCount = 0x80;
static const int keyCodeToFbk[] = {
    FBK_A, // 00
    FBK_S, // 01
    FBK_D, // 02
    FBK_F, // 03
    FBK_H, // 04
    FBK_G, // 05
    FBK_Z, // 06
    FBK_X, // 07
    FBK_C, // 08
    FBK_V, // 09
    0,     // 0a
    FBK_B, // 0b
    FBK_Q, // 0c
    FBK_W, // 0d
    FBK_E, // 0e
    FBK_R, // 0f
    FBK_Y, // 10
    FBK_T, // 11
    FBK_1, // 12
    FBK_2, // 13
    FBK_3, // 14
    FBK_4, // 15
    FBK_6, // 16
    FBK_5, // 17
    FBK_EQUALS, // 18
    FBK_9, // 19
    FBK_7, // 1a
    FBK_MINUS,  // 1b
    FBK_8, // 1c
    FBK_0, // 1d
    FBK_RBRACKET, // 1e
    FBK_O, // 1f
    FBK_U, // 20
    FBK_LBRACKET, // 21
    FBK_I, // 22
    FBK_P, // 23
    FBK_RETURN, // 24
    FBK_L, // 25
    FBK_J, // 26
    FBK_APOSTROPHE, // 27
    FBK_K, // 28
    FBK_SEMICOLON, // 29
    FBK_BACKSLASH, // 2a
    FBK_COMMA, // 2b
    FBK_SLASH, // 2c
    FBK_N, // 2d
    FBK_M, // 2e
    FBK_PERIOD, // 2f
    FBK_TAB, // 30
    FBK_SPACE, // 31
    FBK_GRAVE, // 32
    FBK_BACK, // 33
    0,     // 34
    FBK_ESCAPE, // 35
    FBK_RWIN, // 36
    FBK_LWIN, // 37
    FBK_LSHIFT, // 38
    0, // 39 - kVK_CapsLock
    FBK_LALT, // 3a
    FBK_LCONTROL, // 3b
    FBK_RSHIFT, // 3c
    FBK_RALT, // 3d
    FBK_RCONTROL, // 3e
    0,     // 3f - kVK_Function
    0,     // 40 - kVK_F17
    FBK_DECIMAL, // 41
    0,     // 42
    FBK_MULTIPLY, // 43
    0,     // 44
    FBK_ADD, // 45
    0,     // 46
    0,     // 47 - kVK_ANSI_KeypadClear
    0,     // 48
    0,     // 49
    0,     // 4a
    FBK_DIVIDE, // 4b
    FBK_NUMPADENTER, // 4c
    0,     // 4d
    FBK_SUBTRACT, // 4e
    0,     // 4f
    0,     // 50
    FBK_NUMPADEQUALS, // 51
    FBK_NUMPAD0, // 52
    FBK_NUMPAD1, // 53
    FBK_NUMPAD2, // 54
    FBK_NUMPAD3, // 55
    FBK_NUMPAD4, // 56
    FBK_NUMPAD5, // 57
    FBK_NUMPAD6, // 58
    FBK_NUMPAD7, // 59
    0,     // 5a
    FBK_NUMPAD8, // 5b
    FBK_NUMPAD9, // 5c
    0, // 5d
    0, // 5e
    0, // 5f
    FBK_F5, // 60
    FBK_F6, // 61
    FBK_F7, // 62
    0, // 63 - kVK_F3
    FBK_F8, // 64
    FBK_F9, // 65
    0, // 66
    FBK_F11, // 67
    0, // 68
    FBK_F13, // 69
    0, // 6a
    FBK_F14, // 6b
    0, // 6c
    FBK_F10, // 6d
    0, // 6e
    FBK_F12, // 6f
    0, // 70
    FBK_F15, // 71
    0, // 72 - kVK_Help
    FBK_HOME, // 73
    FBK_PRIOR, // 74
    FBK_DELETE, // 75
    FBK_F4, // 76
    FBK_END, // 77
    FBK_F2, // 78
    FBK_NEXT, // 79
    FBK_F1, // 7a
    FBK_LEFTARROW, // 7b
    FBK_RIGHTARROW, // 7c
    FBK_DOWNARROW, // 7d
    FBK_UPARROW, // 7e
    0,
};
static unsigned char keyState[256];
static unsigned char simKeyState[256];

@implementation FBInput

#pragma mark - Init and dealloc

- (instancetype) init
{
    if (self = [super init]) {
    }

    return self;
}

#pragma mark - Interface

- (void) simReset
{
    simKeyState[FBK_F3] = 1;
}

- (void) keyDown:(NSEvent *) theEvent
{
    if (theEvent.keyCode >= 0 && theEvent.keyCode < keyCodeCount) {
        int fbkCode = keyCodeToFbk[theEvent.keyCode];
        if (fbkCode > 0) keyState[fbkCode] = 1;
    }
}

- (void) keyUp:(NSEvent *) theEvent
{
    if (theEvent.keyCode >= 0 && theEvent.keyCode < keyCodeCount) {
        int fbkCode = keyCodeToFbk[theEvent.keyCode];
        if (fbkCode > 0) keyState[fbkCode] = 0;
    }
}

- (void) flagsChanged:(NSEvent *) theEvent
{
}

- (BOOL) usesMouse
{
    if (!bDrvOkay)
        return NO;

    struct BurnInputInfo bii;
    for (int i = 0; BurnDrvGetInputInfo(&bii, i) == 0; i++)
        if (bii.nType == BIT_ANALOG_REL && strstr(bii.szName, "mouse") == 0)
            return YES;

    return NO;
}

@end

#pragma mark - FinalBurn

bool bAlwaysProcessKeyboardInput = 0;

bool AppProcessKeyboardInput()
{
    return true;
}

#pragma mark - FinalBurn callbacks

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
    memset(keyState, 0, sizeof(keyState));
    memset(simKeyState, 0, sizeof(simKeyState));
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
    if (i == 0)
        switch (nAxis) {
            case 0: return AppDelegate.sharedInstance.input.mouseCoords.x;
            case 1: return AppDelegate.sharedInstance.input.mouseCoords.y;
        }

    return 0;
}

int MacOSinpState(int nCode)
{
    if (nCode < 0)
        return 0;

    if (nCode < 0x100) {
        if (simKeyState[nCode]) {
            simKeyState[nCode] = 0;
            return 1;
        }
        return keyState[nCode];
    }

    if (nCode < 0x4000)
        return 0;

    if (nCode < 0x8000) {
        // FIXME!!
//        // Codes 4000-8000 = Joysticks
//        int nJoyNumber = (nCode - 0x4000) >> 8;
//
//        // Find the joystick state in our array
//        return JoystickState(nJoyNumber, nCode & 0xFF);
        return 0;
    }

    if (nCode < 0xC000) {
        if (((nCode - 0x8000) >> 8) == 0) {
            switch (nCode & 0x7f) {
                case 0: return (AppDelegate.sharedInstance.input.mouseButtonStates & FBN_LMB) != 0;
                case 1: return (AppDelegate.sharedInstance.input.mouseButtonStates & FBN_RMB) != 0;
                case 2: return (AppDelegate.sharedInstance.input.mouseButtonStates & FBN_MMB) != 0;
            }
        }
    }

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
