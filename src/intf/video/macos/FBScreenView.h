// Portions from FinalBurn X: Port of FinalBurn to OS X
//   https://github.com/0xe1f/FinalBurn-X
//
// Copyright (C) Akop Karapetyan
//   Licensed under the Apache License, Version 2.0 (the "License");
//   http://www.apache.org/licenses/LICENSE-2.0

#import <Cocoa/Cocoa.h>

#import "FBVideo.h"

@protocol FBScreenViewDelegate<NSObject>

@optional
- (void) mouseDidIdle;
- (void) mouseStateDidChange;
- (void) mouseDidMove:(NSPoint) point;
- (void) mouseButtonStateChange:(NSEvent *) event;

@end

@interface FBScreenView : NSOpenGLView<FBVideoDelegate>

- (NSSize) screenSize;

@property (nonatomic, weak) id<FBScreenViewDelegate> delegate;

@end
