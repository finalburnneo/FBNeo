//
//  FBPreferencesController.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/24/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBPreferencesController.h"

#import "NSWindowController+Core.h"

@interface FBPreferencesController()

- (void) resetDipSwitches:(NSArray *) switches;

@end

@implementation FBPreferencesController
{
    NSArray *dipSwitches;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"Preferences"]) {
    }

    return self;
}

- (void) awakeFromNib
{
    [self.runloop addObserver:self];
}

- (void) dealloc
{
    [self.runloop removeObserver:self];
}

#pragma mark - NSWindowController

- (void) windowDidLoad
{
    toolbar.selectedItemIdentifier = [NSUserDefaults.standardUserDefaults objectForKey:@"selectedPreferencesTab"];
    [self resetDipSwitches:[self.runloop dipSwitches]];
}

#pragma mark - Actions

- (void) tabChanged:(id) sender
{
    NSToolbarItem *item = (NSToolbarItem *) sender;
    NSString *tabId = item.itemIdentifier;

    toolbar.selectedItemIdentifier = tabId;
    [NSUserDefaults.standardUserDefaults setObject:tabId
                                            forKey:@"selectedPreferencesTab"];
}

- (void) showPreviousTab:(id) sender
{
    __block int selected = -1;
    NSArray<NSToolbarItem *> *items = toolbar.visibleItems;
    [items enumerateObjectsUsingBlock:^(NSToolbarItem *item, NSUInteger idx, BOOL *stop) {
        if ([item.itemIdentifier isEqualToString:toolbar.selectedItemIdentifier]) {
            selected = (int) idx;
            *stop = YES;
        }
    }];

    if (selected < 0 || --selected < 0)
        selected = items.count - 1;

    NSString *nextId = items[selected].itemIdentifier;
    toolbar.selectedItemIdentifier = nextId;

    [NSUserDefaults.standardUserDefaults setObject:nextId
                                            forKey:@"selectedPreferencesTab"];
}

- (void) showNextTab:(id) sender
{
    __block int selected = -1;
    NSArray<NSToolbarItem *> *items = toolbar.visibleItems;
    [items enumerateObjectsUsingBlock:^(NSToolbarItem *item, NSUInteger idx, BOOL *stop) {
        if ([item.itemIdentifier isEqualToString:toolbar.selectedItemIdentifier]) {
            selected = (int) idx;
            *stop = YES;
        }
    }];

    if (selected < 0 || ++selected >= items.count)
        selected = 0;

    NSString *nextId = items[selected].itemIdentifier;
    toolbar.selectedItemIdentifier = nextId;

    [NSUserDefaults.standardUserDefaults setObject:nextId
                                            forKey:@"selectedPreferencesTab"];
}

#pragma mark - NSTabViewDelegate

- (void) tabView:(NSTabView *) tabView
didSelectTabViewItem:(NSTabViewItem *) tabViewItem
{
    // FIXME!!
//    [self sizeWindowToTabContent:tabViewItem.identifier];
}

#pragma mark - NSTableViewDataSource

- (NSInteger) numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == dipswitchTableView)
        return dipSwitches.count;

    return 0;
}

- (id) tableView:(NSTableView *) tableView
objectValueForTableColumn:(NSTableColumn *) tableColumn
             row:(NSInteger) row
{
    if (tableView == dipswitchTableView) {
        FBDipSwitch *sw = [dipSwitches objectAtIndex:row];
        if ([tableColumn.identifier isEqualToString:@"name"]) {
            return sw.name;
        } else if ([tableColumn.identifier isEqualToString:@"value"]) {
            NSPopUpButtonCell* cell = [tableColumn dataCell];
            [cell removeAllItems];

            __block NSUInteger enabledIndex = -1;
            [sw.switches enumerateObjectsUsingBlock:^(FBDipSwitch *sub, NSUInteger idx, BOOL *stop) {
                [cell addItemWithTitle:sub.name];
                if (idx == sw.selectedIndex)
                    enabledIndex = idx;
            }];

            return @(enabledIndex);
        }
    }

    return nil;
}

- (void) tableView:(NSTableView *) tableView
    setObjectValue:(id) object
    forTableColumn:(NSTableColumn *) tableColumn
               row:(NSInteger) row
{
    if (tableView == dipswitchTableView) {
        // FIXME!!
//        if ([[tableColumn identifier] isEqualToString:@"value"]) {
//            FXEmulatorController *emulator = [[FXAppDelegate sharedInstance] emulator];
//            [[emulator dipState] setGroup:row
//                                 toOption:[object unsignedIntegerValue]];
//            [[_dipList objectAtIndex:row] setSelection:[object intValue]];
//        }
    }
}

#pragma mark - FBMainThreadDelegate

- (void) driverInitDidStart
{
    [self resetDipSwitches:nil];
}

- (void) driverInitDidEnd:(NSString *) name
                  success:(BOOL) success
{
    [self resetDipSwitches:[self.runloop dipSwitches]];
}

#pragma mark - Private

- (void) resetDipSwitches:(NSArray *) switches
{
    dipSwitches = switches;
    // FIXME!!
//    [self->resetDipSwitchesButton setEnabled:[_dipList count] > 0];
    dipswitchTableView.enabled = dipSwitches.count > 0;
    [dipswitchTableView reloadData];
}

@end
