//
//  FBMainThread.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBMainThread.h"
#import "AppDelegate.h"

#include "main.h"

typedef enum LogEntryType {
    LogEntryMessage,
    LogEntryError,
} LogEntryType;

@implementation FBMainThread
{
    NSString *pathToLoad;
    NSMutableString *log;
}

- (instancetype) init
{
    if (self = [super init]) {
        log = [NSMutableString new];
    }
    return self;
}

#pragma mark - Public

- (void) load:(NSString *) path
{
    pathToLoad = path;
}

- (NSString *) log
{
    return log;
}

#pragma mark - Private

- (void) updateProgress:(const char *) message
                   type:(LogEntryType) entryType
{
    [log appendFormat:@"%c %s\n", entryType == LogEntryError ? '!' : ' ', message];
}

#pragma mark - NSThread

- (void) main
{
    SetNVRAMPath([AppDelegate.sharedInstance.nvramPath cStringUsingEncoding:NSUTF8StringEncoding]);

    while (!self.isCancelled) {
        if (pathToLoad == nil) {
            [NSThread sleepForTimeInterval:.5]; // Pause until there's something to load
            continue;
        }

        NSString *setPath = [[pathToLoad stringByDeletingLastPathComponent] stringByAppendingString:@"/"];
        NSString *setName = [[pathToLoad lastPathComponent] stringByDeletingPathExtension];

        {
            id<FBMainThreadDelegate> del = _delegate;
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([del respondsToSelector:@selector(driverInitDidStart)])
                    [del driverInitDidStart];
            });
        }

        if (!MainInit([setPath cStringUsingEncoding:NSUTF8StringEncoding],
                      [setName cStringUsingEncoding:NSUTF8StringEncoding])) {
            pathToLoad = nil;

            {
                id<FBMainThreadDelegate> del = _delegate;
                dispatch_async(dispatch_get_main_queue(), ^{
                    if ([del respondsToSelector:@selector(driverInitDidEnd:success:)])
                        [del driverInitDidEnd:setName
                                      success:NO];
                });
            }

            continue;
        }

        _running = pathToLoad;
        pathToLoad = nil;

        {
            id<FBMainThreadDelegate> del = _delegate;
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([del respondsToSelector:@selector(driverInitDidEnd:success:)])
                    [del driverInitDidEnd:setName
                                  success:YES];
                if ([del respondsToSelector:@selector(gameSessionDidStart:)])
                    [del gameSessionDidStart:setName];
            });
        }

        while (!self.isCancelled && pathToLoad == nil)
            MainFrame();

        {
            id<FBMainThreadDelegate> del = _delegate;
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([del respondsToSelector:@selector(gameSessionDidEnd)])
                    [del gameSessionDidEnd];
            });
        }

        _running = nil;

        MainEnd();
    }
}

@end

#pragma mark - FinalBurn callbacks

int ProgressUpdateBurner(double dProgress, const char* pszText, bool bAbs)
{
    [AppDelegate.sharedInstance.runloop updateProgress:pszText
                                                  type:LogEntryMessage];
    return 0;
}

int AppError(char* szText, int bWarning)
{
    [AppDelegate.sharedInstance.runloop updateProgress:szText
                                                  type:LogEntryError];
    return 0;
}
