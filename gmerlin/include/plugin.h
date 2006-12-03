/*****************************************************************
 
  plugin.h
 
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

#ifndef __BG_PLUGIN_H_
#define __BG_PLUGIN_H_

#include <gavl/gavl.h>
#include "parameter.h"
#include "streaminfo.h"

/** \defgroup plugin Plugins
 *  \brief Plugin types and associated functions
 *
 *  Gmerlin plugins are structs which contain function pointers and
 *  other data. The API looks a bit complicated, but many functions are
 *  optional, so plugins can, in prinpiple, be very simple.
 *  All plugins are based on a common struct (\ref bg_plugin_common_t),
 *  which contains an identifier for the plugin type. The bg_plugin_common_t
 *  pointer can be casted to the derived plugin type.
 *
 *  The application calls the functions in the order, in which they are
 *  defined. Some functions are mandatory from the plugin view (i.e. they
 *  must be non-null), some functions are mandatory for the application
 *  (i.e. the application must check for them and call them if they are
 *  present.
 *
 *  The configuration of the plugins works entirely through the
 *  parameter passing mechanisms (see \ref parameter). Configurable
 *  plugins only need to define get_parameters and set_parameter methods.
 *  Encoding plugins have an additional layer, which allows setting
 *  parameters individually for each stream.
 *  
 *  Events, which are caught by the plugins (e.g. song name change)
 *  are propagated through optional callbacks. Not all plugins support
 *  callbacks, not all applications use them.
 */



/** \defgroup plugin_flags Plugin flags
 *  \ingroup plugin
 *  \brief Macros for the plugin flags
 *
 *
 *  All plugins must have at least one flag set.
 *  @{
 */

#define BG_PLUGIN_REMOVABLE    (1<<0)  //!< Plugin handles removable media (CD, DVD etc.)
#define BG_PLUGIN_FILE         (1<<1)  //!< Plugin reads/writes files
#define BG_PLUGIN_RECORDER     (1<<2)  //!< Plugin does hardware recording

#define BG_PLUGIN_URL          (1<<3)  //!< Plugin can load URLs
#define BG_PLUGIN_PLAYBACK     (1<<4)  //!< Plugin is an audio or video driver for playback

#define BG_PLUGIN_BYPASS       (1<<5)  //!< Plugin can send A/V data directly to the output bypassing the player engine

#define BG_PLUGIN_KEEP_RUNNING (1<<6) //!< Plugin should not be stopped and restarted if tracks change

#define BG_PLUGIN_INPUT_HAS_SYNC (1<<7) //!< For input plugins in bypass mode: Plugin will set the time via callback

#define BG_PLUGIN_STDIN         (1<<8)  //!< Plugin can read from stdin ("-")

#define BG_PLUGIN_ALL 0xFFFFFFFF //!< Mask of all possible plugin flags

/** @}
 */



#define BG_PLUGIN_API_VERSION 7

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */

#define BG_GET_PLUGIN_API_VERSION \
  extern int get_plugin_api_version(); \
  extern int get_plugin_api_version() { return BG_PLUGIN_API_VERSION; }

#define BG_PLUGIN_PRIORITY_MIN 1
#define BG_PLUGIN_PRIORITY_MAX 10

/** \defgroup plugin_i Media input
 *  \ingroup plugin
 *  \brief Media input
 */ 

/** \ingroup plugin_i
 *  \brief Stream actions
 *
 *  These describe how streams should be handled by the input
 *  plugin. Note that by default, each stream is switched off.
 */

typedef enum
  {
    BG_STREAM_ACTION_OFF = 0, //!< Stream is switched off
    BG_STREAM_ACTION_DECODE,  //!< Stream is switched on and will be decoded
    
    /*
     */
    
    BG_STREAM_ACTION_BYPASS, //!< A/V data will bypass the player. It will only be chosen if the BG_PLUGIN_BYPASS flag (see below) is present. Currently, this is used only for Audio-CD playback to the soundcard.
    
    /*
     *  Future support for compressed frames
     *  must go here
     */

    /* BG_STREAM_ACTION_READRAW */
    
  } bg_stream_action_t;

/***************************************************
 * Plugin API
 *
 * Plugin dlls contain a symbol "the_plugin",
 * which points to one of the structures below.
 * The member functions are described below.
 *
 ***************************************************/

/*
 * Plugin types
 */

/** \ingroup plugin
 *  \brief Plugin types
 */

typedef enum
  {
    BG_PLUGIN_NONE                       = 0,      //!< None or undefined
    BG_PLUGIN_INPUT                      = (1<<0), //!< Media input
    BG_PLUGIN_OUTPUT_AUDIO               = (1<<1), //!< Audio output
    BG_PLUGIN_OUTPUT_VIDEO               = (1<<2), //!< Video output
    BG_PLUGIN_RECORDER_AUDIO             = (1<<3), //!< Audio recorder
    BG_PLUGIN_RECORDER_VIDEO             = (1<<4), //!< Video recorder
    BG_PLUGIN_ENCODER_AUDIO              = (1<<5), //!< Encoder for audio only
    BG_PLUGIN_ENCODER_VIDEO              = (1<<6), //!< Encoder for video only
    BG_PLUGIN_ENCODER_SUBTITLE_TEXT      = (1<<7), //!< Encoder for text subtitles only
    BG_PLUGIN_ENCODER_SUBTITLE_OVERLAY   = (1<<8), //!< Encoder for overlay subtitles only
    BG_PLUGIN_ENCODER                    = (1<<9), //!< Encoder for multiple kinds of streams
    BG_PLUGIN_ENCODER_PP                 = (1<<10),//!< Encoder postprocessor (e.g. CD burner)
    BG_PLUGIN_IMAGE_READER               = (1<<11),//!< Image reader
    BG_PLUGIN_IMAGE_WRITER               = (1<<12) //!< Image writer
  } bg_plugin_type_t;


