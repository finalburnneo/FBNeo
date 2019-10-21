// Portions from FinalBurn X: Port of FinalBurn to OS X
//   https://github.com/0xe1f/FinalBurn-X
//
// Copyright (C) Akop Karapetyan
//   Licensed under the Apache License, Version 2.0 (the "License");
//   http://www.apache.org/licenses/LICENSE-2.0

#import "FBVideo.h"

#import "AppDelegate.h"

#include "burner.h"

@implementation FBVideo

#pragma mark - Init and dealloc

- (instancetype) init
{
    if (self = [super init]) {
    }

    return self;
}

- (void) dealloc
{
	_delegate = nil;
}

#pragma mark - Intermediaries

- (BOOL) renderToSurface:(unsigned char *) buffer
{
    id<FBVideoDelegate> vd = [self delegate];
    if ([vd respondsToSelector:@selector(renderFrame:)])
        [vd renderFrame:buffer];

    return YES;
}

- (void) notifyTextureReadyOfWidth:(int) texWidth
                            height:(int) texHeight
                         isRotated:(BOOL) isRotated
                     bytesPerPixel:(int) bpp
                        screenSize:(NSSize) size
{
    dispatch_async(dispatch_get_main_queue(), ^{
        id<FBVideoDelegate> vd = [self delegate];
        if ([vd respondsToSelector:@selector(initTextureOfWidth:height:isRotated:bytesPerPixel:)])
            [vd initTextureOfWidth:texWidth
                    height:texHeight
                 isRotated:isRotated
             bytesPerPixel:bpp];

        if ([vd respondsToSelector:@selector(screenSizeDidChange:)])
            [vd screenSizeDidChange:size];
    });
}

- (NSSize) gameScreenSize
{
    if (nBurnDrvActive == ~0U)
        return NSZeroSize;

    int w, h;
    BurnDrvGetVisibleSize(&w, &h);
    return NSMakeSize(w, h);
}

@end

#pragma mark - FinalBurn callbacks

static unsigned char *screenBuffer = NULL;
static int bufferWidth = 0;
static int bufferHeight = 0;
static int bufferBytesPerPixel = 0;

static int MacOSVideoInit()
{
    int gameWidth;
    int gameHeight;
    int rotationMode = 0;

    BurnDrvGetVisibleSize(&gameWidth, &gameHeight);

    if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
        rotationMode |= 1;
    }

    if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
        rotationMode ^= 2;
    }

    nVidImageWidth = gameWidth;
    nVidImageHeight = gameHeight;
    nVidImageDepth = 24;
    nVidImageBPP = 3;
    if (!rotationMode) {
        nVidImagePitch = nVidImageWidth * nVidImageBPP;
    } else {
        nVidImagePitch = nVidImageHeight * nVidImageBPP;
    }

    SetBurnHighCol(nVidImageDepth);

    bufferBytesPerPixel = nVidImageBPP;
    bufferWidth = gameWidth;
    bufferHeight = gameHeight;

    int bufSize = bufferWidth * bufferHeight * nVidImageBPP;
    free(screenBuffer);
    screenBuffer = (unsigned char *) malloc(bufSize);

    if (screenBuffer == NULL)
        return 1;

    nBurnBpp = nVidImageBPP;
    nBurnPitch = nVidImagePitch;
    pVidImage = screenBuffer;

    memset(screenBuffer, 0, bufSize);

    int textureWidth;
    int textureHeight;
    BOOL isRotated = rotationMode & 1;

    if (!isRotated) {
        textureWidth = bufferWidth;
        textureHeight = bufferHeight;
    } else {
        textureWidth = bufferHeight;
        textureHeight = bufferWidth;
    }

    NSSize screenSize = NSMakeSize((CGFloat)bufferWidth,
                                   (CGFloat)bufferHeight);

    [AppDelegate.sharedInstance.video notifyTextureReadyOfWidth:textureWidth
                                                         height:textureHeight
                                                      isRotated:isRotated
                                                  bytesPerPixel:bufferBytesPerPixel
                                                     screenSize:screenSize];

    return 0;
}

static int MacOSVideoExit()
{
    free(screenBuffer);
    screenBuffer = NULL;

    return 0;
}

static int MacOSVideoFrame(bool redraw)
{
    if (pVidImage == NULL)
        return NO;

    if (redraw) {
        if (BurnDrvRedraw()) {
            BurnDrvFrame();
        }
    } else {
        BurnDrvFrame();
    }

    return 0;
}

static int MacOSVideoPaint(int validate)
{
    return [AppDelegate.sharedInstance.video renderToSurface:screenBuffer] ? 0 : 1;
}

static int MacOSVideoScale(RECT*, int, int)
{
	return 0;
}

static int MacOSVideoGetSettings(InterfaceInfo *info)
{
	return 0;
}

struct VidOut VidOutMacOS = {
    MacOSVideoInit,
    MacOSVideoExit,
    MacOSVideoFrame,
    MacOSVideoPaint,
    MacOSVideoScale,
    MacOSVideoGetSettings,
    _T("MacOS Video"),
};
