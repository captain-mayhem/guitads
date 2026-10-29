/*
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadscsnd.cpp - TADS Compressed Sound class
Function
  Decodes a WAV, MPEG audio or Ogg Vorbis sound with the engine's decoder
  (CGE::AudioDecoder) and streams the PCM to the sound card through
  CTadsAudioDevice (the engine's audio stream).  See guit3/migration.md
  sections 3.7 and 5.29.  The input file is read through the portable TADS
  osfile API (htmltads/imgui/migration.md section 5.4/I).
Notes

Modified
  04/26/02 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>

#include "tadsplat.h"

/* TADS OS layer - portable file I/O (osfoprb / osfseek / osfcls) */
#include <os.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "tadssnd.h"
#include "tadscsnd.h"

#include <audio/audiodecoder.h>


/* ------------------------------------------------------------------------ */
/*
 *   Implementation
 */


/*
 *   create
 */
CTadsCompressedAudio::CTadsCompressedAudio(
    const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
    class CTadsAudioControl *ctl,
    void (*done_func)(void *, int), void *done_func_ctx)
    : CTadsAudioPlayer(done_func, done_func_ctx)
{
    /* set a reference on behalf of our caller */
    refcnt_.store(1);

    /* create the output device */
    dev_ = CTadsAudioDevice::create();

    /* not registered with the audio controller yet */
    registered_ = FALSE;

    /* use full volume initially, unless we hear otherwise */
    init_vol_ = 10000;
    cur_vol_ = 10000;

    /* not stopping */
    stop_flag_ = false;

    /* remember and place a reference on the audio controller */
    audio_control_ = ctl;
    ctl->audioctl_add_ref();

    /* we're not finished yet */
    is_done_ = FALSE;

    /* no URL yet */
    url_ = 0;

    /* remember the filename */
    fname_.set(fname);

    /* open the file for binary reading through the OS layer */
    in_file_ = osfoprb(fname, OSFTBIN);

    /* remember the file starting seek position and size */
    in_file_start_ = file_start_ofs;
    in_file_size_ = file_size;

    /* seek to the start of the stream (for an embedded resource) */
    if (in_file_ != 0)
        osfseek(in_file_, file_start_ofs, OSFSK_SET);
}

/*
 *   destroy
 */
CTadsCompressedAudio::~CTadsCompressedAudio()
{
    /*
     *   If we have a background thread, wait for it - unless the current
     *   thread *is* the background thread, in which case we'd deadlock
     *   waiting for ourself (and there'd be no point, since we're the one
     *   doing the work).
     */
    join_worker();

    /* make sure we've closed our buffer */
    close_playback_buffer();

    /* release the output device */
    if (dev_ != 0)
    {
        delete dev_;
        dev_ = 0;
    }

    /* release our audio controller interface */
    audio_control_->audioctl_release();

    /* close our file */
    if (in_file_ != 0)
        osfcls(in_file_);
}

/*
 *   open an audio buffer
 */
int CTadsCompressedAudio::open_playback_buffer(int freq, int bits_per_sample,
                                               int num_channels)
{
    /* open the output stream */
    if (dev_ == 0 || dev_->open(freq, bits_per_sample, num_channels) != 0)
        return 2;

    /* we're now playing, so register with the audio controller */
    audio_control_->register_active_sound(this);
    registered_ = TRUE;

    /*
     *   Set the initial volume.  If application-wide muting is active, set
     *   the volume to zero; otherwise use the initial volume, so that we
     *   respect any volume change that was set before we opened the stream.
     */
    cur_vol_ = init_vol_;
    dev_->set_volume(audio_control_->get_mute_sound() ? 0 : init_vol_);

    /* success */
    return 0;
}

/*
 *   Change the muting status
 */
void CTadsCompressedAudio::on_mute_change(int mute)
{
    if (dev_ != 0)
        dev_->set_volume(mute ? 0 : cur_vol_);
}

/*
 *   close the audio buffer
 */
void CTadsCompressedAudio::close_playback_buffer()
{
    /* if we never opened the stream, there's nothing to do */
    if (dev_ == 0 || !dev_->is_open())
    {
        /* still unregister if we somehow registered */
        if (registered_)
        {
            audio_control_->unregister_active_sound(this);
            registered_ = FALSE;
        }
        return;
    }

    /*
     *   If we cancelled playback prematurely, drop whatever is still queued;
     *   otherwise let the queued audio finish playing out.
     */
    if (stop_flag_)
        dev_->halt();
    else
        dev_->drain();

    /* close the output stream */
    dev_->close();

    /* we're no longer playing, so unregister with the controller */
    if (registered_)
    {
        audio_control_->unregister_active_sound(this);
        registered_ = FALSE;
    }
}

/*
 *   Write to the audio buffer.  This puts data into the buffer, blocking
 *   until enough space is available to store the data.  The decoder should
 *   call this frequently to ensure that we keep the buffer prepared with
 *   data to play back while we're decoding.
 */
void CTadsCompressedAudio::write_playback_buffer(const char *buf, int bytes)
{
    /* if the decoder is stopping, don't bother */
    if (stop_flag_)
        return;

    if (dev_ != 0)
        dev_->write(buf, bytes);
}

/*
 *   Halt playback
 */
void CTadsCompressedAudio::halt_playback_buffer()
{
    if (dev_ != 0)
        dev_->halt();
}

/* ------------------------------------------------------------------------ */
/*
 *   HTML TADS Interface
 */

/*
 *   Play our file asynchronously
 */
