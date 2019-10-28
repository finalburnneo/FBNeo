//
//  FBInput.h
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/20/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface FBInput : NSObject

@property NSPoint mouseCoords;

- (void) simReset;
- (void) keyDown:(NSEvent *) theEvent;
- (void) keyUp:(NSEvent *) theEvent;
- (void) flagsChanged:(NSEvent *) theEvent;

@end
