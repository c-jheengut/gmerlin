/*****************************************************************
 
  player_ov.c
 
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
#include <stdio.h>
#include <stdlib.h>

#include <keycodes.h>

#include <player.h>
#include <playerprivate.h>

struct bg_player_ov_context_s
  {
  bg_plugin_handle_t * plugin_handle;
  bg_ov_plugin_t     * plugin;
  void               * priv;
  bg_player_t        * player;
  bg_ov_callbacks_t callbacks;
  gavl_video_frame_t * frame;
  gavl_time_t frame_time;

  gavl_video_frame_t * still_frame;
  pthread_mutex_t     still_mutex;
  
  const char * error_msg;
  int still_shown;

  gavl_overlay_t       current_subtitle;
  gavl_overlay_t     * next_subtitle;
  int subtitle_id; /* Stream id for subtitles in the output plugin */
  int has_subtitle;

  bg_osd_t * osd;
  int osd_id;
  gavl_overlay_t * osd_ovl;
  
  bg_msg_queue_t * msg_queue;
  };

/* Callback functions */

static void key_callback(void * data, int key, int mask)
  {
  bg_player_ov_context_t * ctx = (bg_player_ov_context_t*)data;
  //  fprintf(stderr, "Key callback %d, 0x%02x\n", key, mask);

  switch(key)
    {
    /* Take the keys, we can handle from here */
    case BG_KEY_LEFT:
      if(mask == BG_KEY_SHIFT_MASK)
        bg_player_set_volume_rel(ctx->player,  -1.0);
      else if(mask == BG_KEY_CONTROL_MASK)
        bg_player_seek_rel(ctx->player,   -2 * GAVL_TIME_SCALE );
      break;
    case BG_KEY_RIGHT:
      if(mask == BG_KEY_SHIFT_MASK)
        bg_player_set_volume_rel(ctx->player,  1.0);
      else if(mask == BG_KEY_CONTROL_MASK)
        bg_player_seek_rel(ctx->player,   2 * GAVL_TIME_SCALE );
      break;
    case BG_KEY_0:
      bg_player_seek(ctx->player, 0 );
      break;
    case BG_KEY_SPACE:
      bg_player_pause(ctx->player);
      break;
    default: /* Broadcast this */
      bg_player_key_pressed(ctx->player, key, mask);
      break;
    }

  }

static void button_callback(void * data, int x, int y, int button, int mask)
  {
  bg_player_ov_context_t * ctx = (bg_player_ov_context_t*)data;

  if(button == 4)
    bg_player_seek_rel(ctx->player, 2 * GAVL_TIME_SCALE );
  else if(button == 5)
    bg_player_seek_rel(ctx->player, - 2 * GAVL_TIME_SCALE );
  
  //  fprintf(stderr, "Button callback %d %d (Button %d)\n", x, y, button);
  }

static void brightness_callback(void * data, float val)
  {
  bg_player_ov_context_t * ctx = (bg_player_ov_context_t*)data;
  //  fprintf(stderr, "Brightness callback %f\n", val);
  bg_osd_set_brightness_changed(ctx->osd, val, ctx->frame_time);
  }

static void saturation_callback(void * data, float val)
  {
  bg_player_ov_context_t * ctx = (bg_player_ov_context_t*)data;
  //  fprintf(stderr, "Saturation callback %f\n", val);
  bg_osd_set_saturation_changed(ctx->osd, val, ctx->frame_time);
  }

static void contrast_callback(void * data, float val)
  {
  bg_player_ov_context_t * ctx = (bg_player_ov_context_t*)data;
  //  fprintf(stderr, "Contrast callback %f\n", val);
  bg_osd_set_contrast_changed(ctx->osd, val, ctx->frame_time);
  }

static void handle_messages(bg_player_ov_context_t * ctx, gavl_time_t time)
  {
  bg_msg_t * msg;
  int id;
  float arg_f;
  
  while((msg = bg_msg_queue_try_lock_read(ctx->msg_queue)))
    {
    id = bg_msg_get_id(msg);
    switch(id)
      {
      case BG_PLAYER_MSG_VOLUME_CHANGED:
        arg_f = bg_msg_get_arg_float(msg, 0);
        bg_osd_set_volume_changed(ctx->osd, (arg_f - BG_PLAYER_VOLUME_MIN)/(-BG_PLAYER_VOLUME_MIN),
                                  time);
        break;
      default:
        break;
      }

    bg_msg_queue_unlock_read(ctx->msg_queue);
    
    }
  }

