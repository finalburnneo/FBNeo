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

@end
