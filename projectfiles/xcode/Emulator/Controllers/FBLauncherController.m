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

#pragma mark - FBLauncherItem

@implementation FBLauncherItem

- (instancetype) init
{
    if (self = [super init]) {
        _subsets = [NSMutableArray new];
    }
    return self;
}

@end

#pragma mark - FBLauncherController

@interface FBLauncherController ()

- (void) reloadSets:(NSArray<FBROMSet *> *) romSets;
- (NSString *) setCachePath;
- (NSSet<NSString *> *) supportedRomSets;

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
    if ([NSFileManager.defaultManager fileExistsAtPath:self.setCachePath]) {
        // Restore from cache
        NSArray<FBROMSet *> *cached = [NSKeyedUnarchiver unarchiveObjectWithFile:self.setCachePath];
        if (cached)
            [self reloadSets:cached];
    }

    if (_romSets.count == 0)
        [self rescanSets:self];
}

#pragma mark - NSWindowDelegate

- (void) windowWillLoad
{
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

    [self.window beginSheet:progressPanel
          completionHandler:^(NSModalResponse returnCode) { }];
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

    [self.window endSheet:progressPanel];

    if (!romSets)
        return; // Cancelled, or otherwise failed

    // Save to cache
    [NSKeyedArchiver archiveRootObject:romSets
                                toFile:self.setCachePath];

    [self reloadSets:romSets];
}

#pragma mark - FBDropFileScrollView

- (BOOL) isDropAcceptable:(NSArray<NSString *> *) paths
{
    __block BOOL acceptable = YES;
    NSSet<NSString *> *supportedFormats = self.appDelegate.supportedFormats;
    NSSet<NSString *> *supportedArchives = self.supportedRomSets;

    [paths enumerateObjectsUsingBlock:^(NSString *path, NSUInteger idx, BOOL *stop) {
        NSString *filename = path.lastPathComponent;
        // Check extension && archive name
        if (![supportedFormats containsObject:filename.pathExtension]
            || ![supportedArchives containsObject:filename.stringByDeletingPathExtension]) {
            acceptable = NO;
            *stop = YES;
        }
    }];

    return acceptable;
}

- (void) dropDidComplete:(NSArray<NSString *> *) paths
{
    // FIXME
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

- (void) launchSet:(id) sender
{
    FBLauncherItem *item = romSetTreeController.selectedObjects.lastObject;
    if (!item || item.status == FBROMSET_STATUS_UNPLAYABLE)
        return;

    // FIXME: hack!
    // Ask emulator to load a (possibly) non-existent file
    // The loader doesn't really care whether the file exists - it only
    // uses the set's path and file name to determine which set to load
    // and from where.
    NSString *file = [item.name stringByAppendingPathExtension:@"zip"];
    [self.appDelegate loadPath:[self.appDelegate.romPath stringByAppendingPathComponent:file]];
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
        FBLauncherItem *item = [FBLauncherItem new];
        item.name = obj.name;
        item.title = obj.title;
        item.status = obj.status;

        if (!obj.parent)
            [(NSMutableArray *) _romSets addObject:item];
        else {
            NSMutableArray *childSet = childSetsByName[obj.parent];
            if (!childSet) {
                childSet = [NSMutableArray new];
                childSetsByName[obj.parent] = childSet;
            }
            [childSet addObject:item];
        }
    }];

    // Sweep through master list and add any child sets
    [_romSets enumerateObjectsUsingBlock:^(FBLauncherItem *obj, NSUInteger idx, BOOL *stop) {
        NSArray *children = childSetsByName[obj.name];
        if (children)
            [obj.subsets addObjectsFromArray:children];
    }];

    [romSetTreeController rearrangeObjects];
}

- (NSString *) setCachePath
{
    return [self.appDelegate.supportPath stringByAppendingPathComponent:@"SetCache.plist"];
}

- (NSSet<NSString *> *) supportedRomSets
{
    NSMutableSet *set = [NSMutableSet new];
    [_romSets enumerateObjectsUsingBlock:^(FBLauncherItem *item, NSUInteger idx, BOOL *stop) {
        [set addObject:item.name];
        [item.subsets enumerateObjectsUsingBlock:^(FBLauncherItem *sub, NSUInteger idx, BOOL *stop) {
            [set addObject:sub.name];
        }];
    }];

    return set;
}

@end
