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
    NSString *documentPath = [NSBundle.mainBundle pathForResource:@"License"
                                                           ofType:@"rtf"];
    [NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:documentPath]];
}


@end
