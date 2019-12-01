//
//  FBLauncherController.h
//  Emulator
//
//  Created by Akop Karapetyan on 11/30/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "FBScanner.h"

@interface FBLauncherController : NSWindowController<FBScannerDelegate>
{
    IBOutlet NSPanel *progressPanel;
    IBOutlet NSProgressIndicator *progressPanelBar;
    IBOutlet NSButton *progressPanelCancelButton;
    IBOutlet NSTextField *progressPanelLabel;

    IBOutlet NSTreeController *romSetTreeController;
}

- (IBAction) rescanSets:(id) sender;
- (IBAction) cancelProgress:(id) sender;

@property (readonly) NSArray<NSDictionary *> *romSets;

@end
