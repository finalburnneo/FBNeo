// Portions from FinalBurn X: Port of FinalBurn to OS X
//   https://github.com/0xe1f/FinalBurn-X
//
// Copyright (C) Akop Karapetyan
//   Licensed under the Apache License, Version 2.0 (the "License");
//   http://www.apache.org/licenses/LICENSE-2.0

#include <OpenGL/gl.h>
#include <time.h>

#import "FBScreenView.h"
#import "AppDelegate.h"

#define HIDE_CURSOR_TIMEOUT_SECONDS 1.0f

@interface FBScreenView ()

+ (int) powerOfTwoClosestTo:(int) number;
- (void) resetProjection;
- (void) handleMouseChange:(NSPoint) position;
- (void) resetTrackingArea;

@end

@implementation FBScreenView
{
    GLuint screenTextureId;
    unsigned char *texture;
    int imageWidth;
    int imageHeight;
    BOOL isRotated;
    BOOL isFlipped;
    int textureWidth;
    int textureHeight;
    int textureBytesPerPixel;
    NSLock *renderLock;
    NSSize screenSize;
    CFAbsoluteTime _lastMouseAction;
    NSPoint _lastCursorPosition;
    NSTrackingArea *_trackingArea;
    NSRect viewBounds; // Access from non-UI thread
    GLint textureFormat;
}

#pragma mark - Initialize, Dealloc

- (void) dealloc
{
    glDeleteTextures(1, &screenTextureId);
    free(texture);
}

- (void) awakeFromNib
{
    renderLock = [[NSLock alloc] init];
    _lastMouseAction = CFAbsoluteTimeGetCurrent();
    _lastCursorPosition = NSMakePoint(-1, -1);

    [self resetTrackingArea];
}

#pragma mark - Cocoa Callbacks

- (void) prepareOpenGL
{
    [super prepareOpenGL];

    NSLog(@"FBScreenView/prepareOpenGL");

    [renderLock lock];

    // Synchronize buffer swaps with vertical refresh rate
    GLint swapInt = 1;
    [self.openGLContext setValues:&swapInt
                     forParameter:NSOpenGLCPSwapInterval];

    glClearColor(0, 0, 0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenTextures(1, &screenTextureId);

    [renderLock unlock];
}

- (void) drawRect:(NSRect)dirtyRect
{
    [renderLock lock];

    [self.openGLContext makeCurrentContext];

    glClear(GL_COLOR_BUFFER_BIT);

    GLfloat coordX = (GLfloat)imageWidth / textureWidth;
    GLfloat coordY = (GLfloat)imageHeight / textureHeight;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, screenTextureId);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    NSSize size = [self bounds].size;
    CGFloat offset = 0;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-offset, 0.0, 0.0);
    glTexCoord2f(coordX, 0.0);
    glVertex3f(size.width + offset, 0.0, 0.0);
    glTexCoord2f(coordX, coordY);
    glVertex3f(size.width + offset, size.height, 0.0);
    glTexCoord2f(0.0, coordY);
    glVertex3f(-offset, size.height, 0.0);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    [self.openGLContext flushBuffer];

    [renderLock unlock];
}

- (void) reshape
{
    viewBounds = [self bounds];
    [renderLock lock];

    [self.openGLContext makeCurrentContext];
    [self.openGLContext update];

    [self resetProjection];

    glClear(GL_COLOR_BUFFER_BIT);

    [renderLock unlock];
}

#pragma mark - FBVideoRenderDelegate

- (void)initTextureOfWidth:(int) width
                    height:(int) height
                 isRotated:(BOOL) rotated
                 isFlipped:(BOOL) flipped
             bytesPerPixel:(int) bytesPerPixel
{
    NSLog(@"FBScreenView/initTexture");

    [renderLock lock];

    [self.openGLContext makeCurrentContext];

    free(texture);

    imageWidth = width;
    imageHeight = height;
    isRotated = rotated;
    isFlipped = flipped;
    textureWidth = [FBScreenView powerOfTwoClosestTo:imageWidth];
    textureHeight = [FBScreenView powerOfTwoClosestTo:imageHeight];
    textureBytesPerPixel = bytesPerPixel;
    screenSize = NSMakeSize((CGFloat)width, (CGFloat)height);
    textureFormat = GL_UNSIGNED_SHORT_5_6_5;

    int texSize = textureWidth * textureHeight * bytesPerPixel;
    texture = (unsigned char *) malloc(texSize);

    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, screenTextureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 textureWidth, textureHeight,
                 0, GL_RGB, textureFormat, texture);

    glDisable(GL_TEXTURE_2D);

    [self resetProjection];

    [renderLock unlock];
}

