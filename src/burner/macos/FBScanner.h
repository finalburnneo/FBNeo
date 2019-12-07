//
//  FBScanner.h
//  Emulator
//
//  Created by Akop Karapetyan on 11/30/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Foundation/Foundation.h>

#define FBROMSET_STATUS_OK         0
#define FBROMSET_STATUS_UNPLAYABLE 2

@interface FBROMSet: NSObject<NSCoding>

@property NSString *parent;
@property NSString *name;
@property NSString *title;
@property unsigned char status;

@end

@protocol FBScannerDelegate<NSObject>

@optional
- (void) scanDidStart;
- (void) scanDidProgress:(float) progress;
- (void) scanDidEnd:(NSArray<FBROMSet *> *) romSets;

@end

@interface FBScanner : NSThread

@property NSString *rootPath;

@property (nonatomic, weak) id<FBScannerDelegate> delegate;

@end
