/*****************************************************************
 
  e_lqt.c
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <string.h>
#include <plugin.h>
#include <utils.h>
#include <log.h>

#include "lqt_common.h"
#include "lqtgavl.h"

#define LOG_DOMAIN "e_lqt"

static bg_parameter_info_t stream_parameters[] = 
  {
    {
      name:      "codec",
      long_name: "Codec",
    },
  };

typedef struct
  {
  int max_riff_size;
  char * filename;
  quicktime_t * file;
  
  //  bg_parameter_info_t * parameters;

  char * audio_codec_name;
  char * video_codec_name;

  bg_parameter_info_t * audio_parameters;
  bg_parameter_info_t * video_parameters;
  
  lqt_file_type_t file_type;
  int make_streamable;
  
  int num_video_streams;
  int num_audio_streams;
    
  struct
    {
    gavl_audio_format_t format;
    lqt_codec_info_t ** codec_info;
    } * audio_streams;

  struct
    {
    gavl_video_format_t format;
    uint8_t ** rows;
    lqt_codec_info_t ** codec_info;
    } * video_streams;
  char * error_msg;
  } e_lqt_t;

static void * create_lqt()
  {
  e_lqt_t * ret = calloc(1, sizeof(*ret));
  return ret;
  }

static const char * get_error_lqt(void * priv)
  {
  e_lqt_t * lqt;
  lqt = (e_lqt_t*)priv;
  return lqt->error_msg;
  }

static struct
  {
  int type_mask;
  char * extension;
  }
extensions[] =
  {
    { LQT_FILE_QT  | LQT_FILE_QT_OLD,      ".mov" },
    { LQT_FILE_AVI | LQT_FILE_AVI_ODML, ".avi" },
    { LQT_FILE_MP4,                        ".mp4" },
    { LQT_FILE_M4A,                        ".m4a" },
  };

static const char * get_extension_lqt(void * data)
  {
  int i;
  e_lqt_t * e = (e_lqt_t*)data;

  for(i = 0; i < sizeof(extensions)/sizeof(extensions[0]); i++)
    {
    if(extensions[i].type_mask & e->file_type)
      return extensions[i].extension;
    }
  return extensions[0].extension; /* ".mov" */
  }

static int open_lqt(void * data, const char * filename,
                    bg_metadata_t * metadata)
  {
  char * track_string;
  e_lqt_t * e = (e_lqt_t*)data;
  
  if(e->make_streamable && !(e->file_type & (LQT_FILE_AVI|LQT_FILE_AVI_ODML)))
    e->filename = bg_sprintf("%s.tmp", filename);
  else
    e->filename = bg_strdup(e->filename, filename);

  e->file = lqt_open_write(e->filename, e->file_type);
  //  fprintf(stderr, "lqt_open_write %d\n", e->file_type);
  if(!e->file)
    {
    e->error_msg = bg_sprintf("Cannot open file %s", e->filename);
    return 0;
    }

  if(e->file_type == LQT_FILE_AVI_ODML)
    lqt_set_max_riff_size(e->file, e->max_riff_size);
    
  /* Set metadata */

  if(metadata->copyright)
    quicktime_set_copyright(e->file, metadata->copyright);
  if(metadata->title)
    quicktime_set_name(e->file, metadata->title);

  if(metadata->comment)
    lqt_set_comment(e->file, metadata->comment);
  if(metadata->artist)
    lqt_set_artist(e->file, metadata->artist);
  if(metadata->genre)
    lqt_set_genre(e->file, metadata->genre);
  if(metadata->track)
    {
    track_string = bg_sprintf("%d", metadata->track);
    lqt_set_track(e->file, track_string);
    free(track_string);
    }
  if(metadata->album)
    lqt_set_album(e->file, metadata->album);
  if(metadata->author)
    lqt_set_author(e->file, metadata->author);
  return 1;
  }

