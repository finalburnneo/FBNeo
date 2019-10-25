//
//  FBMainThread.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface FBMainThread (Etc)

- (BOOL) isPaused;
- (void) setPaused:(BOOL) isPaused;

@end
