//
//  FBLauncherController.h
//  Emulator
//
//  Created by Akop Karapetyan on 11/30/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "FBScanner.h"
#import "FBDropFileScrollView.h"

@interface FBLauncherItem: NSObject

@property NSString *name;
@property NSString *title;
@property unsigned char status;
@property (readonly) NSMutableArray<FBLauncherItem *> *subsets;

@end

@interface FBLauncherController : NSWindowController<FBScannerDelegate, FBDropFileScrollViewDelegate>
{
    IBOutlet NSPanel *progressPanel;
    IBOutlet NSProgressIndicator *progressPanelBar;
    IBOutlet NSButton *progressPanelCancelButton;
    IBOutlet NSTextField *progressPanelLabel;

    IBOutlet NSTreeController *romSetTreeController;
}

- (IBAction) rescanSets:(id) sender;
- (IBAction) cancelProgress:(id) sender;
- (IBAction) launchSet:(id) sender;

@property (readonly) NSArray<FBLauncherItem *> *romSets;

@end
