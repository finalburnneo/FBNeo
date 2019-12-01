//
//  FBScanner.mm
//  Emulator
//
//  Created by Akop Karapetyan on 11/30/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBScanner.h"

#include "burner.h"
#include "burnint.h"
#include "driverlist.h"

#pragma mark - FBROMSet

@implementation FBROMSet

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

@end

#pragma mark - FBScanner

@interface FBScanner()

- (FBROMSet *) newRomSetWithIndex:(int) index;

@end

@implementation FBScanner
{
}

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

- (void) dealloc
{
    NSLog(@"Cleaning up");
}

#pragma mark - NSThread

- (void) main
{
    if (!_rootPath)
        return;

    snprintf(szAppRomPaths[0], MAX_PATH, "%s/",
             [_rootPath cStringUsingEncoding:NSUTF8StringEncoding]);

    id<FBScannerDelegate> del = _delegate;
    if ([del respondsToSelector:@selector(scanDidStart)])
        dispatch_async(dispatch_get_main_queue(), ^{ [del scanDidStart]; });

    UINT32 originallyActive = nBurnDrvActive;
    for (unsigned int i = 0; i < nBurnDrvCount && !self.cancelled; i++) {
        FBROMSet *set = [self newRomSetWithIndex:i];
        if (!set)
            continue;

        del = _delegate;
        if ([del respondsToSelector:@selector(progressDidUpdate:)])
            dispatch_async(dispatch_get_main_queue(), ^{
                [del progressDidUpdate:(float)i/nBurnDrvCount];
            });

        nBurnDrvActive = i;
        set.status = BzipOpen(TRUE);
        BzipClose();
        nBurnDrvActive = originallyActive;
    }

    del = _delegate;
    if ([del respondsToSelector:@selector(scanDidEnd)])
        dispatch_async(dispatch_get_main_queue(), ^{ [del scanDidEnd]; });
}

- (FBROMSet *) newRomSetWithIndex:(int) index
{
    if (index < 0 || index >= nBurnDrvCount)
        return nil;

    const struct BurnDriver *driver = pDriver[index];

    FBROMSet *set = [FBROMSet new];
    set.name = [NSString stringWithCString:driver->szShortName
                                  encoding:NSASCIIStringEncoding];
    if (driver->szFullNameW)
        set.title = [[NSString alloc] initWithBytes:driver->szFullNameW
                                        length:sizeof(wchar_t) * wcslen(driver->szFullNameW)
                                      encoding:NSUTF32LittleEndianStringEncoding];
    else if (driver->szFullNameA)
        set.title = [NSString stringWithCString:driver->szFullNameA
                                       encoding:NSASCIIStringEncoding];
    if (driver->szParent)
        set.parent = [NSString stringWithCString:driver->szParent
                                        encoding:NSASCIIStringEncoding];

    return set;
}

@end