/** \ingroup plugin
 *  \brief Device description
 *
 *  The find_devices() function of a plugin returns
 *  a NULL terminated array of devices. It's used mainly for input plugins,
 *  which access multiple drives. For output plugins, devices are normal parameters.
 */

typedef struct
  {
  char * device; //!< String, which can be passed to the open() method
  char * name;   //!< More humanized description, might be NULL
  } bg_device_info_t;

/** \ingroup plugin
 *  \brief Append device info to an existing array and return the new array.
 *  \param arr An array (can be NULL)
 *  \param device Device string
 *  \param name Humanized description (can be NULL)
 *  \returns Newly allocated array. The source array is freed.
 *
 *  This is used mainly by the device detection routines of the plugins
 */

bg_device_info_t * bg_device_info_append(bg_device_info_t * arr,
                                         const char * device,
                                         const char * name);

/** \ingroup plugin
 *  \brief Free an array of device descriptions
 *  \param arr a device array
 */

void bg_device_info_destroy(bg_device_info_t * arr);

/* Common part */

/** \ingroup plugin
 *  \brief Base structure common to all plugins
 */

typedef struct bg_plugin_common_s
  {
  char             * name;       //!< Unique short name
  char             * long_name;  //!< Humanized name for GUI widgets
  char             * mimetypes;  //!< Mimetypes this plugin can handle (space separated)
  char             * extensions;  //!< File extensions this plugin can handle (space separated)
  bg_plugin_type_t type;  //!< Type
  int              flags;  //!< Flags (see defines)

  /*
   *  If there might be more than one plugin for the same
   *  job, there is a priority (0..10) which is used for the
   *  decision
   */
  
  int              priority; //!< Priority (between 1 and 10).
  
  /** \brief Create the instance, return handle.
   *  \returns A private handle, which is the first argument to all subsequent functions.
   */
  
  void * (*create)();
      
  /** \brief Destroy plugin instance
   *  \param priv The handle returned by the create() method
   *
   * Destroy everything, making it ready for dlclose()
   * This function might also be called on opened plugins,
   * so the plugins should call their close()-function from
   * within the destroy method.
   */

  void (*destroy)(void* priv);

  /** \brief Get available parameters
   *  \param priv The handle returned by the create() method
   *  \returns a NULL terminated parameter array.
   *
   *  The returned array is owned (an should be freed) by the plugin.
   */

  bg_parameter_info_t * (*get_parameters)(void * priv);

  /** \brief Set configuration parameter (optional)
   */
    
  bg_set_parameter_func_t set_parameter;

  /** \brief Get configuration parameter (optional)
   *
   *  This must only return parameters, which are changed internally
   *  by the plugins.
   */
  
  bg_get_parameter_func_t get_parameter;

  /** \brief Return a human readable description of the last error (optional)
   *  \param priv The handle returned by the create() method
   *  \returns the last error or NULL
   */
   
  const char * (*get_error)(void* priv);

  /** \brief Check, is a device can be opened by the plugin (optional)
   *  \param device The device as passed to the open() method
   *  \param name Returns the name if available
   *  \returns 1 if the device is supported, 0 else
   *
   *  The name should be set to NULL before this call, and must be freed
   *  if it's non-NULL after the call.
   */
  
  int (*check_device)(const char * device, char ** name);
  

  /** \brief Get an array of all supported devices found on the system
   *  \returns A NULL terminated device array
   *
   *  The returned array must be freed with \ref bg_device_info_destroy by
   *  the caller.
   */
  
  bg_device_info_t * (*find_devices)();
    
  } bg_plugin_common_t;

/*
 *  Plugin callbacks: Functions called by the
 *  plugin to reflect user input or other changes
 *  Applications might pass NULL callbacks,
 *  so plugins MUST check for valid callbacks structure
 *  before calling any of these functions
 */

/* Input plugin */

/** \ingroup plugin_i
 *  \brief Callbacks for input plugins
 *
 *  Passing the callback structure to the plugin is optional. Futhermore,
 *  any of the callback functions is optional (i.e. can be NULL). The plugin
 *  might use the callbacks for propagating events.
 */

typedef struct bg_input_callbacks_s
  {
  /** \brief Track changed
   *  \param data The data member of this bg_input_callbacks_s struct
   *  \param track The track number starting with 0
   *
   *  This is called by plugins, which support multiple tracks and can switch to a new track without
   *  closing/reopening (e.g. the audio-cd player)
   */
   void (*track_changed)(void * data, int track);

  /** \brief Time changed
   *  \param data The data member of this bg_input_callbacks_s struct
   *  \param time The current time
   *
   *  This is used only by plugins, which do playback without the player engine
   *  (currently only the audio-cd player) to update the displayed time. Normal plugins never call this.
   */
  
  void (*time_changed)(void * data, gavl_time_t time); //!< Called if the time changed (used only for plugins with the 
  
  /** \brief Duration changed
   *  \param data The data member of this bg_input_callbacks_s struct
   *  \param time The new duration
   */
  
  void (*duration_changed)(void * data, gavl_time_t duration);

  /** \brief Name changed
   *  \param data The data member of this bg_input_callbacks_s struct
   *  \param time The new name
   *
   *  This is for web-radio stations, which send song-names.
   */
  
  void (*name_changed)(void * data, const char * name);

  /** \brief Metadata changed
   *  \param data The data member of this bg_input_callbacks_s struct
   *  \param m The new metadata
   *
   *  This is for web-radio stations, which send metadata for each song.
   */
  
  void (*metadata_changed)(void * data, const bg_metadata_t * m);

  /** \brief Buffer callback
   *  \param data The data member of this bg_input_callbacks_s struct
   *  \param percentage The buffer fullness (0.0..1.0)
   *
   *  For network connections, plugins might buffer data. This takes some time,
   *  so they notify the API about the progress with this callback.
   */
  
  void (*buffer_notify)(void * data, float percentage);

  /** \brief Authentication callback
   *  \param data The data member of this bg_input_callbacks_s struct
   *  \param resource Name of the resource (e.g. server name)
   *  \param username Returns the username 
   *  \param password Returns the password
   *  \returns 1 if username and password are available, 0 else.
   *
   *  For sources, which require authentication (like FTP servers), this
   *  function is called by the plugin to get username and password.
   *  Username and password must be returned in newly allocated strings, which
   *  will be freed by the plugin.
   */

  int (*user_pass)(void * data, const char * resource,
                   char ** username, char ** password);
 
  void * data; //!< Application specific data passed as the first argument to all callbacks.
  
  } bg_input_callbacks_t;

