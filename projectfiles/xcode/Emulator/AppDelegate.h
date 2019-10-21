//
//  AppDelegate.h
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/15/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "FBScreenView.h"

#import "FBVideo.h"
#import "FBInput.h"
#import "FBMainThread.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, FBScreenViewDelegate, FBMainThreadDelegate>
{
    IBOutlet FBScreenView *screen;
    IBOutlet NSView *spinner;
    IBOutlet NSTextField *label;
}

+ (AppDelegate *) sharedInstance;

@property (readonly) FBVideo *video;
@property (readonly) FBInput *input;

- (IBAction) resizeNormalSize:(id) sender;
- (IBAction) resizeDoubleSize:(id) sender;

@end
