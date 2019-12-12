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

#import "FBVideo.h"

@protocol FBScreenViewDelegate<NSObject>

@optional
- (void) mouseDidIdle;
- (void) mouseStateDidChange;
- (void) mouseDidMove:(NSPoint) point;
- (void) mouseButtonStateDidChange:(NSEvent *) event;

@end

@interface FBScreenView : NSOpenGLView<FBVideoDelegate>

- (NSSize) screenSize;

@property (nonatomic, weak) id<FBScreenViewDelegate> delegate;

@end