/*************************************************
 * MEDIA INPUT
 *************************************************/

/** \ingroup plugin_i
 *  \brief Input plugin
 *
 *  This is for all kinds of media inputs (files, disks, urls, etc), except recording from
 *  hardware devices (see \ref plugin_ra and \ref plugin_rv).
 *
 *
 */

typedef struct bg_input_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types
  
  char * protocols; //!< Space separated list of protocols this plugin can handle

  /** \brief Set callbacks
   *  \param priv The handle returned by the create() method
   *  \param callbacks Callback structure initialized by the caller before
   *
   * Set callback functions, which will be called by the plugin.
   * Defining as well as calling this function is optional. Any of the
   * members of callbacks can be NULL.
   */
  
  void (*set_callbacks)(void * priv, bg_input_callbacks_t * callbacks);
  
  /** \brief Open file/url/device
   *  \param priv The handle returned by the create() method
   *  \param arg Filename, URL or device name
   *  \returns 1 on success, 0 on failure
   */
  int (*open)(void * priv, const char * arg);

  /** \brief Open plugin from filedescriptor (optional)
   *  \param priv The handle returned by the create() method
   *  \param fd Open filedescriptor
   *  \param total_bytes Totally available bytes or 0 if unknown
   *  \param mimetype Mimetype from http header (or NULL)
   *  \returns 1 on success, 0 on failure
   */

  int (*open_fd)(void * priv, int fd, int64_t total_bytes,
                 const char * mimetype);

  /** \brief Get the disc name (optional)
   *  \param priv The handle returned by the create() method
   *  \returns The name of the disc if any
   *
   *  This is only for plugins, which access removable discs (e.g. CDs).
   */
  
  const char * (*get_disc_name)(void * priv);

  /** \brief Eject disc (optional)
   *  \param priv The handle returned by the create() method
   *  \returns 1 if eject was successful
   *
   *  This is only for plugins, which access removable discs (e.g. CDs).
   *  \todo This function doesn't work reliably now. Either fix or remove this
   */
  
  int (*eject_disc)(const char * device);
  
  /** \brief Get the number of tracks
   *  \param priv The handle returned by the create() method
   *  \returns The number of tracks
   *
   *  This can be NULL for plugins, which support just one track.
   */
  
  int (*get_num_tracks)(void * priv);
  
  /** \brief Return information about a track
   *  \param priv The handle returned by the create() method
   *  \param track Track index starting with 0
   *  \returns The track info
   *
   *  The following fields MUST be valid after this call:
   *  - num_audio_streams
   *  - num_video_streams
   *  - num_subtitle_streams
   *  - duration
   *  - Name (If NULL, the filename minus the suffix will be used)
   *
   * Other data, especially audio and video formats, will become valid after the
   * start() call (see below).
   */
  
  bg_track_info_t * (*get_track_info)(void * priv, int track);

  /** \brief Set the track to be played
   *  \param priv The handle returned by the create() method
   *  \param track Track index starting with 0
   *
   *  This has to be defined only if the plugin supports multiple tracks.
   */
    
  int (*set_track)(void * priv, int track);
    
  /*
   *  These functions set the audio- video- and subpicture streams
   *  as well as programs (== DVD Angles). All these start with 0
   *
   *  Arguments for actions are defined in the enum bg_stream_action_t
   *  above. Plugins must return FALSE on failure (e.g. no such stream)
   *
   *  Functions must be defined only, if the corresponding stream
   *  type is supported by the plugin and can be switched.
   *  Single stream plugins can leave these NULL
   *  Gmerlin will never try to call these functions on nonexistent streams
   */

  /** \brief Setup audio stream
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index starting with 0
   *  \param action What to do with the stream
   *  \returns 1 on success, 0 on failure
   */
  
  int (*set_audio_stream)(void * priv, int stream, bg_stream_action_t action);

  /** \brief Setup video stream
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index starting with 0
   *  \param action What to do with the stream
   *  \returns 1 on success, 0 on failure
   */

  int (*set_video_stream)(void * priv, int stream, bg_stream_action_t action);

  /** \brief Setup still image stream
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index starting with 0
   *  \param action What to do with the stream
   *  \returns 1 on success, 0 on failure
   */

  int (*set_still_stream)(void * priv, int stream, bg_stream_action_t action);

  /** \brief Setup subtitle stream
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index starting with 0
   *  \param action What to do with the stream
   *  \returns 1 on success, 0 on failure
   */

  int (*set_subtitle_stream)(void * priv, int stream, bg_stream_action_t action);
  
  /** \brief Start decoding
   *  \param priv The handle returned by the create() method
   *  \returns 1 on success, 0 on error
   *   
   *  After this call, all remaining members of the track info returned earlier
   *  (especially audio- and video formats) must be valid.
   *
   *  From the plugins point of view, this is the last chance to return 0
   *  if something fails
   */
  
  int (*start)(void * priv);

  /** \brief Read audio samples
   *  \param priv The handle returned by the create() method
   *  \param frame The frame, where the samples will be copied
   *  \param stream Stream index starting with 0
   *  \param num_samples Number of samples
   *  \returns The number of decoded samples, 0 means EOF.
   *
   *  The num_samples argument can be larger than the samples_per_frame member of the
   *  video format. This means, that all audio decoding plugins must have an internal
   *  buffering mechanism.
   */
  
  int (*read_audio_samples)(void * priv, gavl_audio_frame_t* frame, int stream,
                            int num_samples);

  /** \brief Read a video frame
   *  \param priv The handle returned by the create() method
   *  \param frame The frame, where the image will be copied
   *  \param stream Stream index starting with 0
   *  \returns 1 if a frame was decoded, 0 means EOF.
   */
  
  int (*read_video_frame)(void * priv, gavl_video_frame_t* frame, int stream);

  /** \brief Query if a new subtitle is available
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index starting with 0
   *  \returns 1 if a subtitle is available, 0 else.
   */
  
  int (*has_subtitle)(void * priv, int stream);
    
  /** \brief Read one pixmap subtitle
   *  \param priv The handle returned by the create() method
   *  \param ovl Where the overlay will be copied
   *  \param stream Stream index starting with 0
   *  \returns 1 if a subtitle was decoded, 0 else
   *
   *  EOF in a graphical subtitle stream is reached if
   *  - has_subtitle() returned 1 and
   *  - read_subtitle_overlay() returned 0
   */
  
  int (*read_subtitle_overlay)(void * priv,
                               gavl_overlay_t*ovl, int stream);

  /** \brief Read one text subtitle
   *  \param priv The handle returned by the create() method
   *  \param text Where the text will be copied, the buffer will be realloc()ed.
   *  \param text_alloc Allocated bytes for text. Will be updated by the function.
   *  \param start_time Returns the start time of the subtitle
   *  \param duration Returns the duration of the subtitle
   *  \param stream Stream index starting with 0
   *  \returns 1 if a subtitle was decoded, 0 else
   *
   *  EOF in a text subtitle stream is reached if
   *  - has_subtitle() returned 1 and
   *  - read_subtitle_text() returned 0
   *
   *  This function automatically handles the text buffer (and text_alloc).
   *  Just set both to zero before the first call and free() the text buffer
   *  after the last call (if non-NULL).
   */
  
  int (*read_subtitle_text)(void * priv,
                            char ** text, int * text_alloc,
                            int64_t * start_time,
                            int64_t * duration, int stream);
  
  /* The following 3 functions are only meaningful for plugins, which
     have the BG_PLUGIN_BYPASS flag set. */

  /** \brief Update a plugin in bypass mode
   *  \param priv The handle returned by the create() method
   *  \returns 1 on success, 0 on error
   *
   *  For plugins in bypass mode, this function must be called
   *  periodically by the application, so the plugin can call the
   *  callbacks if something interesting happened.
   */
    
  int (*bypass)(void * priv);
  
  /** \brief pause a plugin in bypass mode
   *  \param priv The handle returned by the create() method
   *  \param pause 1 for pausing, 0 for resuming
   */
  
  void (*bypass_set_pause)(void * priv, int pause);

  /** \brief Set volume for a plugin in bypass mode
   *  \param priv The handle returned by the create() method
   *  \param volume Volume in dB (0 is maximum).
   *
   * This function is optional.
   */
  
  void (*bypass_set_volume)(void * priv, float volume);
    
  /** \brief Seek within a media track
   *  \param priv The handle returned by the create() method
   *  \param time Time to seek to
   *  
   *  Media streams are supposed to be seekable, if this
   *  function is non-NULL AND the duration field of the track info
   *  is > 0 AND the seekable flag in the track info is nonzero.
   *  The time argument might be changed to the correct value
   */

  void (*seek)(void * priv, gavl_time_t * time);

  /** \brief Stop playback
   *  \param priv The handle returned by the create() method
   *  
   * This is used for plugins in bypass mode to stop playback.
   * The plugin can be started again after
   */

  void (*stop)(void * priv);
  
  /** \brief Close plugin
   *  \param priv The handle returned by the create() method
   *
   *  Close the file/device/url.
   */

  void (*close)(void * priv);
  
  } bg_input_plugin_t;

