//
//  FBPreferencesController.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/24/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "FBMainThread.h"

@interface FBPreferencesController : NSWindowController<NSTabViewDelegate, NSTableViewDataSource, FBMainThreadDelegate>
{
    IBOutlet NSToolbar *toolbar;
    IBOutlet NSTabView *contentTabView;
    IBOutlet NSTableView *dipswitchTableView;
    IBOutlet NSButton *restoreDipButton;
}

- (IBAction) tabChanged:(id) sender;
- (IBAction) showPreviousTab:(id) sender;
- (IBAction) showNextTab:(id) sender;
- (IBAction) restoreDipToDefault:(id) sender;

@end
