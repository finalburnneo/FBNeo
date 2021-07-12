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

#import "FBInputMap.h"

@implementation FBInputMap
{
	NSMutableDictionary<NSString*, NSNumber*> *_map;
}

#pragma mark - init, dealloc

- (instancetype) init
{
	if (self = [super init]) {
		_map = [NSMutableDictionary new];
	}

	return self;
}

#pragma mark - NSCoding

- (instancetype) initWithCoder:(NSCoder *) coder
{
	if (self = [super init]) {
        _map = [coder decodeObjectForKey:@"map"];
	}
	
	return self;
}

- (void) encodeWithCoder:(NSCoder *) coder
{
	[coder encodeObject:_map forKey:@"map"];
}

#pragma mark - Public

- (void) mapVirtualCode:(NSString *) virt
             toPhysical:(int) phys
{
    if ([self physicalCodeForVirtual:virt] == phys) {
        return;
    }
    if (phys == FBInputMapNone) {
        [_map removeObjectForKey:virt];
    } else {
        [_map setObject:@(phys)
                 forKey:virt];
    }
    self.isDirty = YES;
}

- (int) physicalCodeForVirtual:(NSString *) virt
{
    NSNumber *num = [_map objectForKey:virt];
    return num ? [num intValue] : FBInputMapNone;
}

@end
