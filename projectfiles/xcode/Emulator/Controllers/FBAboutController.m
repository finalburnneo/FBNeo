//
//  FBAboutController.m
//  Emulator
//
//  Created by Akop Karapetyan on 12/06/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBAboutController.h"

#pragma mark - FBWhitePanelView

@implementation FBWhitePanelView

- (void) drawRect:(NSRect) dirtyRect
{
    [NSColor.whiteColor setFill];
    NSRectFill(dirtyRect);
}

@end

#pragma mark - FBAboutController

@interface FBAboutController ()

@end

@implementation FBAboutController

- (id) init
{
    if (self = [super initWithWindowNibName:@"About"]) {
    }

    return self;
}

- (void) awakeFromNib
{
    NSDictionary *infoDict = NSBundle.mainBundle.infoDictionary;
    NSMutableAttributedString *appName = appNameField.attributedStringValue.mutableCopy;
    [appName beginEditing];
    [appName replaceCharactersInRange:NSMakeRange(0, appName.length)
                           withString:infoDict[@"CFBundleName"]];
    [appName applyFontTraits:NSBoldFontMask
                       range:NSMakeRange(appName.length - 3, 3)];
    [appName endEditing];

    appNameField.attributedStringValue = appName;
    versionNumberField.stringValue = [NSString stringWithFormat:NSLocalizedString(@"Version %@", nil),
                                      infoDict[@"CFBundleShortVersionString"]];
}

#pragma mark - Actions

- (void) openLicense:(id) sender
{
    NSString *documentPath = [NSBundle.mainBundle pathForResource:@"license"
                                                           ofType:@"txt"];
    [NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:documentPath]];
}


@end
