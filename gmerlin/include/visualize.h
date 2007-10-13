
typedef struct bg_visualizer_s bg_visualizer_t;

bg_visualizer_t * bg_visualizer_create(bg_plugin_registry_t * plugin_reg);

void bg_visualizer_destroy(bg_visualizer_t *);

bg_parameter_info_t * bg_visualizer_get_parameters(bg_visualizer_t*);

void bg_visualizer_set_parameter(void * priv,
                                 const char * name,
                                 const bg_parameter_value_t * v);

/* Open visualization stuff, start visualizer thread */

void bg_visualizer_open(bg_visualizer_t * v,
                        const gavl_audio_format_t * format,
                        bg_plugin_handle_t * ov_handle);

/* Set new audio format without stopping the visualization thread */

void bg_visualizer_set_audio_format(bg_visualizer_t * v,
                                    const gavl_audio_format_t * format);

void bg_visualizer_update(bg_visualizer_t * v, gavl_audio_frame_t *);

void bg_visualizer_close(bg_visualizer_t * v);

int bg_visualizer_is_enabled(bg_visualizer_t * v);

int bg_visualizer_need_restart(bg_visualizer_t * v);

/* Pause and resume the visualizer. Everything can be
   changed in between */

int bg_visualizer_start(bg_visualizer_t * v);

int bg_visualizer_stop(bg_visualizer_t * v);

