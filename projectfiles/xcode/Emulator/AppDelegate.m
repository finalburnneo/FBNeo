//
//  AppDelegate.m
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/15/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "AppDelegate.h"

#import <IOKit/pwr_mgt/IOPMLib.h>

#import "FBEmulatorController.h"
#import "FBLogViewerController.h"
#import "FBPreferencesController.h"

@interface AppDelegate ()

- (NSString *) appSupportPath;

@end

static AppDelegate *sharedInstance = nil;

@implementation AppDelegate
{
    FBLogViewerController *logViewer;
    FBPreferencesController *prefs;
    FBEmulatorController *emulator;

    IOPMAssertionID sleepAssertId;
}

- (void) awakeFromNib
{
    sleepAssertId = kIOPMNullAssertionID;

    sharedInstance = self;

    _audio = [FBAudio new];
    _input = [FBInput new];
    _supportedFormats = @[ @"zip", @"7z" ];

    _runloop = [FBMainThread new];
    _video = [FBVideo new];
}

+ (AppDelegate *) sharedInstance
{
    return sharedInstance;
}

#pragma mark - NSApplicationDelegate

- (void) applicationWillFinishLaunching:(NSNotification *) notification
{
    NSString *path = [NSBundle.mainBundle pathForResource:@"Defaults"
                                                   ofType:@"plist"];
    [NSUserDefaults.standardUserDefaults registerDefaults:[NSDictionary dictionaryWithContentsOfFile:path]];

    _supportPath = self.appSupportPath;
    _nvramPath = [_supportPath stringByAppendingPathComponent:@"NVRAM"];
    _dipSwitchPath = [_supportPath stringByAppendingPathComponent:@"DIPSwitches"];

    NSArray *paths = @[
        _nvramPath,
        _dipSwitchPath,
    ];

    for (NSString *path in paths)
        if (![NSFileManager.defaultManager fileExistsAtPath:path])
            [NSFileManager.defaultManager createDirectoryAtPath:path
                                    withIntermediateDirectories:YES
                                                     attributes:nil
                                                          error:NULL];
}

- (void) applicationDidFinishLaunching:(NSNotification *) aNotification {
    NSLog(@"applicationDidFinishLaunching");
    emulator = [FBEmulatorController new];
    [emulator showWindow:self];
    [_runloop start];
}

- (void) applicationWillTerminate:(NSNotification *) aNotification {
    NSLog(@"applicationWillTerminate");
    [_runloop cancel];

    // Give the emulation thread some time to finish
    NSDate *start = [NSDate date];
    while (!_runloop.isFinished && start.timeIntervalSinceNow > -2)
        [NSThread sleepForTimeInterval:0.25];
}

- (BOOL) application:(NSApplication *) sender
            openFile:(NSString *) filename
{
    NSLog(@"application:openFile:");
    [self loadPath:filename];
    return YES;
}

#pragma mark - Actions

- (void) openDocument:(id) sender
{
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.title = NSLocalizedString(@"Open", nil);
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.allowedFileTypes = _supportedFormats;

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

#pragma mark - Public

- (void) loadPath:(NSString *) path
{
    [_runloop load:path];
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:[NSURL fileURLWithPath:path]];
}

- (void) suppressScreenSaver
{
    if (sleepAssertId == kIOPMNullAssertionID) {
        IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
                                    kIOPMAssertionLevelOn,
                                    (__bridge CFStringRef) @"FBNeo",
                                    &sleepAssertId);
#if DEBUG
        NSLog(@"suppressScreenSaver");
#endif
    }
}

- (void) restoreScreenSaver
{
    if (sleepAssertId != kIOPMNullAssertionID) {
        IOPMAssertionRelease(sleepAssertId);
        sleepAssertId = kIOPMNullAssertionID;
#if DEBUG
        NSLog(@"restoreScreenSaver");
#endif
    }
}

#pragma mark - Private

- (NSString *) appSupportPath
{
    NSURL *url = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                      inDomains:NSUserDomainMask].lastObject;
    return [url URLByAppendingPathComponent:NSBundle.mainBundle.infoDictionary[(NSString *)kCFBundleNameKey]].path;
}

@end
