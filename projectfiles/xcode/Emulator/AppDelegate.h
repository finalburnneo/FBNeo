//
//  AppDelegate.h
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/15/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

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

@property (readonly) NSString *supportPath;
@property (readonly) NSString *nvramPath;
@property (readonly) NSString *dipSwitchPath;

@property (readonly) NSArray *supportedFormats;

- (IBAction) displayLogViewer:(id) sender;
- (IBAction) displayPreferences:(id) sender;

@end
