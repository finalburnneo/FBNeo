/*****************************************************************************
**
** Sound engine based on SDL_audio:
**
**   SDL - Simple DirectMedia Layer
**   Copyright (C) 1997-2012 Sam Lantinga (slouken@libsdl.org)
**
******************************************************************************
*/
#import <Foundation/Foundation.h>
#include <AudioUnit/AudioUnit.h>

@protocol SDLAudioDelegate

@required
- (void) mixSoundFromBuffer:(SInt16 *) mixBuffer
                      bytes:(UInt32) bytes;

@end

@interface SDLAudioEngine : NSObject

@property (nonatomic, weak) id<SDLAudioDelegate> delegate;
@property float volume;

- (id)initWithSampleRate:(NSUInteger)sampleRate
                channels:(NSUInteger)channels
                 samples:(NSUInteger)samples
          bitsPerChannel:(UInt32)bitsPerChannel;

- (void) pause;
- (void) resume;

@end
