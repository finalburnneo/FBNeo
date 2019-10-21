//
//  FBMainThread.h
//  Emulator
//
//  Created by Akop Karapetyan on 10/16/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol FBMainThreadDelegate<NSObject>

- (void) driverInitDidStart;
- (void) driverInitDidEnd:(NSString *) name
                  success:(BOOL) success;
- (void) gameSessionDidStart:(NSString *) name;
- (void) gameSessionDidEnd;

@end

@interface FBMainThread : NSThread

@property (readonly) NSString *running;
@property (nonatomic, weak) id<FBMainThreadDelegate> delegate;

- (void) load:(NSString *) path;

@end
