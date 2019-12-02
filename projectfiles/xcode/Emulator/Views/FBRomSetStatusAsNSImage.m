//
//  FBRomSetStatusAsNSImage.m
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 12/01/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "FBRomSetStatusAsNSImage.h"

#import "FBScanner.h"

@implementation FBRomSetStatusAsNSImage

+ (Class) transformedValueClass
{
    return [NSImage class];
}

+ (BOOL) allowsReverseTransformation
{
    return NO;
}

- (id) transformedValue:(id) value
{
    switch ([value integerValue]) {
        case FBROMSET_STATUS_OK:
        case 1:
            return [NSImage imageNamed:@"NSStatusAvailable"];
        default:
            return [NSImage imageNamed:@"NSStatusNone"];
    }
}

@end
