//
//  FBMainThread.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "NSWindowController+Core.h"

@implementation NSWindowController (Core)

- (AppDelegate *) appDelegate
{
    return AppDelegate.sharedInstance;
}

- (FBMainThread *) runloop
{
    return AppDelegate.sharedInstance.runloop;
}

- (FBVideo *) video
{
    return AppDelegate.sharedInstance.video;
}

- (FBInput *) input
{
    return AppDelegate.sharedInstance.input;
}

- (FBAudio *) audio
{
    return AppDelegate.sharedInstance.audio;
}

@end
