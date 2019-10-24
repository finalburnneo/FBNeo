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
    NSMutableArray *observers;
}

- (instancetype) init
{
    if (self = [super init]) {
        log = [NSMutableString new];
        observers = [NSMutableArray new];
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

- (void) addObserver:(id<FBMainThreadDelegate>) observer
{
    @synchronized (observer) {
        [observers addObject:observer];
    }
}

- (void) removeObserver:(id<FBMainThreadDelegate>) observer
{
    @synchronized (observer) {
        [observers removeObject:observer];
    }
}

#pragma mark - Private

- (void) updateProgress:(const char *) message
                   type:(LogEntryType) entryType
{
    NSString *str = [NSString stringWithFormat:@"%c %s\n",
                     entryType == LogEntryError ? '!' : ' ', message];

    [log appendString:str];
    for (id<FBMainThreadDelegate> o in observers) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if ([o respondsToSelector:@selector(logDidUpdate:)])
                [o logDidUpdate:str];
        });
    }
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

        log.string = @"";
        @synchronized (observers) {
            for (id<FBMainThreadDelegate> o in observers) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if ([o respondsToSelector:@selector(logDidClear)])
                        [o logDidClear];
                    if ([o respondsToSelector:@selector(driverInitDidStart)])
                        [o driverInitDidStart];
                });
            }
        }

        if (!MainInit([setPath cStringUsingEncoding:NSUTF8StringEncoding],
                      [setName cStringUsingEncoding:NSUTF8StringEncoding])) {
            pathToLoad = nil;

            @synchronized (observers) {
                for (id<FBMainThreadDelegate> o in observers) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        if ([o respondsToSelector:@selector(driverInitDidEnd:success:)])
                            [o driverInitDidEnd:setName
                                        success:NO];
                    });
                }
            }

            continue;
        }

        _running = pathToLoad;
        pathToLoad = nil;

        @synchronized (observers) {
            for (id<FBMainThreadDelegate> o in observers) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if ([o respondsToSelector:@selector(driverInitDidEnd:success:)])
                        [o driverInitDidEnd:setName
                                    success:YES];
                    if ([o respondsToSelector:@selector(gameSessionDidStart:)])
                        [o gameSessionDidStart:setName];
                });
            }
        }

        while (!self.isCancelled && pathToLoad == nil)
            MainFrame();

        @synchronized (observers) {
            for (id<FBMainThreadDelegate> o in observers) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if ([o respondsToSelector:@selector(gameSessionDidEnd)])
                        [o gameSessionDidEnd];
                });
            }
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
