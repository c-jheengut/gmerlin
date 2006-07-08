/*****************************************************************
 
  audio_faad2.c
 
  Copyright (c) 2003-2006 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <faad.h>

#include <config.h>
#include <codecs.h>
#include <avdec_private.h>

#define LOG_DOMAIN "faad2"

typedef struct
  {
  faacDecHandle dec;
  float * sample_buffer;
  int sample_buffer_size;

  uint8_t * data;
  uint8_t * data_ptr;
  int data_size;
  int data_alloc;
  
  gavl_audio_frame_t * frame;
  int last_block_size;
  } faad_priv_t;

static int get_data(bgav_stream_t * s)
  {
  faad_priv_t * priv;
  bgav_packet_t * p;
  int buffer_offset;
  
  priv = (faad_priv_t *)(s->data.audio.decoder->priv);

  
  p = bgav_demuxer_get_packet_read(s->demuxer, s);
  if(!p)
    return 0;
  
  if(priv->data_alloc < p->data_size + priv->data_size)
    {
    buffer_offset = priv->data_ptr - priv->data;
    
    priv->data_alloc = p->data_size + priv->data_size + 32;
    priv->data = realloc(priv->data, priv->data_alloc);
    priv->data_ptr = priv->data + buffer_offset;
    }

  if(priv->data_size)
    memmove(priv->data, priv->data_ptr, priv->data_size);
  priv->data_ptr = priv->data;
  
  memcpy(priv->data + priv->data_size, p->data, p->data_size);
  priv->data_size += p->data_size;
  bgav_demuxer_done_packet_read(s->demuxer, p);

  return 1;
  }

static struct
  {
  int faad_channel;
  gavl_channel_id_t gavl_channel;
  }
channels[] =
  {
    { FRONT_CHANNEL_CENTER,  GAVL_CHID_FRONT_CENTER },
    { FRONT_CHANNEL_LEFT,    GAVL_CHID_FRONT_LEFT },
    { FRONT_CHANNEL_RIGHT,   GAVL_CHID_FRONT_RIGHT },
    { SIDE_CHANNEL_LEFT,     GAVL_CHID_SIDE_LEFT },
    { SIDE_CHANNEL_RIGHT,    GAVL_CHID_SIDE_RIGHT },
    { BACK_CHANNEL_LEFT,     GAVL_CHID_REAR_LEFT },
    { BACK_CHANNEL_RIGHT,    GAVL_CHID_REAR_RIGHT },
    { BACK_CHANNEL_CENTER,   GAVL_CHID_REAR_CENTER },
    { LFE_CHANNEL,           GAVL_CHID_LFE },
    { UNKNOWN_CHANNEL,       GAVL_CHID_NONE }
  };

static gavl_channel_id_t get_channel(int ch)
  {
  int i;
  for(i = 0; i < sizeof(channels)/sizeof(channels[0]); i++)
    {
    if(channels[i].faad_channel == ch)
      return channels[i].gavl_channel;
    }
  return GAVL_CHID_AUX;
  }

static void set_channel_setup(faacDecFrameInfo * frame_info,
                              gavl_audio_format_t * format)
  {
  int i;
  for(i = 0; i < format->num_channels; i++)
    format->channel_locations[i] = get_channel(frame_info->channel_position[i]);
  }

static int decode_frame(bgav_stream_t * s)
  {
  faacDecFrameInfo frame_info;
  faad_priv_t * priv;
  
  priv = (faad_priv_t *)(s->data.audio.decoder->priv);

  memset(&frame_info, 0, sizeof(&frame_info));
  
  if(priv->data_size < FAAD_MIN_STREAMSIZE*GAVL_MAX_CHANNELS)
    if(!get_data(s))
      return 0;

  /*
   * Dirty trick: Frames from some mp4 files are randomly padded with
   * one byte, padding bits are (hopefully always) set to zero.
   * This enables playback of BigBounc1960_256kb.mp4
   */
#if 1
  if(*priv->data_ptr == 0x00)
    {
    priv->data_ptr++;
    priv->data_size--;
    }
#endif
  while(1)
    {
    priv->frame->samples.f = faacDecDecode(priv->dec,
                                           &frame_info,
                                           priv->data_ptr,
                                           priv->data_size);
    priv->data_ptr  += frame_info.bytesconsumed;
    priv->data_size -= frame_info.bytesconsumed;
    
    if(!priv->frame->samples.f)
      {
      if(frame_info.error == 14) /* Too little data */
        {
        if(!get_data(s))
          return 0;
        }
      else
        {
        bgav_log(s->opt, BGAV_LOG_ERROR, LOG_DOMAIN,
                 "faad2: faacDecDecode failed %s",
                faacDecGetErrorMessage(frame_info.error));
        //    priv->data_size = 0;
        //    priv->frame->valid_samples = 0;
        return 0; /* Recatching the stream is doomed to failure, so we end here */
        }
      }
    else
      break;
    }
  if(s->data.audio.format.channel_locations[0] == GAVL_CHID_NONE)
    {
    set_channel_setup(&frame_info,
                      &(s->data.audio.format));
    }
  if(!s->description)
    {
    switch(frame_info.object_type)
      {
      case MAIN:
        s->description = bgav_sprintf("%s", "AAC Main profile");
        break;
      case LC:
        s->description = bgav_sprintf("%s", "AAC Low Complexity profile (LC)");
        break;
      case SSR:
        s->description = bgav_sprintf("%s", "AAC Scalable Sample Rate profile (SSR)");
        break;
      case LTP:
        s->description = bgav_sprintf("%s", "AAC Long Term Prediction (LTP)");
        break;
      case HE_AAC:
        s->description = bgav_sprintf("%s", "HE-AAC");
        break;
      case ER_LC:
      case ER_LTP:
      case LD:
      case DRM_ER_LC: /* special object type for DRM */
        s->description = bgav_sprintf("%s", "MPEG_2/4 AAC");
        break;
      }
    }
  priv->frame->valid_samples = frame_info.samples  / s->data.audio.format.num_channels;
  priv->last_block_size = priv->frame->valid_samples;
  
  
  return 1;
  }

