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

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

- (void) main
{
    NSString *path = [[_fileToOpen stringByDeletingLastPathComponent] stringByAppendingString:@"/"];
    NSString *setname = [[_fileToOpen lastPathComponent] stringByDeletingPathExtension];

    if (!MainInit([path cStringUsingEncoding:NSUTF8StringEncoding], [setname cStringUsingEncoding:NSUTF8StringEncoding]))
        return;

    NSLog(@"Entering main loop");
    while (!self.isCancelled) {
        MainFrame();
    }
    NSLog(@"Exited main loop");

    MainEnd();
}

@end
