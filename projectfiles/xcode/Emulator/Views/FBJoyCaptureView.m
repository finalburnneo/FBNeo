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

#import <Carbon/Carbon.h>

#import "FBJoyCaptureView.h"

#import "FBInputConstants.h"

@interface FBJoyCaptureView ()

- (NSRect) unmapRect;
- (BOOL) canUnmap;

@end

@implementation FBJoyCaptureView

static NSMutableDictionary *codeLookupTable;
static NSMutableDictionary *reverseCodeLookupTable;

+ (void) initialize
{
    codeLookupTable = [NSMutableDictionary new];
    reverseCodeLookupTable = [NSMutableDictionary new];
	
	NSDictionary *dict = @{ @"Up": @(FBGamepadUp),
							@"Down": @(FBGamepadDown),
							@"Left": @(FBGamepadLeft),
							@"Right": @(FBGamepadRight) };
	
	[dict enumerateKeysAndObjectsUsingBlock:^(NSString *desc, NSNumber *mask, BOOL * _Nonnull stop) {
		[codeLookupTable setObject:desc
							forKey:mask];
		[reverseCodeLookupTable setObject:mask
								   forKey:desc];
	}];
	
    for (int i = 1; i <= 31; i++) {
        NSNumber *code = @(FBMakeButton(i));
        NSString *desc = [NSString stringWithFormat:NSLocalizedString(@"Button %d", @"Joystick button"), i];
        [codeLookupTable setObject:desc
                            forKey:code];
        [reverseCodeLookupTable setObject:code
                                   forKey:desc];
    }
}

#pragma mark - Input events

- (BOOL) becomeFirstResponder
{
    if ([super becomeFirstResponder]) {
        [self setEditable:NO];
        [self setSelectable:NO];
        
        return YES;
    }
    
    return NO;
}

- (void) mouseDown:(NSEvent *) theEvent
{
    [super mouseDown:theEvent];
    
    if ([self canUnmap]) {
        NSPoint mousePosition = [self convertPoint:[theEvent locationInWindow] fromView:nil];
        if (NSPointInRect(mousePosition, [self unmapRect])) {
            [self captureCode:FBGamepadNone];
        }
    }
}

- (void) keyDown:(NSEvent *) theEvent
{
}

- (void) keyUp:(NSEvent *) theEvent
{
}

- (BOOL) captureCode:(int) code
{
    NSString *name = [FBJoyCaptureView descriptionForCode:code];
    if (!name) {
        name = @"";
    }
    
    // Update the editor's text with the code's description
    [[self textStorage] replaceCharactersInRange:NSMakeRange(0, self.textStorage.length)
                                      withString:name];
    
    // Resign first responder (closes the editor)
    [[self window] makeFirstResponder:(NSView *)self.delegate];
    
    return YES;
}

+ (NSString *) descriptionForCode:(int) code
{
	if (code != FBGamepadNone) {
		return [codeLookupTable objectForKey:@(code)];
    }
	
    return @"";
}

+ (int) codeForDescription:(NSString *) description
{
    if (description && [description length] > 0) {
        NSNumber *code = [reverseCodeLookupTable objectForKey:description];
        if (code) {
            return [code intValue];
        }
    }
	
	return FBGamepadNone;
}

#pragma mark - Private methods

- (BOOL) canUnmap
{
    return ([self string] && [[self string] length] > 0);
}

- (NSRect) unmapRect
{
    NSRect cellFrame = [self bounds];
    
    CGFloat diam = cellFrame.size.height * .70;
    return NSMakeRect(cellFrame.origin.x + cellFrame.size.width - cellFrame.size.height,
                      cellFrame.origin.y + (cellFrame.size.height - diam) / 2.0,
                      diam, diam);
}

#pragma mark - NSTextView

- (void) drawRect:(NSRect) dirtyRect
{
    [super drawRect:dirtyRect];
    
    [[NSGraphicsContext currentContext] saveGraphicsState];
    
    if ([self canUnmap]) {
        NSRect circleRect = [self unmapRect];
        NSBezierPath *path = [NSBezierPath bezierPathWithOvalInRect:circleRect];
        
        [[NSColor darkGrayColor] set];
        [path fill];
        
        NSRect dashRect = NSInsetRect(circleRect,
                                      circleRect.size.width * 0.2,
                                      circleRect.size.height * 0.6);
        
        path = [NSBezierPath bezierPathWithRect:dashRect];
        
        [[NSColor whiteColor] set];
        [path fill];
    } else {
        NSBezierPath *bgRect = [NSBezierPath bezierPathWithRect:[self bounds]];
        
        [[NSColor controlBackgroundColor] set];
        [bgRect fill];
        
        NSMutableParagraphStyle *mpstyle = [NSParagraphStyle.defaultParagraphStyle mutableCopy];
        [mpstyle setAlignment:[self alignment]];
        
        NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:
                                    mpstyle, NSParagraphStyleAttributeName,
                                    [self font], NSFontAttributeName,
                                    [NSColor disabledControlTextColor], NSForegroundColorAttributeName,
                                    
                                    nil];
        
        [@"..." drawInRect:[self bounds] withAttributes:attributes];
    }
    
    [[NSGraphicsContext currentContext] restoreGraphicsState];
}

@end
