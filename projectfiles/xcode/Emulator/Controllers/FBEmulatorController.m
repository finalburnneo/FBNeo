//
//  FBEmulatorController.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/25/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBEmulatorController.h"

#import "AppDelegate.h"
#import "NSWindowController+Core.h"

@interface FBEmulatorController ()

- (void) resizeFrame:(NSSize) newSize
             animate:(BOOL) animate;

@end

@implementation FBEmulatorController
{
    BOOL cursorVisible;
    BOOL autoPaused;
    NSTitlebarAccessoryViewController *tbAccessory;
    NSArray *defaultsToObserve;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"Emulator"]) {
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
    cursorVisible = YES;

    defaultsToObserve = @[ @"pauseWhenInactive" ];

    tbAccessory = [NSTitlebarAccessoryViewController new];
    tbAccessory.view = spinner;
    tbAccessory.layoutAttribute = NSLayoutAttributeRight;

    screen.delegate = self;
    self.video.delegate = screen;
    [self.runloop addObserver:self];

    label.stringValue = NSLocalizedString(@"Drop a set here to load it.", nil);
    label.hidden = NO;
    screen.hidden = YES;

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
    if (autoPaused && self.runloop.isPaused)
        self.runloop.paused = NO;
    autoPaused = NO;

    if ([NSUserDefaults.standardUserDefaults boolForKey:@"suppressScreenSaver"])
        [self.appDelegate suppressScreenSaver];
}

- (void) windowDidResignKey:(NSNotification *) notification
{
    if (!cursorVisible) {
        cursorVisible = YES;
        [NSCursor unhide];
    }

    if ([NSUserDefaults.standardUserDefaults boolForKey:@"pauseWhenInactive"]
        && !self.runloop.isPaused) {
        self.runloop.paused = YES;
        autoPaused = YES;
    }

    [self.appDelegate restoreScreenSaver];
}

- (void) windowWillClose:(NSNotification *) notification
{
    NSLog(@"windowWillClose");
    for (NSString *key in defaultsToObserve)
        [NSUserDefaults.standardUserDefaults removeObserver:self
                                                 forKeyPath:key
                                                    context:NULL];

    NSLog(@"Emulator window closed; shutting down");
    if (NSApplication.sharedApplication.isRunning)
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApplication.sharedApplication terminate:nil];
        });
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
            autoPaused = YES;
        }
    }
}

#pragma mark - FBScreenViewDelegate

- (void) mouseDidIdle
{
    if (cursorVisible) {
        cursorVisible = NO;
        [NSCursor hide];
    }
}

- (void) mouseStateDidChange
{
    if (!cursorVisible) {
        cursorVisible = YES;
        [NSCursor unhide];
    }
}

- (void) mouseDidMove:(NSPoint) point
{
    NSLog(@"%.02f,%.02f", point.x, point.y);
    self.input.mouseCoords = point;
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

    screen.hidden = NO;
    label.hidden = YES;

    [self.window makeFirstResponder:screen];
}

- (void) gameSessionDidEnd
{
    NSLog(@"gameSessionDidEnd");
    screen.hidden = YES;
    label.hidden = NO;

    self.window.title = NSBundle.mainBundle.infoDictionary[@"CFBundleName"];
}

- (void) driverInitDidStart
{
    [spinner.subviews.firstObject startAnimation:self];
    [self.window addTitlebarAccessoryViewController:tbAccessory];

    label.stringValue = NSLocalizedString(@"Please wait...", nil);
    self.window.title = NSLocalizedString(@"Loading...", nil);
}

- (void) driverInitDidEnd:(NSString *) name
                  success:(BOOL) success
{
    [spinner.subviews.firstObject stopAnimation:self];
    [tbAccessory removeFromParentViewController];

    if (success) {
        label.stringValue = NSLocalizedString(@"Starting...", nil);
        if ([NSUserDefaults.standardUserDefaults boolForKey:@"pauseWhenInactive"]
            && !self.window.isKeyWindow) {
            self.runloop.paused = YES;
            autoPaused = YES;
        }
        self.window.title = self.runloop.title;
    } else {
        label.stringValue = [NSString stringWithFormat:NSLocalizedString(@"Error loading \"%@\".", nil), name];
        [self.appDelegate displayLogViewer:self];
        self.window.title = NSBundle.mainBundle.infoDictionary[(NSString *)kCFBundleNameKey];
    }
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

@end
