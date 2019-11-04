//
//  FBInput.h
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/20/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#define FBN_LMB 0x1
#define FBN_RMB 0x2
#define FBN_MMB 0x4

@interface FBInput : NSObject

@property NSPoint mouseCoords;
@property uint8 mouseButtonStates;

- (void) simReset;
- (BOOL) usesMouse;
- (void) keyDown:(NSEvent *) theEvent;
- (void) keyUp:(NSEvent *) theEvent;
- (void) flagsChanged:(NSEvent *) theEvent;

@end
