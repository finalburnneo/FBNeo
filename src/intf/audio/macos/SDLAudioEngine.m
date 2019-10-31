/*****************************************************************************
 **
 ** Sound engine based on SDL_audio:
 **
 **   SDL - Simple DirectMedia Layer
 **   Copyright (C) 1997-2012 Sam Lantinga (slouken@libsdl.org)
 **
 ******************************************************************************
 */
#import "SDLAudioEngine.h"

static OSStatus audioCallback(void *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp *inTimeStamp,
                              UInt32 inBusNumber, UInt32 inNumberFrames,
                              AudioBufferList *ioData);

#define VOLUME_RANGE (40)

@interface SDLAudioEngine ()

- (void) renderSoundToStream:(AudioBufferList *)ioData;

@end

@implementation SDLAudioEngine
{
	BOOL isPaused;
	BOOL isReady;
	
	void *buffer;
	UInt32 bufferOffset;
	UInt32 bufferSize;
	
	AudioUnit outputAudioUnit;
}

- (void) dealloc
{
    if (isReady) {
        OSStatus result;
        struct AURenderCallbackStruct callback;
        
        result = AudioOutputUnitStop(outputAudioUnit);
		
        callback.inputProc = 0;
        callback.inputProcRefCon = 0;
        result = AudioUnitSetProperty(outputAudioUnit,
                                      kAudioUnitProperty_SetRenderCallback,
                                      kAudioUnitScope_Input, 0, &callback,
                                      sizeof(callback));
        
        result = AudioComponentInstanceDispose(outputAudioUnit);
        isReady = NO;
    }
    
    free(buffer);
    buffer = NULL;
}

#pragma mark - Private Methods

- (id)initWithSampleRate:(NSUInteger)sampleRate
                channels:(NSUInteger)channels
                 samples:(NSUInteger)samples
          bitsPerChannel:(UInt32)bitsPerChannel
{
    if ((self = [super init])) {
        isPaused = NO;
        isReady = NO;
        
        buffer = NULL;
        
        OSStatus result = noErr;
        AudioComponent comp;
        AudioComponentDescription desc;
        struct AURenderCallbackStruct callback;
        AudioStreamBasicDescription requestedDesc;
        
        // Setup a AudioStreamBasicDescription with the requested format
        requestedDesc.mFormatID = kAudioFormatLinearPCM;
        requestedDesc.mFormatFlags = kLinearPCMFormatFlagIsPacked;
        requestedDesc.mChannelsPerFrame = (UInt32)channels;
        requestedDesc.mSampleRate = sampleRate;
        
        requestedDesc.mBitsPerChannel = (UInt32)bitsPerChannel;
        requestedDesc.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
        
        requestedDesc.mFramesPerPacket = 1;
        requestedDesc.mBytesPerFrame = requestedDesc.mBitsPerChannel * requestedDesc.mChannelsPerFrame / 8;
        requestedDesc.mBytesPerPacket = requestedDesc.mBytesPerFrame * requestedDesc.mFramesPerPacket;
        
        // Locate the default output audio unit
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_DefaultOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        desc.componentFlags = 0;
        desc.componentFlagsMask = 0;
        
        comp = AudioComponentFindNext(NULL, &desc);
        if (comp != NULL) {
            // Open & initialize the default output audio unit
            result = AudioComponentInstanceNew(comp, &outputAudioUnit);
            if (result == noErr) {
                result = AudioUnitInitialize(outputAudioUnit);
                if (result == noErr) {
                    // Set the input format of the audio unit
                    result = AudioUnitSetProperty(outputAudioUnit,
                                                  kAudioUnitProperty_StreamFormat,
                                                  kAudioUnitScope_Input, 0, &requestedDesc,
                                                  sizeof(requestedDesc));
                    
                    if (result == noErr) {
                        // Set the audio callback
                        callback.inputProc = audioCallback;
                        callback.inputProcRefCon = (__bridge void *)self;
                        result = AudioUnitSetProperty(outputAudioUnit,
                                                      kAudioUnitProperty_SetRenderCallback,
                                                      kAudioUnitScope_Input, 0, &callback,
                                                      sizeof(callback));
                        
                        if (result == noErr) {
                            bufferSize = bitsPerChannel / 8;
                            bufferSize *= channels;
                            bufferSize *= samples;
                            bufferOffset = bufferSize;
                            buffer = calloc(bufferSize, 1);
                            
                            // Finally, start processing of the audio unit
                            result = AudioOutputUnitStart (outputAudioUnit);
                            if (result == noErr)
                                isReady = YES;
                        }
                    }
                }
            }
        }
    }

    return self;
}

- (void) renderSoundToStream:(AudioBufferList *)ioData
{
    UInt32 remaining, len;
    AudioBuffer *abuf;
    void *ptr;
    
    if (isPaused || !isReady) {
        for (int i = 0; i < ioData->mNumberBuffers; i++) {
            abuf = &ioData->mBuffers[i];
            memset(abuf->mData, 0, abuf->mDataByteSize);
        }
        
        return;
    }
    
    for (int i = 0; i < ioData->mNumberBuffers; i++) {
        abuf = &ioData->mBuffers[i];
        remaining = abuf->mDataByteSize;
        ptr = abuf->mData;
        
        while (remaining > 0) {
            if (bufferOffset >= bufferSize) {
                memset(buffer, 0, bufferSize);
                
                @synchronized(self) {
                    [self.delegate mixSoundFromBuffer:buffer
                                                bytes:bufferSize];
                }
                
                bufferOffset = 0;
            }
            
            len = bufferSize - bufferOffset;
            if (len > remaining)
                len = remaining;

            memcpy(ptr, buffer + bufferOffset, len);
            ptr = (char *)ptr + len;
            remaining -= len;
            bufferOffset += len;
        }
    }
}

#pragma mark - Public Methods

- (float) volume
{
    AudioUnitParameterValue val;
    AudioUnitGetParameter(outputAudioUnit, kHALOutputParam_Volume,
                          kAudioUnitScope_Global, 0, &val);
    return (float) val;
}

- (void) setVolume:(float) value
{
    AudioUnitSetParameter(outputAudioUnit, kHALOutputParam_Volume,
						  kAudioUnitScope_Global, 0,
                          MAX(0.0f, MIN(value, 1.0f)), 0);
}

- (void) pause
{
    if (!isPaused)
        isPaused = YES;
}

- (void) resume
{
    if (isPaused)
        isPaused = NO;
}

#pragma mark - Miscellaneous Callbacks

static OSStatus audioCallback(void *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp *inTimeStamp,
                              UInt32 inBusNumber, UInt32 inNumberFrames,
                              AudioBufferList *ioData)
{
    SDLAudioEngine *sound = (__bridge SDLAudioEngine *) inRefCon;
    [sound renderSoundToStream:ioData];
    
    return 0;
}

@end
