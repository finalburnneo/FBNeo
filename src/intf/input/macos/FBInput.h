//
//  FBInput.h
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/15/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface FBInput : NSObject

- (void) keyDown:(NSEvent *) theEvent;
- (void) keyUp:(NSEvent *) theEvent;
- (void) flagsChanged:(NSEvent *) theEvent;

@end
