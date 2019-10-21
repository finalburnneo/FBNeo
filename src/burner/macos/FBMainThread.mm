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

@interface FBMainThread()

- (NSString *) setPath;
- (NSString *) setName;

@end

@implementation FBMainThread

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

#pragma mark - NSThread

- (void) main
{
    while (!self.isCancelled) {
        if (!_fileToOpen) {
            [NSThread sleepForTimeInterval:.5]; // Pause until there's something to load
            continue;
        }

        if (!MainInit([[self setPath] cStringUsingEncoding:NSUTF8StringEncoding],
                      [[self setName] cStringUsingEncoding:NSUTF8StringEncoding]))
            return;

        NSLog(@"Entering main loop");
        NSString *loaded = _fileToOpen;
        while (!self.isCancelled) {
            if ([loaded isNotEqualTo:_fileToOpen])
                break;
            MainFrame();
        }
        NSLog(@"Exited main loop");

        MainEnd();
    }
}

#pragma mark - Private

- (NSString *) setPath
{
    return [[_fileToOpen stringByDeletingLastPathComponent] stringByAppendingString:@"/"];
}

- (NSString *) setName
{
    return [[_fileToOpen lastPathComponent] stringByDeletingPathExtension];
}

@end
