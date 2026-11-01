/*
 *   Copyright (c) 2026 by the tads-runner contributors.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsmidi.h - TADS MIDI support
Function
  Plays a MIDI file with a software synthesizer: TinySoundFont renders it
  with a General MIDI soundfont (soundfont/TimGM6mb.sf2, next to the
  executable), and the PCM goes to the sound card through CTadsAudioDevice
  (tadsaudiodev.h) on the engine's audio stream, like the digitized sounds
  (tadscsnd.h).
Notes
  Until guit3/migration.md section 5.33, this was the Win32 client's MIDI
  player on Windows' midiStream sequencer (and DirectMusic, compiled out),
  and guit3 had no MIDI off Windows.

  The MIDI file is read through the portable TADS osfile API, from the
  sound's slice of its file - an embedded resource starts somewhere inside
  a .gam/.t3 or resource file.
Modified
  01/17/98 MJRoberts  - Creation (the Win32 player)
  2026                - rewritten on TinySoundFont
*/

#ifndef TADSMIDI_H
#define TADSMIDI_H

#include <atomic>
#include <mutex>

#include "tadsplat.h"

/* TADS OS interfaces - for file I/O */
#include <os.h>

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSSND_H
#include "tadssnd.h"
#endif
#include "tadsaudiodev.h"


/*
 *   MIDI file player
 */
class CTadsMidiFilePlayer: public CTadsAudioPlayer
{
public:
    /*
     *   create a player for the MIDI file stored at 'seek_pos' in the file
     *   'fname', 'data_size' bytes long
     */
    CTadsMidiFilePlayer(class CTadsAudioControl *ctl,
                        const textchar_t *fname,
                        long seek_pos, long data_size,
                        void (*done_func)(void *, int), void *done_func_ctx);

    /* reference counting */
    void AddRef()
    {
        refcnt_.fetch_add(1);
    }
    void Release()
    {
        if (refcnt_.fetch_sub(1) == 1)
            delete this;
    }

    /*
     *   Begin playing the MIDI file.  Returns immediately after starting
     *   playback (playback continues in a background thread).  Returns zero
     *   on success, non-zero on failure.  When playback is finished, the
     *   'done' callback is invoked on the main thread.
     */
    int play();

    /* is the sound playing? */
    int is_playing();

    /* determine if we've been stopped */
    int is_stopped() { return is_done_; }

    /* get the track time in milliseconds */
    long get_track_len_ms() { return track_len_ms_; }

    /*
     *   Stop playback.  If 'sync' is true, it means that we must not
     *   return until the sound has actually stopped and the callback has
     *   been invoked; otherwise, we may return before the callback is
     *   invoked.
     */
    void stop(int sync);

    /* invoke the 'done' callback, and forget it so we don't repeat it */
    void call_done_callback();

    /*
     *   CTadsAudioPlayer implementation
     */

    /* set the playback volume, 0..10000 */
    void set_audio_volume(int vol);

    /* mute playback */
    void on_mute_change(int mute);

    /*
     *   static termination - this should be called on app exit to release
     *   the soundfont
     */
    static void class_terminate();

private:
    /* accessible via Release() only */
    virtual ~CTadsMidiFilePlayer();

    /* playback thread entrypoint */
    void do_play_thread();

    /* render the MIDI messages to the output device */
    void do_rendering();

    /*
     *   Get a synthesizer of our own on the soundfont, loading the soundfont
     *   on first use.  Returns null if there's no soundfont.  Free it with
     *   close_synth().
     */
    static struct tsf *open_synth();
    static void close_synth(struct tsf *synth);

    /* the loaded MIDI messages, or null if the file couldn't be read */
    struct tml_message *midi_;

    /* the track length in milliseconds */
    long track_len_ms_;

    /* our synthesizer */
    struct tsf *synth_;

    /* the output playback device */
    CTadsAudioDevice *dev_;

    /* our system audio controller */
    class CTadsAudioControl *audio_control_;

    /* true once we've registered with the audio controller */
    int registered_;

    /* current volume, 0..10000, and the muting status */
    std::atomic<int> cur_vol_;
    std::atomic<bool> muted_;

    /* flag: we've finished playback */
    int is_done_;

    /* flag: stop rendering as soon as possible */
    std::atomic<bool> stop_flag_;

    /* protects call_done_callback() against concurrent invocation */
    std::mutex cb_mutex_;

    /* reference count */
    std::atomic<long> refcnt_;

    /*
     *   The soundfont, as loaded.  Each player plays on a copy of it, which
     *   shares the samples; the copies' reference count isn't atomic, so
     *   copying and closing are done under the mutex.
     */
    static struct tsf *soundfont_;
    static bool soundfont_tried_;
    static std::mutex soundfont_mutex_;
};

#endif /* TADSMIDI_H */