static int init_faad2(bgav_stream_t * s)
  {
  faad_priv_t * priv;
  unsigned long samplerate;
  unsigned char channels;
  char result;
  faacDecConfigurationPtr cfg;
  
  priv = calloc(1, sizeof(*priv));
  priv->dec = faacDecOpen();
  priv->frame = gavl_audio_frame_create(NULL);
  s->data.audio.decoder->priv = priv;
  
  /* Init the library using a DecoderSpecificInfo */


  if(!s->ext_size)
    {
    if(!get_data(s))
      return 0;

    result = faacDecInit(priv->dec, priv->data_ptr,
                         priv->data_size,
                         &samplerate, &channels);

    priv->data_size -= result;
    priv->data_ptr += result;
    }
  else
    {
    result = faacDecInit2(priv->dec, s->ext_data,
                          s->ext_size,
                          &samplerate, &channels);
    }

  /* Some mp4 files have a wrong samplerate in the sample description,
     so we correct it here */

  s->data.audio.format.samplerate = samplerate;
    
  s->data.audio.format.num_channels = channels;
  s->data.audio.format.sample_format = GAVL_SAMPLE_FLOAT;
  //  s->data.audio.format.sample_format = GAVL_SAMPLE_S16;
  s->data.audio.format.interleave_mode = GAVL_INTERLEAVE_ALL;
  s->data.audio.format.samples_per_frame = 1024;
    
  cfg = faacDecGetCurrentConfiguration(priv->dec);
  cfg->outputFormat = FAAD_FMT_FLOAT;
  // cfg->outputFormat = FAAD_FMT_16BIT;
  faacDecSetConfiguration(priv->dec, cfg);

  /* Decode a first frame to get the channel setup and the description */
  if(!decode_frame(s))
    return 0;
  
  
  return 1;
  }


// static int frame_number = 0;


static int decode_faad2(bgav_stream_t * s, gavl_audio_frame_t * f,
                        int num_samples)
  {
  faad_priv_t * priv;
  int samples_copied;
  int samples_decoded = 0;

  priv = (faad_priv_t *)(s->data.audio.decoder->priv);
  
  while(samples_decoded < num_samples)
    {
    if(!priv->frame->valid_samples)
      {
      if(!decode_frame(s))
        {
        if(f)
          f->valid_samples = samples_decoded;
        return samples_decoded;
        }
      }
    samples_copied = gavl_audio_frame_copy(&(s->data.audio.format),
                                           f,
                                           priv->frame,
                                           samples_decoded, /* out_pos */
                                           priv->last_block_size - priv->frame->valid_samples,  /* in_pos */
                                           num_samples - samples_decoded, /* out_size, */
                                           priv->frame->valid_samples /* in_size */);
    priv->frame->valid_samples -= samples_copied;
    samples_decoded += samples_copied;
    }
  if(f)
    f->valid_samples = samples_decoded;
  return samples_decoded;
  }

static void close_faad2(bgav_stream_t * s)
  {
  faad_priv_t * priv;
  priv = (faad_priv_t *)(s->data.audio.decoder->priv);
  if(priv->dec)
    faacDecClose(priv->dec);
  if(priv->data)
    free(priv->data);
  gavl_audio_frame_null(priv->frame);
  gavl_audio_frame_destroy(priv->frame);
  free(priv);
  }

static void resync_faad2(bgav_stream_t * s)
  {
  faad_priv_t * priv;
  priv = (faad_priv_t *)(s->data.audio.decoder->priv);
  priv->frame->valid_samples = 0;
  priv->sample_buffer_size = 0;
  priv->data_size = 0;
  
  }

static bgav_audio_decoder_t decoder =
  {
    name:   "FAAD AAC audio decoder",
    fourccs: (uint32_t[]){ BGAV_MK_FOURCC('m','p','4','a'),
                           BGAV_MK_FOURCC('a','a','c',' '),
                           BGAV_MK_FOURCC('A','A','C',' '),
                           BGAV_MK_FOURCC('A','A','C','P'),
                      0x0 },
    
    init:   init_faad2,
    decode: decode_faad2,
    close:  close_faad2,
    resync:  resync_faad2
  };

void bgav_init_audio_decoders_faad2()
  {
  bgav_audio_decoder_register(&decoder);
  }

