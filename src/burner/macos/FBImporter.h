//
//  FBImporter.h
//  Emulator
//
//  Created by Akop Karapetyan on 12/02/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol FBImporterDelegate<NSObject>

@optional
- (void) importDidStart;
- (void) importDidProgress:(float) progress;
- (void) importDidEnd:(BOOL) isCancelled
          filesCopied:(int) count;

@end

@interface FBImporter : NSThread

@property NSArray<NSString *> *sourcePaths;
@property NSString *destinationPath;

@property (nonatomic, weak) id<FBImporterDelegate> delegate;

@end
