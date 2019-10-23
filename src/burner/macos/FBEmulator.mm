//
//  FBEmulator.mm
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/22/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBEmulator.h"

#import "AppDelegate.h"

#include "burner.h"

@implementation FBEmulator

#pragma mark - Init and dealloc

- (instancetype) init
{
    if (self = [super init]) {
    }

    return self;
}

#pragma mark - Public

- (NSString *) title
{
    return [NSString stringWithCString:BurnDrvGetText(DRV_FULLNAME)
                              encoding:NSUTF8StringEncoding];
}

#pragma mark - Private

- (void) updateProgress:(const char *) message
                isError:(BOOL) error
{
    if (!error)
        [_delegate driverProgressUpdate:[NSString stringWithCString:message
                                                           encoding:NSUTF8StringEncoding]];
    else
        [_delegate driverProgressError:[NSString stringWithCString:message
                                                          encoding:NSUTF8StringEncoding]];
}

@end

#pragma mark - FinalBurn callbacks

int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs)
{
    [AppDelegate.sharedInstance.emulator updateProgress:pszText
                                                isError:NO];
    return 0;
}

int AppError(TCHAR* szText, int bWarning)
{
    [AppDelegate.sharedInstance.emulator updateProgress:szText
                                                isError:YES];
    return 0;
}
