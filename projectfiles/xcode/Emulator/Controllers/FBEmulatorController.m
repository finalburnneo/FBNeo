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

// FIXME: on catalina, it still starts in white
// FIXME: don't add bogus paths to recently used!
#import "FBEmulatorController.h"

#import "AppDelegate.h"
#import "NSWindowController+Core.h"

@interface FBEmulatorController ()

- (void) resizeFrame:(NSSize) newSize
             animate:(BOOL) animate;
- (void) lockCursor;
- (void) unlockCursor;
- (void) hideCursor;
- (void) unhideCursor:(BOOL) force;
- (NSPoint) convertPointToScreen:(NSPoint) point;

@end

@implementation FBEmulatorController
{
    BOOL isCursorLocked;
    BOOL isCursorVisible;
    BOOL isAutoPaused;
    NSArray *defaultsToObserve;
    BOOL driverLoadError;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"Emulator"]) {
        isCursorLocked = NO;
        isCursorVisible = YES;
        isAutoPaused = NO;
    }

    return self;
}

- (void) dealloc
{
    screen.delegate = nil;
    self.video.delegate = nil;
    [self.runloop removeObserver:self];
}

- (void) awakeFromNib
{
    defaultsToObserve = @[
        @"pauseWhenInactive",
        @"suppressScreenSaver",
        @"hideLockOptions",
        @"masterVolume",
    ];

    NSTitlebarAccessoryViewController *tba = [NSTitlebarAccessoryViewController new];
    tba.view = accView;
    tba.layoutAttribute = NSLayoutAttributeRight;
    lockText.stringValue = NSLocalizedString(@"⌘+click", nil);
    lockText.hidden = YES;
    lockIcon.image = [NSImage imageNamed:@"NSLockUnlockedTemplate"];
    lockIcon.enabled = NO;
    lockIcon.hidden = [NSUserDefaults.standardUserDefaults boolForKey:@"hideLockOptions"];
    [self.window addTitlebarAccessoryViewController:tba];
    self.window.backgroundColor = NSColor.blackColor;

    screen.delegate = self;
    self.video.delegate = screen;
    [self.runloop addObserver:self];

    [self.window registerForDraggedTypes:@[NSFilenamesPboardType]];
    for (NSString *key in defaultsToObserve)
        [NSUserDefaults.standardUserDefaults addObserver:self
                                              forKeyPath:key
                                                 options:NSKeyValueObservingOptionNew
                                                 context:NULL];
}

#pragma mark - NSWindowDelegate

- (void) windowDidBecomeKey:(NSNotification *) notification
{
    [self.input setFocus:YES];
    if (isAutoPaused && self.runloop.isPaused)
        self.runloop.paused = NO;
    isAutoPaused = NO;

    if ([NSUserDefaults.standardUserDefaults boolForKey:@"suppressScreenSaver"])
        [self.appDelegate suppressScreenSaver];
}

- (void) windowDidResignKey:(NSNotification *) notification
{
    [self.input setFocus:NO];
    [self unlockCursor];

    if ([NSUserDefaults.standardUserDefaults boolForKey:@"pauseWhenInactive"]
        && !self.runloop.isPaused) {
        self.runloop.paused = YES;
        isAutoPaused = YES;
    }

    [self.appDelegate restoreScreenSaver];
}

- (NSSize) windowWillResize:(NSWindow *) sender
                     toSize:(NSSize) frameSize
{
    NSSize screenSize = [self.video gameScreenSize];
    if (screenSize.width != 0 && screenSize.height != 0) {
        NSRect windowFrame = [[self window] frame];
        NSView *contentView = [[self window] contentView];
        NSRect viewRect = [contentView convertRect:[contentView bounds]
                                            toView:nil];
        NSRect contentRect = [[self window] contentRectForFrameRect:windowFrame];

        CGFloat screenRatio = screenSize.width / screenSize.height;

        float marginY = viewRect.origin.y + windowFrame.size.height - contentRect.size.height;
        float marginX = contentRect.size.width - viewRect.size.width;

        // Clamp the minimum height
        if ((frameSize.height - marginY) < screenSize.height) {
            frameSize.height = screenSize.height + marginY;
        }

        // Set the screen width as a percentage of the screen height
        frameSize.width = (frameSize.height - marginY) * screenRatio + marginX;
    }

    return frameSize;
}

#pragma mark - Notifications

