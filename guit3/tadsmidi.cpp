/*
 *   Copyright (c) 2026 by the tads-runner contributors.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsmidi.cpp - TADS MIDI support
Function
  Plays a MIDI file: TinyMidiLoader (tml.h) reads it, TinySoundFont (tsf.h)
  renders it with the General MIDI soundfont, and CTadsAudioDevice (the
  engine's audio stream) plays the PCM.  See guit3/migration.md section 5.33.
Notes

Modified
  01/17/98 MJRoberts  - Creation (the Win32 player)
  2026                - rewritten on TinySoundFont
*/

#include <stdio.h>
#include <string.h>

#include <vector>

#include "tadsplat.h"

/* TADS OS layer - portable file I/O (osfoprb / osfseek / osfcls) */
#include <os.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "tadssnd.h"
#include "tadsmidi.h"
#include "guios.h"

#define TSF_IMPLEMENTATION
#include "tsf.h"
#define TML_IMPLEMENTATION
#include "tml.h"


/* ------------------------------------------------------------------------ */
/*
 *   Synthesizer settings
 */

/* output sample rate */
static const int midi_sample_rate = 44100;

/*
 *   the synthesizer's gain: TinySoundFont at 0 dB already clips with an
 *   organ note and a drum (peak 1.13 with TimGM6mb), and a General MIDI
 *   piece plays many voices at once
 */
static const float midi_gain_db = -6.0f;

/* frames rendered per write to the device */
static const int midi_chunk_frames = 2048;

/*
 *   Longest time we let notes ring out after the last MIDI message, in
 *   milliseconds - the release of the last notes, or notes the file never
 *   switched off.
 */
static const double midi_tail_ms = 3000.0;

/* the soundfont, relative to the executable's folder */
static const char midi_soundfont_path[] = "soundfont/TimGM6mb.sf2";


/* ------------------------------------------------------------------------ */
/*
 *   Read 'len' bytes at 'ofs' in the file 'fname'; a negative 'len' reads
 *   to the end of the file
 */
static bool read_file_slice(const char *fname, long ofs, long len,
                            std::vector<uint8_t> &data)
{
    osfildef *fp = osfoprb(fname, OSFTBIN);
    if (fp == 0)
        return false;

    if (len < 0 && osfseek(fp, 0, OSFSK_END) == 0)
        len = osfpos(fp) - ofs;

    data.resize(len > 0 ? (size_t)len : 0);
    bool ok = len > 0
              && osfseek(fp, ofs, OSFSK_SET) == 0
              && osfrb(fp, data.data(), (size_t)len) == 0;
    osfcls(fp);
    return ok;
}


/* ------------------------------------------------------------------------ */
/*
 *   The soundfont
 */
tsf *CTadsMidiFilePlayer::soundfont_ = 0;
bool CTadsMidiFilePlayer::soundfont_tried_ = false;
std::mutex CTadsMidiFilePlayer::soundfont_mutex_;

tsf *CTadsMidiFilePlayer::open_synth()
{
    std::lock_guard<std::mutex> lk(soundfont_mutex_);

    /* load the soundfont on first use - only try once */
    if (!soundfont_tried_)
    {
        soundfont_tried_ = true;

        /* it's in the executable's folder */
        char exe[OSFNMAX];
        char dir[OSFNMAX];
        char path[OSFNMAX];
        gui_get_exe_path(exe, sizeof(exe));
        os_get_path_name(dir, sizeof(dir), exe);
        os_build_full_path(path, sizeof(path), dir, midi_soundfont_path);

        /* read it through the OS layer, and load it from memory */
        std::vector<uint8_t> data;
        if (read_file_slice(path, 0, -1, data))
            soundfont_ = tsf_load_memory(data.data(), (int)data.size());
        if (soundfont_ == 0)
            oshtml_dbg_printf("midi: can't load the soundfont %s\n", path);
    }

    /* each player gets a copy - the copies share the samples */
    return soundfont_ != 0 ? tsf_copy(soundfont_) : 0;
}

void CTadsMidiFilePlayer::close_synth(tsf *synth)
{
    std::lock_guard<std::mutex> lk(soundfont_mutex_);
    tsf_close(synth);
}