int  bg_player_ov_has_plugin(bg_player_ov_context_t * ctx)
  {
  return (ctx->plugin_handle ? 1 : 0);
  }

/* Create frame */

void * bg_player_ov_create_frame(void * data)
  {
  gavl_video_frame_t * ret;
  bg_player_ov_context_t * ctx;
  ctx = (bg_player_ov_context_t *)data;

  if(ctx->plugin->alloc_frame)
    {
    bg_plugin_lock(ctx->plugin_handle);
    ret = ctx->plugin->alloc_frame(ctx->priv);
    bg_plugin_unlock(ctx->plugin_handle);
    }
  else
    ret = gavl_video_frame_create(&(ctx->player->video_stream.output_format));

  //  fprintf(stderr, "gavl_video_frame_clear %d %d %d\n", ret->strides[0], ret->strides[1], ret->strides[2]);
  //  gavl_video_format_dump(&(ctx->player->video_stream.output_format));
  gavl_video_frame_clear(ret, &(ctx->player->video_stream.output_format));
  
  return (void*)ret;
  }

void bg_player_ov_destroy_frame(void * data, void * frame)
  {
  bg_player_ov_context_t * ctx;
  ctx = (bg_player_ov_context_t *)data;
  
  if(ctx->plugin->free_frame)
    {
    bg_plugin_lock(ctx->plugin_handle);
    ctx->plugin->free_frame(ctx->priv, (gavl_video_frame_t*)frame);
    bg_plugin_unlock(ctx->plugin_handle);
    }
  else
    gavl_video_frame_destroy((gavl_video_frame_t*)frame);
  }

void bg_player_ov_create(bg_player_t * player)
  {
  bg_player_ov_context_t * ctx;
  ctx = calloc(1, sizeof(*ctx));
  ctx->player = player;

  ctx->msg_queue = bg_msg_queue_create();
  
  pthread_mutex_init(&(ctx->still_mutex),(pthread_mutexattr_t *)0);
  
  /* Load output plugin */
  
  ctx->callbacks.key_callback    = key_callback;
  ctx->callbacks.button_callback = button_callback;

  ctx->callbacks.brightness_callback = brightness_callback;
  ctx->callbacks.saturation_callback = saturation_callback;
  ctx->callbacks.contrast_callback   = contrast_callback;
  
  ctx->callbacks.data = ctx;
  player->ov_context = ctx;

  ctx->osd = bg_osd_create();
  }

void bg_player_ov_standby(bg_player_ov_context_t * ctx)
  {
  //  fprintf(stderr, "bg_player_ov_standby\n");
  
  if(!ctx->plugin_handle)
    return;

  bg_plugin_lock(ctx->plugin_handle);

  if(ctx->plugin->show_window)
    ctx->plugin->show_window(ctx->priv, 0);
  bg_plugin_unlock(ctx->plugin_handle);
  }


void bg_player_ov_set_plugin(bg_player_t * player, bg_plugin_handle_t * handle)
  {
  bg_player_ov_context_t * ctx;

  ctx = player->ov_context;

  if(ctx->plugin_handle)
    bg_plugin_unref(ctx->plugin_handle);
  

  ctx->plugin_handle = handle;


  ctx->plugin = (bg_ov_plugin_t*)(ctx->plugin_handle->plugin);
  ctx->priv = ctx->plugin_handle->priv;

  bg_plugin_lock(ctx->plugin_handle);
  if(ctx->plugin->set_callbacks)
    ctx->plugin->set_callbacks(ctx->priv, &(ctx->callbacks));
  bg_plugin_unlock(ctx->plugin_handle);

  }

void bg_player_ov_destroy(bg_player_t * player)
  {
  bg_player_ov_context_t * ctx;
  
  ctx = player->ov_context;
  
  if(ctx->plugin_handle)
    bg_plugin_unref(ctx->plugin_handle);
  bg_osd_destroy(ctx->osd);

  bg_msg_queue_destroy(ctx->msg_queue);
  
  free(ctx);
  }

