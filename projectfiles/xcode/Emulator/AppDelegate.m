//
//  AppDelegate.m
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/15/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "AppDelegate.h"

// FIXME: errors during load
// FIXME: starting without ROM selected
// FIXME: dropping file into window
// FIXME: focus on visibility toggle
@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;

- (void) resizeFrame:(NSSize) newSize
             animate:(BOOL) animate;

@end

static AppDelegate *sharedInstance = nil;

@implementation AppDelegate
{
    FBMainThread *main;
    BOOL _cursorVisible;
    NSTitlebarAccessoryViewController *tbAccessory;
}

- (void) dealloc
{
    main.delegate = nil;
    screen.delegate = nil;
    _video.delegate = nil;
}

- (void) awakeFromNib
{
    sharedInstance = self;

    _input = [FBInput new];
    _cursorVisible = YES;

    tbAccessory = [NSTitlebarAccessoryViewController new];
    tbAccessory.view = spinner;
    tbAccessory.layoutAttribute = NSLayoutAttributeRight;

    main = [FBMainThread new];
    main.delegate = self;
    _video = [FBVideo new];
    _video.delegate = screen;
    screen.delegate = self;
}

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSLog(@"applicationDidFinishLaunching");
    [main start];
}


- (void) applicationWillTerminate:(NSNotification *)aNotification {
	// Insert code here to tear down your application
}

- (void) windowWillClose:(NSNotification *) notification
{
    NSLog(@"windowWillClose");
    [main cancel];
}

- (NSSize) windowWillResize:(NSWindow *) sender
                     toSize:(NSSize) frameSize
{
    NSSize screenSize = [_video gameScreenSize];
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

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *) sender
{
    return YES;
}

- (BOOL) application:(NSApplication *) sender
            openFile:(NSString *) filename
{
    NSLog(@"application:openFile:");
    [main load:filename];
    return YES;
}

+ (AppDelegate *) sharedInstance
{
    return sharedInstance;
}

#pragma mark - FBScreenViewDelegate

- (void) mouseDidIdle
{
    if (_cursorVisible) {
        _cursorVisible = NO;
        [NSCursor hide];
    }
}

- (void) mouseStateDidChange
{
    if (!_cursorVisible) {
        _cursorVisible = YES;
        [NSCursor unhide];
    }
}

#pragma mark - FBMainThreadDelegate

- (void) gameSessionDidStart:(NSString *) name
{
    NSLog(@"gameSessionDidStart: %@", name);
    NSSize screenSize = _video.gameScreenSize;
    if (screenSize.width != 0 && screenSize.height != 0)
        [self resizeFrame:NSMakeSize(screenSize.width * 2, screenSize.height * 2)
                  animate:NO];

    screen.hidden = NO;
}

- (void) gameSessionDidEnd
{
    NSLog(@"gameSessionDidEnd");
    screen.hidden = YES;
}

- (void) driverInitDidStart
{
    [spinner.subviews.firstObject startAnimation:self];
    [_window addTitlebarAccessoryViewController:tbAccessory];
}

- (void) driverInitDidEnd:(BOOL) success
{
    [spinner.subviews.firstObject stopAnimation:self];
    [tbAccessory removeFromParentViewController];
}

#pragma mark - Actions

- (void) resizeNormalSize:(id) sender
{
    NSSize screenSize = _video.gameScreenSize;
    if (screenSize.width != 0 && screenSize.height != 0)
        [self resizeFrame:screenSize
                  animate:YES];
}

- (void) resizeDoubleSize:(id) sender
{
    NSSize screenSize = _video.gameScreenSize;
    if (screenSize.width != 0 && screenSize.height != 0)
        [self resizeFrame:NSMakeSize(screenSize.width * 2, screenSize.height * 2)
                  animate:YES];
}

#pragma mark - Private

- (void) resizeFrame:(NSSize) newSize
             animate:(BOOL) animate
{
    // Turn off full screen if active
    if (([[NSApplication sharedApplication] presentationOptions] & NSApplicationPresentationFullScreen) != 0) {
        [_window toggleFullScreen:nil];
    }

    NSRect windowRect = _window.frame;
    NSSize windowSize = windowRect.size;
    NSSize glViewSize = _window.contentView.bounds.size;

    CGFloat newWidth = newSize.width + (windowSize.width - glViewSize.width);
    CGFloat newHeight = newSize.height + (windowSize.height - glViewSize.height);

    NSRect newRect = NSMakeRect(windowRect.origin.x, windowRect.origin.y,
                                newWidth, newHeight);

    [_window setFrame:newRect
              display:YES
              animate:animate];
}

@end