/** \defgroup plugin_oa Audio output
 *  \ingroup plugin
 *  \brief Audio output
 */ 

/** \ingroup plugin_oa
 *  \brief Audio output plugin
 *
 *  This plugin type implements audio playback through a soundcard.
 */

typedef struct bg_oa_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types

  /** \brief Open plugin
   *  \param priv The handle returned by the create() method
   *  \param format The format of the media source
   *
   *  The format will be changed to the nearest format, which is supported
   *  by the plugin. To convert the source format to the output format,
   *  use a \ref gavl_audio_converter_t
   */

  int (*open)(void * priv, gavl_audio_format_t* format);

  /** \brief Start playback
   *  \param priv The handle returned by the create() method
   *
   *  Notify the plugin, that audio playback is about to begin.
   */

  int (*start)(void * priv);
    
  /** \brief Write audio samples
   *  \param priv The handle returned by the create() method
   *  \param frame The audio frame to write.
   */
  
  void (*write_frame)(void * priv, gavl_audio_frame_t* frame);

  /** \brief Get the number of buffered audio samples
   *  \param priv The handle returned by the create() method
   *  \returns The number of buffered samples (both soft- and hardware)
   *  
   *  This function is used for A/V synchronization with the soundcard. If this
   *  function is NULL, software synchronization will be used
   */

  int (*get_delay)(void * priv);
  
  /** \brief Stop playback
   *  \param priv The handle returned by the create() method
   *
   * Notify the plugin, that playback will stop. Playback can be starzed again with
   * start().
   */

  void (*stop)(void * priv);
    
  /** \brief Close plugin
   *  \param priv The handle returned by the create() method
   *
   * Close the plugin. After this call, the plugin can be opened with another format
   */
  
  void (*close)(void * priv);
  } bg_oa_plugin_t;

/*******************************************
 * AUDIO RECORDER
 *******************************************/

