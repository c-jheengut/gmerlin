/*****************************************************************

  gavl.h

  Copyright (c) 2001-2002 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de

  http://gmerlin.sourceforge.net

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

*****************************************************************/

/*
 *  Gmerlin audio video library, a library for conversion of uncompressed
 *  audio- and video data
 */

#ifndef GAVL_H_INCLUDED
#define GAVL_H_INCLUDED


#include <inttypes.h>
#include "gavlconfig.h"

#include "gavltime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Acceleration flags */

#define GAVL_ACCEL_C       (1<<0)
#define GAVL_ACCEL_C_HQ    (1<<1)

#define GAVL_ACCEL_MMX     (1<<2)
#define GAVL_ACCEL_MMXEXT  (1<<3)
#define GAVL_ACCEL_SSE     (1<<4)
#define GAVL_ACCEL_SSE2    (1<<5)
#define GAVL_ACCEL_3DNOW   (1<<6)
  
/*
 *   Return supported CPU acceleration flags
 *   Note, that GAVL_ACCEL_C and GAVL_ACCEL_C_HQ are always supported and
 *   aren't returned here
 */
  
int gavl_accel_supported();

/*
 *  This takes a flag of wanted accel flags and ANDs them with
 *  the actually supported flags. Used mostly internally.
 */

int gavl_real_accel_flags(int wanted_flags);
  
/*************************************
 *  Audio conversions
 *************************************/

typedef struct gavl_audio_converter_s gavl_audio_converter_t;

typedef union gavl_audio_samples_u
  {
  uint8_t * u_8;
  int8_t *  s_8;

  uint16_t * u_16;
  int16_t  * s_16;

  /* We don't have 32 bit ints yet, but we need a
     generic 32 bit type if we handle floats for example */
  
  uint32_t * u_32;
  int32_t  * s_32;
  
  float * f;
  } gavl_audio_samples_t;

typedef union gavl_audio_channels_u
  {
  uint8_t ** u_8;
  int8_t **  s_8;

  uint16_t ** u_16;
  int16_t  ** s_16;

  /* We don't have 32 bit ints yet, but we need a
     generic 32 bit type if we handle floats for example */
  
  uint32_t ** u_32;
  int32_t  ** s_32;

  float ** f;
  
  } gavl_audio_channels_t;
  
typedef struct gavl_audio_frame_s
  {
  gavl_audio_samples_t  samples;
  gavl_audio_channels_t channels;
  int valid_samples;              /* Real number of samples */
  } gavl_audio_frame_t;

typedef enum
  {
    GAVL_SAMPLE_NONE,
    GAVL_SAMPLE_U8,
    GAVL_SAMPLE_S8,
    GAVL_SAMPLE_U16LE,
    GAVL_SAMPLE_S16LE,
    GAVL_SAMPLE_U16BE,
    GAVL_SAMPLE_S16BE,
    GAVL_SAMPLE_FLOAT
  } gavl_sample_format_t;

#ifdef  GAVL_PROCESSOR_BIG_ENDIAN
#define GAVL_SAMPLE_U16NE GAVL_SAMPLE_U16BE
#define GAVL_SAMPLE_S16NE GAVL_SAMPLE_S16BE
#define GAVL_SAMPLE_U16OE GAVL_SAMPLE_U16LE
#define GAVL_SAMPLE_S16OE GAVL_SAMPLE_S16LE
#else
#define GAVL_SAMPLE_U16NE GAVL_SAMPLE_U16LE
#define GAVL_SAMPLE_S16NE GAVL_SAMPLE_S16LE
#define GAVL_SAMPLE_U16OE GAVL_SAMPLE_U16BE
#define GAVL_SAMPLE_S16OE GAVL_SAMPLE_S16BE
#endif

typedef enum
  {
    GAVL_INTERLEAVE_NONE, /* No interleaving, all channels separate */
    GAVL_INTERLEAVE_2,    /* Interleaved pairs of channels          */ 
    GAVL_INTERLEAVE_ALL   /* Everything interleaved                 */
  } gavl_interleave_mode_t;

/*
 *  Audio channel setup: This can be used with
 *  AC3 decoders to support all speaker configurations
 */

