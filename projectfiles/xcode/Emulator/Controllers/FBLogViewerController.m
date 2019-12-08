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
