//
//  FBPreferencesController.m
//  Emulator
//
//  Created by Akop Karapetyan on 10/24/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBPreferencesController.h"

@implementation FBPreferencesController

- (id) init
{
    if (self = [super initWithWindowNibName:@"Preferences"]) {
    }

    return self;
}

#pragma mark - NSWindowController

- (void) windowDidLoad
{
    toolbar.selectedItemIdentifier = [NSUserDefaults.standardUserDefaults objectForKey:@"selectedPreferencesTab"];
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

#pragma mark - NSTabViewDelegate

- (void) tabView:(NSTabView *) tabView
didSelectTabViewItem:(NSTabViewItem *) tabViewItem
{
//    [self sizeWindowToTabContent:tabViewItem.identifier];
}

@end