/*
   This is the GAVL Channel order:
   GAVL_CHANNEL_MONO  |Front  | (lfe) |      |      |      |      |
   GAVL_CHANNEL_1     |Front  | (lfe) |      |      |      |      |
   GAVL_CHANNEL_2     |Front  | (lfe) |      |      |      |      |
   GAVL_CHANNEL_2F    |Front L|Front R| (lfe)|      |      |      |
   GAVL_CHANNEL_3F    |Front L|Front R|Center| (lfe)|      |      |
   GAVL_CHANNEL_2F1R  |Front L|Front R|Rear  | (lfe)|      |      |
   GAVL_CHANNEL_3F1R  |Front L|Front R|Rear  |Center| (lfe)|      |
   GAVL_CHANNEL_2F2R  |Front L|Front R|Rear L|Rear R|Center|      |
   GAVL_CHANNEL_3F2R  |Front L|Front R|Rear L|Rear R|Center| (lfe)|
*/
  
typedef enum
  {
    GAVL_CHANNEL_NONE = 0,
    GAVL_CHANNEL_MONO,
    GAVL_CHANNEL_1,      /* First (left) channel */
    GAVL_CHANNEL_2,      /* Second (right) channel */
    GAVL_CHANNEL_2F,     /* 2 Front channels (Stereo or Dual channels) */
    GAVL_CHANNEL_3F,
    GAVL_CHANNEL_2F1R,
    GAVL_CHANNEL_3F1R,
    GAVL_CHANNEL_2F2R,
    GAVL_CHANNEL_3F2R
  } gavl_channel_setup_t;

/* Structure describing an audio format */
  
typedef struct gavl_audio_format_s
  {
  int samples_per_frame; /* Maximum number of samples per frame */
  int samplerate;
  int num_channels;
  gavl_sample_format_t   sample_format;
  gavl_interleave_mode_t interleave_mode;
  gavl_channel_setup_t   channel_setup;
  int lfe;
  } gavl_audio_format_t;

/* Audio format <-> string conversions */
  
const char * gavl_sample_format_to_string(gavl_sample_format_t);
const char * gavl_channel_setup_to_string(gavl_channel_setup_t);
const char * gavl_interleave_mode_to_string(gavl_interleave_mode_t);
  
/* Maximum number of supported channels */
  
#define GAVL_MAX_CHANNELS 6
  
#define GAVL_AUDIO_DO_BUFFER     (1<<0)
#define GAVL_AUDIO_DOWNMIX_DOLBY (1<<1)
  
typedef struct
  {
  int accel_flags;          /* CPU Acceleration flags */
  int conversion_flags;
  } gavl_audio_options_t;

/* Convenience function */

int gavl_bytes_per_sample(gavl_sample_format_t format);
  
/* Create/destroy audio frame */  
  
gavl_audio_frame_t * gavl_create_audio_frame(const gavl_audio_format_t*);

void gavl_destroy_audio_frame(gavl_audio_frame_t *);

  void gavl_audio_frame_mute(gavl_audio_frame_t *,
                             const gavl_audio_format_t *);
  
    
/* Create/Destroy audio converter */

gavl_audio_converter_t * gavl_audio_converter_create();

void gavl_audio_converter_destroy(gavl_audio_converter_t*);

void gavl_audio_default_options(gavl_audio_options_t * opt);

int gavl_audio_init(gavl_audio_converter_t* cnv,
                    const gavl_audio_options_t * options,
                    const gavl_audio_format_t * input_format,
                    const gavl_audio_format_t * output_format);

/* Convert audio  */
  
int gavl_audio_convert(gavl_audio_converter_t * cnv,
                       gavl_audio_frame_t * input_frame,
                       gavl_audio_frame_t * output_frame);

/* Audio buffer: Can be used if samples_per_frame is different in the
   input and output format, or if the samples, which will be read at
   once, cannot be determined at start */

typedef struct gavl_audio_buffer_s gavl_audio_buffer_t;

/*
 *  Create/destroy the buffer. The samples_per_frame member
 *  should be the desired number at the output
 */
  
gavl_audio_buffer_t * gavl_create_audio_buffer(gavl_audio_format_t *);

void gavl_destroy_audio_buffer(gavl_audio_buffer_t *);

  /*
   *  Buffer audio
   *
   *  output frame is full, if valid_samples == samples_per_frame
   *
   *  If return value is 0, input frame is needed some more times
   */
  
