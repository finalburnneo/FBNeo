// Copyright (c) Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import <Cocoa/Cocoa.h>

#import "FBScanner.h"
#import "FBImporter.h"
#import "FBDropFileScrollView.h"

@interface FBLauncherItem: NSObject

@property NSString *name;
@property NSString *title;
@property unsigned char status;
@property (readonly) NSMutableArray<FBLauncherItem *> *subsets;

@end

@interface FBLauncherController : NSWindowController<FBScannerDelegate, FBImporterDelegate, FBDropFileScrollViewDelegate>
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
