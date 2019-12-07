//
//  FBDropFileScrollView.m
//  FinalBurnNeo
//
//  Created by Akop Karapetyan on 12/01/19.
//  Copyright © 2019 Akop Karapetyan. All rights reserved.
//

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