- (void) observeValueForKeyPath:(NSString *) keyPath
                       ofObject:(id) object
                         change:(NSDictionary *) change
                        context:(void *) context
{
    if ([keyPath isEqualToString:@"pauseWhenInactive"]) {
        BOOL newValue = [[change objectForKey:NSKeyValueChangeNewKey] boolValue];
        if (!self.window.isKeyWindow && newValue && !self.runloop.isPaused) {
            self.runloop.paused = YES;
            isAutoPaused = YES;
        }
    } else if ([keyPath isEqualToString:@"hideLockOptions"]) {
        BOOL newValue = [[change objectForKey:NSKeyValueChangeNewKey] boolValue];
        if (!isCursorLocked)
            lockIcon.hidden = !self.input.usesMouse && newValue;
    } else if ([keyPath isEqualToString:@"masterVolume"]) {
        BOOL newValue = [[change objectForKey:NSKeyValueChangeNewKey] intValue];
        self.audio.volume = newValue / 100.0f;
    }
}

#pragma mark - FBScreenViewDelegate

- (void) mouseDidIdle
{
    [self hideCursor];
}

- (void) mouseStateDidChange
{
    if (!isCursorLocked)
        [self unhideCursor:NO];
}

- (void) mouseDidMove:(NSPoint) point
{
    self.input.mouseCoords = point;
}

- (void) mouseButtonStateDidChange:(NSEvent *) event
{
    if (event.type == NSEventTypeLeftMouseDown
        && (event.modifierFlags & NSEventModifierFlagCommand) != 0) {
        if (isCursorLocked)
            [self unlockCursor];
        else
            [self lockCursor];
    } else if (event.type == NSEventTypeLeftMouseDown
               && !isCursorLocked
               && self.input.usesMouse) {
        [self lockCursor];
    } else switch (event.type) {
        case NSEventTypeLeftMouseDown:
            self.input.mouseButtonStates |= FBN_LMB;
            break;
        case NSEventTypeLeftMouseUp:
            self.input.mouseButtonStates &= ~FBN_LMB;
            break;
        case NSEventTypeRightMouseDown:
            self.input.mouseButtonStates |= FBN_RMB;
            break;
        case NSEventTypeRightMouseUp:
            self.input.mouseButtonStates &= ~FBN_RMB;
            break;
        default:
            break;
    }
}

#pragma mark - Drag & Drop

- (BOOL) performDragOperation:(id<NSDraggingInfo>) sender
{
    NSLog(@"performDragOperation");

    NSPasteboard *pboard = sender.draggingPasteboard;
    NSString *path = [[pboard propertyListForType:NSFilenamesPboardType] firstObject];

    [self.appDelegate loadPath:path];

    return YES;
}

- (NSDragOperation) draggingEntered:(id <NSDraggingInfo>) sender
{
    NSLog(@"draggingEntered");

    NSDragOperation dragOp = NSDragOperationNone;
    if (sender.draggingSourceOperationMask & NSDragOperationCopy) {
        NSPasteboard *pboard = sender.draggingPasteboard;
        NSString *path = [[pboard propertyListForType:NSFilenamesPboardType] firstObject];

        if ([self.appDelegate.supportedFormats containsObject:path.pathExtension])
            dragOp = NSDragOperationCopy;
    }

    return dragOp;
}

#pragma mark - FBMainThreadDelegate

- (void) gameSessionDidStart:(NSString *) name
{
    NSLog(@"gameSessionDidStart: %@", name);
    NSSize screenSize = self.video.gameScreenSize;
    if (screenSize.width != 0 && screenSize.height != 0)
        [self resizeFrame:NSMakeSize(screenSize.width * 2, screenSize.height * 2)
                  animate:NO];

    lockIcon.enabled = YES;
    lockIcon.hidden = [NSUserDefaults.standardUserDefaults boolForKey:@"hideLockOptions"] && !self.input.usesMouse;

    [self.window makeFirstResponder:screen];
}

- (void) gameSessionDidEnd
{
    NSLog(@"gameSessionDidEnd");
    lockIcon.enabled = NO;
    [self unlockCursor];

    self.window.title = NSBundle.mainBundle.infoDictionary[@"CFBundleName"];
    lockIcon.hidden = [NSUserDefaults.standardUserDefaults boolForKey:@"hideLockOptions"];
}

- (void) driverInitDidStart
{
    [progressPanelBar startAnimation:self];
    [self.window beginSheet:progressPanel
          completionHandler:^(NSModalResponse returnCode) { }];

    self.window.title = NSLocalizedString(@"Loading...", nil);
    driverLoadError = NO;
}

- (void) driverInitDidEnd:(NSString *) name
                  success:(BOOL) success
{
    [self.window endSheet:progressPanel];

    if (success) {
        if ([NSUserDefaults.standardUserDefaults boolForKey:@"pauseWhenInactive"]
            && !self.window.isKeyWindow) {
            self.runloop.paused = YES;
            isAutoPaused = YES;
        }
        self.window.title = self.runloop.title;
        if (driverLoadError)
            [self.appDelegate displayLogViewer:self];
    } else {
        self.window.title = NSBundle.mainBundle.infoDictionary[(NSString *)kCFBundleNameKey];
        [self.appDelegate displayLogViewer:self];
    }
}

