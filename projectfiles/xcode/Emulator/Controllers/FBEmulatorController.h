//
//  FBEmulatorController.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/25/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "FBScreenView.h"
#import "FBMainThread.h"

@interface FBEmulatorController : NSWindowController<FBScreenViewDelegate, FBMainThreadDelegate>
{
    IBOutlet FBScreenView *screen;
    IBOutlet NSView *accView;
    IBOutlet NSProgressIndicator *spinner;
    IBOutlet NSButton *lockIcon;
    IBOutlet NSTextField *lockText;
    IBOutlet NSTextField *label;
}

- (IBAction) activateCursorLock:(id) sender;

- (IBAction) resetEmulation:(id) sender;
- (IBAction) togglePause:(id) sender;

- (IBAction) resizeNormalSize:(id) sender;
- (IBAction) resizeDoubleSize:(id) sender;

@end
