//
//  AppDelegate.m
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/15/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "AppDelegate.h"

#import "FBLogViewerController.h"
#import "FBPreferencesController.h"

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;

- (void) resizeFrame:(NSSize) newSize
             animate:(BOOL) animate;
- (void) loadPath: (NSString *) path;
- (NSString *) appSupportPath;

@end

static AppDelegate *sharedInstance = nil;

@implementation AppDelegate
{
    BOOL _cursorVisible;
    NSTitlebarAccessoryViewController *tbAccessory;
    NSArray *supportedFormats;
    FBLogViewerController *logViewer;
    FBPreferencesController *prefs;
}

- (void) dealloc
{
    [_runloop removeObserver:self];
    screen.delegate = nil;
    _video.delegate = nil;
}

- (void) awakeFromNib
{
    sharedInstance = self;

    _input = [FBInput new];
    _cursorVisible = YES;
    supportedFormats = @[ @"zip", @"7z" ];

    tbAccessory = [NSTitlebarAccessoryViewController new];
    tbAccessory.view = spinner;
    tbAccessory.layoutAttribute = NSLayoutAttributeRight;

    _runloop = [FBMainThread new];
    [_runloop addObserver:self];
    _video = [FBVideo new];
    _video.delegate = screen;
    screen.delegate = self;
    _emulator = [FBEmulator new];

    label.stringValue = NSLocalizedString(@"Drop a set here to load it.", nil);
    label.hidden = NO;
    screen.hidden = YES;

    [_window registerForDraggedTypes:@[NSFilenamesPboardType]];
}

- (void) applicationWillFinishLaunching:(NSNotification *) notification
{
    _supportPath = self.appSupportPath;
    _nvramPath = [_supportPath stringByAppendingPathComponent:@"NVRAM"];

    NSArray *paths = @[
        _nvramPath
    ];

    [paths enumerateObjectsUsingBlock:^(NSString *path, NSUInteger idx, BOOL *stop) {
        if (![NSFileManager.defaultManager fileExistsAtPath:path])
            [NSFileManager.defaultManager createDirectoryAtPath:path
                                    withIntermediateDirectories:YES
                                                     attributes:nil
                                                          error:NULL];
    }];
}

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSLog(@"applicationDidFinishLaunching");
    [_runloop start];
}


- (void) applicationWillTerminate:(NSNotification *)aNotification {
	// Insert code here to tear down your application
}

- (void) windowWillClose:(NSNotification *) notification
{
    NSLog(@"windowWillClose");
    [_runloop cancel];

    NSLog(@"Emulator window closed; shutting down");
    [[NSApplication sharedApplication] terminate:nil];
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

- (BOOL) application:(NSApplication *) sender
            openFile:(NSString *) filename
{
    NSLog(@"application:openFile:");
    [self loadPath:filename];
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
    label.hidden = YES;

    _window.title = _emulator.title;
    [_window makeFirstResponder:screen];
}

- (void) gameSessionDidEnd
{
    NSLog(@"gameSessionDidEnd");
    screen.hidden = YES;
    label.hidden = NO;

    _window.title = NSBundle.mainBundle.infoDictionary[@"CFBundleName"];
}

- (void) driverInitDidStart
{
    [spinner.subviews.firstObject startAnimation:self];
    [_window addTitlebarAccessoryViewController:tbAccessory];

    label.stringValue = NSLocalizedString(@"Please wait...", nil);
    _window.title = NSLocalizedString(@"Loading...", nil);
}

- (void) driverInitDidEnd:(NSString *) name
                  success:(BOOL) success
{
    [spinner.subviews.firstObject stopAnimation:self];
    [tbAccessory removeFromParentViewController];

    if (success)
        label.stringValue = NSLocalizedString(@"Starting...", nil);
    else
        label.stringValue = [NSString stringWithFormat:NSLocalizedString(@"Error loading \"%@\".", nil), name];

    if (!success)
        [self displayLogViewer:nil];

    _window.title = NSBundle.mainBundle.infoDictionary[(NSString *)kCFBundleNameKey];
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

- (void) openDocument:(id) sender
{
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.title = NSLocalizedString(@"Open", nil);
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.allowedFileTypes = supportedFormats;

    [panel beginWithCompletionHandler:^(NSInteger result){
        if (result == NSModalResponseOK)
            [self loadPath:panel.URLs.firstObject.path];
    }];
}

- (void) displayLogViewer:(id) sender
{
    if (!logViewer)
        logViewer = [FBLogViewerController new];
    [logViewer showWindow:self];
}

- (void) displayPreferences:(id) sender
{
    if (!prefs)
        prefs = [FBPreferencesController new];
    [prefs showWindow:self];
}

- (void) togglePause:(id) sender
{
    _runloop.paused = !_runloop.isPaused;
}

#pragma mark - Drag & Drop

- (BOOL) performDragOperation:(id<NSDraggingInfo>) sender
{
    NSLog(@"performDragOperation");

    NSPasteboard *pboard = sender.draggingPasteboard;
    NSString *path = [[pboard propertyListForType:NSFilenamesPboardType] firstObject];

    [self loadPath:path];

    return YES;
}

- (NSDragOperation) draggingEntered:(id <NSDraggingInfo>) sender
{
    NSLog(@"draggingEntered");

    NSDragOperation dragOp = NSDragOperationNone;
    if (sender.draggingSourceOperationMask & NSDragOperationCopy) {
        NSPasteboard *pboard = sender.draggingPasteboard;
        NSString *path = [[pboard propertyListForType:NSFilenamesPboardType] firstObject];

        if ([supportedFormats containsObject:path.pathExtension])
            dragOp = NSDragOperationCopy;
    }

    return dragOp;
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

- (void) loadPath:(NSString *) path
{
    [_runloop load:path];
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:[NSURL fileURLWithPath:path]];
}

- (NSString *) appSupportPath
{
    NSURL *url = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                      inDomains:NSUserDomainMask].lastObject;
    return [url URLByAppendingPathComponent:NSBundle.mainBundle.infoDictionary[(NSString *)kCFBundleNameKey]].path;
}

@end