void CTadsMidiFilePlayer::class_terminate()
{
    /* the players' copies keep the samples until they're closed too */
    std::lock_guard<std::mutex> lk(soundfont_mutex_);
    tsf_close(soundfont_);
    soundfont_ = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   MIDI file player
 */

CTadsMidiFilePlayer::CTadsMidiFilePlayer(
    CTadsAudioControl *ctl, const textchar_t *fname,
    long seek_pos, long data_size,
    void (*done_func)(void *, int), void *done_func_ctx)
    : CTadsAudioPlayer(done_func, done_func_ctx)
{
    /* set a reference on behalf of our caller */
    refcnt_.store(1);

    /* remember and place a reference on the audio controller */
    audio_control_ = ctl;
    ctl->audioctl_add_ref();
    registered_ = FALSE;

    /* full volume, unless we hear otherwise */
    cur_vol_ = 10000;
    muted_ = false;

    is_done_ = FALSE;
    stop_flag_ = false;

    /* load the MIDI messages - a MIDI file is small */
    midi_ = 0;
    track_len_ms_ = 0;
    std::vector<uint8_t> data;
    if (read_file_slice(fname, seek_pos, data_size, data))
        midi_ = tml_load_memory(data.data(), (int)data.size());
    if (midi_ != 0)
    {
        unsigned int len = 0;
        tml_get_info(midi_, 0, 0, 0, 0, &len);
        track_len_ms_ = (long)len;
    }
    else
        oshtml_dbg_printf("midi: %s can't be read\n", fname);

    /* get a synthesizer, and the output device */
    synth_ = (midi_ != 0 ? open_synth() : 0);
    dev_ = (synth_ != 0 ? CTadsAudioDevice::create() : 0);
}

CTadsMidiFilePlayer::~CTadsMidiFilePlayer()
{
    /* wait for the playback thread, unless it's the one deleting us */
    join_worker();

    /* release the output device */
    if (dev_ != 0)
    {
        dev_->close();
        delete dev_;
    }

    /* release the synthesizer and the MIDI messages */
    if (synth_ != 0)
        close_synth(synth_);
    tml_free(midi_);

    /* release our audio controller interface */
    if (registered_)
        audio_control_->unregister_active_sound(this);
    audio_control_->audioctl_release();
}

/*
 *   Begin playback
 */
int CTadsMidiFilePlayer::play()
{
    /* we need the MIDI file, the soundfont and the sound output */
    if (dev_ == 0)
    {
        stop_evt_.set();
        return 1;
    }

    is_done_ = FALSE;
    stop_flag_ = false;

    /* set a reference on behalf of the thread */
    AddRef();

    /* create the thread that renders the music */
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
        stop_evt_.set();
        return 2;
    }

    /* success */
    return 0;
}

/*
 *   Playback thread
 */
void CTadsMidiFilePlayer::do_play_thread()
{
    /* signal that playback has started */
    mark_start_time();
    start_evt_.set();

    /* open the output for the synthesizer's stereo float samples */
    if (dev_->open(midi_sample_rate, 32, 2) == 0)
    {
        /* we're playing, so register with the audio controller */
        audio_control_->register_active_sound(this);
        registered_ = TRUE;

        /* set the volume, respecting muting */
        muted_ = audio_control_->get_mute_sound() != 0;
        dev_->set_volume(muted_ ? 0 : cur_vol_.load());

        /* play the music */
        do_rendering();

        /*
         *   if we were cancelled, drop whatever is still queued; otherwise
         *   let it play out
         */
        if (stop_flag_)
            dev_->halt();
        else
            dev_->drain();
        dev_->close();

        /* we're no longer playing */
        audio_control_->unregister_active_sound(this);
        registered_ = FALSE;
    }

    /* signal that playback has finished */
    stop_evt_.set();

    /* call the 'done' callback */
    call_done_callback();
}

/*
 *   Render the MIDI messages
 */
