//
//  FBMainThread.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "AppDelegate.h"

@interface NSWindowController (Core)

- (AppDelegate *) appDelegate;
- (FBMainThread *) runloop;
- (FBVideo *) video;
- (FBInput *) input;
- (FBAudio *) audio;

@end
