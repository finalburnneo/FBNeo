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

#import "FBPreferencesController.h"

#import "NSWindowController+Core.h"

@interface FBPreferencesController()

- (void) resetDipSwitches:(NSArray *) switches;
- (void) resetButtonList;
- (void) resetInputDevices;

@end

@implementation FBPreferencesController
{
    NSArray<FBDipSetting *> *dipSwitches;
    NSMutableArray<NSDictionary *> *_inputDeviceList;
    NSMutableDictionary<NSString *, NSDictionary *> *_inputDeviceMap;
    NSString *_selectedInputDeviceId;
}

- (id) init
{
    if (self = [super initWithWindowNibName:@"Preferences"]) {
        _inputDeviceList = [NSMutableArray new];
        _inputDeviceMap = [NSMutableDictionary new];
    }

    return self;
}

- (void) awakeFromNib
{
    [self.runloop addObserver:self];
    _selectedInputDeviceId = @"keyboard";
    NSDictionary *kybd = @{ @"id": @"keyboard",
                            @"title": NSLocalizedString(@"Keyboard", @"Device") };
    [_inputDeviceList addObject:kybd];
    [_inputDeviceMap setObject:kybd
                        forKey:@"keyboard"];

    AKGamepadManager *gm = AKGamepadManager.sharedInstance;
    for (int i = 0, n = (int) gm.gamepadCount; i < n; i++) {
        AKGamepad *gamepad = [gm gamepadAtIndex:i];
        NSString *key = gamepad.vendorProductString;
        NSDictionary *gp = @{ @"id": key,
                              @"title": gamepad.name };

        [_inputDeviceList addObject:gp];
        [_inputDeviceMap setObject:gp
                            forKey:key];
    }

    [self resetInputDevices];
    [gm addObserver:self];
}

- (void) dealloc
{
    [AKGamepadManager.sharedInstance removeObserver:self];
    [self.runloop removeObserver:self];
}

#pragma mark - NSWindowController

- (void) windowDidLoad
{
    toolbar.selectedItemIdentifier = [NSUserDefaults.standardUserDefaults objectForKey:@"selectedPreferencesTab"];
    [self resetDipSwitches:[self.runloop dipSwitches]];
}

#pragma mark - AKGamepadDelegate

- (void) gamepadDidConnect:(AKGamepad *) gamepad
{
    NSString *key = gamepad.vendorProductString;
    @synchronized (_inputDeviceList) {
        if (![_inputDeviceMap objectForKey:key]) {
            NSDictionary *gp = @{ @"id": key,
                                  @"title": gamepad.name };
            [_inputDeviceMap setObject:gp
                                forKey:key];
            [_inputDeviceList addObject:gp];
        }
    }
    [self resetInputDevices];
}

- (void) gamepadDidDisconnect:(AKGamepad *) gamepad
{
    NSString *key = gamepad.vendorProductString;
    @synchronized (_inputDeviceList) {
        NSDictionary *gp = [_inputDeviceMap objectForKey:key];
        [_inputDeviceMap removeObjectForKey:key];
        [_inputDeviceList removeObject:gp];
    }
    [self resetInputDevices];
}

- (void) gamepad:(AKGamepad *) gamepad
        xChanged:(NSInteger) newValue
          center:(NSInteger) center
       eventData:(AKGamepadEventData *) eventData
{
    // FIXME!!
//    if ([[gamepad vendorProductString] isEqualToString:_selectedInputDeviceId]) {
//        if ([[self window] firstResponder] == _joyCaptureView) {
//            if (center - newValue > FXDeadzoneSize) {
//                [_joyCaptureView captureCode:FXGamepadLeft];
//            } else if (newValue - center > FXDeadzoneSize) {
//                [_joyCaptureView captureCode:FXGamepadRight];
//            }
//        }
//    }
}

- (void) gamepad:(AKGamepad *) gamepad
        yChanged:(NSInteger) newValue
          center:(NSInteger) center
       eventData:(AKGamepadEventData *) eventData
{
    // FIXME!!
//    if ([[gamepad vendorProductString] isEqualToString:_selectedInputDeviceId]) {
//        if ([[self window] firstResponder] == _joyCaptureView) {
//            if (center - newValue > FXDeadzoneSize) {
//                [_joyCaptureView captureCode:FXGamepadUp];
//            } else if (newValue - center > FXDeadzoneSize) {
//                [_joyCaptureView captureCode:FXGamepadDown];
//            }
//        }
//    }
}

