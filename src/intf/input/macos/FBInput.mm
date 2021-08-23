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

#import "FBInputConstants.h"
#import "AppDelegate.h"
#import "NSString+Etc.h"

#include "burner.h"

#define DEBUG_GP

static const int keyCodeCount = 0x80;
static const int maxJoysticks = 8;
static const int maxJoyButtons = 128;
static const int deadzoneWidth = 50;
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
static unsigned char joyState[maxJoysticks][256];

#pragma mark - FBInputInfo

@implementation FBInputInfo

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

- (instancetype) initWithCode:(NSString *) code
                        title:(NSString *) title
{
    if (self = [super init]) {
        _code = code;
        _title = title;
    }
    return self;
}

- (int) playerIndex
{
    if (_code.length < 4) {
        return false;
    }
    char digit = [_code characterAtIndex:1];
    if ([_code characterAtIndex:0] == 'p'
        && digit >= '0' && digit <= '9'
        && [_code characterAtIndex:2] == ' ') {
        return digit - '0';
    }
    return -1;
}

- (NSString *) neutralTitle
{
    if (_title.length < 4) {
        return _title;
    }
    char digit = [_title characterAtIndex:1];
    if ([_title characterAtIndex:0] == 'P'
        && digit >= '0' && digit <= '9'
        && [_title characterAtIndex:2] == ' ') {
        return [_title substringFromIndex:3];
    }
    return _title;
}

@end

#pragma mark - FBInput

@interface FBInput()

- (void) initControls:(struct GameInp *) pgi
                  szi:(const char *) szi;
- (void) resetConnectedMaps;

@end

@implementation FBInput
{
    NSMutableDictionary<NSString*,FBInputMap*> *connectedMaps;
    BOOL _hasFocus;
}

#pragma mark - Init and dealloc

- (instancetype) init
{
    if (self = [super init]) {
        [AKGamepadManager.sharedInstance addObserver:self];
        connectedMaps = [NSMutableDictionary new];
        _hasFocus = NO;
    }

    return self;
}

- (void) dealloc
{
    [AKGamepadManager.sharedInstance removeObserver:self];
}

#pragma mark - AKGamepadDelegate

- (void) gamepadDidConnect:(AKGamepad *) gamepad
{
    // FIXME!!
//    NSString *gpId = [gamepad vendorProductString];
//    FXButtonMap *map = [_config mapWithId:gpId];
//    if (!map) {
//        map = [self defaultGamepadMap:gamepad];
//        [map setDeviceId:gpId];
//        [_config setMap:map];
//    }

#ifdef DEBUG_GP
    NSLog(@"Gamepad \"%@\" connected to port %i",
          gamepad.name, (int) gamepad.index);
#endif
}

- (void) gamepadDidDisconnect:(AKGamepad *) gamepad
{
#ifdef DEBUG_GP
    NSLog(@"Gamepad \"%@\" disconnected from port %i",
          gamepad.name, (int) gamepad.index);
#endif
}

- (void) gamepad:(AKGamepad *) gamepad
        xChanged:(NSInteger) newValue
          center:(NSInteger) center
       eventData:(AKGamepadEventData *) eventData
{
    if (gamepad.index >= maxJoysticks) {
        return;
    }
    joyState[gamepad.index][FBGamepadLeft] = center - newValue > deadzoneWidth; // L
    joyState[gamepad.index][FBGamepadRight] = newValue - center > deadzoneWidth; // R
#ifdef DEBUG_GP
    NSLog(@"Joystick X: %ld (center: %ld) on gamepad %@",
          newValue, center, gamepad);
#endif
}

- (void) gamepad:(AKGamepad *) gamepad
        yChanged:(NSInteger) newValue
          center:(NSInteger) center
       eventData:(AKGamepadEventData *) eventData
{
    if (gamepad.index >= maxJoysticks) {
        return;
    }
    joyState[gamepad.index][FBGamepadUp] = center - newValue > deadzoneWidth; // U
    joyState[gamepad.index][FBGamepadDown] = newValue - center > deadzoneWidth; // D
#ifdef DEBUG_GP
    NSLog(@"Joystick Y: %ld (center: %ld) on gamepad %@",
          newValue, center, gamepad);
#endif
}

- (void) gamepad:(AKGamepad *) gamepad
          button:(NSUInteger) index
          isDown:(BOOL) isDown
       eventData:(AKGamepadEventData *) eventData
{
    if (gamepad.index >= maxJoysticks || index > maxJoyButtons) {
        return;
    }
    joyState[gamepad.index][index] = isDown;
#ifdef DEBUG_GP
    NSLog(@"Button %ld on gamepad '%@' %@", index, gamepad,
          isDown ? @"down" : @"up");
#endif
}