void CTadsMidiFilePlayer::do_rendering()
{
    tsf_set_output(synth_, TSF_STEREO_INTERLEAVED, midi_sample_rate,
                   midi_gain_db);

    /* channel 10 (9 counting from 0) is the General MIDI drum kit */
    tsf_channel_set_bank_preset(synth_, 9, 128, 0);

    std::vector<float> pcm(midi_chunk_frames * 2);
    const double ms_per_frame = 1000.0 / midi_sample_rate;
    const tml_message *msg = midi_;
    double now_ms = 0.0;
    double tail_end_ms = 0.0;
    while (!stop_flag_)
    {
        /*
         *   After the last message, release the notes still on, and let
         *   them ring out until they're silent (or for midi_tail_ms at most).
         */
        if (msg == 0)
        {
            if (tail_end_ms == 0.0)
            {
                tsf_note_off_all(synth_);
                tail_end_ms = now_ms + midi_tail_ms;
            }
            if (tsf_active_voice_count(synth_) == 0 || now_ms >= tail_end_ms)
                break;
        }

        /*
         *   Render a chunk, in blocks short enough that the messages are
         *   applied on time: before each block, apply the messages due by
         *   the block's end.
         */
        for (int done = 0; done < midi_chunk_frames; )
        {
            int block = midi_chunk_frames - done;
            if (block > TSF_RENDER_EFFECTSAMPLEBLOCK)
                block = TSF_RENDER_EFFECTSAMPLEBLOCK;

            now_ms += block * ms_per_frame;
            for ( ; msg != 0 && msg->time <= now_ms; msg = msg->next)
            {
                switch (msg->type)
                {
                case TML_PROGRAM_CHANGE:
                    tsf_channel_set_presetnumber(synth_, msg->channel,
                                                 msg->program,
                                                 msg->channel == 9);
                    break;

                case TML_NOTE_ON:
                    tsf_channel_note_on(synth_, msg->channel, msg->key,
                                        msg->velocity / 127.0f);
                    break;

                case TML_NOTE_OFF:
                    tsf_channel_note_off(synth_, msg->channel, msg->key);
                    break;

                case TML_PITCH_BEND:
                    tsf_channel_set_pitchwheel(synth_, msg->channel,
                                               msg->pitch_bend);
                    break;

                case TML_CONTROL_CHANGE:
                    tsf_channel_midi_control(synth_, msg->channel,
                                             msg->control,
                                             msg->control_value);
                    break;
                }
            }

            tsf_render_float(synth_, pcm.data() + done * 2, block, 0);
            done += block;
        }

        /* this blocks until the device has room for the chunk */
        dev_->write((const char *)pcm.data(),
                    (int)(pcm.size() * sizeof(float)));
    }
}

/*
 *   Is the track playing?
 */
int CTadsMidiFilePlayer::is_playing()
{
    /* we're playing until the 'stop' event is signaled */
    return !stop_evt_.is_set();
}

/*
 *   Stop playback.  The 'done' callback will still be invoked in the usual
 *   manner.
 */
void CTadsMidiFilePlayer::stop(int sync)
{
    /* stop rendering */
    stop_flag_ = true;

    /* make the output stop right away so 'sync' callers don't wait it out */
    if (dev_ != 0)
        dev_->halt();

    /* wait for playback to stop */
    if (sync)
        sync_on_stop();
}

/*
 *   Set the muting status
 */
void CTadsMidiFilePlayer::on_mute_change(int mute)
{
    muted_ = (mute != 0);
    if (dev_ != 0)
        dev_->set_volume(muted_ ? 0 : cur_vol_.load());
}

/*
 *   Set the playback volume, 0..10000
 */
void CTadsMidiFilePlayer::set_audio_volume(int vol)
{
    cur_vol_ = vol;

    /* the device keeps a volume set before it's opened */
    if (dev_ != 0)
        dev_->set_volume(muted_ ? 0 : vol);
}

/*
 *   Invoke the 'done' callback
 */
void CTadsMidiFilePlayer::call_done_callback()
{
    /* don't enqueue the notification twice */
    std::lock_guard<std::mutex> lk(cb_mutex_);

    /* we play the track once; repeating is up to the caller */
    send_done_message(1);
    is_done_ = TRUE;
}
