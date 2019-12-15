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

#import "FBVideo.h"
#import "FBInput.h"
#import "FBAudio.h"
#import "FBMainThread.h"

@interface AppDelegate : NSObject <NSApplicationDelegate>

+ (AppDelegate *) sharedInstance;

- (void) loadPath: (NSString *) path;
- (void) suppressScreenSaver;
- (void) restoreScreenSaver;

@property (readonly) FBMainThread *runloop;
@property (readonly) FBVideo *video;
@property (readonly) FBInput *input;
@property (readonly) FBAudio *audio;

@property (readonly) NSString *romPath;
@property (readonly) NSString *supportPath;
@property (readonly) NSString *nvramPath;
@property (readonly) NSString *dipSwitchPath;

@property (readonly) NSSet<NSString *> *supportedFormats;

- (IBAction) displayLogViewer:(id) sender;
- (IBAction) displayPreferences:(id) sender;
- (IBAction) displayLauncher:(id) sender;
- (IBAction) displayAbout:(id) sender;

@end
