//
//  FBMainThread.h
//  Emulator
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

#import <Foundation/Foundation.h>

@interface FBDipOption: NSObject

@property NSString *name;
@property uint32 start;
@property uint8 flags;
@property uint8 mask;
@property uint8 setting;

@end

@interface FBDipSetting: NSObject

@property NSString *name;
@property int selectedIndex;
@property int defaultIndex;
@property NSArray<FBDipOption *> *switches;

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

@property (readonly) BOOL isRunning;
@property BOOL dipSwitchesDirty;

- (NSString *) log;
- (void) load:(NSString *) path;

- (void) addObserver:(id<FBMainThreadDelegate>) observer;
- (void) removeObserver:(id<FBMainThreadDelegate>) observer;

@end

@interface FBMainThread (Etc)

- (BOOL) isPaused;
- (void) setPaused:(BOOL) isPaused;

- (NSString *) title;
- (NSString *) setName;

- (NSArray<FBDipSetting *> *) dipSwitches;
- (void) applyDip:(FBDipOption *) option;
- (BOOL) restoreDipState:(NSString *) path;
- (BOOL) saveDipState:(NSString *) path;

@end
