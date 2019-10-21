//
//  AppDelegate.m
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/15/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "AppDelegate.h"

#import "FBMainThread.h"

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;

- (NSSize) gameScreenSize;

@end

static AppDelegate *sharedInstance = nil;

@implementation AppDelegate
{
    FBMainThread *main;
    BOOL _cursorVisible;
}

- (void)dealloc
{
    screen.delegate = nil;
}

- (void) awakeFromNib
{
    sharedInstance = self;
    _video = [FBVideo new];
    _input = [FBInput new];
    main = [FBMainThread new];

    _cursorVisible = YES;
    screen.delegate = self;
    _video.delegate = screen;
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
    NSSize screenSize = [self gameScreenSize];
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

    return frameSize;
}

- (void) windowDidResize:(NSNotification *) notification
{
    NSSize screenSize = [self gameScreenSize];
    if (screenSize.width != 0 && screenSize.height != 0) {
        NSRect windowFrame = [[self window] frame];
        NSRect contentRect = [[self window] contentRectForFrameRect:windowFrame];

        NSString *screenSizeString = NSStringFromSize(screenSize);
        NSString *actualSizeString = NSStringFromSize(contentRect.size);

        [[NSUserDefaults standardUserDefaults] setObject:actualSizeString
                                                  forKey:[@"preferredSize-" stringByAppendingString:screenSizeString]];
    }
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *) sender
{
    return YES;
}

- (BOOL) application:(NSApplication *) sender
            openFile:(NSString *) filename
{
    main.fileToOpen = filename;
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

#pragma mark - Private

extern int BurnDrvGetVisibleSize(int* pnWidth, int* pnHeight);

- (NSSize) gameScreenSize
{
    int w, h;
    BurnDrvGetVisibleSize(&w, &h);
    return NSMakeSize(w, h);
}

@end
