//
//  FBPreferencesController.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/24/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface FBPreferencesController : NSWindowController<NSTabViewDelegate>
{
    IBOutlet NSToolbar *toolbar;
    IBOutlet NSTabView *contentTabView;
}

- (IBAction) tabChanged:(id) sender;

@end