static int add_audio_stream_lqt(void * data, gavl_audio_format_t * format)
  {
  e_lqt_t * e = (e_lqt_t*)data;

  e->audio_streams =
    realloc(e->audio_streams,
            (e->num_audio_streams+1)*sizeof(*(e->audio_streams)));
  memset(&(e->audio_streams[e->num_audio_streams]), 0,
         sizeof(*(e->audio_streams)));
  gavl_audio_format_copy(&(e->audio_streams[e->num_audio_streams].format),
                         format);
  
  e->num_audio_streams++;
  return e->num_audio_streams-1;
  }

static int add_video_stream_lqt(void * data, gavl_video_format_t* format)
  {
  e_lqt_t * e = (e_lqt_t*)data;

  e->video_streams =
    realloc(e->video_streams,
            (e->num_video_streams+1)*sizeof(*(e->video_streams)));
  memset(&(e->video_streams[e->num_video_streams]), 0,
         sizeof(*(e->video_streams)));
  
  gavl_video_format_copy(&(e->video_streams[e->num_video_streams].format),
                         format);

  /* AVIs are made with constant framerates only */
  if((e->file_type & (LQT_FILE_AVI|LQT_FILE_AVI_ODML)))
    e->video_streams[e->num_video_streams].format.framerate_mode = GAVL_FRAMERATE_CONSTANT;
  
  e->num_video_streams++;
  return e->num_video_streams-1;
  }

static void get_audio_format_lqt(void * data, int stream, gavl_audio_format_t * ret)
  {
  e_lqt_t * e = (e_lqt_t*)data;

  gavl_audio_format_copy(ret, &(e->audio_streams[stream].format));
  
  }
  
static void get_video_format_lqt(void * data, int stream, gavl_video_format_t * ret)
  {
  e_lqt_t * e = (e_lqt_t*)data;
  
  gavl_video_format_copy(ret, &(e->video_streams[stream].format));
  }

static int start_lqt(void * data)
  {
  int i;
  e_lqt_t * e = (e_lqt_t*)data;

  for(i = 0; i < e->num_audio_streams; i++)
    {
    lqt_gavl_get_audio_format(e->file,
                              i,
                              &(e->audio_streams[i].format));
    }
  for(i = 0; i < e->num_video_streams; i++)
    {
    lqt_gavl_get_video_format(e->file,
                              i,
                              &(e->video_streams[i].format));
    }
  return 1;
  }


static void write_audio_frame_lqt(void * data, gavl_audio_frame_t* frame,
                                  int stream)
  {
  e_lqt_t * e = (e_lqt_t*)data;

  lqt_encode_audio_raw(e->file, frame->samples.s_8, frame->valid_samples, stream);
  
  }

static void write_video_frame_lqt(void * data, gavl_video_frame_t* frame,
                                  int stream)
  {
  e_lqt_t * e = (e_lqt_t*)data;

  lqt_gavl_encode_video(e->file, stream, frame, e->video_streams[stream].rows);
  }

static void close_lqt(void * data, int do_delete)
  {
  int i;
  char * filename_final, *pos;
  e_lqt_t * e = (e_lqt_t*)data;

  if(!e->file)
    return;

  //  fprintf(stderr, "close_lqt\n");
  
  quicktime_close(e->file);
  e->file = (quicktime_t*)0;
  
  if(do_delete)
    remove(e->filename);

  else if(e->make_streamable && !(e->file_type & (LQT_FILE_AVI|LQT_FILE_AVI_ODML)))
    {
    filename_final = bg_strdup((char*)0, e->filename);
    pos = strrchr(filename_final, '.');
    *pos = '\0';
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Making streamable....");
    quicktime_make_streamable(e->filename, filename_final);
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Making streamable....done");
    remove(e->filename);
    free(filename_final);
    }
  
  if(e->filename)
    {
    free(e->filename);
    e->filename = (char*)0;
    }
  if(e->audio_streams)
    {
    for(i = 0; i < e->num_audio_streams; i++)
      {
      if(e->audio_streams[i].codec_info)
        lqt_destroy_codec_info(e->audio_streams[i].codec_info);
      }

    free(e->audio_streams);
    e->audio_streams = NULL;
    }
  if(e->video_streams)
    {
    for(i = 0; i < e->num_video_streams; i++)
      {
      if(e->video_streams[i].codec_info)
        lqt_destroy_codec_info(e->video_streams[i].codec_info);
      lqt_gavl_rows_destroy(e->video_streams[i].rows);
      }
    
    free(e->video_streams);
    e->video_streams = NULL;
    }
  e->num_audio_streams = 0;
  e->num_video_streams = 0;
  
  }