- (void) renderFrame:(unsigned char *) bitmap
{
    if (NSPointInRect(_lastCursorPosition, viewBounds)) {
        CFAbsoluteTime interval = CFAbsoluteTimeGetCurrent() - _lastMouseAction;
        if (interval > HIDE_CURSOR_TIMEOUT_SECONDS) {
            if ([_delegate respondsToSelector:@selector(mouseDidIdle)]) {
                [_delegate mouseDidIdle];
            }
            _lastCursorPosition.x = -1;
        }
    }

    [renderLock lock];

    [self.openGLContext makeCurrentContext];

    glClear(GL_COLOR_BUFFER_BIT);

    GLfloat coordX = (GLfloat)imageWidth / textureWidth;
    GLfloat coordY = (GLfloat)imageHeight / textureHeight;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, screenTextureId);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    unsigned char *ps = (unsigned char *) bitmap;
    unsigned char *pd = (unsigned char *) texture;

    int bitmapPitch = imageWidth * textureBytesPerPixel;
    int texturePitch = textureWidth * textureBytesPerPixel;
    for (int y = imageHeight; y--; ) {
        memcpy(pd, ps, bitmapPitch);
        pd += texturePitch;
        ps += bitmapPitch;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureWidth, textureHeight, 0,
             GL_RGB, textureFormat, texture);

    NSSize size = viewBounds.size;
    CGFloat offset = 0;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-offset, 0.0, 0.0);
    glTexCoord2f(coordX, 0.0);
    glVertex3f(size.width + offset, 0.0, 0.0);
    glTexCoord2f(coordX, coordY);
    glVertex3f(size.width + offset, size.height, 0.0);
    glTexCoord2f(0.0, coordY);
    glVertex3f(-offset, size.height, 0.0);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    [self.openGLContext flushBuffer];

    [renderLock unlock];
}

#pragma mark - Keyboard

- (BOOL) acceptsFirstResponder
{
    return YES;
}

- (void) keyDown:(NSEvent *) theEvent
{
//    NSLog(@"FBScreenView/keyDown: %@", theEvent);
    [AppDelegate.sharedInstance.input keyDown:theEvent];
}

- (void) keyUp:(NSEvent *) theEvent
{
//    NSLog(@"FBScreenView/keyUp: %@", theEvent);
    [AppDelegate.sharedInstance.input keyUp:theEvent];
}

- (void) flagsChanged:(NSEvent *) theEvent
{
//    NSLog(@"FBScreenView/flagsChanged: %@", theEvent);
    [AppDelegate.sharedInstance.input flagsChanged:theEvent];
}

#pragma mark - Mouse tracking

- (void) mouseEntered:(NSEvent *) event
{
    [self handleMouseChange:event.locationInWindow];
}

- (void) mouseMoved:(NSEvent *) event
{
    if ([_delegate respondsToSelector:@selector(mouseDidMove:)])
        [_delegate mouseDidMove:NSMakePoint(event.deltaX, event.deltaY)];
    [self handleMouseChange:event.locationInWindow];
}

- (void) mouseExited:(NSEvent *) event
{
    [self handleMouseChange:NSMakePoint(-1, -1)];
}

- (void) mouseDown:(NSEvent *) event
{
    if ([_delegate respondsToSelector:@selector(mouseButtonStateChange:)])
        [_delegate mouseButtonStateChange:event];
    [self handleMouseChange:event.locationInWindow];
}

- (void) mouseUp:(NSEvent *) event
{
    if ([_delegate respondsToSelector:@selector(mouseButtonStateChange:)])
        [_delegate mouseButtonStateChange:event];
}

- (void) mouseDragged:(NSEvent *) event
{
    if ([_delegate respondsToSelector:@selector(mouseDidMove:)])
        [_delegate mouseDidMove:NSMakePoint(event.deltaX, event.deltaY)];
}

- (void) rightMouseDown:(NSEvent *) event
{
    if ([_delegate respondsToSelector:@selector(mouseButtonStateChange:)])
        [_delegate mouseButtonStateChange:event];
    [self handleMouseChange:event.locationInWindow];
}

- (void) rightMouseUp:(NSEvent *) event
{
    if ([_delegate respondsToSelector:@selector(mouseButtonStateChange:)])
        [_delegate mouseButtonStateChange:event];
}

- (void) rightMouseDragged:(NSEvent *) event
{
    if ([_delegate respondsToSelector:@selector(mouseDidMove:)])
        [_delegate mouseDidMove:NSMakePoint(event.deltaX, event.deltaY)];
}

#pragma mark - Public methods

- (NSSize) screenSize
{
    return screenSize;
}

#pragma mark - Private methods

- (void) resetTrackingArea
{
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }

    NSTrackingAreaOptions opts = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect;
    _trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                                 options:opts
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void) handleMouseChange:(NSPoint) position
{
    _lastMouseAction = CFAbsoluteTimeGetCurrent();
    _lastCursorPosition = [self convertPoint:position
                                    fromView:nil];

    if ([_delegate respondsToSelector:@selector(mouseStateDidChange)])
        [_delegate mouseStateDidChange];
}

- (void)resetProjection
{
    NSSize size = [self bounds].size;

    glViewport(0, 0, size.width, size.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (!isRotated) {
         glOrtho(0, size.width, size.height, 0, -1, 1);
    } else {
        glRotatef(isFlipped ? 270 : 90.0, 0.0, 0.0, 1.0);
         glOrtho(0, size.width, size.height, 0, -1, 5);
    }

    glMatrixMode(GL_MODELVIEW);
}

+ (int)powerOfTwoClosestTo:(int)number
{
    int rv = 1;
    while (rv < number) rv *= 2;
    return rv;
}

@end
