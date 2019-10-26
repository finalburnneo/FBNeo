//
//  FBMainThread.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBMainThread.h"

#include "burner.h"

// Placing burner.h stuff in the NSThread file
// messes with thread execution for some reason,
// so I'm implementing it as a category - AK

@implementation FBMainThread (Etc)

- (BOOL) isPaused
{
    return bRunPause;
}

- (void) setPaused:(BOOL) isPaused
{
    if (!bDrvOkay || bRunPause == isPaused)
        return;

    bRunPause = isPaused;
    if (isPaused)
        AudSoundStop();
    else
        AudSoundPlay();
}

- (NSString *) title
{
    return [NSString stringWithCString:BurnDrvGetText(DRV_FULLNAME)
                              encoding:NSUTF8StringEncoding];
}

- (NSArray *) dipSwitches
{
    if (!bDrvOkay)
        return nil;

    NSMutableArray *switches = [NSMutableArray new];
    FBDipSwitch *active;

    int offset = 0;
    BurnDIPInfo dipSwitch;
    for (int i = 0; BurnDrvGetDIPInfo(&dipSwitch, i) == 0; i++) {
        if (dipSwitch.nFlags == 0xf0)
            offset = dipSwitch.nInput;
        if (!dipSwitch.szText) // defaruto
            continue;

        FBDipSwitch *sw = [FBDipSwitch new];
        sw.name = [NSString stringWithCString:dipSwitch.szText
                                     encoding:NSUTF8StringEncoding];

        if (dipSwitch.nFlags & 0x40) {
            active = sw;
            sw.selectedIndex = 0;
            sw.switches = [NSMutableArray new];
            [switches addObject:sw];
        } else {
            sw.start = dipSwitch.nInput + offset;
            sw.mask = dipSwitch.nMask;
            sw.setting = dipSwitch.nSetting;
            [(NSMutableArray *) active.switches addObject:sw];

            BurnDIPInfo dsw2;
            for (int j = 0; BurnDrvGetDIPInfo(&dsw2, j) == 0; j++)
                if (dsw2.nFlags == 0xff
                    && dsw2.nInput == dipSwitch.nInput
                    && (dsw2.nSetting & dipSwitch.nMask) == dipSwitch.nSetting)
                        active.selectedIndex = active.switches.count - 1;
        }
    }

    return switches;
}

@end

#pragma mark - FBDipSwitch

@implementation FBDipSwitch

@end
