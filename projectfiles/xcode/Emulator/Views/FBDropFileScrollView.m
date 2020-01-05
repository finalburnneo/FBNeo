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

#import "FBDropFileScrollView.h"

@implementation FBDropFileScrollView

- (id) initWithCoder:(NSCoder *) coder
{
    if ((self = [super initWithCoder:coder]) != nil) {
        [self registerForDraggedTypes:@[NSFilenamesPboardType]];
    }
    
    return self;
}

#pragma mark - Drag & Drop

- (BOOL) prepareForDragOperation:(id<NSDraggingInfo>) sender
{
    return [sender.draggingPasteboard.types containsObject:NSFilenamesPboardType];
}

- (BOOL) performDragOperation:(id<NSDraggingInfo>) sender
{
    if ([_delegate respondsToSelector:@selector(dropDidComplete:)])
        [_delegate dropDidComplete:[sender.draggingPasteboard propertyListForType:NSFilenamesPboardType]];
    return YES;
}

- (NSDragOperation) draggingEntered:(id <NSDraggingInfo>) sender
{
    if (sender.draggingSourceOperationMask & NSDragOperationCopy) {
        NSPasteboard *pb = sender.draggingPasteboard;
        if ([pb.types containsObject:NSFilenamesPboardType]) {
            if (![_delegate respondsToSelector:@selector(isDropAcceptable:)]
                || [_delegate isDropAcceptable:[pb propertyListForType:NSFilenamesPboardType]])
                return NSDragOperationCopy;
        }
    }
    return NSDragOperationNone;
}

- (void) draggingExited:(id<NSDraggingInfo>) sender
{
}

@end
