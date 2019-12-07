//
//  FBAboutController.h
//  Emulator
//
//  Created by Akop Karapetyan on 12/06/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface FBWhitePanelView : NSView

@end

@interface FBAboutController : NSWindowController
{
    IBOutlet NSTextField *versionNumberField;
    IBOutlet NSTextField *appNameField;
}

- (IBAction) openLicense:(id) sender;

@end
