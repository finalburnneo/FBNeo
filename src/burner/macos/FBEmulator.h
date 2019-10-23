//
//  FBEmulator.h
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/22/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol FBEmulatorDelegate<NSObject>

- (void) driverProgressUpdate:(NSString *) message;
- (void) driverProgressError:(NSString *) message;

@end

@interface FBEmulator : NSObject

- (NSString *) title;

@property (nonatomic, weak) id<FBEmulatorDelegate> delegate;

@end
