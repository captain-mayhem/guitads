/*
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadscsnd.h - TADS Compressed Sound class
Function
  Plays a digitized sound - WAV, MPEG audio or Ogg Vorbis - by streaming it
  from disk, through the decoder, and into the sound card.

  The decoding is the engine's (CGE::AudioDecoder: miniaudio's WAV and MP3
  decoders and libvorbis), and the output is CTadsAudioDevice
  (tadsaudiodev.h) on the engine's audio stream.  guit3 used to have a
  decoder class for each format (CWavW32, CMpegAmpW32 on the amp decoder,
  CVorbisW32); see guit3/migration.md section 5.29.
Notes
  The sound is read through the portable TADS osfile API (osfoprb / osfrb /
  osfseek / osfcls), from the stream's slice of its file - an embedded
  resource starts somewhere inside a .gam/.t3 or resource file.  See
  htmltads/imgui/migration.md section 5.4/I.
Modified
  04/26/02 MJRoberts  - Creation
*/

#ifndef TADSCSND_H
#define TADSCSND_H

#include <stdio.h>
#include <string.h>

#include <atomic>
#include <mutex>
#include <vector>
#include <stdint.h>

#include "tadsplat.h"

/* TADS OS layer - for osfildef and the portable file-I/O calls */
#include <os.h>

#include "tadshtml.h"
#include "tadssnd.h"
#include "tadsaudiodev.h"


/* ------------------------------------------------------------------------ */
/*
 *   TADS Compressed Audio class.  It plays WAV, MPEG audio and Ogg Vorbis
 *   alike: the engine's decoder tells them apart by their contents.
 */
class CTadsCompressedAudio: public CTadsAudioPlayer
{
public:
    /* -------------------------------------------------------------------- */
    /*
     *   HTML TADS interface.  The HTML TADS compressed audio objects use
     *   this interface to control sound playback.
     */

    /* reference management */
    void AddRef()
    {
        refcnt_.fetch_add(1);
    }
    void Release()
    {
        if (refcnt_.fetch_sub(1) == 1)
            delete this;
    }

    /* play back a file */
    int play(const textchar_t *url);

    /* wait until playback is finished */
    void wait_until_done();

    /* call the 'done' callback */
    void call_done_callback();

    /* determine if we're still playing */
    int is_playing();

    /* determine if we've been stopped */
    int is_stopped() { return is_done_; }

    /* halt playback */
    void stop(int sync);

    /* get our open input file (null if the file couldn't be opened) */
    osfildef *get_file_handle() const { return in_file_; }

    /*
     *   CTadsAudioPlayer implementation
     */

    /* set the volume level, as a value from 0 to 10000 */
    void set_audio_volume(int level);

    /* turn muting on or off */
    void on_mute_change(int mute);

    /* get the track time in milliseconds (0 if it can't be determined) */
    long get_track_len_ms();

    /*
     *   create a player for the sound stored at 'file_start_ofs' in the
     *   file 'fname', 'file_size' bytes long
     */
    CTadsCompressedAudio(
        const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
        class CTadsAudioControl *ctl,
        void (*done_func)(void *, int), void *done_func_ctx);

protected:
    /* destroy - only accessible via Release() */
    virtual ~CTadsCompressedAudio();

private:
    /* -------------------------------------------------------------------- */
    /*
     *   private internal operations
     */

    /* main playback thread entrypoint */
    void do_play_thread();

    /*
     *   Decode the file, writing the PCM to the playback buffer as it goes.
     *   'fp' is our open input file, already positioned at the start of the
     *   audio stream.
     */
    void do_decoding(osfildef *fp, DWORD file_size);

    /*
     *   read the sound's bytes from 'fp', which is positioned at its start;
     *   returns false if the file is shorter than the sound
     */
    static bool read_sound(osfildef *fp, DWORD file_size,
                           std::vector<uint8_t> &data);

    /* open the output audio buffer */
    int open_playback_buffer(int freq, int bits_per_sample,
                             int number_of_channels);

    /* close the output audio buffer */
    void close_playback_buffer();

    /* write decoded PCM data to the playback buffer */
    void write_playback_buffer(const char *buf, int bytes);

    /* halt playback of the output stream */
    void halt_playback_buffer();

    /*
     *   member variables
     */

    /*
     *   the file we're to play back, its starting seek location, and its
     *   size in bytes
     */
    CStringBuf fname_;
    osfildef *in_file_;
    DWORD in_file_start_;
    DWORD in_file_size_;

    /* our application-wide audio controller */
    class CTadsAudioControl *audio_control_;

    /* the output playback device */
    CTadsAudioDevice *dev_;

    /* true once we've registered with the audio controller */
    int registered_;

    /*
     *   volume level, 0..10000.  init_vol_ is applied when the device is
     *   opened; cur_vol_ tracks the last non-muted level so we can restore
     *   it when muting is turned back off.
     */
    int init_vol_;
    int cur_vol_;

    /* flag: we're finished with playback */
    int is_done_;

    /* flag: stop decoding as soon as possible */
    std::atomic<bool> stop_flag_;

    /* protects call_done_callback() against concurrent invocation */
    std::mutex cb_mutex_;

    /* the URL of the sound resource (used only for displaying errors) */
    const char *url_;

    /* reference count */
    std::atomic<long> refcnt_;
};

#endif /* TADSCSND_H */
