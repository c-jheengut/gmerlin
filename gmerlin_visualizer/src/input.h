/*****************************************************************

  input.c

  Copyright (c) 2001 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de

  http://gmerlin.sourceforge.net

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

*****************************************************************/

#include <gmerlin/plugin.h>
#include <gmerlin/cfg_registry.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/utils.h>

#include "vis_plugin.h"

/* This file is part of gmerlin_vizualizer */

/* Some macros for the conversions */

#define INPUT_NEED_MONO        (1<<0)
#define INPUT_NEED_FREQ_STEREO (1<<1)
#define INPUT_NEED_FREQ_MONO   (1<<2)

typedef struct
  {
  int do_convert_gavl;
  
  gavl_audio_frame_t * input_frame;
  gavl_audio_frame_t * frame;  
  gavl_audio_converter_t * cnv;

  bg_plugin_registry_t * plugin_reg;
  bg_cfg_registry_t    * cfg_reg;

  bg_plugin_handle_t * input_handle;
  bg_ra_plugin_t     * input;

  vis_plugin_audio_t audio_frame;
    
  vis_plugin_handle_t * active_plugins;

  fft_state * state;

  gfloat fft_scratch[257];

  } input_t;

extern input_t * the_input;

int input_create(char ** error_msg);

void input_add_plugin(input_t * c, vis_plugin_handle_t * plugin);
void input_remove_plugin(input_t * c, const vis_plugin_info_t * info);

int input_iteration(void * data);

void input_cleanup();
