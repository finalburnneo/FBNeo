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

@interface FBLauncherController ()

- (void) reloadSets:(NSArray<FBROMSet *> *) romSets;

@end

@implementation FBLauncherController
{
    FBScanner *scanner;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"Launcher"]) {
        _romSets = [NSMutableArray new];
    }

    return self;
}

- (void) awakeFromNib
{
}

#pragma mark - NSWindowDelegate

- (void) windowWillLoad
{
    if (_romSets.count == 0)
        [self rescanSets:self];
}

- (void) windowWillClose:(NSNotification *) notification
{
    @synchronized (self) {
        [scanner cancel];
    }
}

#pragma mark - FBScannerDelegate

- (void) scanDidStart
{
    progressPanelBar.doubleValue = 0;
    progressPanelLabel.stringValue = NSLocalizedString(@"Scanning...", nil);
    progressPanelCancelButton.enabled = YES;

    [NSApp beginSheet:progressPanel
       modalForWindow:[self window]
        modalDelegate:self
       didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
          contextInfo:nil];
}

- (void) progressDidUpdate:(float) progress
{
    progressPanelBar.doubleValue = progress;
}

- (void) scanDidEnd:(NSArray<FBROMSet *> *) romSets
{
    @synchronized (self) {
        scanner = nil;
    }

    [NSApp endSheet:progressPanel];

    if (!romSets)
        return; // Cancelled, or otherwise failed

    [self reloadSets:romSets];
    [romSetTreeController rearrangeObjects];
}

#pragma mark - Callbacks

- (void) didEndSheet:(NSWindow *) sheet
          returnCode:(NSInteger) returnCode
         contextInfo:(void *) contextInfo
{
    [sheet orderOut:self];
}

#pragma mark - Actions

- (void) rescanSets:(id) sender
{
    @synchronized (self) {
        if (!scanner) {
            scanner = [FBScanner new];
            scanner.rootPath = AppDelegate.sharedInstance.romPath;
            scanner.delegate = self;
            [scanner start];
        }
    }
}

- (void) cancelProgress:(id) sender
{
    progressPanelCancelButton.enabled = NO;
    progressPanelLabel.stringValue = NSLocalizedString(@"Cancelling...", nil);

    @synchronized (self) {
        [scanner cancel];
    }
}

#pragma mark - Private

- (void) reloadSets:(NSArray<FBROMSet *> *) romSets
{
    // Clear the master list
    [(NSMutableArray *) _romSets removeAllObjects];

    // First, sweep the sets and add root-level sets to the master list,
    // and child sets to respective sub-list
    NSMutableDictionary<NSString *, NSMutableArray *> *childSetsByName = [NSMutableDictionary new];
    [romSets enumerateObjectsUsingBlock:^(FBROMSet *obj, NSUInteger idx, BOOL *stop) {
        NSDictionary *set = @{
            @"name": obj.name,
            @"title": obj.title,
            @"subsets": [NSMutableArray new],
        };
        if (!obj.parent)
            [(NSMutableArray *)_romSets addObject:set];
        else {
            NSMutableArray *childSet = childSetsByName[obj.parent];
            if (!childSet) {
                childSet = [NSMutableArray new];
                childSetsByName[obj.parent] = childSet;
            }
            [childSet addObject:set];
        }
    }];

    // Sweep through master list and add any child sets
    [_romSets enumerateObjectsUsingBlock:^(NSDictionary *obj, NSUInteger idx, BOOL *stop) {
        NSArray *children = childSetsByName[obj[@"name"]];
        if (children)
            [obj[@"subsets"] addObjectsFromArray:children];
    }];
}

@end
