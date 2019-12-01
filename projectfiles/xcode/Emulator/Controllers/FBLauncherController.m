//
//  FBLauncherController.m
//  Emulator
//
//  Created by Akop Karapetyan on 11/30/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBLauncherController.h"

#import "AppDelegate.h"
#import "NSWindowController+Core.h"

@implementation FBLauncherController
{
    FBScanner *scanner;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"Launcher"]) {
    }

    return self;
}

- (void) awakeFromNib
{
}

- (void) hiya:(id) sender
{
    // FIXME!!
    // FIXME!!
    if (!scanner) {
        scanner = [FBScanner new];
        scanner.rootPath = AppDelegate.sharedInstance.romPath;
        scanner.delegate = self;
        [scanner start];
    }
}

#pragma mark - NSWindowDelegate

- (void) windowWillClose:(NSNotification *) notification
{
    [scanner cancel];
    scanner = nil;
}

#pragma mark - FBScannerDelegate

- (void) scanDidStart
{
    NSLog(@"scan started");
}

- (void) scanDidEnd
{
    scanner = nil;
    NSLog(@"scan ended");
}

@end
