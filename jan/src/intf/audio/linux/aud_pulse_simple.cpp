// C++11 simple pulse audio driver
#include <iostream>
#include <thread>
#include <chrono>
#include <pulse/simple.h>
#include <pulse/error.h>
#include "burner.h"
#include "ringbuffer.h"

static ring_buffer<short> *buffer = nullptr;
static pa_simple *pa_stream = nullptr;
static std::thread *streamer_thread = nullptr;
static volatile bool streamer_stop = false;
static volatile bool streamer_is_running = false;

// Samples per segment
static int samples_per_segment = 0;
static unsigned int pas_sound_fps;
static int (*pas_get_next_sound)(int);

static int pas_default_sound_filler(int)
{
    if (nAudNextSound == nullptr)
        return 1;
    memset(nAudNextSound, 0, nAudSegLen * 4);
    return 0;
}

static int pas_blank_sound()
{
    if (nAudNextSound != nullptr)
        AudWriteSilence();
    return 0;
}

static int pas_sound_check()
{
    // 5 segments ahead...
    if (buffer->size() >= (samples_per_segment * nAudSegCount)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return 0;
    }

    pas_get_next_sound(1);
    buffer->write(nAudNextSound, samples_per_segment);
    return 0;
}

static int pas_exit()
{
    nAudNextSound = NULL;
    return 0;
}

static int pas_set_callback(int (*callback)(int))
{
    if (callback == NULL) {
        pas_get_next_sound = pas_default_sound_filler;
    } else {
        pas_get_next_sound = callback;
    }
    return 0;
}

static void pas_audio_streamer(void)
{
    short *buf = new short[samples_per_segment];
    streamer_is_running = true;

    while (!streamer_stop) {
        // playing...
        if (bAudPlaying) {

            if (buffer->size() >= samples_per_segment) {
                buffer->read(buf, samples_per_segment);
            } else {
                memset(buf, 0, samples_per_segment * 2);
            }

            pa_simple_write(pa_stream, buf, samples_per_segment * 2, NULL);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
    delete [] buf;
    streamer_is_running = false;
    streamer_stop = false;
}

static int pas_init()
{
    pas_sound_fps = nAppVirtualFps;
    nAudSegLen = (nAudSampleRate[0] * 100 + (pas_sound_fps / 2)) / pas_sound_fps;

    // samples per segment
    samples_per_segment = nAudSegLen * 2;

    // seglen * 2 channels * 2 bytes per sample (16bits)
    nAudAllocSegLen = samples_per_segment * 2;

    nAudNextSound = new short[samples_per_segment];

    pas_set_callback(nullptr);
    pas_default_sound_filler(0);

    pBurnSoundOut = nAudNextSound;
    nBurnSoundRate = nAudSampleRate[0];
    nBurnSoundLen = nAudAllocSegLen;

    pa_sample_spec specs;
    specs.channels = 2;
    specs.format = PA_SAMPLE_S16LE;
    specs.rate = nAudSampleRate[0];

    pa_buffer_attr attributes;
    attributes.maxlength = -1;
    attributes.minreq = -1;
    attributes.prebuf = -1;
    attributes.tlength = nAudAllocSegLen * nAudSegCount;

    if (streamer_thread) {
        streamer_stop = true;
        while (streamer_is_running) {
            std::this_thread::yield();
        }
        streamer_stop = false;
        delete streamer_thread;
    }

    // destroy previous pulse audio stream
    if (pa_stream) {
        pa_simple_flush(pa_stream, NULL);
        pa_simple_free(pa_stream);
    }

    // destroy previous ring buffer
    if (buffer) {
        delete buffer;
    }

    // 6 segmentos no buffer
    buffer = new ring_buffer<short>(samples_per_segment * nAudSegCount * 2);
    buffer->virtual_write(samples_per_segment * (nAudSegCount));
    pa_stream = pa_simple_new(NULL,
                              "fbalpha",
                              PA_STREAM_PLAYBACK,
                              NULL,
                              "fbalpha",
                              &specs,
                              NULL,
                              &attributes,
                              NULL);
    streamer_thread = new std::thread(pas_audio_streamer);
    streamer_thread->detach();
    bAudOkay = 1;
    return 0;
}

static int pas_play()
{
    bAudPlaying = 1;
    return 0;
}

static int pas_stop()
{
    bAudPlaying = 0;
    return 0;
}

static int pas_set_volume()
{
    return 1;
}

static int pas_get_settings(InterfaceInfo *)
{
    return 0;
}

struct AudOut AudOutPulseSimple = {
    pas_blank_sound,
    pas_sound_check,
    pas_init,
    pas_set_callback,
    pas_play,
    pas_stop,
    pas_exit,
    pas_set_volume,
    pas_get_settings,
    _T("PulseAudio (Simple) audio output")
};
