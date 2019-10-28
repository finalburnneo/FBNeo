//
//  FBInput.mm
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/20/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBAudio.h"

#import "AppDelegate.h"

extern int nSDLVolume;

@implementation FBAudio

#pragma mark - Init and dealloc

- (instancetype) init
{
    if (self = [super init]) {
    }

    return self;
}

#pragma mark - Interface

- (float) volume
{
    return MAX(0.0f, MIN((float) nSDLVolume, 128.0f)) / 128.0f;
}

- (void) setVolume:(float) volume
{
    nSDLVolume = 128 * MAX(0, MIN(1.0, volume));
}

@end
