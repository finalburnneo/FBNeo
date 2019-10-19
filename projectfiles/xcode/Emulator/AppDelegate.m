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
    NSLog(@"awakeFromNib");
    main = [FBMainThread new];
    [main start];
}

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Insert code here to initialize your application
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

@end
