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

#import "FBAudio.h"

#import "AppDelegate.h"
#import "SDLAudioEngine.h"

#include "burner.h"

static int macOSGetNextSoundFiller(int draw);

@interface FBAudio ()

- (void) cleanup;
- (void) togglePlayState:(BOOL) play;

@end

@implementation FBAudio
{
    SDLAudioEngine *engine;
    int (*audioCallback)(int);
    int soundLoopLength;
    short *soundBuffer;
    int playPosition;
    int fillSegment;
}

#pragma mark - Init and dealloc

- (instancetype) init
{
    if (self = [super init]) {
    }

    return self;
}

- (void) dealloc
{
    [self cleanup];
}

#pragma mark - Public

- (float) volume
{
    return engine.volume;
}

- (void) setVolume:(float) volume
{
    engine.volume = volume;
}

#pragma mark - Core callbacks

- (BOOL) initCore
{
    NSLog(@"audio/init");

    int sampleRate = 44100;
    nAudNextSound = NULL;
    audioCallback = NULL;
    int soundFps = nAppVirtualFps;
    nAudSegLen = (sampleRate * 100 + (soundFps / 2)) / soundFps;
    soundLoopLength = (nAudSegLen * nAudSegCount) * 4;

    int bufferSize = 64;
    for (; bufferSize < (nAudSegLen >> 1); bufferSize *= 2);

    soundBuffer = (short *) calloc(soundLoopLength, 1);
    if (soundBuffer == NULL) {
        [self cleanup];
        return NO;
    }

    nAudNextSound = (short *) calloc(nAudSegLen << 2, 1);
    if (nAudNextSound == NULL)
        return NO;

    playPosition = 0;
    fillSegment = nAudSegCount - 1;

    engine = [[SDLAudioEngine alloc] initWithSampleRate:sampleRate
                                               channels:2
                                                samples:bufferSize
                                         bitsPerChannel:16];

    nBurnSoundRate = sampleRate;
    nBurnSoundLen = nAudSegLen;

    engine.delegate = self;

    return YES;
}

- (void) setCallback:(int(*)(int)) callback
{
    audioCallback = (callback == NULL) ? macOSGetNextSoundFiller : callback;
}

- (BOOL) clear
{
    if (nAudNextSound)
        memset(nAudNextSound, 0, nAudSegLen << 2);

    return YES;
}

- (void) togglePlayState:(BOOL) play
{
    if (!play) {
        [engine pause];
        bAudPlaying = 0;
    } else {
        [engine resume];
        bAudPlaying = 1;
    }
}

#pragma mark - SDLAudioDelegate

- (void) mixSoundFromBuffer:(SInt16 *) stream
                      bytes:(UInt32) len
{
    if (soundBuffer == NULL)
        return;

    int end = playPosition + len;
    if (end > soundLoopLength) {
        memcpy(stream, (UInt8 *)soundBuffer + playPosition, soundLoopLength - playPosition);
        end -= soundLoopLength;
        memcpy((UInt8 *)stream + soundLoopLength - playPosition, (UInt8 *)soundBuffer, end);
        playPosition = end;
    } else {
        memcpy(stream, (UInt8 *)soundBuffer + playPosition, len);
        playPosition = end;

        if (playPosition == soundLoopLength)
            playPosition = 0;
    }
}

#define WRAP_INC(x) { x++; if (x >= nAudSegCount) x = 0; }

- (BOOL) checkAudio
{
    if (!bAudPlaying)
        return YES;

    // Since the sound buffer is smaller than a segment, only fill
    // the buffer up to the start of the currently playing segment
    int playSegment = playPosition / (nAudSegLen << 2) - 1;
    if (playSegment >= nAudSegCount)
        playSegment -= nAudSegCount;
    if (playSegment < 0)
        playSegment = nAudSegCount - 1;

    if (fillSegment == playSegment) {
        [NSThread sleepForTimeInterval:.001];
        return YES;
    }

    // work out which seg we will fill next
    int followingSegment = fillSegment;
    WRAP_INC(followingSegment);
    while (fillSegment != playSegment) {
        int draw = (followingSegment == playSegment);
        audioCallback(draw);

        memcpy((char *)soundBuffer + fillSegment * (nAudSegLen << 2),
               nAudNextSound, nAudSegLen << 2);

        fillSegment = followingSegment;
        WRAP_INC(followingSegment);
    }

    return YES;
}

#pragma mark - Etc

- (void) cleanup
{
    free(soundBuffer);
    soundBuffer = NULL;
    free(nAudNextSound);
    nAudNextSound = NULL;
    engine = nil;
}

@end

#pragma mark - FinalBurn callbacks

static int macOSGetNextSoundFiller(int draw)
{
    if (nAudNextSound == NULL)
        return 1;

    // Write silence into buffer
    memset(nAudNextSound, 0, nAudSegLen << 2);
    return 0;
}

static int macOSAudioBlankSound()
{
    return [AppDelegate.sharedInstance.audio clear] ? 0 : 1;
}

static int macOSAudioCheck()
{
    return [AppDelegate.sharedInstance.audio checkAudio] ? 0 : 1;
}

static int macOSAudioInit()
{
    return [AppDelegate.sharedInstance.audio initCore] ? 0 : 1;
}

static int macOSAudioSetCallback(int (*callback)(int))
{
    [AppDelegate.sharedInstance.audio setCallback:callback];
    return 0;
}

static int macOSAudioPlay()
{
    [AppDelegate.sharedInstance.audio togglePlayState:YES];
    return 0;
}

static int macOSAudioStop()
{
    [AppDelegate.sharedInstance.audio togglePlayState:NO];
    return 0;
}

static int macOSAudioExit()
{
    [AppDelegate.sharedInstance.audio cleanup];
    return 0;
}

static int macOSAudioSetVolume()
{
    return 1;
}

static int macOSAudioGetSettings(InterfaceInfo *info)
{
    return 0;
}

struct AudOut AudOutMacOS = {
    macOSAudioBlankSound,
    macOSAudioCheck,
    macOSAudioInit,
    macOSAudioSetCallback,
    macOSAudioPlay,
    macOSAudioStop,
    macOSAudioExit,
    macOSAudioSetVolume,
    macOSAudioGetSettings,
    "MacOS Audio output"
};
