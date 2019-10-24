//
//  LogViewerController.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/23/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface FBLogViewerController : NSWindowController<NSWindowDelegate>
{
    IBOutlet NSTextView *textView;
}

@end
