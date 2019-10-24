//
//  LogViewerController.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/23/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBLogViewerController.h"

#import "AppDelegate.h"

@interface FBLogViewerController()

- (void) loadText:(NSString *) text;

@end

@implementation FBLogViewerController
{
    NSFont *plainFont;
    NSFont *boldFont;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"LogViewer"]) {
        plainFont = [NSFont fontWithName:@"Menlo" size:11];
        boldFont = [NSFont fontWithName:@"Menlo-Bold" size:11];
    }

    return self;
}

#pragma mark - NSWindowController

- (void) windowDidLoad
{
    [super windowDidLoad];

    [self loadText:AppDelegate.sharedInstance.runloop.log];
}

#pragma mark - Private

- (void) loadText:(NSString *) text
{
    NSMutableAttributedString *str = [[NSMutableAttributedString alloc] initWithString:text];

    [str addAttribute:NSFontAttributeName
                value:plainFont
                range:NSMakeRange(0, str.length)];

    NSRegularExpression *rx = [NSRegularExpression regularExpressionWithPattern:@"^!.*$"
                                                                        options:NSRegularExpressionAnchorsMatchLines
                                                                          error:NULL];

    NSArray *matches = [rx matchesInString:text
                                   options:0
                                     range:NSMakeRange(0, text.length)];
    for (NSTextCheckingResult *match in matches) {
        [str addAttribute:NSForegroundColorAttributeName
                    value:NSColor.redColor
                    range:match.range];
        [str addAttribute:NSFontAttributeName
                    value:boldFont
                    range:match.range];
    }

    textView.textStorage.attributedString = str;
}

@end