/** \defgroup plugin_ra Audio recorder
 *  \ingroup plugin
 *  \brief Audio recorder
 */ 

/** \ingroup plugin_ra
 *  \brief Audio recorder
 *
 *  Audio recording support from the soundcard
 */

typedef struct bg_ra_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types

  /** \brief Open plugin
   *  \param priv The handle returned by the create() method
   *  \param format The desired format
   *
   *  The format will be changed to the nearest format, which is supported
   *  by the plugin. To convert the source format to the output format,
   *  use a \ref gavl_audio_converter_t
   */
  
  int (*open)(void * priv, gavl_audio_format_t * format);

  /** \brief Read audio samples
   *  \param priv The handle returned by the create() method
   *  \param frame The frame where the samples will be copied
   *  \param num_samples The number of samples to read
   */
  
  void (*read_frame)(void * priv, gavl_audio_frame_t * frame,int num_samples);

  /** \brief Close plugin
   *  \param priv The handle returned by the create() method
   */
  
  void (*close)(void * priv);
  } bg_ra_plugin_t;

/*******************************************
 * VIDEO OUTPUT
 *******************************************/

/* Callbacks */

/** \defgroup plugin_ov Video output
 *  \ingroup plugin
 *  \brief Video output
 */ 

/** \ingroup plugin_ov
 * \brief Callbacks for the video output plugin
 *
 */

typedef struct bg_ov_callbacks_s
  {
  /** \brief Keyboard callback
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param key Key code (see \ref keycodes) 
   *  \param key Modifier mask (see \ref keycodes) 
   */
  
  void (*key_callback)(void * data, int key, int mask);

  /** \brief Mouse button callback
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param x Horizontal cursor position in image coordinates
   *  \param y Vertical cursor position in image coordinates
   *  \param button Number of the mouse button, which was pressed (starting with 1)
   *
   *
   */
  
  void (*button_callback)(void * data, int x, int y, int button, int mask);

  /** \brief Show/hide callback
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param show 1 if the window is shown now, 0 if it is hidden.
   */
   
  void (*show_window)(void * data, int show);

  /** \brief Brightness change callback
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param val New value (0.0..1.0)
   *
   *  This callback can be used to update OSD when the brightness changed.
   */
  
  void (*brightness_callback)(void * data, float val);

  /** \brief Saturation change callback
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param val New value (0.0..1.0)
   *
   *  This callback can be used to update OSD when the saturation changed.
   */

  void (*saturation_callback)(void * data, float val);

  /** \brief Contrast change callback
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param val New value (0.0..1.0)
   *
   *  This callback can be used to update OSD when the contrast changed.
   */

  void (*contrast_callback)(void * data, float val);
  
  void * data;//!< Application specific data passed as the first argument to all callbacks.
  } bg_ov_callbacks_t;

/* Plugin structure */

/** \ingroup plugin_ov
 * \brief Video output plugin
 *
 * This handles video output and still-image display.
 * In a window based system, it will typically open a new window,
 * which is owned by the plugin.
 */

typedef struct bg_ov_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types
  
  /** \brief Set callbacks
   *  \param priv The handle returned by the create() method
   *  \param callbacks Callback structure initialized by the caller before
   */

  void (*set_callbacks)(void * priv, bg_ov_callbacks_t * callbacks);
  
  /** \brief Open plugin
   *  \param priv The handle returned by the create() method
   *  \param format Video format
   *  \param window_title Window title
   *
   *  The format will be changed to the nearest format, which is supported
   *  by the plugin. To convert the source format to the output format,
   *  use a \ref gavl_video_converter_t
   */
         
  int  (*open)(void * priv, gavl_video_format_t * format, const char * window_title);

  /** \brief Allocate a video frame
   *  \param priv The handle returned by the create() method
   *  \returns a newly allocated video frame
   *
   *  This optional method allocates a video frame in a plugin specific manner
   *  (e.g. in a shared memory segment). If this funtion is defined, all frames
   *  which are passed to the plugin, must be allocated by this function.
   *  Before the plugin is closed, all created frames must be freed with
   *  the free_frame() method.
   */
  
  gavl_video_frame_t * (*alloc_frame)(void * priv);

  
  /** \brief Add a stream for transparent overlays
   *  \param priv The handle returned by the create() method
   *  \param format Format of the overlays
   *  \returns The index of the overlay stream
   *
   *  It's up to the plugin, if they are realized in hardware or
   *  with a gavl_overlay_blend_context_t, but they must be there.
   *  add_overlay_stream() must be called after open()
   *
   *  An application can have more than one overlay stream. Typical
   *  is one for subtitles and one for OSD.
   */
  
  int (*add_overlay_stream)(void * priv, const gavl_video_format_t * format);

  /** \brief Set an overlay for a specific stream
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index returned by add_overlay_stream()
   *  \param ovl New overlay or NULL
   */
  
  void (*set_overlay)(void * priv, int stream, gavl_overlay_t * ovl);
  
  /** \brief Display a frame of a video stream
   *  \param priv The handle returned by the create() method
   *  \param frame Frame to display
   *  
   *  This is for video playback
   */

  void (*put_video)(void * priv, gavl_video_frame_t*frame);

  /** \brief Display a still image
   *  \param priv The handle returned by the create() method
   *  \param frame Frame to display
   *  
   *  This function is like put_video() with the diffderence, that
   *  the frame will be remembered and redisplayed, when an expose event
   *  is received.
   */
  
  void (*put_still)(void * priv, gavl_video_frame_t*frame);

  /** \brief Get all events from the queue and handle them
   *  \param priv The handle returned by the create() method
   *  
   *  This function  processes and handles all events, which were
   *  received from the windowing system. It calls mouse and key-callbacks,
   *  and redisplayed the image when in still mode.
   */
  
  void (*handle_events)(void * priv);

  /** \brief Free a frame created with the alloc_frame() method.
   *  \param priv The handle returned by the create() method
   *  \param frame The frame to be freed
   */

  void (*free_frame)(void * priv, gavl_video_frame_t * frame);

  /** \brief Close the plugin
   *  \param priv The handle returned by the create() method
   *
   *  Close everything so the plugin can be opened with a differtent format
   *  after.
   */
  
  void (*close)(void * priv);

  /** \brief Show or hide the window
   *  \param priv The handle returned by the create() method
   *  \param show 1 for showing, 0 for hiding
   */
  void (*show_window)(void * priv, int show);
  } bg_ov_plugin_t;

