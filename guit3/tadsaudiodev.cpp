/*
 *   Copyright (c) 2024 by the tads-runner contributors.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsaudiodev.cpp - PCM playback device for guit3 on the engine's audio
Function
  Implements CTadsAudioDevice (see tadsaudiodev.h) on the engine's
  CGE::AudioStream, plus the cross-thread 'done' callback queue that
  replaces the old HTMLM_SOUND_DONE window message.
Notes
  The stream is a node of the engine's mixer, so guit3's sounds mix with
  each other there instead of each opening a device of its own (until
  guit3/migration.md section 5.29, this file opened a miniaudio device per
  sound).  AudioStream has the semantics this interface needs: write()
  blocks until the samples are queued, halt() and set_volume() may come
  from other threads, and a volume set before open() is kept.
Modified
  2024 - creation, as part of the guit3 audio-backend migration
*/

#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>

#include <audio/audio.h>
#include <audio/audiostream.h>
#include <system/engine.h>

#include "tadsaudiodev.h"
#include "tadssnd.h"

/* ------------------------------------------------------------------------ */
/*
 *   CTadsAudioDevice on the engine's audio stream
 */
namespace
{

/* the engine's audio, or null if it isn't open */
CGE::Audio *engine_audio()
{
    CGE::Engine *eng = CGE::Engine::instance();
    CGE::Audio *audio = eng != 0 ? eng->getAudio() : 0;
    return audio != 0 && audio->isOpen() ? audio : 0;
}

class CTadsAudioDevice_cge: public CTadsAudioDevice
{
public:
    CTadsAudioDevice_cge(std::unique_ptr<CGE::AudioStream> stream)
        : stream_(std::move(stream))
    {
    }

    int open(int freq, int bits_per_sample, int num_channels)
    {
        return stream_->open(freq, bits_per_sample, num_channels) ? 0 : 1;
    }

    void write(const char *buf, int bytes)
    {
        if (bytes > 0)
            stream_->write(buf, (size_t)bytes);
    }

    void halt() { stream_->halt(); }
    void drain() { stream_->drain(); }
    void close() { stream_->close(); }

    void set_volume(int vol)
    {
        stream_->setVolume((float)std::clamp(vol, 0, 10000) / 10000.0f);
    }

    int is_open() const { return stream_->isOpen() ? 1 : 0; }

private:
    std::unique_ptr<CGE::AudioStream> stream_;
};

} /* anonymous namespace */

CTadsAudioDevice *CTadsAudioDevice::create()
{
    CGE::Audio *audio = engine_audio();
    if (audio == 0)
        return 0;
    std::unique_ptr<CGE::AudioStream> stream = audio->createStream();
    if (stream == 0)
        return 0;
    return new CTadsAudioDevice_cge(std::move(stream));
}

bool CTadsAudioDevice::is_available()
{
    /*
     *   miniaudio falls back to its Null backend (which discards everything)
     *   when no real one loads - e.g. on Linux without libpulse or
     *   libasound installed.  That's no audio at all, so report it as such
     *   rather than playing silently.
     */
    CGE::Audio *audio = engine_audio();
    return audio != 0 && audio->getBackendName() != "Null";
}

/* ------------------------------------------------------------------------ */
/*
 *   Cross-thread 'done' callback queue
 */
namespace
{
struct done_entry_t
{
    CTadsAudioPlayer *player;
    int repeat_count;
};

std::mutex g_done_mutex;
std::deque<done_entry_t> g_done_queue;
}

void tads_audio_post_done_callback(CTadsAudioPlayer *player, int repeat_count)
{
    if (player == 0)
        return;

    std::lock_guard<std::mutex> lk(g_done_mutex);
    done_entry_t e;
    e.player = player;
    e.repeat_count = repeat_count;
    g_done_queue.push_back(e);
}

void tads_audio_run_done_callbacks()
{
    for (;;)
    {
        done_entry_t e;
        {
            std::lock_guard<std::mutex> lk(g_done_mutex);
            if (g_done_queue.empty())
                return;
            e = g_done_queue.front();
            g_done_queue.pop_front();
        }

        /*
         *   on_sound_done() invokes the resource's 'done' callback and then
         *   releases the reference the worker thread added on our behalf.
         */
        e.player->on_sound_done(e.repeat_count);
    }
}
