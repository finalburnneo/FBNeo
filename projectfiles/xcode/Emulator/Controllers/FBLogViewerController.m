//
//  LogViewerController.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/23/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBLogViewerController.h"
#import "NSWindowController+Core.h"

#import "AppDelegate.h"

@interface NSString (LogFormatter)

- (NSMutableAttributedString *) applyFormat;

@end

@implementation FBLogViewerController
{
    NSMutableAttributedString *logContent;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"LogViewer"]) {
        logContent = [NSMutableAttributedString new];
    }

    return self;
}

- (void) awakeFromNib
{
    [self.runloop addObserver:self];
}

- (void) dealloc
{
    [self.runloop removeObserver:self];
}

#pragma mark - NSWindowController

- (void) windowDidLoad
{
    [super windowDidLoad];

    [logContent appendAttributedString:[self.runloop.log applyFormat]];
    textView.textStorage.attributedString = logContent;
    [textView scrollToEndOfDocument:self];
}

#pragma mark - FBMainThreadDelegate

- (void) logDidUpdate:(NSString *) message
{
    [logContent appendAttributedString:[message applyFormat]];
    textView.textStorage.attributedString = logContent;

    if (self.window.visible)
        [textView scrollToEndOfDocument:self];
}

- (void) logDidClear
{
    [logContent setAttributedString:[NSAttributedString new]];
}

@end

#pragma mark - NSString (LogFormatter)

@implementation NSString (LogFormatter)

- (NSMutableAttributedString *) applyFormat
{
    NSMutableAttributedString *str = [[NSMutableAttributedString alloc] initWithString:self];
    [str addAttribute:NSFontAttributeName
                value:[NSFont fontWithName:@"Menlo" size:11]
                range:NSMakeRange(0, str.length)];

    NSRegularExpression *rx = [NSRegularExpression regularExpressionWithPattern:@"^!.*$"
                                                                        options:NSRegularExpressionAnchorsMatchLines
                                                                          error:NULL];

    NSArray *matches = [rx matchesInString:self
                                   options:0
                                     range:NSMakeRange(0, self.length)];

    for (NSTextCheckingResult *match in matches) {
        [str addAttribute:NSForegroundColorAttributeName
                    value:NSColor.redColor
                    range:match.range];
        [str addAttribute:NSFontAttributeName
                    value:[NSFont fontWithName:@"Menlo-Bold" size:11]
                    range:match.range];
    }

    return str;
}

@end
