// Portions from FinalBurn X: Port of FinalBurn to OS X
//   https://github.com/0xe1f/FinalBurn-X
//
// Copyright (C) Akop Karapetyan
//   Licensed under the Apache License, Version 2.0 (the "License");
//   http://www.apache.org/licenses/LICENSE-2.0

#import <Foundation/Foundation.h>

@protocol FBVideoDelegate<NSObject>

@optional
- (void) screenSizeDidChange:(NSSize) newSize;
- (void) initTextureOfWidth:(int) width
                     height:(int) height
                  isRotated:(BOOL) rotated
                  isFlipped:(BOOL) flipped
              bytesPerPixel:(int) bytesPerPixel;
- (void) renderFrame:(unsigned char *) bitmap;

@end

@interface FBVideo : NSObject

@property (nonatomic, weak) id<FBVideoDelegate> delegate;

- (NSSize) gameScreenSize;

@end