#pragma mark - Interface

- (void) setFocus:(BOOL) focus
{
    _hasFocus = focus;
}

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

- (NSArray<FBInputInfo *> *) allInputs
{
    NSMutableArray<FBInputInfo *> *inputs = [NSMutableArray array];
    if (nBurnDrvActive == ~0U) {
        return inputs;
    }
    struct BurnInputInfo bii;
    for (int i = 0; i < 0x1000; i++) {
        if (BurnDrvGetInputInfo(&bii, i)) {
            break;
        }
        if (bii.nType == BIT_DIGITAL) {
            [inputs addObject:[[FBInputInfo alloc] initWithCode:[NSString stringWithCString:bii.szInfo
                                                                                   encoding:NSASCIIStringEncoding]
                                                          title:[NSString stringWithCString:bii.szName
                                                                                   encoding:NSASCIIStringEncoding]]];
        }
    }

    return inputs;
}

- (FBInputMap *) loadMapForDeviceId:(NSString *) deviceId
                            setName:(NSString *) setName
{
    NSString *path = [AppDelegate.sharedInstance.inputMapPath stringByAppendingPathComponent:
                      [[NSString stringWithFormat:@"%@-%@.plist", setName, deviceId] sanitizeForFilename]];

    FBInputMap *map = [NSKeyedUnarchiver unarchiveObjectWithFile:path];
    if (!map) {
        map = [FBInputMap new];
    }

    return map;
}

- (BOOL) saveMap:(FBInputMap *) map
     forDeviceId:(NSString *) deviceId
         setName:(NSString *) setName
{
    NSString *path = [AppDelegate.sharedInstance.inputMapPath stringByAppendingPathComponent:
                      [[NSString stringWithFormat:@"%@-%@.plist", setName, deviceId] sanitizeForFilename]];
    if ([NSKeyedArchiver archiveRootObject:map
                                    toFile:path]) {
        map.isDirty = NO;
        return YES;
    }
    return NO;
}

- (void) resetConnectedMaps
{
    [connectedMaps removeAllObjects];
    NSString *setName = AppDelegate.sharedInstance.runloop.setName;
    if (!setName) {
        return;
    }
    NSArray<AKGamepad*> *gamepads = [AKGamepadManager.sharedInstance allConnected];
    [gamepads enumerateObjectsUsingBlock:^(AKGamepad *obj, NSUInteger idx, BOOL *stop) {
        NSString *deviceId = obj.vendorProductString;
        if ([connectedMaps objectForKey:deviceId]) {
            return;
        }
        [connectedMaps setObject:[self loadMapForDeviceId:deviceId
                                                  setName:setName]
                          forKey:deviceId];
    }];
}

- (void) initControls:(struct GameInp *) pgi
                  szi:(const char *) szi
{
    NSArray<AKGamepad*> *gamepads = [AKGamepadManager.sharedInstance allConnected];
    if (strnlen(szi, 3) >= 3
        && szi[0] == 'p'
        && szi[1] >= '0'
        && szi[1] <= '9'
        && szi[2] == ' ') {
        int player = szi[1] - '0';
        if (player > gamepads.count) {
            return;
        }
        AKGamepad *gp = [gamepads objectAtIndex:player-1];
        FBInputMap *map = [connectedMaps objectForKey:gp.vendorProductString];
        int code = [map physicalCodeForVirtual:[NSString stringWithCString:szi
                                                                  encoding:NSASCIIStringEncoding]];
        if (code != -1) {
            pgi->nInput = GIT_SWITCH;
            pgi->Input.Switch.nCode = ((player << 8) | 0x4000) | (code & 0xff);
        }
    }
}

@end

#pragma mark - FinalBurn

bool bAlwaysProcessKeyboardInput = 0;

bool AppProcessKeyboardInput()
{
    return true;
}

#pragma mark - FinalBurn callbacks

int MacOSinpInitControls(struct GameInp *pgi, const char *szi)
{
    [AppDelegate.sharedInstance.input initControls:pgi
                                               szi:szi];
    return 0;
}

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
    [AppDelegate.sharedInstance.input resetConnectedMaps];

    memset(keyState, 0, sizeof(keyState));
    memset(simKeyState, 0, sizeof(simKeyState));
    memset(joyState, 0, sizeof(joyState));
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
        int joyIndex = (nCode - 0x4000) >> 8;
        return joyState[joyIndex - 1][nCode & 0xff];
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
