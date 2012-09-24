
#include <gavl/gavf.h>
#include <gavl/connectors.h>

#include <gmerlin/parameter.h>

/* Reader */

typedef struct bg_plug_s bg_plug_t;

bg_plug_t * bg_plug_create_reader(void);
bg_plug_t * bg_plug_create_writer(void);

void bg_plug_reader_destroy(bg_plug_t *);

const bg_parameter_info_t * bg_plug_get_parameters(bg_plug_t *);
void bg_plug_set_parameter(void * data, const char * name,
                           const bg_parameter_value_t * val);

int bg_plug_open(bg_plug_t *, const char * location);

gavf_t * bg_plug_reader_get_gavf(bg_plug_t *);

int bg_plug_start_write(bg_plug_t * p);


/* Optimized audio/video I/O */