/*******************************************
 * VIDEO RECORDER
 *******************************************/

/** \defgroup plugin_rv Video recorder
 *  \ingroup plugin
 *  \brief Video recorder
 */ 


/** \ingroup plugin_rv
 *  \brief Video recorder plugin
 */

typedef struct bg_rv_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types

  /** \brief Open plugin
   *  \param priv The handle returned by the create() method
   *  \param format Video format
   *
   *  The format will be changed to the nearest format, which is supported
   *  by the plugin. To convert the source format to the output format,
   *  use a \ref gavl_video_converter_t
   */
  
  int (*open)(void * priv, gavl_video_format_t * format);

  /** \brief Allocate a video frame
   *  \param priv The handle returned by the create() method
   *  \returns a newly allocated video frame
   *
   *  This optional method allocates a video frame in a plugin specific manner
   *  (e.g. in a shared memory segment). If this funtion is defined, all frames
   *  which are passed to the plugin, must be allocated by this function.
   *  Before the plugin is closed, all created frames must be freed with
   *  the free_frame() method.
   */
  
  gavl_video_frame_t * (*alloc_frame)(void * priv);

  /** \brief Read a video frame
   *  \param priv The handle returned by the create() method
   *  \param frame Where the frame will be copied
   *  \returns 1 if a frame was read, 0 on error
   */
  int (*read_frame)(void * priv, gavl_video_frame_t * frame);

  /** \brief Free a frame created with the alloc_frame() method.
   *  \param priv The handle returned by the create() method
   *  \param frame The frame to be freed
   */
  
  void (*free_frame)(void * priv, gavl_video_frame_t * frame);

  /** \brief Close the plugin
   *  \param priv The handle returned by the create() method
   *
   *  Close everything so the plugin can be opened with a differtent format
   *  after.
   */

  void (*close)(void * priv);
  
  } bg_rv_plugin_t;

/*******************************************
 * ENCODER
 *******************************************/

/** \defgroup plugin_e Encoder
 *  \ingroup plugin
 *  \brief Encoder
 */ 

/** \ingroup plugin_e
 *  \brief Encoder plugin
 */

