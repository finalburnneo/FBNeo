//
//  FBEmulator.mm
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 10/22/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import "FBEmulator.h"

#include "burner.h"

@implementation FBEmulator

#pragma mark - Init and dealloc

- (instancetype) init
{
    if (self = [super init]) {
    }

    return self;
}

#pragma mark - Public

- (NSString *) title
{
    return [NSString stringWithCString:BurnDrvGetText(DRV_FULLNAME)
                              encoding:NSUTF8StringEncoding];
}

@end
