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

#import <Cocoa/Cocoa.h>

#import "AKGamepadManager.h"
#import "FBInputMap.h"

#define FBN_LMB 0x1
#define FBN_RMB 0x2
#define FBN_MMB 0x4

@interface FBInputInfo : NSObject

@property NSString *code;
@property NSString *title;

- (instancetype) initWithCode:(NSString *) code
                        title:(NSString *) title;
- (int) playerIndex;
- (NSString *) neutralTitle;

@end

@interface FBInput : NSObject<AKGamepadEventDelegate>

@property NSPoint mouseCoords;
@property uint8 mouseButtonStates;

- (void) simReset;
- (BOOL) usesMouse;
- (void) keyDown:(NSEvent *) theEvent;
- (void) keyUp:(NSEvent *) theEvent;
- (void) flagsChanged:(NSEvent *) theEvent;
- (NSArray<FBInputInfo *> *) allInputs;
- (void) setFocus:(BOOL) focus;
- (FBInputMap *) loadMapForDeviceId:(NSString *) deviceId
                            setName:(NSString *) setName;
- (BOOL) saveMap:(FBInputMap *) map
     forDeviceId:(NSString *) deviceId
         setName:(NSString *) setName;

@end
