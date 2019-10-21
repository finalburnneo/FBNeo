//
//  FBMainThread.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBMainThread.h"

extern int MainInit(const char *, const char *);
extern int MainFrame();
extern int MainEnd();

@implementation FBMainThread
{
    NSString *pathToLoad;
}

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

#pragma mark - Public

- (void) load:(NSString *) path
{
    pathToLoad = path;
}

#pragma mark - NSThread

- (void) main
{
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
                [del driverInitDidStart];
            });
        }

        if (!MainInit([setPath cStringUsingEncoding:NSUTF8StringEncoding],
                      [setName cStringUsingEncoding:NSUTF8StringEncoding])) {
            pathToLoad = nil;

            {
                id<FBMainThreadDelegate> del = _delegate;
                dispatch_async(dispatch_get_main_queue(), ^{
                    [del driverInitDidEnd:NO];
                });
            }

            continue;
        }

        _running = pathToLoad;
        pathToLoad = nil;

        {
            id<FBMainThreadDelegate> del = _delegate;
            dispatch_async(dispatch_get_main_queue(), ^{
                [del driverInitDidEnd:YES];
                [del gameSessionDidStart:setName];
            });
        }

        while (!self.isCancelled && pathToLoad == nil)
            MainFrame();

        {
            id<FBMainThreadDelegate> del = _delegate;
            dispatch_async(dispatch_get_main_queue(), ^{
                [del gameSessionDidEnd];
            });
        }

        _running = nil;

        MainEnd();
    }
}

@end