int bg_player_ov_init(bg_player_ov_context_t * ctx)
  {
  gavl_video_format_t osd_format;
  int result;

  ctx->next_subtitle    = (gavl_overlay_t*)0;
  ctx->has_subtitle = 0;
  
  gavl_video_format_copy(&(ctx->player->video_stream.output_format),
                         &(ctx->player->video_stream.input_format));

  bg_plugin_lock(ctx->plugin_handle);
  result = ctx->plugin->open(ctx->priv,
                             &(ctx->player->video_stream.output_format),
                             "Video output");
  if(result && ctx->plugin->show_window)
    ctx->plugin->show_window(ctx->priv, 1);
  else if(!result)
    {
    if(ctx->plugin->common.get_error)
      ctx->error_msg = ctx->plugin->common.get_error(ctx->priv);
    bg_plugin_unlock(ctx->plugin_handle);
    return result;
    }

  memset(&(osd_format), 0, sizeof(osd_format));
  
  bg_osd_init(ctx->osd, &(ctx->player->video_stream.output_format),
              &osd_format);

  ctx->osd_id = ctx->plugin->add_overlay_stream(ctx->priv,
                                                      &osd_format);
  ctx->osd_ovl = bg_osd_get_overlay(ctx->osd);
  
  bg_plugin_unlock(ctx->plugin_handle);
  return result;
  }

void bg_player_ov_update_still(bg_player_ov_context_t * ctx)
  {
  bg_fifo_state_t state;

  //  fprintf(stderr, "bg_player_ov_update_still\n");
  pthread_mutex_lock(&ctx->still_mutex);

  if(ctx->frame)
    bg_fifo_unlock_read(ctx->player->video_stream.fifo);
  //  fprintf(stderr, "bg_player_ov_update_still 1\n");

  ctx->frame = bg_fifo_lock_read(ctx->player->video_stream.fifo, &state);
  //  fprintf(stderr, "bg_player_ov_update_still 2 state: %d\n", state);

  if(!ctx->still_frame)
    {
    //      fprintf(stderr, "create_frame....");
    ctx->still_frame = bg_player_ov_create_frame(ctx);
    //      fprintf(stderr, "done\n");
    }
  
  if(ctx->frame)
    {
    gavl_video_frame_copy(&(ctx->player->video_stream.output_format),
                          ctx->still_frame, ctx->frame);
    //      fprintf(stderr, "Unlock read....%p", ctx->frame);
    bg_fifo_unlock_read(ctx->player->video_stream.fifo);
    ctx->frame = (gavl_video_frame_t*)0;
    //      fprintf(stderr, "Done\n");
    }
  else
    {
    //    fprintf(stderr, "update_still: Got no frame\n");
    gavl_video_frame_clear(ctx->still_frame, &(ctx->player->video_stream.output_format));
    }
  
  bg_plugin_lock(ctx->plugin_handle);
  ctx->plugin->put_still(ctx->priv, ctx->still_frame);
  bg_plugin_unlock(ctx->plugin_handle);

  
  pthread_mutex_unlock(&ctx->still_mutex);
  }

void bg_player_ov_cleanup(bg_player_ov_context_t * ctx)
  {
  pthread_mutex_lock(&ctx->still_mutex);
  if(ctx->still_frame)
    {
    //      fprintf(stderr, "Destroy still frame...");
    bg_player_ov_destroy_frame(ctx, ctx->still_frame);
      //      fprintf(stderr, "done\n");
    ctx->still_frame = (gavl_video_frame_t*)0;
    }
  if(ctx->current_subtitle.frame)
    {
    gavl_video_frame_destroy(ctx->current_subtitle.frame);
    ctx->current_subtitle.frame = (gavl_video_frame_t*)0;
    }
  
  pthread_mutex_unlock(&ctx->still_mutex);

  bg_plugin_lock(ctx->plugin_handle);
  ctx->plugin->close(ctx->priv);
  bg_plugin_unlock(ctx->plugin_handle);
  }

void bg_player_ov_reset(bg_player_t * player)
  {
  bg_player_ov_context_t * ctx;
  ctx = player->ov_context;

  if(ctx->has_subtitle)
    {
    ctx->plugin->set_overlay(ctx->priv, ctx->subtitle_id, (gavl_overlay_t*)0);
    ctx->has_subtitle = 0;
    }
  
  ctx->next_subtitle = (gavl_overlay_t*)0;
  //  fprintf(stderr, "Resetting subtitles\n");
  }

/* Set this extra because we must initialize subtitles after the video output */
void bg_player_ov_set_subtitle_format(void * data, const gavl_video_format_t * format)
  {
  bg_player_ov_context_t * ctx;
  ctx = (bg_player_ov_context_t*)data;

  /* Add subtitle stream for plugin */
  
  ctx->subtitle_id = ctx->plugin->add_overlay_stream(ctx->priv, format);

  /* Allocate private overlay frame */
  ctx->current_subtitle.frame = gavl_video_frame_create(format);

  //  fprintf(stderr, "bg_player_ov_set_subtitle_format\n");
  }


const char * bg_player_ov_get_error(bg_player_ov_context_t * ctx)
  {
  return ctx->error_msg;
  }