- (void) logDidUpdate:(NSString *) message
{
    if ([message hasPrefix:@"!"])
        driverLoadError = YES;
}

#pragma mark - Actions

- (BOOL) validateMenuItem:(NSMenuItem *) menuItem
{
    if (menuItem.action == @selector(resetEmulation:))
        return self.runloop.isRunning;
    else if (menuItem.action == @selector(togglePause:)) {
        if (self.runloop.isRunning && self.runloop.isPaused)
            menuItem.title = NSLocalizedString(@"Resume", nil);
        else
            menuItem.title = NSLocalizedString(@"Pause", nil);
        return self.runloop.isRunning;
    }

    return menuItem.isEnabled;
}

- (void) resetEmulation:(id) sender
{
    [self.input simReset];
}

- (void) togglePause:(id) sender
{
    self.runloop.paused = !self.runloop.isPaused;
}

- (void) resizeNormalSize:(id) sender
{
    NSSize screenSize = self.video.gameScreenSize;
    if (screenSize.width != 0 && screenSize.height != 0)
        [self resizeFrame:screenSize
                  animate:YES];
}

- (void) resizeDoubleSize:(id) sender
{
    NSSize screenSize = self.video.gameScreenSize;
    if (screenSize.width != 0 && screenSize.height != 0)
        [self resizeFrame:NSMakeSize(screenSize.width * 2, screenSize.height * 2)
                  animate:YES];
}

- (void) activateCursorLock:(id) sender
{
    if (!isCursorLocked)
        [self lockCursor];
}

#pragma mark - Private

- (void) resizeFrame:(NSSize) newSize
             animate:(BOOL) animate
{
    // Turn off full screen if active
    if ((NSApplication.sharedApplication.presentationOptions & NSApplicationPresentationFullScreen) != 0) {
        [self.window toggleFullScreen:nil];
    }

    NSRect windowRect = self.window.frame;
    NSSize windowSize = windowRect.size;
    NSSize glViewSize = self.window.contentView.bounds.size;

    CGFloat newWidth = newSize.width + (windowSize.width - glViewSize.width);
    CGFloat newHeight = newSize.height + (windowSize.height - glViewSize.height);

    NSRect newRect = NSMakeRect(windowRect.origin.x, windowRect.origin.y,
                                newWidth, newHeight);

    [self.window setFrame:newRect
                  display:YES
                  animate:animate];
}

- (void) hideCursor
{
    if (isCursorVisible) {
        isCursorVisible = NO;
        [NSCursor hide];
#ifdef DEBUG
    NSLog(@"hideCursor");
#endif
    }
}

- (void) unhideCursor:(BOOL) force
{
    if (!isCursorVisible || force) {
        isCursorVisible = YES;
        [NSCursor unhide];
#ifdef DEBUG
    NSLog(@"unhideCursor");
#endif
    }
}

- (void) lockCursor
{
    if (!isCursorLocked) {
        CGAssociateMouseAndMouseCursorPosition(false);

        CGFloat offset = self.window.screen.frame.size.height + self.window.screen.frame.origin.y;
        NSPoint centerScreen = [self convertPointToScreen:NSMakePoint(NSMidX(screen.bounds),
                                                                      NSMidY(screen.bounds))];
        NSPoint screenCoords = NSMakePoint(centerScreen.x, offset - centerScreen.y);
        CGWarpMouseCursorPosition(NSPointToCGPoint(screenCoords));

        isCursorLocked = YES;
        lockIcon.image = [NSImage imageNamed:@"NSLockLockedTemplate"];
        lockIcon.hidden = NO;
        lockText.hidden = NO;
#ifdef DEBUG
    NSLog(@"lockCursor");
#endif
    }

    [self hideCursor];
}

- (void) unlockCursor
{
    if (isCursorLocked) {
        CGAssociateMouseAndMouseCursorPosition(true);
        isCursorLocked = NO;
        lockIcon.image = [NSImage imageNamed:@"NSLockUnlockedTemplate"];
        lockIcon.hidden = [NSUserDefaults.standardUserDefaults boolForKey:@"hideLockOptions"] && !self.input.usesMouse;
        lockText.hidden = YES;
#ifdef DEBUG
    NSLog(@"unlockCursor");
#endif
    }

    [self unhideCursor:YES];
}

- (NSPoint) convertPointToScreen:(NSPoint) point
{
    NSWindow *window = self.window;
    if (@available(macOS 10.12, *))
        return [window convertPointToScreen:point];

    NSRect frame = window.frame;
    point.x += frame.origin.x;
    point.y += frame.origin.y;

    return point;
}

@end
