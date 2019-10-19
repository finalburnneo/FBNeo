//
//  FBMainThread.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBMainThread.h"

extern int MainInit();
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
    if (!MainInit())
        return;

    NSLog(@"Entering main loop");
    while (!self.isCancelled) {
        MainFrame();
    }
    NSLog(@"Exited main loop");

    MainEnd();
}

@end