typedef struct bg_encoder_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types
  
  int max_audio_streams;  //!< Maximum number of audio streams. -1 means infinite
  int max_video_streams;  //!< Maximum number of video streams. -1 means infinite
  int max_subtitle_text_streams;//!< Maximum number of text subtitle streams. -1 means infinite
  int max_subtitle_overlay_streams;//!< Maximum number of overlay subtitle streams. -1 means infinite

  /** \brief Return the file extension
   *  \param priv The handle returned by the create() method
   *  \returns The file extension
   *
   *  If a plugin supports more than one output format, the actual format
   *  is configured as a parameter. This function returns the extension
   *  according to the format.
   */
  
  const char * (*get_extension)(void * priv);
  
  /** \brief Open a file
   *  \param priv The handle returned by the create() method
   *  \param filename Name of the file to be opened
   *  \param metadata Metadata to be written to the file
   */
  
  int (*open)(void * data, const char * filename, bg_metadata_t * metadata);

  /** \brief Return the filename, which can be passed to the player
   *  \param priv The handle returned by the create() method
   *
   *  This must be implemented only if the plugin creates
   *  files with names different from the the filename passed to
   *  the open() function
   */
  
  const char * (*get_filename)(void*);

  /* Return per stream parameters */

  /** \brief Get audio related parameters
   *  \param priv The handle returned by the create() method
   *  \returns NULL terminated array of parameter descriptions
   *
   *  The returned parameters are owned by the plugin and must not be freed.
   */
  
  bg_parameter_info_t * (*get_audio_parameters)(void * priv);

  /** \brief Get video related parameters
   *  \param priv The handle returned by the create() method
   *  \returns NULL terminated array of parameter descriptions
   *
   *  The returned parameters are owned by the plugin and must not be freed.
   */

  bg_parameter_info_t * (*get_video_parameters)(void * priv);

  /** \brief Get text subtitle related parameters
   *  \param priv The handle returned by the create() method
   *  \returns NULL terminated array of parameter descriptions
   *
   *  The returned parameters are owned by the plugin and must not be freed.
   */

  bg_parameter_info_t * (*get_subtitle_text_parameters)(void * priv);

  /** \brief Get overlay subtitle related parameters
   *  \param priv The handle returned by the create() method
   *  \returns NULL terminated array of parameter descriptions
   *
   *  The returned parameters are owned by the plugin and must not be freed.
   */

  bg_parameter_info_t * (*get_subtitle_overlay_parameters)(void * priv);
  
  /* Add streams. The formats can be changed, be sure to get the
   * final formats with get_[audio|video]_format after starting the plugin
   * Return value is the index of the added stream.
   */

  /** \brief Add an audio stream
   *  \param priv The handle returned by the create() method
   *  \param format Format of the source
   *  \returns Index of this stream (starting with 0)
   *  
   *  The format might be changed to the nearest format supported by
   *  the plugin. Use \ref get_audio_format to get the actual format
   *  needed by the plugin, after \ref start() was called.
   */
  
  int (*add_audio_stream)(void * priv, gavl_audio_format_t * format);

  /** \brief Add a video stream
   *  \param priv The handle returned by the create() method
   *  \param format Format of the source
   *  \returns Index of this stream (starting with 0)
   *  
   *  The format might be changed to the nearest format supported by
   *  the plugin. Use \ref get_video_format to get the actual format
   *  needed by the plugin, after \ref start() was called.
   */
  
  int (*add_video_stream)(void * priv, gavl_video_format_t * format);

  /** \brief Add a text subtitle stream
   *  \param priv The handle returned by the create() method
   *  \param language as ISO 639-2 code (3 characters+'\\0') or NULL
   *  \returns Index of this stream (starting with 0)
   */
  
  int (*add_subtitle_text_stream)(void * priv, const char * language);

  /** \brief Add a text subtitle stream
   *  \param priv The handle returned by the create() method
   *  \param language as ISO 639-2 code (3 characters+'\\0') or NULL
   *  \param format Format of the source
   *  \returns Index of this stream (starting with 0)
   *
   *  The format might be changed to the nearest format supported by
   *  the plugin. Use \ref get_subtitle_overlay_format
   *  to get the actual format
   *  needed by the plugin, after \ref start was called.
   */
  
  int (*add_subtitle_overlay_stream)(void * priv, const char * language,
                                     gavl_video_format_t * format);
  
  /* Set parameters for the streams */

  /** \brief Set audio encoding parameter
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param name Name of the parameter
   *  \param v Value
   *
   *  Use this function with parameters obtained by
   *  \ref get_audio_parameters.
   */
  
  void (*set_audio_parameter)(void * priv, int stream, char * name,
                              bg_parameter_value_t * v);

  /** \brief Set video encoding parameter
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param name Name of the parameter
   *  \param v Value
   *
   *  Use this function with parameters obtained by
   *  \ref get_video_parameters.
   */

  
  void (*set_video_parameter)(void * priv, int stream, char * name,
                              bg_parameter_value_t * v);

  /** \brief Set text subtitle encoding parameter
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param name Name of the parameter
   *  \param v Value
   *
   *  Use this function with parameters obtained by
   *  \ref get_subtitle_text_parameters.
   */
  
  void (*set_subtitle_text_parameter)(void * priv, int stream, char * name,
                                      bg_parameter_value_t * v);

  /** \brief Set text subtitle encoding parameter
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param name Name of the parameter
   *  \param v Value
   *
   *  Use this function with parameters obtained by
   *  \ref get_subtitle_overlay_parameters.
   */
  
  void (*set_subtitle_overlay_parameter)(void * priv, int stream, char * name,
                                         bg_parameter_value_t * v);
  
  /** \brief Setup multipass video encoding.
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param pass Number of this pass (starting with 1)
   *  \param total_passes Number of total passes
   *  \param stats_file Name of a file, which can be used for multipass statistics
   *  \returns 0 if multipass transcoding is not supported and can be ommitted, 1 else
   */
  int (*set_video_pass)(void * priv, int stream, int pass, int total_passes,
                        const char * stats_file);
  
  /** \brief Set up all codecs and prepare for encoding
   *  \param priv The handle returned by the create() method
   *  \returns 0 on error, 1 on success
   *
   *  Optional function for preparing the actual encoding. Applications must
   *  check for this function and call it when available.
   */
  
  int (*start)(void * priv);
  
  /*
   *  After setting the parameters, get the formats, you need to deliver the frames in
   */

  /** \brief Get audio format
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param ret Returns format
   *
   *  Call this after calling \ref start() if it's defined.
   */
  
  void (*get_audio_format)(void * priv, int stream, gavl_audio_format_t*ret);

  /** \brief Get video format
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param ret Returns format
   *
   *  Call this after calling \ref start() if it's defined.
   */

  void (*get_video_format)(void * priv, int stream, gavl_video_format_t*ret);

  /** \brief Get video format of an overlay subtitle stream
   *  \param priv The handle returned by the create() method
   *  \param stream Stream index (starting with 0)
   *  \param ret Returns format
   *
   *  Call this after calling \ref start() if it's defined.
   */

  void (*get_subtitle_overlay_format)(void * priv, int stream,
                                      gavl_video_format_t*ret);

  /*
   *  Encode audio/video
   */

  /** \brief Write audio samples
   *  \param priv The handle returned by the create() method
   *  \param frame Frame with samples
   *  \param stream Stream index (starting with 0)
   *  \returns 1 is the data was successfully written, 0 else
   *
   *  The actual number of samples must be stored in the valid_samples member of
   *  the frame.
   */
  
  int (*write_audio_frame)(void * data,gavl_audio_frame_t * frame, int stream);

  /** \brief Write video frame
   *  \param priv The handle returned by the create() method
   *  \param frame Frame
   *  \param stream Stream index (starting with 0)
   *  \returns 1 is the data was successfully written, 0 else
   */

  int (*write_video_frame)(void * data,gavl_video_frame_t * frame, int stream);

  /** \brief Write a text subtitle
   *  \param priv The handle returned by the create() method
   *  \param frame The text
   *  \param start Start of the subtitle
   *  \param duration Duration of the subtitle
   *  \param stream Stream index (starting with 0)
   *  \returns 1 is the data was successfully written, 0 else
   */
  
  int (*write_subtitle_text)(void * data,const char * text,
                             gavl_time_t start,
                             gavl_time_t duration, int stream);

  /** \brief Write an overlay subtitle
   *  \param priv The handle returned by the create() method
   *  \param ovl An overlay
   *  \param stream Stream index (starting with 0)
   *  \returns 1 is the data was successfully written, 0 else
   */
  
  int (*write_subtitle_overlay)(void * data, gavl_overlay_t * ovl, int stream);
  
  /** \brief Close encoder
   *  \param priv The handle returned by the create() method
   *  \param do_delete Set this to 1 to delete all created files
   *  \returns 1 is the file was sucessfully closed, 0 else
   *
   *  After calling this function, the plugin should be destroyed.
   */

  int (*close)(void * data, int do_delete);
  } bg_encoder_plugin_t;