static void ping_func(void * data)
  {
  bg_player_ov_context_t * ctx;
  ctx = (bg_player_ov_context_t*)data;

  //  fprintf(stderr, "Ping func\n");  
  
  pthread_mutex_lock(&ctx->still_mutex);
  
  if(!ctx->still_shown)
    {
    if(ctx->frame)
      {
      //      fprintf(stderr, "create_frame....");
      ctx->still_frame = bg_player_ov_create_frame(data);
      //      fprintf(stderr, "done\n");
      
      gavl_video_frame_copy(&(ctx->player->video_stream.output_format),
                            ctx->still_frame, ctx->frame);
      //      fprintf(stderr, "Unlock read....%p", ctx->frame);
      bg_fifo_unlock_read(ctx->player->video_stream.fifo);
      ctx->frame = (gavl_video_frame_t*)0;
      //      fprintf(stderr, "Done\n");
      
      //      fprintf(stderr, "Put still...");
      bg_plugin_lock(ctx->plugin_handle);
      
      ctx->plugin->put_still(ctx->priv, ctx->still_frame);
      bg_plugin_unlock(ctx->plugin_handle);
      //      fprintf(stderr, "Done\n");
      }
    
    ctx->still_shown = 1;
    }
  //  fprintf(stderr, "handle_events...");
  bg_plugin_lock(ctx->plugin_handle);
  ctx->plugin->handle_events(ctx->priv);
  bg_plugin_unlock(ctx->plugin_handle);
  //  fprintf(stderr, "Done\n");
  
  pthread_mutex_unlock(&ctx->still_mutex);
  }