- (void) gamepad:(AKGamepad *) gamepad
          button:(NSUInteger) index
          isDown:(BOOL) isDown
       eventData:(AKGamepadEventData *) eventData
{
    // FIXME!!
//    if ([[gamepad vendorProductString] isEqualToString:_selectedInputDeviceId]) {
//        if ([[self window] firstResponder] == _joyCaptureView) {
//            [_joyCaptureView captureCode:FXMakeButton(index)];
//        }
//    }
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

- (void) restoreDipToDefault:(id) sender
{
    for (FBDipSetting *sw in dipSwitches) {
        if (sw.selectedIndex != sw.defaultIndex) {
            sw.selectedIndex = sw.defaultIndex;
            [self.runloop applyDip:sw.switches[sw.defaultIndex]];
        }
    }
    [dipswitchTableView reloadData];
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
        FBDipSetting *sw = [dipSwitches objectAtIndex:row];
        if ([tableColumn.identifier isEqualToString:@"name"]) {
            return sw.name;
        } else if ([tableColumn.identifier isEqualToString:@"value"]) {
            [tableColumn.dataCell removeAllItems];
            NSMutableSet *used = [NSMutableSet new];
            for (FBDipOption *opt in sw.switches) {
                // NSPopupButtonCell discards duplicated titles,
                // so add a count suffix when a dip name is duplicated (e.g.
                // SFIII, 'Region'/'Asia')
                NSString *name = opt.name;
                for (int i = 2;[used containsObject:name]; i++)
                    name = [NSString stringWithFormat:@"%@ (%d)", opt.name, i];

                [used addObject:name];
                [tableColumn.dataCell addItemWithTitle:name];
            }
            return @(sw.selectedIndex);
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
        if ([tableColumn.identifier isEqualToString:@"value"]) {
            dipSwitches[row].selectedIndex = [object intValue];
            [self.runloop applyDip:dipSwitches[row].switches[[object intValue]]];
        }
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
    restoreDipButton.enabled = dipswitchTableView.enabled = dipSwitches.count > 0;
    [dipswitchTableView reloadData];
}

- (void) resetButtonList
{
    [inputTableView abortEditing];
/* FIXME!!
    NSInteger index = inputDevicesPopUp.indexOfSelectedItem;
    if (index < _inputDeviceList.count) {
        _selectedInputDeviceId = [[_inputDeviceList objectAtIndex:index] objectForKey:@"id"];
    }

    [_inputList removeAllObjects];

    BOOL isKeyboard = [@"keyboard" isEqualToString:_selectedInputDeviceId];
    FXEmulatorController *emulator = [[FXAppDelegate sharedInstance] emulator];

    [[[emulator driver] buttons] enumerateObjectsUsingBlock:^(FXButton *b, NSUInteger idx, BOOL *stop) {
        if (isKeyboard) {
            FXButtonConfig *bc = [FXButtonConfig new];
            [bc setName:[b name]];
            [bc setTitle:[b title]];
            [bc setVirtualCode:[b code]];
            [_inputList addObject:bc];
        } else if ([b playerIndex] == 1) {
            FXButtonConfig *bc = [FXButtonConfig new];
            [bc setName:[b name]];
            [bc setTitle:[b neutralTitle]];
            [bc setVirtualCode:[b code]];
            [_inputList addObject:bc];
        }
    }];
    [inputTableView setEnabled:[_inputList count] > 0];
    [inputTableView reloadData];
 */
}

- (void) resetInputDevices
{
    [inputDevicesPopUp removeAllItems];
    [_inputDeviceList enumerateObjectsUsingBlock:^(NSDictionary *gp, NSUInteger idx, BOOL *stop) {
        [inputDevicesPopUp addItemWithTitle:[gp objectForKey:@"title"]];
    }];

    [inputDevicesPopUp selectItemAtIndex:0]; // select the keyboard
    [self resetButtonList];
}

@end
