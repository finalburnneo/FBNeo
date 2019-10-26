//
//  FBMainThread.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface FBDipSwitch: NSObject

@property NSString *name;
@property uint32 start;
@property uint8 flags;
@property uint8 mask;
@property uint8 setting;
@property NSArray *switches;
@property int selectedIndex;

@end

@protocol FBMainThreadDelegate<NSObject>

@optional
- (void) driverInitDidStart;
- (void) driverInitDidEnd:(NSString *) name
                  success:(BOOL) success;
- (void) gameSessionDidStart:(NSString *) name;
- (void) gameSessionDidEnd;

- (void) logDidUpdate:(NSString *) message;
- (void) logDidClear;

@end

@interface FBMainThread : NSThread

@property (readonly) NSString *running;

- (NSString *) log;
- (void) load:(NSString *) path;

- (void) addObserver:(id<FBMainThreadDelegate>) observer;
- (void) removeObserver:(id<FBMainThreadDelegate>) observer;

@end

@interface FBMainThread (Etc)

- (BOOL) isPaused;
- (void) setPaused:(BOOL) isPaused;

- (NSString *) title;

- (NSArray *) dipSwitches;

@end
