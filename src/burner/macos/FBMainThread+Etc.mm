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

#import "FBMainThread.h"

#import "AppDelegate.h"

#include "burner.h"
#include "burnint.h"
#include "driverlist.h"

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

- (NSString *) setName
{
    if (nBurnDrvActive == ~0U) {
        return nil;
    }
    return [NSString stringWithCString:BurnDrvGetText(DRV_NAME)
                              encoding:NSASCIIStringEncoding];
}

- (NSString *) title
{
    if (!bDrvOkay)
        return nil;

    const wchar_t *unicodeName = pDriver[nBurnDrvActive]->szFullNameW;
    if (unicodeName)
        return [[NSString alloc] initWithBytes:unicodeName
                                        length:sizeof(wchar_t) * wcslen(unicodeName)
                                      encoding:NSUTF32LittleEndianStringEncoding];

    const char *asciiName = pDriver[nBurnDrvActive]->szFullNameA;
    if (asciiName)
        return [NSString stringWithCString:asciiName
                                  encoding:NSASCIIStringEncoding];

    return nil;
}

#pragma mark - DIP switches

- (NSArray<FBDipSetting *> *) dipSwitches
{
    if (!bDrvOkay)
        return nil;

    NSMutableArray *switches = [NSMutableArray new];
    FBDipSetting *active;

    int offset = 0;
    BurnDIPInfo dipSwitch;
    for (int i = 0; BurnDrvGetDIPInfo(&dipSwitch, i) == 0; i++) {
        if (!dipSwitch.szText) // defaruto
            continue;

        if (dipSwitch.nFlags == 0xf0)
            offset = dipSwitch.nInput;
        if (dipSwitch.nFlags & 0x40) {
            FBDipSetting *set = [FBDipSetting new];
            set.name = [NSString stringWithCString:dipSwitch.szText
                                          encoding:NSUTF8StringEncoding];

            active = set;
            set.selectedIndex = 0;
            set.switches = [NSMutableArray new];
            [switches addObject:set];
        } else {
            FBDipOption *opt = [FBDipOption new];
            opt.name = [NSString stringWithCString:dipSwitch.szText
                                          encoding:NSUTF8StringEncoding];
            opt.start = dipSwitch.nInput + offset;
            opt.mask = dipSwitch.nMask;
            opt.setting = dipSwitch.nSetting;

            // Default switch
            BurnDIPInfo dsw2;
            for (int j = 0; BurnDrvGetDIPInfo(&dsw2, j) == 0; j++)
                if (dsw2.nFlags == 0xff && dsw2.nInput == dipSwitch.nInput
                    && (dsw2.nSetting & dipSwitch.nMask) == dipSwitch.nSetting)
                        active.defaultIndex = active.selectedIndex = active.switches.count;

            // Active switch
            struct GameInp *pgi = GameInp + opt.start;
            if ((pgi->Input.Constant.nConst & opt.mask) == dipSwitch.nSetting)
                active.selectedIndex = active.switches.count;

            [(NSMutableArray *) active.switches addObject:opt];
        }
    }

    return switches;
}

- (void) applyDip:(FBDipOption *) option
{
    if (bDrvOkay) {
        struct GameInp *pgi = GameInp + option.start;
        pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~option.mask) | (option.setting & option.mask);
        self.dipSwitchesDirty = YES;
    }
}

- (BOOL) saveDipState:(NSString *) path
{
    NSLog(@"saveDipState");

    NSArray<FBDipSetting *> *switches = self.dipSwitches;
    if (!switches || switches.count < 1)
        return YES;

    for (FBDipSetting *sw in switches)
        if (sw.defaultIndex != sw.selectedIndex)
            goto save;

    return YES;

save:
    FILE *f = fopen([path cStringUsingEncoding:NSUTF8StringEncoding], "w");
    if (!f)
        return NO;

    for (FBDipSetting *sw in switches) {
        FBDipOption *opt = sw.switches[sw.selectedIndex];
        fprintf(f, "%x %d %02x\n", opt.start, sw.selectedIndex, opt.setting);
    }

    fclose(f);
    return YES;
}

- (BOOL) restoreDipState:(NSString *) path
{
    NSLog(@"restoreDipState");

    NSArray<FBDipSetting *> *switches = self.dipSwitches;
    if (!switches || switches.count < 1)
        return YES;

    FILE *f = fopen([path cStringUsingEncoding:NSUTF8StringEncoding], "r");
    if (!f)
        return NO;

    int swIndex;
    uint32 start;
    uint32 setting;
    for (int i = 0; fscanf(f, "%x %d %x", &start, &swIndex, &setting) == 3 && i < switches.count; i++) {
        FBDipSetting *sw = switches[i];
        if (sw.selectedIndex != swIndex && swIndex < sw.switches.count)
            [self applyDip:sw.switches[swIndex]];
    }

    fclose(f);

    [AppDelegate.sharedInstance.input simReset];
    return YES;
}

@end

#pragma mark - FBDipSetting

@implementation FBDipSetting

- (instancetype) init
{
    if (self = [super init]) {
        _switches = [NSMutableArray new];
    }
    return self;
}

@end

#pragma mark - FBDipOption

@implementation FBDipOption

@end
