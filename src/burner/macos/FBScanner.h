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

#import <Foundation/Foundation.h>

#define FBROMSET_STATUS_OK         0
#define FBROMSET_STATUS_UNPLAYABLE 1

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
