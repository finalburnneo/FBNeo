//
//  FBLauncherController.h
//  Emulator
//
//  Created by Akop Karapetyan on 11/30/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "FBScanner.h"

@interface FBLauncherController : NSWindowController<FBScannerDelegate>
{
}

- (IBAction) hiya:(id) sender;

@end
