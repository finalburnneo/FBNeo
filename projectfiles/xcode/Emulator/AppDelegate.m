//(
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

@end

@implementation AppDelegate
{
    FBMainThread *main;
}

- (void) awakeFromNib
{
    main = [FBMainThread new];
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

@end