int gavl_buffer_audio(gavl_audio_buffer_t * b,
                      gavl_audio_frame_t * in,
                      gavl_audio_frame_t * out);
  
  
/**********************************************
 *  Video conversions routines
 **********************************************/
  
typedef struct gavl_video_converter_s gavl_video_converter_t;

typedef struct gavl_video_frame_s
  {
  /* For planar formsts */
  
  uint8_t * y;
  uint8_t * u;
  uint8_t * v;

  int y_stride;
  int u_stride;
  int v_stride;
 
  /* For packed formats */
  
  uint8_t * pixels;
  int pixels_stride;
  
  void * user_data;   /* For storing private data             */

  gavl_time_t time;       /* us timestamp */
  
  } gavl_video_frame_t;

typedef enum 
  {
    GAVL_COLORSPACE_NONE = 0,
    GAVL_RGB_15,
    GAVL_BGR_15,
    GAVL_RGB_16,
    GAVL_BGR_16,
    GAVL_RGB_24,
    GAVL_BGR_24,
    GAVL_RGB_32,
    GAVL_BGR_32,
    GAVL_RGBA_32,
    GAVL_YUY2,
    GAVL_YUV_420_P,
    GAVL_YUV_422_P
  } gavl_colorspace_t;

typedef struct 
  {
  int width;
  int height;
  gavl_colorspace_t colorspace;
  float framerate;
  int free_framerate;   /* Framerate will be based on timestamps only */
  } gavl_video_format_t;

#define GAVL_SCANLINE (1<<0)

typedef struct
  {
  int accel_flags; /* CPU Acceleration flags */
  int conversion_flags;

  float crop_factor; /* Not used yet (for scaling) */
  
  /* Background color (0x0000 - 0xFFFF) */

  uint16_t background_red;
  uint16_t background_green;
  uint16_t background_blue;
  
  } gavl_video_options_t;

/***************************************************
 * Create and destroy video converters
 ***************************************************/

gavl_video_converter_t * gavl_create_video_converter();

void gavl_destroy_video_converter(gavl_video_converter_t*);

/***************************************************
 * Default Options
 ***************************************************/

void gavl_video_default_options(gavl_video_options_t * opt);

/***************************************************
 * Set formats
 ***************************************************/

int gavl_video_init(gavl_video_converter_t* cnv,
                    const gavl_video_options_t * options,
                    const gavl_video_format_t * input_format,
                    const gavl_video_format_t * output_format);

/***************************************************
 * Convert a frame
 ***************************************************/

void  gavl_video_convert(gavl_video_converter_t * cnv,
                        gavl_video_frame_t * input_frame,
                        gavl_video_frame_t * output_frame);

/*******************************************************
 * Create a video frame with memory aligned scanlines
 *******************************************************/

gavl_video_frame_t * gavl_create_video_frame(gavl_video_format_t*);

/*******************************************************
 * Destroy video frame 
 *******************************************************/

void gavl_destroy_video_frame(gavl_video_frame_t*);

/*****************************************************
 *  Zero all pointers of a video frame
 *******************************************************/

void gavl_video_frame_null(gavl_video_frame_t*);

/*****************************************************
 *  Allocate a video frame for a given format
 *****************************************************/

void gavl_video_frame_alloc(gavl_video_frame_t * frame,
                            gavl_video_format_t * format);

/*****************************************************
 *  Free memory of a video frame
 *****************************************************/

void gavl_video_frame_free(gavl_video_frame_t*);
  
/*******************************************************
 * Clear video frame (make it black)
 *******************************************************/

void gavl_clear_video_frame(gavl_video_frame_t * frame,
                            gavl_video_format_t * format);

/*******************************************************
 * Colorspace related functions
 *******************************************************/

int gavl_colorspace_is_rgb(gavl_colorspace_t colorspace);
int gavl_colorspace_is_yuv(gavl_colorspace_t colorspace);
int gavl_colorspace_has_alpha(gavl_colorspace_t colorspace);

const char * gavl_colorspace_to_string(gavl_colorspace_t colorspace);
gavl_colorspace_t gavl_string_to_colorspace(const char *);

int gavl_num_colorspaces();
gavl_colorspace_t gavl_get_colorspace(int index);

#ifdef __cplusplus
}
#endif

#endif /* GAVL_H_INCLUDED */
