// Copyright (c) Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

- (void) importFiles:(NSArray<NSString *> *) files;
- (void) initiateScan;

@end

@implementation FBLauncherController
{
    FBScanner *scanner;
    FBImporter *importer;
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
        int reqdCacheVersion = [NSBundle.mainBundle.infoDictionary[@"FBSetCacheVersion"] intValue];
        int cachedVersion = [NSUserDefaults.standardUserDefaults integerForKey:@"cachedVersion"];

        NSLog(@"Checking cache data; required:(%d); present:(%d)",
              reqdCacheVersion, cachedVersion);

        // Restore from cache
        NSArray<FBROMSet *> *cached = nil;
        if (cachedVersion == reqdCacheVersion)
            cached = [NSKeyedUnarchiver unarchiveObjectWithFile:self.setCachePath];

        if (cached) {
            [self reloadSets:cached];

            // Restore previous selection
            NSData *data = [NSUserDefaults.standardUserDefaults objectForKey:@"selectedGame"];
            romSetTreeController.selectionIndexPaths = [NSKeyedUnarchiver unarchiveTopLevelObjectWithData:data
                                                                                                    error:nil];
        }
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
    [NSUserDefaults.standardUserDefaults setObject:[NSKeyedArchiver archivedDataWithRootObject:romSetTreeController.selectionIndexPaths]
                                            forKey:@"selectedGame"];
    [importer cancel];
    [scanner cancel];
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

- (void) scanDidProgress:(float) progress
{
    progressPanelBar.doubleValue = progress;
}

- (void) scanDidEnd:(NSArray<FBROMSet *> *) romSets
{
    scanner = nil;

    [self.window endSheet:progressPanel];

    if (!romSets)
        return; // Cancelled, or otherwise failed

    // Save to cache
    [NSKeyedArchiver archiveRootObject:romSets
                                toFile:self.setCachePath];
    [NSUserDefaults.standardUserDefaults setInteger:[NSBundle.mainBundle.infoDictionary[@"FBSetCacheVersion"] intValue]
                                            forKey:@"cachedVersion"];

    [self reloadSets:romSets];
}

#pragma mark - FBImporterDelegate

- (void) importDidStart
{
    progressPanelBar.doubleValue = 0;
    progressPanelLabel.stringValue = NSLocalizedString(@"Importing...", nil);
    progressPanelCancelButton.enabled = YES;

    [self.window beginSheet:progressPanel
          completionHandler:^(NSModalResponse returnCode) { }];
}

- (void) importDidProgress:(float) progress
{
    progressPanelBar.doubleValue = progress;
}

- (void) importDidEnd:(BOOL) isCancelled
          filesCopied:(int) count
{
    importer = nil;
    NSLog(@"Import complete; %d files copied", count);

    [self.window endSheet:progressPanel];

    if (isCancelled || count < 1)
        return;

    [self rescanSets:self];
}

#pragma mark - FBDropFileScrollView

- (BOOL) isDropAcceptable:(NSArray<NSString *> *) paths
{
    __block BOOL acceptable = YES;
    NSSet<NSString *> *supportedFormats = self.appDelegate.supportedFormats;
    NSSet<NSString *> *supportedArchives = self.supportedRomSets;
    NSString *romPath = self.appDelegate.romPath;

    [paths enumerateObjectsUsingBlock:^(NSString *path, NSUInteger idx, BOOL *stop) {
        NSString *filename = path.lastPathComponent;
        NSString *parent = path.stringByDeletingLastPathComponent.stringByResolvingSymlinksInPath;

        // Check extension && archive name
        if (![supportedFormats containsObject:filename.pathExtension]
            || ![supportedArchives containsObject:filename.stringByDeletingPathExtension]
            || [parent isEqualToString:romPath]) {
            acceptable = NO;
            *stop = YES;
        }
    }];

    return acceptable;
}

- (void) dropDidComplete:(NSArray<NSString *> *) paths
{
    [self importFiles:paths];
}

#pragma mark - Actions

- (void) rescanSets:(id) sender
{
    [self initiateScan];
}

- (void) cancelProgress:(id) sender
{
    progressPanelCancelButton.enabled = NO;
    progressPanelLabel.stringValue = NSLocalizedString(@"Cancelling...", nil);

    [importer cancel];
    [scanner cancel];
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
    // Save current selection
    NSArray<NSIndexPath *> *selection = romSetTreeController.selectionIndexPaths;

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

    // Restore selection
    if (selection)
        romSetTreeController.selectionIndexPaths = selection;
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

- (void) importFiles:(NSArray<NSString *> *) files
{
    if (scanner || importer)
        return;

    importer = [FBImporter new];
    importer.destinationPath = AppDelegate.sharedInstance.romPath;
    importer.sourcePaths = files;
    importer.delegate = self;

    [importer start];
}

- (void) initiateScan
{
    if (scanner || importer)
        return;

    scanner = [FBScanner new];
    scanner.rootPath = AppDelegate.sharedInstance.romPath;
    scanner.delegate = self;

    [scanner start];
}

@end
