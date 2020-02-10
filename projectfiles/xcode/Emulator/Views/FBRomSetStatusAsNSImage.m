// Copyright (c) Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
        case 2:
            return [NSImage imageNamed:@"NSStatusAvailable"];
        default:
            return [NSImage imageNamed:@"NSStatusNone"];
    }
}

@end