int CTadsCompressedAudio::play(const textchar_t *url)
{
    /* remember the URL */
    url_ = url;

    /* we're not done yet */
    is_done_ = FALSE;

    /* not stopping yet */
    stop_flag_ = false;

    /* set a reference on behalf of the thread */
    AddRef();

    /* create the thread that will do the actual work */
    try
    {
        run_worker([this]()
        {
            do_play_thread();

            /* release the thread's reference on 'this' */
            Release();

#ifdef _WIN32
            /* release this thread's TADS TLS object */
            CTadsApp::on_thread_exit();
#endif
        });
    }
    catch (...)
    {
        /* we won't need the thread reference after all */
        Release();

        /* signal the termination event */
        stop_evt_.set();

        /* return failure */
        return 1;
    }

    /* success */
    return 0;
}

/*
 *   Determine if playback is still underway.  Returns true if playback is
 *   active, false if playback has finished.
 */
int CTadsCompressedAudio::is_playing()
{
    /* we're playing if the 'stop' event hasn't been signaled yet */
    return !stop_evt_.is_set();
}

/*
 *   wait until playback is finished
 */
void CTadsCompressedAudio::wait_until_done()
{
    stop_evt_.wait();
}

/*
 *   Set the volume; the range of 'vol' is 0 to 10000
 */
void CTadsCompressedAudio::set_audio_volume(int vol)
{
    /* remember it as our current (non-muted) level */
    cur_vol_ = vol;

    /* also record it as the initial level for a not-yet-open stream */
    init_vol_ = vol;

    /* push it to the device (which stores it if the stream isn't open yet) */
    if (dev_ != 0)
        dev_->set_volume(vol);
}

/*
 *   Playback thread main entrypoint
 */
void CTadsCompressedAudio::do_play_thread()
{
    /* signal that playback has started */
    mark_start_time();
    start_evt_.set();

    /* decode the file */
    do_decoding(in_file_, in_file_size_);

    /* if playback was cancelled, drop anything still queued */
    if (stop_flag_)
        halt_playback_buffer();

    /* close the audio device */
    close_playback_buffer();

    /* signal that playback has finished */
    stop_evt_.set();

    /* call the 'done' callback */
    call_done_callback();
}

/*
 *   Read the sound's bytes from the file
 */
bool CTadsCompressedAudio::read_sound(osfildef *fp, DWORD file_size,
                                      std::vector<uint8_t> &data)
{
    data.resize(file_size);
    return file_size != 0 && osfrb(fp, data.data(), file_size) == 0;
}

/*
 *   Decode the file
 */
void CTadsCompressedAudio::do_decoding(osfildef *fp, DWORD file_size)
{
    /* the engine's decoder reads the sound from memory */
    std::vector<uint8_t> data;
    if (fp == 0 || !read_sound(fp, file_size, data))
        return;

    /* open the decoder - it tells WAV, MPEG and Ogg Vorbis by the contents */
    std::unique_ptr<CGE::AudioDecoder> dec =
        CGE::AudioDecoder::open(std::move(data));
    if (dec == 0)
    {
        oshtml_dbg_printf("audio: %s can't be decoded\n",
                          url_ != 0 ? url_ : fname_.get());
        return;
    }

    /* open the playback buffer for the decoder's 32-bit float samples */
    int channels = dec->getChannels();
    if (open_playback_buffer(dec->getSampleRate(), 32, channels) != 0)
        return;

    /* decode in small chunks, so that decoding stays ahead of playback */
    const size_t frames = 2048;
    std::vector<float> pcm(frames * channels);
    while (!stop_flag_)
    {
        size_t got = dec->read(pcm.data(), frames);
        if (got == 0)
            break;
        write_playback_buffer((const char *)pcm.data(),
                              (int)(got * channels * sizeof(float)));
    }
}

/*
 *   Get the track length
 */
long CTadsCompressedAudio::get_track_len_ms()
{
    /*
     *   open a separate handle to the file, to avoid interfering with the
     *   playback thread
     */
    osfildef *fp = osfoprb(fname_.get(), OSFTBIN);
    if (fp == 0)
        return 0;

    /* read the sound from its start, in case it's an embedded resource */
    std::vector<uint8_t> data;
    bool ok = osfseek(fp, in_file_start_, OSFSK_SET) == 0
              && read_sound(fp, in_file_size_, data);
    osfcls(fp);
    if (!ok)
        return 0;

    /*
     *   the length in sample frames (an MP3 is read through for it) gives
     *   the time
     */
    std::unique_ptr<CGE::AudioDecoder> dec =
        CGE::AudioDecoder::open(std::move(data));
    if (dec == 0)
        return 0;
    return (long)(dec->getLength() * 1000 / dec->getSampleRate());
}

/*
 *   Stop playback
 */
void CTadsCompressedAudio::stop(int sync)
{
    /* set the flag to indicate that we shouldn't do any more decoding */
    stop_flag_ = true;

    /* make the output stop right away so 'sync' callers don't wait it out */
    halt_playback_buffer();

    /* wait until we get confirmation, if synchronizing */
    if (sync)
        sync_on_stop();
}

/* invoke the 'done' callback */
void CTadsCompressedAudio::call_done_callback()
{
    /*
     *   Protect this as a critical section - we don't want to enqueue the
     *   notification twice if another thread decides to handle it too.
     */
    std::lock_guard<std::mutex> lk(cb_mutex_);

    /*
     *   enqueue the 'done' notification for the main thread.  We never play
     *   a sound more than once, so the repeat count is always 1.
     */
    send_done_message(1);

    /* set our 'done' flag */
    is_done_ = TRUE;
}
