//
//  LogViewerController.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/23/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "FBMainThread.h"

@interface FBLogViewerController : NSWindowController<NSWindowDelegate, FBMainThreadDelegate>
{
    IBOutlet NSTextView *textView;
}

@end