/*******************************************
 * ENCODER Postprocessor
 *******************************************/

/** \defgroup plugin_e_pp Encoding postprocessor
 *  \ingroup plugin
 *  \brief Encoding postprocessor
 *
 *  The postprocessing plugins take one or more files
 *  for further processing them. There are postprocessors
 *  for creating and (optionally) burning audio CDs and VCDs.
 */


/** \ingroup plugin_e_pp
 *  \brief Callbacks for postprocessing
 *
 */

typedef struct
  {
  /** \brief Callback describing the current action
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param action A string describing the current action
   *
   *  Action can be something like "Burning track 1/10".
   */
  void (*action_callback)(void * data, char * action);

  /** \brief Callback describing the progress of the current action
   *  \param data The data member of this bg_ov_callbacks_s struct
   *  \param perc Percentage (0.0 .. 1.0)
   *
   *  This is exclusively for updating progress bars in
   *  GUI applications. Note, that some postprocessors
   *  reset the progress during postprocessing.
   */
  
  void (*progress_callback)(void * data, float perc);

  void * data; //!< Application specific data passed as the first argument to all callbacks.

  } bg_e_pp_callbacks_t;

/** \ingroup plugin_e_pp
 *  \brief Encoding postprocessor
 *
 */

typedef struct bg_encoder_pp_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types
  
  int max_audio_streams;  //!< Maximum number of audio streams. -1 means infinite

  int max_video_streams;  //!< Maximum number of video streams. -1 means infinite

  /** \brief Set callbacks
   *  \param priv The handle returned by the create() method
   *  \param callbacks Callback structure initialized by the caller before
   *
   */

  void (*set_callbacks)(void * priv,bg_e_pp_callbacks_t * callbacks);
  
  /** \brief Initialize
   *  \param priv The handle returned by the create() method
   *
   *  This functions clears all tracks and makes the plugin ready to
   *  add new tracks.
   */
  
  int (*init)(void * priv);

  /** \brief Add a transcoded track
   *  \param priv The handle returned by the create() method
   *  \param filename Name of the media file
   *  \param metadata Metadata for the track
   *  \param pp_only Set this to 1 if this file was not encoded and is only postprocessed
   *
   *  Send a track to the postprocessor. This plugin will store all necessary data until
   *  the \ref run() method is called. Some postprocessors might do some sanity tests
   *  on the file e.g. the audio CD burner will reject anything except WAV-files.
   *  
   *  Usually, it is assumed, that files were encoded for the only purpose of postprocessing
   *  them. This means, they will be deleted if the cleanup flag is passed to the \ref run()
   *  function. If you set the pp_only argument to 1, you tell, that this file should be
   *  kept after postprocessing.
   */
  
  void (*add_track)(void * priv, const char * filename,
                    bg_metadata_t * metadata, int pp_only);
  
  /** \brief Start postprocessing
   *  \param priv The handle returned by the create() method
   *  \param directory The directory, where output files can be written
   *  \param cleanup Set this to 1 to erase all source files and temporary files after finishing
   * Run can be a long operation, it should be called from a separate
   * thread launched by the application and the callbacks should be
   * used for progress reporting
   */
  
  void (*run)(void * priv, const char * directory, int cleanup);

  /** \brief Stop postprocessing
   *  \param priv The handle returned by the create() method
   *
   *  Call this function to cancel a previously called \ref run() function.
   *  The plugin must implement the stop mechanism in a thread save manner,
   *  i.e. the stop method must savely be callable from another thread than the
   *  one, which called \ref run().
   */
  
  void (*stop)(void * priv);
  } bg_encoder_pp_plugin_t;


/*******************************************
 * Image reader
 *******************************************/

/** \brief Image reader plugin
 */

typedef struct bg_image_reader_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types

  /** \brief Read the file header
   *  \param priv The handle returned by the create() method
   *  \param filename Filename
   *  \param format Returns the format of the image
   *  \returns 1 on success, 0 on error.
   */
  
  int (*read_header)(void * priv, const char * filename,
                     gavl_video_format_t * format);

  /** \brief Read the image
   *  \param priv The handle returned by the create() method
   *  \param frame The frame, where the image will be copied
   *  
   *  After reading the image the plugin is cleaned up, so \ref read_header()
   *  can be called again after that. If frame is NULL, no image is read,
   *  and the plugin is reset.
   */
  int (*read_image)(void * priv, gavl_video_frame_t * frame);
  } bg_image_reader_plugin_t;

/*******************************************
 * Image writer
 *******************************************/

/** \brief Image writer plugin
 *
 */


typedef struct bg_image_writer_plugin_s
  {
  bg_plugin_common_t common; //!< Infos and functions common to all plugin types

  /** \brief Return the file extension
   *  \param priv The handle returned by the create() method
   *  \returns The extension
   *
   *  This function is mandatory for all plugins. Most plugins will always
   *  return the same extension. Others might have multiple supported formats,
   *  which are selected through parameters.
   */

  const char * (*get_extension)(void * priv);
  
  /** \brief Write the file header
   *  \param priv The handle returned by the create() method
   *  \param format Video format
   *  \returns 1 on success, 0 on error.
   *
   *  The format will be changed to the nearest format, which is supported
   *  by the plugin. To convert the source format to the output format,
   *  use a \ref gavl_video_converter_t
   */
  
  int (*write_header)(void * priv, const char * filename,
                      gavl_video_format_t * format);
  
  /** \brief Write the image
   *  \param priv The handle returned by the create() method
   *  \param frame The frame containing the image
   *  \returns 1 on success, 0 on error.
   *  
   *  After writing the image the plugin is cleaned up, so \ref write_header()
   *  can be called again after that. If frame is NULL, no image is read,
   *  and the plugin is reset.
   */

  int (*write_image)(void * priv, gavl_video_frame_t * frame);
  } bg_image_writer_plugin_t;

#endif // __BG_PLUGIN_H_
