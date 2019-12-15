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

#import "FBScreenView.h"
#import "FBMainThread.h"

@interface FBEmulatorController : NSWindowController<FBScreenViewDelegate, FBMainThreadDelegate>
{
    IBOutlet FBScreenView *screen;
    IBOutlet NSView *accView;
    IBOutlet NSButton *lockIcon;
    IBOutlet NSTextField *lockText;
    IBOutlet NSPanel *progressPanel;
    IBOutlet NSProgressIndicator *progressPanelBar;
}

- (IBAction) activateCursorLock:(id) sender;

- (IBAction) resetEmulation:(id) sender;
- (IBAction) togglePause:(id) sender;

- (IBAction) resizeNormalSize:(id) sender;
- (IBAction) resizeDoubleSize:(id) sender;

@end