void * bg_player_ov_thread(void * data)
  {
  gavl_overlay_t tmp_overlay;
  
  bg_player_ov_context_t * ctx;
  gavl_time_t diff_time;
  gavl_time_t current_time;
  bg_fifo_state_t state;
  
  ctx = (bg_player_ov_context_t*)data;

  bg_player_add_message_queue(ctx->player,
                              ctx->msg_queue);

  
  //  fprintf(stderr, "Starting ov thread\n");

  while(1)
    {
    if(!bg_player_keep_going(ctx->player, ping_func, ctx))
      {
      //      fprintf(stderr, "bg_player_keep_going returned 0\n");
      break;
      }
    if(ctx->frame)
      {
      //      fprintf(stderr, "Unlock_read %p...", ctx->frame);
      bg_fifo_unlock_read(ctx->player->video_stream.fifo);
      //      fprintf(stderr, "done\n");
      
      ctx->frame = (gavl_video_frame_t*)0;
      }

    pthread_mutex_lock(&ctx->still_mutex);
    if(ctx->still_frame)
      {
      //      fprintf(stderr, "Destroy still frame...");
      bg_player_ov_destroy_frame(data, ctx->still_frame);
      //      fprintf(stderr, "done\n");
      ctx->still_frame = (gavl_video_frame_t*)0;
      }
    pthread_mutex_unlock(&ctx->still_mutex);
    
    ctx->still_shown = 0;

    //    fprintf(stderr, "Lock read\n");
    ctx->frame = bg_fifo_lock_read(ctx->player->video_stream.fifo, &state);
    //    fprintf(stderr, "Lock read done %p\n", ctx->frame);
    if(!ctx->frame)
      {
      //      fprintf(stderr, "Got no frame\n");
      break;
      }

    /* Get frame time */
    ctx->frame_time = gavl_time_unscale(ctx->player->video_stream.output_format.timescale,
                                        ctx->frame->time_scaled);

    //    fprintf(stderr, "OV: Frame time: %lld\n", ctx->frame_time);
    
    /* Subtitle handling */
    if(ctx->player->do_subtitle_text || ctx->player->do_subtitle_overlay)
      {

      /* Try to get next subtitle */
      if(!ctx->next_subtitle)
        {
        ctx->next_subtitle = bg_fifo_try_lock_read(ctx->player->subtitle_stream.fifo,
                                                  &state);
        }
      //      fprintf(stderr, "Subtitle %p\n", ctx->next_subtitle);
      /* Check if the overlay is expired */
      if(ctx->has_subtitle)
        {
        if(bg_overlay_too_old(ctx->frame_time,
                              ctx->current_subtitle.frame->time_scaled,
                              ctx->current_subtitle.frame->duration_scaled))
          {
          ctx->plugin->set_overlay(ctx->priv, ctx->subtitle_id, (gavl_overlay_t*)0);
#if 0
          fprintf(stderr, "Overlay expired (%f > %f + %f)\n",
                  gavl_time_to_seconds(ctx->frame_time),
                  gavl_time_to_seconds(ctx->current_subtitle.frame->time_scaled),
                  gavl_time_to_seconds(ctx->current_subtitle.frame->duration_scaled));
#endif
          ctx->has_subtitle = 0;
          }
        }
      
      /* Check if new overlay should be used */
      
      if(ctx->next_subtitle)
        {
        if(!bg_overlay_too_new(ctx->frame_time,
                               ctx->next_subtitle->frame->time_scaled))
          {
          memcpy(&tmp_overlay, ctx->next_subtitle, sizeof(tmp_overlay));
          memcpy(ctx->next_subtitle, &(ctx->current_subtitle),
                 sizeof(tmp_overlay));
          memcpy(&(ctx->current_subtitle), &tmp_overlay, sizeof(tmp_overlay));
          ctx->plugin->set_overlay(ctx->priv, ctx->subtitle_id,
                                   &(ctx->current_subtitle));
          
          //          fprintf(stderr, "Using New Overlay\n");
          ctx->has_subtitle = 1;
          ctx->next_subtitle = (gavl_overlay_t*)0;
          bg_fifo_unlock_read(ctx->player->subtitle_stream.fifo);
          }
#if 0
        else
          fprintf(stderr, "Not using new overlay: %f, %f\n",
                  gavl_time_to_seconds(ctx->frame_time),
                  gavl_time_to_seconds(ctx->next_subtitle->frame->time_scaled));
#endif
        }
      }
    /* Handle message */
    handle_messages(ctx, ctx->frame_time);
    
    /* Display OSD */

    if(bg_osd_overlay_valid(ctx->osd, ctx->frame_time))
      ctx->plugin->set_overlay(ctx->priv, ctx->osd_id, ctx->osd_ovl);
    else
      ctx->plugin->set_overlay(ctx->priv, ctx->osd_id, (gavl_overlay_t*)0);
    
    /* Check Timing */
    bg_player_time_get(ctx->player, 1, &current_time);
    
#if 0
    fprintf(stderr, "F: %f, C: %f\n",
            gavl_time_to_seconds(ctx->frame_time),
            gavl_time_to_seconds(current_time));
#endif

    diff_time =  ctx->frame_time - current_time;
    
    /* Wait until we can display the frame */
    if(diff_time > 0)
      gavl_time_delay(&diff_time);
    
    /* TODO: Drop frames */
    else if(diff_time < -100000)
      {
      //        fprintf(stderr, "Warning, frame dropping not yet implemented\n");
      }
    
    //    fprintf(stderr, "Frame time: %lld\n", frame->time);
    bg_plugin_lock(ctx->plugin_handle);
    //    fprintf(stderr, "Put video\n");
    ctx->plugin->put_video(ctx->priv, ctx->frame);
    ctx->plugin->handle_events(ctx->priv);
    
    bg_plugin_unlock(ctx->plugin_handle);
    }

  bg_player_delete_message_queue(ctx->player,
                              ctx->msg_queue);

  
  //  fprintf(stderr, "ov thread finisheded\n");
  return NULL;
  }

void * bg_player_ov_still_thread(void *data)
  {
  bg_player_ov_context_t * ctx;
  gavl_time_t delay_time = gavl_seconds_to_time(0.02);
  
  ctx = (bg_player_ov_context_t*)data;
  
  /* Put the image into the window once and handle only events thereafter */
  //  fprintf(stderr, "Starting still loop\n");

  ctx->still_shown = 0;
  while(1)
    {
    if(!bg_player_keep_going(ctx->player, NULL, NULL))
      {
      //      fprintf(stderr, "bg_player_keep_going returned 0\n");
      break;
      }
    if(!ctx->still_shown)
      {
      bg_player_ov_update_still(ctx);
      ctx->still_shown = 1;
      }
    //    fprintf(stderr, "Handle events...");
    bg_plugin_lock(ctx->plugin_handle);
    ctx->plugin->handle_events(ctx->priv);
    bg_plugin_unlock(ctx->plugin_handle);
    //    fprintf(stderr, "Handle events done\n");
    gavl_time_delay(&delay_time);
    }
  //  fprintf(stderr, "still thread finished\n");
  return NULL;
  }

bg_parameter_info_t * bg_player_get_osd_parameters(bg_player_t * p)
  {
  return bg_osd_get_parameters(p->ov_context->osd);
  }

void bg_player_set_osd_parameter(void * data, char * name, bg_parameter_value_t*val)
  {
  bg_player_t * p = (bg_player_t *)data;
  bg_osd_set_parameter(p->ov_context->osd, name, val);
  }