static void destroy_lqt(void * data)
  {
  e_lqt_t * e = (e_lqt_t*)data;

  close_lqt(data, 1);
  
  if(e->audio_parameters)
    bg_parameter_info_destroy_array(e->audio_parameters);
  if(e->video_parameters)
    bg_parameter_info_destroy_array(e->video_parameters);

  if(e->error_msg)
    free(e->error_msg);
  free(e);
  }

static void create_parameters(e_lqt_t * e)
  {
  e->audio_parameters = calloc(2, sizeof(*(e->audio_parameters)));
  e->video_parameters = calloc(2, sizeof(*(e->video_parameters)));

  bg_parameter_info_copy(&(e->audio_parameters[0]), &(stream_parameters[0]));
  bg_parameter_info_copy(&(e->video_parameters[0]), &(stream_parameters[0]));
  
  bg_lqt_create_codec_info(&(e->audio_parameters[0]),
                           1, 0, 1, 0);
  bg_lqt_create_codec_info(&(e->video_parameters[0]),
                           0, 1, 1, 0);
  
  }

static bg_parameter_info_t common_parameters[] =
  {
    {
      name:      "format",
      long_name: "Format",
      type:      BG_PARAMETER_STRINGLIST,
      multi_names:    (char*[]) { "quicktime", "avi", "avi_opendml",   "mp4", "m4a", (char*)0 },
      multi_labels:   (char*[]) { "Quicktime", "AVI", "AVI (Opendml)", "MP4", "M4A", (char*)0 },
      val_default: { val_str: "quicktime" },
    },
    {
      name:      "make_streamable",
      long_name: "Make streamable",
      type:      BG_PARAMETER_CHECKBUTTON,
      help_string: "Make the file streamable afterwards (uses twice the diskspace)",
    },
    {
      name:      "max_riff_size",
      long_name: "Maximum RIFF size",
      type:      BG_PARAMETER_INT,
      val_min:     { val_i: 1 },
      val_max:     { val_i: 1024 },
      val_default: { val_i: 1024 },
      help_string: "Maximum RIFF size (in MB) for OpenDML AVIs. The default (1GB) is reasonable and should only be changed by people who know what they do.",
    },
    { /* End of parameters */ }
  };

static bg_parameter_info_t * get_parameters_lqt(void * data)
  {
  return common_parameters;
  }

static void set_parameter_lqt(void * data, char * name,
                              bg_parameter_value_t * val)
  {
  e_lqt_t * e = (e_lqt_t*)data;
  if(!name)
    return;

  else if(!strcmp(name, "format"))
    {
    if(!strcmp(val->val_str, "quicktime"))
      e->file_type = LQT_FILE_QT;
    else if(!strcmp(val->val_str, "avi"))
      e->file_type = LQT_FILE_AVI;
    else if(!strcmp(val->val_str, "avi_opendml"))
      e->file_type = LQT_FILE_AVI_ODML;
    else if(!strcmp(val->val_str, "mp4"))
      e->file_type = LQT_FILE_MP4;
    else if(!strcmp(val->val_str, "m4a"))
      e->file_type = LQT_FILE_M4A;
    }
  else if(!strcmp(name, "make_streamable"))
    e->make_streamable = val->val_i;
  else if(!strcmp(name, "max_riff_size"))
    e->max_riff_size = val->val_i;
  
  }

static bg_parameter_info_t * get_audio_parameters_lqt(void * data)
  {
  e_lqt_t * e = (e_lqt_t*)data;
  
  if(!e->audio_parameters)
    create_parameters(e);
  
  return e->audio_parameters;
  }

static bg_parameter_info_t * get_video_parameters_lqt(void * data)
  {
  e_lqt_t * e = (e_lqt_t*)data;
  
  if(!e->video_parameters)
    create_parameters(e);
  
  return e->video_parameters;
  }

