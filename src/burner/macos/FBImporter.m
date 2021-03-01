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

#import "FBImporter.h"

#pragma mark - FBImporter

@implementation FBImporter
{
}

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

#pragma mark - NSThread

- (void) main
{
    NSArray<NSString *> *sourcePaths = [NSArray arrayWithArray:_sourcePaths];
    NSString *destinationPath = _destinationPath;

    if (!sourcePaths || sourcePaths.count == 0 || !destinationPath)
        return;

    id<FBImporterDelegate> del = _delegate;
    if ([del respondsToSelector:@selector(importDidStart)])
        dispatch_async(dispatch_get_main_queue(), ^{ [del importDidStart]; });

    __block int filesCopied = 0;
    [sourcePaths enumerateObjectsUsingBlock:^(NSString *source, NSUInteger idx, BOOL *stop) {
        NSString *sourceFilename = source.lastPathComponent;
        NSURL *destinationUrl = [NSURL fileURLWithPath:[destinationPath stringByAppendingPathComponent:sourceFilename]];

        // FIXME...
        NSError *error = nil;
        // Trash any existing file
        [NSFileManager.defaultManager trashItemAtURL:destinationUrl
                                    resultingItemURL:nil
                                               error:&error];
        // Copy new file to destination
        BOOL copyOK = [NSFileManager.defaultManager copyItemAtURL:[NSURL fileURLWithPath:source]
                                                            toURL:destinationUrl
                                                            error:&error];

        if (self.isCancelled)
            *stop = YES;
        if (copyOK)
            filesCopied++;

        id<FBImporterDelegate> del = _delegate;
        if ([del respondsToSelector:@selector(importDidProgress:)])
            dispatch_async(dispatch_get_main_queue(), ^{
                [del importDidProgress:(float) idx / sourcePaths.count];
            });
    }];

    del = _delegate;
    if ([del respondsToSelector:@selector(importDidEnd:filesCopied:)])
        dispatch_async(dispatch_get_main_queue(), ^{
            [del importDidEnd:self.isCancelled
                  filesCopied:filesCopied];
        });
}

@end
