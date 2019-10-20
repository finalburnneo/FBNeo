// Portions from FinalBurn X: Port of FinalBurn to OS X
//   https://github.com/0xe1f/FinalBurn-X
//
// Copyright (C) Akop Karapetyan
//   Licensed under the Apache License, Version 2.0 (the "License");
//   http://www.apache.org/licenses/LICENSE-2.0

#include <OpenGL/gl.h>
#include <time.h>

#import "FBScreenView.h"

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
	int textureWidth;
	int textureHeight;
	int textureBytesPerPixel;
	NSLock *renderLock;
	NSSize screenSize;
	CFAbsoluteTime _lastMouseAction;
	NSPoint _lastCursorPosition;
	NSTrackingArea *_trackingArea;
    NSRect viewBounds; // Access from non-UI thread
}

#pragma mark - Initialize, Dealloc

- (void) dealloc
{
    glDeleteTextures(1, &screenTextureId);
    free(self->texture);
}

- (void) awakeFromNib
{
    self->renderLock = [[NSLock alloc] init];
	_lastMouseAction = CFAbsoluteTimeGetCurrent();
	_lastCursorPosition = NSMakePoint(-1, -1);

	[self resetTrackingArea];
}

#pragma mark - Cocoa Callbacks

- (void) prepareOpenGL
{
    [super prepareOpenGL];
    
    NSLog(@"FBScreenView/prepareOpenGL");
    
    [self->renderLock lock];
	
    // Synchronize buffer swaps with vertical refresh rate
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt
					   forParameter:NSOpenGLCPSwapInterval];
    
    glClearColor(0, 0, 0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glGenTextures(1, &screenTextureId);
    
    [self->renderLock unlock];
}

- (BOOL) acceptsFirstResponder
{
	return YES;
}

- (void) drawRect:(NSRect)dirtyRect
{
    [self->renderLock lock];
	
    NSOpenGLContext *nsContext = [self openGLContext];
    [nsContext makeCurrentContext];
    
    glClear(GL_COLOR_BUFFER_BIT);
	
    GLfloat coordX = (GLfloat)self->imageWidth / self->textureWidth;
    GLfloat coordY = (GLfloat)self->imageHeight / self->textureHeight;
    
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
    
    [nsContext flushBuffer];
    
    [self->renderLock unlock];
}

- (void)reshape
{
    viewBounds = [self bounds];
    [self->renderLock lock];
	
    [[self openGLContext] makeCurrentContext];
    [[self openGLContext] update];
    
    [self resetProjection];
    
    glClear(GL_COLOR_BUFFER_BIT);
    
    [self->renderLock unlock];
}

#pragma mark - FBVideoRenderDelegate

- (void)initTextureOfWidth:(int)width
                    height:(int)height
                 isRotated:(BOOL)rotated
             bytesPerPixel:(int)bytesPerPixel
{
    NSLog(@"FBScreenView/initTexture");
    
    [self->renderLock lock];
	
    NSOpenGLContext *nsContext = [self openGLContext];
    [nsContext makeCurrentContext];
    
    free(self->texture);
    
    self->imageWidth = width;
    self->imageHeight = height;
    self->isRotated = rotated;
    self->textureWidth = [FBScreenView powerOfTwoClosestTo:self->imageWidth];
    self->textureHeight = [FBScreenView powerOfTwoClosestTo:self->imageHeight];
    self->textureBytesPerPixel = bytesPerPixel;
    self->screenSize = NSMakeSize((CGFloat)width, (CGFloat)height);
    
    int texSize = self->textureWidth * self->textureHeight * bytesPerPixel;
    self->texture = (unsigned char *)malloc(texSize);
    
    glEnable(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, screenTextureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glTexImage2D(GL_TEXTURE_2D, 0, bytesPerPixel,
                 self->textureWidth, self->textureHeight,
                 0, GL_BGR, GL_UNSIGNED_BYTE, self->texture);
    
    glDisable(GL_TEXTURE_2D);
    
    [self resetProjection];
    
    [self->renderLock unlock];
}

- (void)renderFrame:(unsigned char *)bitmap
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
	
    [self->renderLock lock];
	
    NSOpenGLContext *nsContext = [self openGLContext];
    [nsContext makeCurrentContext];
    
    glClear(GL_COLOR_BUFFER_BIT);
    
    GLfloat coordX = (GLfloat)self->imageWidth / self->textureWidth;
    GLfloat coordY = (GLfloat)self->imageHeight / self->textureHeight;
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, screenTextureId);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    
    for (int y = 0; y < self->imageHeight; y += 1) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, self->imageWidth, 1,
                        GL_BGR, GL_UNSIGNED_BYTE,
                        bitmap + y * self->imageWidth * self->textureBytesPerPixel);
    }
    
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
    
    [nsContext flushBuffer];

    [self->renderLock unlock];
}

#pragma mark - Mouse tracking

- (void) mouseEntered:(NSEvent *) event
{
	[self handleMouseChange:[event locationInWindow]];
}

- (void) mouseMoved:(NSEvent *) event
{
	[self handleMouseChange:[event locationInWindow]];
}

- (void) mouseExited:(NSEvent *) event
{
	[self handleMouseChange:NSMakePoint(-1, -1)];
}

- (void) mouseDown:(NSEvent *) event
{
	[self handleMouseChange:[event locationInWindow]];
}

- (void) rightMouseDown:(NSEvent *) event
{
	[self handleMouseChange:[event locationInWindow]];
}

#pragma mark - Public methods

- (NSSize) screenSize
{
    return self->screenSize;
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
	
	if ([_delegate respondsToSelector:@selector(mouseStateDidChange)]) {
		[_delegate mouseStateDidChange];
	}
}

- (void)resetProjection
{
    NSSize size = [self bounds].size;
    
    glViewport(0, 0, size.width, size.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
	if (!self->isRotated) {
	 	glOrtho(0, size.width, size.height, 0, -1, 1);
	}
	else
	{
		glRotatef(90.0, 0.0, 0.0, 1.0);
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
