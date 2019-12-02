//
//  FBDropFileScrollView.h
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 12/01/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol FBDropFileScrollViewDelegate<NSObject>

@optional
- (BOOL) isDropAcceptable:(NSArray<NSString *> *) paths;
- (void) dropDidComplete:(NSArray<NSString *> *) paths;

@end

@interface FBDropFileScrollView : NSScrollView<NSDraggingDestination>

@property (nonatomic, weak) IBOutlet id<FBDropFileScrollViewDelegate> delegate;

@end