static void set_audio_parameter_lqt(void * data, int stream, char * name,
                                    bg_parameter_value_t * val)
  {
  e_lqt_t * e = (e_lqt_t*)data;
    
  if(!name)
    return;

  if(!strcmp(name, "codec"))
    {
    /* Now we can add the stream */

    e->audio_streams[stream].codec_info = lqt_find_audio_codec_by_name(val->val_str);
    
    lqt_gavl_add_audio_track(e->file, &e->audio_streams[stream].format,
                             *e->audio_streams[stream].codec_info);
    }
  else
    {
    //    fprintf(stderr, "set_audio_parameter_lqt %s\n", name);

    bg_lqt_set_audio_parameter(e->file,
                               stream,
                               name,
                               val,
                               e->audio_streams[stream].codec_info[0]->encoding_parameters);
      
    }

  

  }

static int set_video_pass_lqt(void * data, int stream, int pass,
                              int total_passes, const char * stats_file)
  {
  e_lqt_t * e = (e_lqt_t*)data;
  return lqt_set_video_pass(e->file, pass, total_passes, stats_file, stream);
  }

static void set_video_parameter_lqt(void * data, int stream, char * name,
                                    bg_parameter_value_t * val)
  {
  e_lqt_t * e = (e_lqt_t*)data;
  
  if(!name)
    return;

  if(!strcmp(name, "codec"))
    {
    /* Now we can add the stream */

    e->video_streams[stream].codec_info =
      lqt_find_video_codec_by_name(val->val_str);
    
    if(e->file_type == LQT_FILE_AVI)
      {
      //      e->video_streams[stream].format.image_width *=
      //        e->video_streams[stream].format.pixel_width;
      
      //      e->video_streams[stream].format.image_width /=
      //        e->video_streams[stream].format.pixel_height;

      e->video_streams[stream].format.pixel_width = 1;
      e->video_streams[stream].format.pixel_height = 1;

      //      e->video_streams[stream].format.frame_width =
      //        e->video_streams[stream].format.image_width;
      }
    
    lqt_gavl_add_video_track(e->file, &e->video_streams[stream].format,
                             *e->video_streams[stream].codec_info);
    
    /* Request constant framerate for AVI files */

    if(e->file_type == LQT_FILE_AVI)
      e->video_streams[stream].format.framerate_mode = GAVL_FRAMERATE_CONSTANT;
    
    e->video_streams[stream].rows = lqt_gavl_rows_create(e->file, stream);
    }
  else
    {
    //    fprintf(stderr, "set_video_parameter_lqt %s\n", name);

    bg_lqt_set_video_parameter(e->file,
                               stream,
                               name,
                               val,
                               e->video_streams[stream].codec_info[0]->encoding_parameters);

    }
  }


bg_encoder_plugin_t the_plugin =
  {
    common:
    {
      name:           "e_lqt",       /* Unique short name */
      long_name:      "Quicktime encoder",
      mimetypes:      NULL,
      extensions:     "mov",
      type:           BG_PLUGIN_ENCODER,
      flags:          BG_PLUGIN_FILE,
      priority:       BG_PLUGIN_PRIORITY_MAX,
      create:         create_lqt,
      destroy:        destroy_lqt,
      get_parameters: get_parameters_lqt,
      set_parameter:  set_parameter_lqt,
      get_error:      get_error_lqt,
    },

    max_audio_streams: -1,
    max_video_streams: -1,

    get_audio_parameters: get_audio_parameters_lqt,
    get_video_parameters: get_video_parameters_lqt,

    get_extension:        get_extension_lqt,
    
    open:                 open_lqt,

    add_audio_stream:     add_audio_stream_lqt,
    add_video_stream:     add_video_stream_lqt,
    set_video_pass:       set_video_pass_lqt,
    
    set_audio_parameter:  set_audio_parameter_lqt,
    set_video_parameter:  set_video_parameter_lqt,
    
    get_audio_format:     get_audio_format_lqt,
    get_video_format:     get_video_format_lqt,

    start:                start_lqt,
    
    write_audio_frame: write_audio_frame_lqt,
    write_video_frame: write_video_frame_lqt,
    close:             close_lqt,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
