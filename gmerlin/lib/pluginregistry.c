/*****************************************************************
 
  pluginregistry.c
 
  Copyright (c) 2003-2007 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#include <config.h>

#include <cfg_registry.h>
#include <pluginregistry.h>
#include <pluginreg_priv.h>
#include <config.h>
#include <utils.h>
#include <singlepic.h>

#include <translation.h>

#include <log.h>

#include <bgladspa.h>

#define LOG_DOMAIN "pluginregistry"

struct bg_plugin_registry_s
  {
  bg_plugin_info_t * entries;
  bg_cfg_section_t * config_section;

  bg_plugin_info_t * singlepic_input;
  bg_plugin_info_t * singlepic_stills_input;
  bg_plugin_info_t * singlepic_encoder;

  int encode_audio_to_video;
  int encode_subtitle_text_to_video;
  int encode_subtitle_overlay_to_video;
  
  int encode_pp;
  };

void bg_plugin_info_destroy(bg_plugin_info_t * info)
  {
  if(info->gettext_domain)
    free(info->gettext_domain);
  if(info->gettext_directory)
    free(info->gettext_directory);

  if(info->name)
    free(info->name);
  if(info->long_name)
    free(info->long_name);
  if(info->description)
    free(info->description);
  if(info->mimetypes)
    free(info->mimetypes);
  if(info->extensions)
    free(info->extensions);
  if(info->protocols)
    free(info->protocols);
  if(info->module_filename)
    free(info->module_filename);
  if(info->devices)
    bg_device_info_destroy(info->devices);

  if(info->parameters)
    bg_parameter_info_destroy_array(info->parameters);
  if(info->audio_parameters)
    bg_parameter_info_destroy_array(info->audio_parameters);
  if(info->video_parameters)
    bg_parameter_info_destroy_array(info->video_parameters);
  if(info->subtitle_text_parameters)
    bg_parameter_info_destroy_array(info->subtitle_text_parameters);
  if(info->subtitle_overlay_parameters)
    bg_parameter_info_destroy_array(info->subtitle_overlay_parameters);
  
  free(info);
  }

static void free_info_list(bg_plugin_info_t * entries)
  {
  bg_plugin_info_t * info;
  
  info = entries;

  while(info)
    {
    entries = info->next;
    bg_plugin_info_destroy(info);
    info = entries;
    }
  }

static int compare_swap(bg_plugin_info_t * i1,
                        bg_plugin_info_t * i2)
  {
  if((i1->flags & BG_PLUGIN_FILTER_1) &&
     (i2->flags & BG_PLUGIN_FILTER_1))
    {
    return strcmp(i1->long_name, i2->long_name) > 0;
    }
  else if((!(i1->flags & BG_PLUGIN_FILTER_1)) &&
          (!(i2->flags & BG_PLUGIN_FILTER_1)))
    {
    return i1->priority < i2->priority;
    }
  else if((!(i1->flags & BG_PLUGIN_FILTER_1)) &&
          (i2->flags & BG_PLUGIN_FILTER_1))
    return 1;
  
  return 0;
  }
                           

static bg_plugin_info_t * sort_by_priority(bg_plugin_info_t * list)
  {
  int i, j;
  bg_plugin_info_t * info;
  bg_plugin_info_t ** arr;
  int num_plugins = 0;
  int keep_going;
  
  /* Count plugins */

  info = list;
  while(info)
    {
    num_plugins++;
    info = info->next;
    }

  /* Allocate array */
  arr = malloc(num_plugins * sizeof(*arr));
  info = list;
  for(i = 0; i < num_plugins; i++)
    {
    arr[i] = info;
    info = info->next;
    }

  /* Bubblesort */

  for(i = 0; i < num_plugins - 1; i++)
    {
    keep_going = 0;
    for(j = num_plugins-1; j > i; j--)
      {
      if(compare_swap(arr[j-1], arr[j]))
        {
        info  = arr[j];
        arr[j]   = arr[j-1];
        arr[j-1] = info;
        keep_going = 1;
        }
      }
    if(!keep_going)
      break;
    }

  /* Rechain */

  for(i = 0; i < num_plugins-1; i++)
    arr[i]->next = arr[i+1];
  arr[num_plugins-1]->next = (bg_plugin_info_t*)0;
  list = arr[0];
  /* Free array */
  free(arr);
  
  return list;
  }

static bg_plugin_info_t *
find_by_dll(bg_plugin_info_t * info, const char * filename)
  {
  while(info)
    {
    if(info->module_filename && !strcmp(info->module_filename, filename))
      return info;
    info = info->next;
    }
  return (bg_plugin_info_t*)0;
  }

static bg_plugin_info_t *
find_by_name(bg_plugin_info_t * info, const char * name)
  {
  while(info)
    {
    if(!strcmp(info->name, name))
      return info;
    info = info->next;
    }
  return (bg_plugin_info_t*)0;
  }

const bg_plugin_info_t * bg_plugin_find_by_name(bg_plugin_registry_t * reg,
                                                const char * name)
  {
  return find_by_name(reg->entries, name);
  }

const bg_plugin_info_t * bg_plugin_find_by_protocol(bg_plugin_registry_t * reg,
                                                    const char * protocol)
  {
  const bg_plugin_info_t * info = reg->entries;
  while(info)
    {
    if(bg_string_match(protocol, info->protocols))
      return info;
    info = info->next;
    }
  return (bg_plugin_info_t*)0;
  }



const bg_plugin_info_t * bg_plugin_find_by_filename(bg_plugin_registry_t * reg,
                                                    const char * filename,
                                                    int typemask)
  {
  char * extension;
  bg_plugin_info_t * info, *ret = (bg_plugin_info_t*)0;
  int max_priority = BG_PLUGIN_PRIORITY_MIN - 1;

  if(!filename)
    return (const bg_plugin_info_t*)0;
  
  
  info = reg->entries;
  extension = strrchr(filename, '.');

  if(!extension)
    {
    return (const bg_plugin_info_t *)0;
    }
  extension++;
  
  
  while(info)
    {
    if(!(info->type & typemask) ||
       !(info->flags & BG_PLUGIN_FILE) ||
       !info->extensions)
      {
      info = info->next;
      continue;
      }
    if(bg_string_match(extension, info->extensions))
      {
      if(max_priority < info->priority)
        {
        max_priority = info->priority;
        ret = info;
        }
      // return info;
      }
    info = info->next;
    }
  return ret;
  }

static bg_plugin_info_t * remove_from_list(bg_plugin_info_t * list,
                                           bg_plugin_info_t * info)
  {
  bg_plugin_info_t * before;
  if(info == list)
    {
    list = list->next;
    info->next = (bg_plugin_info_t*)0;
    return list;
    }

  before = list;

  while(before->next != info)
    before = before->next;
    
  before->next = info->next;
  info->next = (bg_plugin_info_t*)0;
  return list;
  }

static bg_plugin_info_t * append_to_list(bg_plugin_info_t * list,
                                         bg_plugin_info_t * info)
  {
  bg_plugin_info_t * end;
  if(!list)
    return info;
  
  end = list;
  while(end->next)
    end = end->next;
  end->next = info;
  return list;
  }

static int check_plugin_version(void * handle)
  {
  int (*get_plugin_api_version)();

  get_plugin_api_version = dlsym(handle, "get_plugin_api_version");
  if(!get_plugin_api_version)
    return 0;

  if(get_plugin_api_version() != BG_PLUGIN_API_VERSION)
    return 0;
  return 1;
  }

static bg_plugin_info_t * get_info(void * test_module, const char * filename)
  {
  bg_encoder_plugin_t * encoder;
  bg_input_plugin_t  * input;

  bg_plugin_info_t * new_info;
  bg_plugin_common_t * plugin;
  void * plugin_priv;
  bg_parameter_info_t * parameter_info;
  
  if(!check_plugin_version(test_module))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Plugin %s has no or wrong version", filename);
    dlclose(test_module);
    return (bg_plugin_info_t*)0;
    }
  plugin = (bg_plugin_common_t*)(dlsym(test_module, "the_plugin"));
  if(!plugin)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "No symbol the_plugin in %s", filename);
    dlclose(test_module);
    return (bg_plugin_info_t*)0;
    }
  if(!plugin->priority)
    bg_log(BG_LOG_WARNING, LOG_DOMAIN, "Warning: Plugin %s has zero priority",
           plugin->name);
  new_info = calloc(1, sizeof(*new_info));
  new_info->name = bg_strdup(new_info->name, plugin->name);

  new_info->long_name =  bg_strdup(new_info->long_name,
                                   plugin->long_name);

  new_info->description = bg_strdup(new_info->description,
                                    plugin->description);
    
  new_info->mimetypes =  bg_strdup(new_info->mimetypes,
                                   plugin->mimetypes);
  new_info->extensions = bg_strdup(new_info->extensions,
                                   plugin->extensions);
  new_info->module_filename = bg_strdup(new_info->module_filename,
                                        filename);

  new_info->gettext_domain = bg_strdup(new_info->gettext_domain,
                                       plugin->gettext_domain);
  new_info->gettext_directory = bg_strdup(new_info->gettext_directory,
                                          plugin->gettext_directory);
  new_info->type        = plugin->type;
  new_info->flags       = plugin->flags;
  new_info->priority    = plugin->priority;

  /* Get parameters */

  plugin_priv = plugin->create();
  
  if(plugin->get_parameters)
    {
    parameter_info = plugin->get_parameters(plugin_priv);
    new_info->parameters = bg_parameter_info_copy_array(parameter_info);
    }
    
  if(plugin->type & (BG_PLUGIN_ENCODER_AUDIO|
                     BG_PLUGIN_ENCODER_VIDEO|
                     BG_PLUGIN_ENCODER_SUBTITLE_TEXT |
                     BG_PLUGIN_ENCODER_SUBTITLE_OVERLAY |
                     BG_PLUGIN_ENCODER ))
    {
    encoder = (bg_encoder_plugin_t*)plugin;
    new_info->max_audio_streams = encoder->max_audio_streams;
    new_info->max_video_streams = encoder->max_video_streams;
    new_info->max_subtitle_text_streams = encoder->max_subtitle_text_streams;
    new_info->max_subtitle_overlay_streams = encoder->max_subtitle_overlay_streams;
    
    if(encoder->get_audio_parameters)
      {
      parameter_info = encoder->get_audio_parameters(plugin_priv);
      new_info->audio_parameters = bg_parameter_info_copy_array(parameter_info);
      }
    
    if(encoder->get_video_parameters)
      {
      parameter_info = encoder->get_video_parameters(plugin_priv);
      new_info->video_parameters = bg_parameter_info_copy_array(parameter_info);
      }
    if(encoder->get_subtitle_text_parameters)
      {
      parameter_info = encoder->get_subtitle_text_parameters(plugin_priv);
      new_info->subtitle_text_parameters = bg_parameter_info_copy_array(parameter_info);
      }
    if(encoder->get_subtitle_overlay_parameters)
      {
      parameter_info = encoder->get_subtitle_overlay_parameters(plugin_priv);
      new_info->subtitle_overlay_parameters =
        bg_parameter_info_copy_array(parameter_info);
      }
    }
  if(plugin->type & BG_PLUGIN_INPUT)
    {
    input = (bg_input_plugin_t*)plugin;
    if(input->protocols)
      new_info->protocols = bg_strdup(new_info->protocols,
                                      input->protocols);
    }
  if(plugin->find_devices)
    new_info->devices = plugin->find_devices();
  
  plugin->destroy(plugin_priv);
  
  return new_info;
  }

static bg_plugin_info_t *
scan_directory_internal(const char * directory, bg_plugin_info_t ** _file_info,
                        int * changed,
                        bg_cfg_section_t * cfg_section, bg_plugin_api_t api)
  {
  bg_plugin_info_t * ret;
  //  bg_plugin_info_t * end = (bg_plugin_info_t *)0;
  DIR * dir;
  struct dirent * entry;
  char filename[PATH_MAX];
  struct stat st;
  char * pos;
  void * test_module;
  
  bg_plugin_info_t * file_info;
  bg_plugin_info_t * new_info;
  bg_plugin_info_t * tmp_info;
  
  bg_cfg_section_t * plugin_section;
  bg_cfg_section_t * stream_section;
  if(_file_info)
    file_info = *_file_info;
  else
    file_info = (bg_plugin_info_t *)0;
  
  ret = (bg_plugin_info_t *)0;
    
  dir = opendir(directory);
  
  if(!dir)
    return (bg_plugin_info_t*)0;

  while((entry = readdir(dir)))
    {
    /* Check for the filename */
    
    pos = strrchr(entry->d_name, '.');
    if(!pos)
      continue;
    
    if(strcmp(pos, ".so"))
      continue;
    
    sprintf(filename, "%s/%s", directory, entry->d_name);
    if(stat(filename, &st))
      continue;

    /* Check if the plugin is already in the registry */

    new_info = find_by_dll(file_info, filename);
    if(new_info)
      {
      if((st.st_mtime == new_info->module_time) &&
         (bg_cfg_section_has_subsection(cfg_section,
                                        new_info->name)))
        {
        file_info = remove_from_list(file_info, new_info);
        
        ret = append_to_list(ret, new_info);
        
        /* Remove other plugins as well */
        while((new_info = find_by_dll(file_info, filename)))
          {
          file_info = remove_from_list(file_info, new_info);
          ret = append_to_list(ret, new_info);
          }
        
        continue;
        }
      }

    if(!(*changed))
      {
      *changed = 1;
      closedir(dir);
      if(_file_info)
        *_file_info = file_info;
      return ret;
      }
    
    /* Open the DLL and see what's inside */
    
    test_module = dlopen(filename, RTLD_NOW);
    if(!test_module)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot dlopen %s: %s", filename, dlerror());
      continue;
      }

    switch(api)
      {
      case BG_PLUGIN_API_GMERLIN:
        new_info = get_info(test_module, filename);
        break;
      case BG_PLUGIN_API_LADSPA:
        new_info = bg_ladspa_get_info(test_module, filename);
      }

    tmp_info = new_info;
    while(tmp_info)
      {
      tmp_info->module_time = st.st_mtime;
    
    
    /* Create parameter entries in the registry */

      plugin_section =
        bg_cfg_section_find_subsection(cfg_section, tmp_info->name);
    
      if(tmp_info->parameters)
        {
        bg_cfg_section_create_items(plugin_section,
                                    tmp_info->parameters);
        }
      if(tmp_info->audio_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$audio");
        
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->audio_parameters);
        }
      if(tmp_info->video_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$video");
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->video_parameters);
        }
      if(tmp_info->subtitle_text_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$subtitle_text");
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->video_parameters);
        }
      if(tmp_info->subtitle_overlay_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$subtitle_overlay");
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->video_parameters);
        }
      tmp_info = tmp_info->next;
      }

    dlclose(test_module);
    ret = append_to_list(ret, new_info);
    }
  
  closedir(dir);
  if(_file_info)
    *_file_info = file_info;
  
  return ret;
  }

static bg_plugin_info_t *
scan_directory(const char * directory, bg_plugin_info_t ** _file_info,
               bg_cfg_section_t * cfg_section, bg_plugin_api_t api)
  {
  int changed = 0;
  bg_plugin_info_t * file_info;
  bg_plugin_info_t * file_info_next;

  bg_plugin_info_t * ret;
  
  ret = scan_directory_internal(directory, _file_info,
                                &changed, cfg_section, api);
  
  /* Check if there are entries from the file info left */
  file_info = *_file_info;

  /* */

  file_info = *_file_info;
  
  while(file_info)
    {
    if(!strncmp(file_info->module_filename, directory, strlen(directory)))
      {
      file_info_next = file_info->next;
      *_file_info = remove_from_list(*_file_info, file_info);
      file_info = file_info_next;
      changed = 1;
      }
    else
      file_info = file_info->next;
    }
  
  if(!changed)
    return ret;
  
  free_info_list(ret);
  ret = scan_directory_internal(directory, _file_info,
                                &changed, cfg_section, api);
  return ret;
  }

bg_plugin_registry_t *
bg_plugin_registry_create(bg_cfg_section_t * section)
  {
  bg_plugin_registry_t * ret;
  bg_plugin_info_t * file_info;
  bg_plugin_info_t * tmp_info;
  bg_plugin_info_t * tmp_info_next;
  char * filename;
  int index, i;
  int do_scan;
  char * env;

  char * path;
  char ** paths;
    
  ret = calloc(1, sizeof(*ret));
  ret->config_section = section;

  /* Load registry file */

  file_info = (bg_plugin_info_t*)0; 
  
  filename = bg_search_file_read("", "plugins.xml");
  if(filename)
    {
    file_info = bg_plugin_registry_load(filename);
    free(filename);
    }

  /* Native plugins */
  tmp_info = scan_directory(GMERLIN_PLUGIN_DIR,
                            &file_info, 
                            section, BG_PLUGIN_API_GMERLIN);
  if(tmp_info)
    ret->entries = append_to_list(ret->entries, tmp_info);
  /* Ladspa plugins */

  env = getenv("LADSPA_PATH");
  if(env)
    path = bg_sprintf("%s:/usr/lib/ladspa:/usr/local/lib/ladspa", env);
  else
    path = bg_sprintf("/usr/lib/ladspa:/usr/local/lib/ladspa");

  paths = bg_strbreak(path, ':');
  if(paths)
    {
    index = 0;
    while(paths[index])
      {
      do_scan = 1;
      for(i = 0; i < index; i++)
        {
        if(!strcmp(paths[index], paths[i]))
          {
          do_scan = 0;
          break;
          }
        }
      if(do_scan)
        {
        tmp_info = scan_directory(paths[index],
                                  &file_info, 
                                  section, BG_PLUGIN_API_LADSPA);
        if(tmp_info)
          ret->entries = append_to_list(ret->entries, tmp_info);
        }
      index++;
      }
    bg_strbreak_free(paths);
    }
  free(path);
  
  /* Now we have all external plugins, time to create the meta plugins */
  
  ret->singlepic_input = bg_singlepic_input_info(ret);
  if(ret->singlepic_input)
    ret->entries = append_to_list(ret->entries, ret->singlepic_input);
  
  ret->singlepic_stills_input = bg_singlepic_stills_input_info(ret);
  if(ret->singlepic_stills_input)
    ret->entries = append_to_list(ret->entries, ret->singlepic_stills_input);
  
  ret->singlepic_encoder = bg_singlepic_encoder_info(ret);
  if(ret->singlepic_encoder)
    ret->entries = append_to_list(ret->entries, ret->singlepic_encoder);
  
  /* Get flags */

  bg_cfg_section_get_parameter_int(ret->config_section,
                                   "encode_audio_to_video",
                                   &(ret->encode_audio_to_video));
  bg_cfg_section_get_parameter_int(ret->config_section,
                                   "encode_subtitle_text_to_video",
                                   &(ret->encode_subtitle_text_to_video));
  bg_cfg_section_get_parameter_int(ret->config_section,
                                   "encode_subtitle_overlay_to_video",
                                   &(ret->encode_subtitle_overlay_to_video));
  
  bg_cfg_section_get_parameter_int(ret->config_section, "encode_pp",
                                   &(ret->encode_pp));
    
  /* Sort */

  ret->entries = sort_by_priority(ret->entries);
  bg_plugin_registry_save(ret->entries);

  /* Kick out unsupported plugins */
  tmp_info = ret->entries;

  while(tmp_info)
    {
    if(tmp_info->flags & BG_PLUGIN_UNSUPPORTED)
      {
      tmp_info_next = tmp_info->next;
      ret->entries = remove_from_list(ret->entries, tmp_info);
      tmp_info = tmp_info_next;
      }
    else
      tmp_info = tmp_info->next;
    }
  
  return ret;
  }

void bg_plugin_registry_destroy(bg_plugin_registry_t * reg)
  {
  bg_plugin_info_t * info;

  info = reg->entries;

  while(info)
    {
    reg->entries = info->next;
    bg_plugin_info_destroy(info);
    info = reg->entries;
    }
  free(reg);
  }

static bg_plugin_info_t * find_by_index(bg_plugin_info_t * info,
                                        int index, uint32_t type_mask,
                                        uint32_t flag_mask)
  {
  int i;
  bg_plugin_info_t * test_info;

  i = 0;
  test_info = info;

  while(test_info)
    {
    if((test_info->type & type_mask) &&
       ((flag_mask == BG_PLUGIN_ALL) ||
        (!test_info->flags && !flag_mask) || (test_info->flags & flag_mask)))
      {
      if(i == index)
        return test_info;
      i++;
      }
    test_info = test_info->next;
    }
  return (bg_plugin_info_t*)0;
  }

static bg_plugin_info_t * find_by_priority(bg_plugin_info_t * info,
                                           uint32_t type_mask,
                                           uint32_t flag_mask)
  {
  bg_plugin_info_t * test_info, *ret = (bg_plugin_info_t*)0;
  int priority_max = BG_PLUGIN_PRIORITY_MIN - 1;
  
  test_info = info;

  while(test_info)
    {
    if((test_info->type & type_mask) &&
       ((flag_mask == BG_PLUGIN_ALL) ||
        (test_info->flags & flag_mask) ||
        (!test_info->flags && !flag_mask)))
      {
      if(priority_max < test_info->priority)
        {
        priority_max = test_info->priority;
        ret = test_info;
        }
      }
    test_info = test_info->next;
    }
  return ret;
  }

const bg_plugin_info_t *
bg_plugin_find_by_index(bg_plugin_registry_t * reg, int index,
                        uint32_t type_mask, uint32_t flag_mask)
  {
  return find_by_index(reg->entries, index,
                       type_mask, flag_mask);
  }

int bg_plugin_registry_get_num_plugins(bg_plugin_registry_t * reg,
                                       uint32_t type_mask, uint32_t flag_mask)
  {
  bg_plugin_info_t * info;
  int ret = 0;
  
  info = reg->entries;

  while(info)
    {
    if((info->type & type_mask) &&
       ((!info->flags && !flag_mask) || (info->flags & flag_mask)))
      ret++;

    info = info->next;
    }
  return ret;
  }

void bg_plugin_registry_set_extensions(bg_plugin_registry_t * reg,
                                       const char * plugin_name,
                                       const char * extensions)
  {
  bg_plugin_info_t * info;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
  if(!(info->flags & BG_PLUGIN_FILE))
    return;
  info->extensions = bg_strdup(info->extensions, extensions);
  
  bg_plugin_registry_save(reg->entries);
  
  }

void bg_plugin_registry_set_protocols(bg_plugin_registry_t * reg,
                                      const char * plugin_name,
                                      const char * protocols)
  {
  bg_plugin_info_t * info;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
  if(!(info->flags & BG_PLUGIN_URL))
    return;
  info->protocols = bg_strdup(info->protocols, protocols);
  bg_plugin_registry_save(reg->entries);

  }

void bg_plugin_registry_set_priority(bg_plugin_registry_t * reg,
                                     const char * plugin_name,
                                     int priority)
  {
  bg_plugin_info_t * info;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
  info->priority = priority;
  reg->entries = sort_by_priority(reg->entries);
  bg_plugin_registry_save(reg->entries);
  }

bg_cfg_section_t *
bg_plugin_registry_get_section(bg_plugin_registry_t * reg,
                               const char * plugin_name)
  {
  return bg_cfg_section_find_subsection(reg->config_section, plugin_name);
  }

static struct
  {
  bg_plugin_type_t type;
  char * key;
  } default_keys[] =
  {
    { BG_PLUGIN_OUTPUT_AUDIO,                    "default_audio_output"   },
    { BG_PLUGIN_OUTPUT_VIDEO,                    "default_video_output"   },
    { BG_PLUGIN_RECORDER_AUDIO,                  "default_audio_recorder" },
    { BG_PLUGIN_RECORDER_VIDEO,                  "default_video_recorder" },
    { BG_PLUGIN_ENCODER_AUDIO,                   "default_audio_encoder"  },
    { BG_PLUGIN_ENCODER_VIDEO|BG_PLUGIN_ENCODER, "default_video_encoder" },
    { BG_PLUGIN_ENCODER_SUBTITLE_TEXT,           "default_subtitle_text_encoder" },
    { BG_PLUGIN_ENCODER_SUBTITLE_OVERLAY,        "default_subtitle_overlay_encoder" },
    { BG_PLUGIN_IMAGE_WRITER,                    "default_image_writer"   },
    { BG_PLUGIN_ENCODER_PP,                      "default_encoder_pp"  },
    { BG_PLUGIN_NONE,                            (char*)NULL              },
  };

static const char * get_default_key(bg_plugin_type_t type)
  {
  int i = 0;
  while(default_keys[i].key)
    {
    if(type == default_keys[i].type)
      return default_keys[i].key;
    i++;
    }
  return (const char*)0;
  }

void bg_plugin_registry_set_default(bg_plugin_registry_t * r,
                                    bg_plugin_type_t type,
                                    const char * name)
  {
  const char * key;

  key = get_default_key(type);
  if(key)
    bg_cfg_section_set_parameter_string(r->config_section, key, name);
  }

const bg_plugin_info_t * bg_plugin_registry_get_default(bg_plugin_registry_t * r,
                                                        bg_plugin_type_t type)
  {
  const char * key;
  const char * name = (const char*)0;
  const bg_plugin_info_t * ret;
  
  key = get_default_key(type);
  if(key)  
    bg_cfg_section_get_parameter_string(r->config_section, key, &name);

  if(!name)
    {
    return find_by_priority(r->entries,
                            type, BG_PLUGIN_ALL);
    }
  else
    {
    ret = bg_plugin_find_by_name(r, name);
    if(!ret)
      ret = find_by_priority(r->entries,
                            type, BG_PLUGIN_ALL);
    return ret;
    }
  }



void bg_plugin_ref(bg_plugin_handle_t * h)
  {
  bg_plugin_lock(h);
  h->refcount++;

  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "bg_plugin_ref %s: %d", h->info->name, h->refcount);
  

  bg_plugin_unlock(h);
  
  }

static void unload_plugin(bg_plugin_handle_t * h)
  {
  bg_cfg_section_t * section;
 
  if(h->plugin->get_parameter)
    {
    section = bg_plugin_registry_get_section(h->plugin_reg, h->info->name);
    bg_cfg_section_get(section,
                         h->plugin->get_parameters(h->priv),
                       h->plugin->get_parameter,
                         h->priv);
    }
  switch(h->info->api)
    {
    case BG_PLUGIN_API_GMERLIN:
      if(h->priv && h->plugin->destroy)
        h->plugin->destroy(h->priv);
      break;
    case BG_PLUGIN_API_LADSPA:
      bg_ladspa_unload(h);
      break;
    }
  
  if(h->dll_handle)
    dlclose(h->dll_handle);
  free(h);
  }

void bg_plugin_unref_nolock(bg_plugin_handle_t * h)
  {
  int refcount;

  h->refcount--;
  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "bg_plugin_unref %s: %d", h->info->name, h->refcount);

  refcount = h->refcount;

  if(!refcount)
    unload_plugin(h);
  }

void bg_plugin_unref(bg_plugin_handle_t * h)
  {
  int refcount;
  bg_plugin_lock(h);
  h->refcount--;
  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "bg_plugin_unref %s: %d", h->info->name, h->refcount);

  refcount = h->refcount;
  bg_plugin_unlock(h);
  if(!refcount)
    unload_plugin(h);
  }

gavl_video_frame_t *
bg_plugin_registry_load_image(bg_plugin_registry_t * r,
                              const char * filename,
                              gavl_video_format_t * format)
  {
  const bg_plugin_info_t * info;
  
  bg_image_reader_plugin_t * ir;
  bg_plugin_handle_t * handle = (bg_plugin_handle_t *)0;
  gavl_video_frame_t * ret = (gavl_video_frame_t*)0;
  
  info = bg_plugin_find_by_filename(r, filename, BG_PLUGIN_IMAGE_READER);

  if(!info)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "No plugin found for image %s", filename);
    goto fail;
    }
  
  handle = bg_plugin_load(r, info);
  if(!handle)
    goto fail;
  
  ir = (bg_image_reader_plugin_t*)(handle->plugin);

  if(!ir->read_header(handle->priv, filename, format))
    goto fail;
  
  ret = gavl_video_frame_create(format);
  if(!ir->read_image(handle->priv, ret))
    goto fail;
  
  bg_plugin_unref(handle);
  return ret;

  fail:
  if(ret)
    gavl_video_frame_destroy(ret);
  return (gavl_video_frame_t*)0;
  }

void
bg_plugin_registry_save_image(bg_plugin_registry_t * r,
                              const char * filename,
                              gavl_video_frame_t * frame,
                              const gavl_video_format_t * format)
  {
  const bg_plugin_info_t * info;
  gavl_video_format_t tmp_format;
  gavl_video_converter_t * cnv;
  bg_image_writer_plugin_t * iw;
  bg_plugin_handle_t * handle = (bg_plugin_handle_t *)0;
  gavl_video_frame_t * tmp_frame = (gavl_video_frame_t*)0;
  
  info = bg_plugin_find_by_filename(r, filename, BG_PLUGIN_IMAGE_WRITER);

  cnv = gavl_video_converter_create();
  
  if(!info)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "No plugin found for image %s", filename);
    goto fail;
    }
  
  handle = bg_plugin_load(r, info);
  if(!handle)
    goto fail;
  
  iw = (bg_image_writer_plugin_t*)(handle->plugin);

  gavl_video_format_copy(&tmp_format, format);
  
  if(!iw->write_header(handle->priv, filename, &tmp_format))
    goto fail;

  if(gavl_video_converter_init(cnv, format, &tmp_format))
    {
    tmp_frame = gavl_video_frame_create(&tmp_format);
    gavl_video_convert(cnv, frame, tmp_frame);
    if(!iw->write_image(handle->priv, tmp_frame))
      goto fail;
    }
  else
    {
    if(!iw->write_image(handle->priv, frame))
      goto fail;
    }
  bg_plugin_unref(handle);
  fail:
  if(tmp_frame)
    gavl_video_frame_destroy(tmp_frame);
  gavl_video_converter_destroy(cnv);
  }


bg_plugin_handle_t * bg_plugin_load(bg_plugin_registry_t * reg,
                                    const bg_plugin_info_t * info)
  {
  bg_plugin_handle_t * ret;
  bg_parameter_info_t * parameters;
  bg_cfg_section_t * section;

  if(!info)
    return (bg_plugin_handle_t*)0;
  
  ret = calloc(1, sizeof(*ret));

  ret->plugin_reg = reg;
  
  pthread_mutex_init(&(ret->mutex),(pthread_mutexattr_t *)0);

  if(info->module_filename)
    {
    /* We need all symbols global because some plugins might reference them */
    ret->dll_handle = dlopen(info->module_filename, RTLD_NOW | RTLD_GLOBAL);
    if(!ret->dll_handle)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot dlopen plugin %s: %s", info->module_filename,
              dlerror());
      goto fail;
      }

    switch(info->api)
      {
      case BG_PLUGIN_API_GMERLIN:
        if(!check_plugin_version(ret->dll_handle))
          {
          bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Plugin %s has no or wrong version",
                 info->module_filename);
          goto fail;
          }
        ret->plugin = dlsym(ret->dll_handle, "the_plugin");
        if(!ret->plugin)
          goto fail;
        ret->priv = ret->plugin->create();
        break;
      case BG_PLUGIN_API_LADSPA:
        if(!bg_ladspa_load(ret, info))
          goto fail;
      }
    }
  else if(reg->singlepic_input &&
          !strcmp(reg->singlepic_input->name, info->name))
    {
    ret->plugin = bg_singlepic_input_get();
    ret->priv = bg_singlepic_input_create(reg);
    }
  else if(reg->singlepic_stills_input &&
          !strcmp(reg->singlepic_stills_input->name, info->name))
    {
    ret->plugin = bg_singlepic_stills_input_get();
    ret->priv = bg_singlepic_stills_input_create(reg);
    }
  else if(reg->singlepic_encoder &&
          !strcmp(reg->singlepic_encoder->name, info->name))
    {
    ret->plugin = bg_singlepic_encoder_get();
    ret->priv = bg_singlepic_encoder_create(reg);
    }
  ret->info = info;

  /* Apply saved parameters */

  if(ret->plugin->get_parameters)
    {
    parameters = ret->plugin->get_parameters(ret->priv);
    
    section = bg_plugin_registry_get_section(reg, info->name);
    
    bg_cfg_section_apply(section, parameters, ret->plugin->set_parameter,
                         ret->priv);
    }
  bg_plugin_ref(ret);
  return ret;

fail:
  pthread_mutex_destroy(&(ret->mutex));
  if(ret->dll_handle)
    dlclose(ret->dll_handle);
  free(ret);
  return (bg_plugin_handle_t*)0;
  }

void bg_plugin_lock(bg_plugin_handle_t * h)
  {
  pthread_mutex_lock(&(h->mutex));
  }

void bg_plugin_unlock(bg_plugin_handle_t * h)
  {
  pthread_mutex_unlock(&(h->mutex));
  }

void bg_plugin_registry_add_device(bg_plugin_registry_t * reg,
                                   const char * plugin_name,
                                   const char * device,
                                   const char * name)
  {
  bg_plugin_info_t * info;

  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;

  info->devices = bg_device_info_append(info->devices,
                                        device, name);

  bg_plugin_registry_save(reg->entries);
  }

void bg_plugin_registry_set_device_name(bg_plugin_registry_t * reg,
                                        const char * plugin_name,
                                        const char * device,
                                        const char * name)
  {
  int i;
  bg_plugin_info_t * info;

  info = find_by_name(reg->entries, plugin_name);
  if(!info || !info->devices)
    return;
  
  i = 0;
  while(info->devices[i].device)
    {
    if(!strcmp(info->devices[i].device, device))
      {
      info->devices[i].name = bg_strdup(info->devices[i].name, name);
      bg_plugin_registry_save(reg->entries);
      return;
      }
    i++;
    }
  
  }

static int my_strcmp(const char * str1, const char * str2)
  {
  if(!str1 && !str2)
    return 0;
  else if(str1 && str2)
    return strcmp(str1, str2); 
  return 1;
  }

void bg_plugin_registry_remove_device(bg_plugin_registry_t * reg,
                                      const char * plugin_name,
                                      const char * device,
                                      const char * name)
  {
  bg_plugin_info_t * info;
  int index;
  int num_devices;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
    
  index = -1;
  num_devices = 0;
  while(info->devices[num_devices].device)
    {
    if(!my_strcmp(info->devices[num_devices].name, name) &&
       !strcmp(info->devices[num_devices].device, device))
      {
      index = num_devices;
      }
    num_devices++;
    }


  if(index != -1)
    memmove(&(info->devices[index]), &(info->devices[index+1]),
            sizeof(*(info->devices)) * (num_devices - index));
    
  bg_plugin_registry_save(reg->entries);
  }

void bg_plugin_registry_find_devices(bg_plugin_registry_t * reg,
                                     const char * plugin_name)
  {
  bg_plugin_info_t * info;
  bg_plugin_handle_t * handle;
  
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;

  handle = bg_plugin_load(reg, info);
    
  bg_device_info_destroy(info->devices);
  info->devices = (bg_device_info_t*)0;
  
  if(!handle || !handle->plugin->find_devices)
    return;

  info->devices = handle->plugin->find_devices();
  bg_plugin_registry_save(reg->entries);
  bg_plugin_unref(handle);
  }

char ** bg_plugin_registry_get_plugins(bg_plugin_registry_t*reg,
                                       uint32_t type_mask,
                                       uint32_t flag_mask)
  {
  int num_plugins, i;
  char ** ret;
  const bg_plugin_info_t * info;
  
  num_plugins = bg_plugin_registry_get_num_plugins(reg, type_mask, flag_mask);
  ret = calloc(num_plugins + 1, sizeof(char*));
  for(i = 0; i < num_plugins; i++)
    {
    info = bg_plugin_find_by_index(reg, i, type_mask, flag_mask);
    ret[i] = bg_strdup(NULL, info->name);
    }
  return ret;
  
  }

void bg_plugin_registry_free_plugins(char ** plugins)
  {
  int index = 0;
  if(!plugins)
    return;
  while(plugins[index])
    {
    free(plugins[index]);
    index++;
    }
  free(plugins);
  
  }

static void load_plugin(bg_plugin_registry_t * reg,
                        const bg_plugin_info_t * info,
                        bg_plugin_handle_t ** ret)
  {
  if(!(*ret) || strcmp((*ret)->info->name, info->name))
    {
    if(*ret)
      {
      bg_plugin_unref(*ret);
      *ret = (bg_plugin_handle_t*)0;
      }
    *ret = bg_plugin_load(reg, info);
    }
  }

int bg_input_plugin_load(bg_plugin_registry_t * reg,
                         const char * location,
                         const bg_plugin_info_t * info,
                         bg_plugin_handle_t ** ret,
                         bg_input_callbacks_t * callbacks)
  {
  const char * real_location;
  char * protocol = (char*)0, * path = (char*)0;
  
  int num_plugins, i;
  uint32_t flags;
  bg_input_plugin_t * plugin;
  int try_and_error = 1;
  const bg_plugin_info_t * first_plugin = (const bg_plugin_info_t*)0;
  
  
  if(!location)
    return 0;
  
  real_location = location;
  
  if(!info) /* No plugin given, seek one */
    {
    if(bg_string_is_url(location))
      {
      if(bg_url_split(location,
                      &protocol,
                      (char **)0, // user,
                      (char **)0, // password,
                      (char **)0, // hostname,
                      (int *)0,   //  port,
                      &path))
        {
        info = bg_plugin_find_by_protocol(reg, protocol);
        if(info)
          {
          if(info->flags & BG_PLUGIN_REMOVABLE)
            real_location = path;
          }
        }
      }
    else if(!strcmp(location, "-"))
      {
      info = bg_plugin_find_by_protocol(reg, "stdin");
      }
    else
      {
      info = bg_plugin_find_by_filename(reg, real_location,
                                        (BG_PLUGIN_INPUT));
      }
    first_plugin = info;
    }
  else
    try_and_error = 0; /* We never try other plugins than the given one */
  
  if(info)
    {
    /* Try to load this */

    load_plugin(reg, info, ret);

    if(!(*ret))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, TRS("Loading plugin \"%s\" failed"),
                                           info->long_name);
      return 0;
      }
    
    plugin = (bg_input_plugin_t*)((*ret)->plugin);

    if(plugin->set_callbacks)
      plugin->set_callbacks((*ret)->priv, callbacks);
    
    if(!plugin->open((*ret)->priv, real_location))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, TRS("Opening %s with \"%s\" failed"),
             real_location, info->long_name);
      }
    else
      {
      if(protocol) free(protocol);
      if(path)     free(path);
      return 1;
      }
    }
  
  if(protocol) free(protocol);
  if(path)     free(path);
  
  if(!try_and_error)
    return 0;
  
  flags = bg_string_is_url(location) ? BG_PLUGIN_URL : BG_PLUGIN_FILE;
  
  num_plugins = bg_plugin_registry_get_num_plugins(reg,
                                                   BG_PLUGIN_INPUT, flags);
  for(i = 0; i < num_plugins; i++)
    {
    info = bg_plugin_find_by_index(reg, i, BG_PLUGIN_INPUT, flags);

    if(info == first_plugin)
      continue;
        
    load_plugin(reg, info, ret);

    if(!*ret)
      continue;
    
    plugin = (bg_input_plugin_t*)((*ret)->plugin);
    if(!plugin->open((*ret)->priv, location))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, TRS("Opening %s with \"%s\" failed"),
             location, info->long_name);
      }
    else
      {
      return 1;
      }
    }
  return 0;
  }

void
bg_plugin_registry_set_encode_audio_to_video(bg_plugin_registry_t * reg,
                                                  int audio_to_video)
  {
  reg->encode_audio_to_video = audio_to_video;
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_audio_to_video", audio_to_video);
  }

int
bg_plugin_registry_get_encode_audio_to_video(bg_plugin_registry_t * reg)
  {
  return reg->encode_audio_to_video;
  }


void
bg_plugin_registry_set_encode_subtitle_text_to_video(bg_plugin_registry_t * reg,
                                                  int subtitle_text_to_video)
  {
  reg->encode_subtitle_text_to_video = subtitle_text_to_video;
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_subtitle_text_to_video",
                                   subtitle_text_to_video);
  }

int
bg_plugin_registry_get_encode_subtitle_text_to_video(bg_plugin_registry_t * reg)
  {
  return reg->encode_subtitle_text_to_video;
  }

void bg_plugin_registry_set_encode_subtitle_overlay_to_video(bg_plugin_registry_t * reg,
                                                  int subtitle_overlay_to_video)
  {
  reg->encode_subtitle_overlay_to_video = subtitle_overlay_to_video;
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_subtitle_overlay_to_video",
                                   subtitle_overlay_to_video);
  }

int bg_plugin_registry_get_encode_subtitle_overlay_to_video(bg_plugin_registry_t * reg)
  {
  return reg->encode_subtitle_overlay_to_video;
  }



void bg_plugin_registry_set_encode_pp(bg_plugin_registry_t * reg,
                                      int use_pp)
  {
  reg->encode_pp = use_pp;
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_pp", use_pp);
  }

int bg_plugin_registry_get_encode_pp(bg_plugin_registry_t * reg)
  {
  return reg->encode_pp;
  }


int bg_plugin_equal(bg_plugin_handle_t * h1,
                     bg_plugin_handle_t * h2)
  {
  return h1 == h2;
  }

void bg_plugin_registry_set_parameter_info(bg_plugin_registry_t * reg,
                                           uint32_t type_mask,
                                           uint32_t flag_mask,
                                           bg_parameter_info_t * ret)
  {
  int num_plugins, i;
  const bg_plugin_info_t * info;
  
  num_plugins =
    bg_plugin_registry_get_num_plugins(reg, type_mask, flag_mask);

  ret->multi_names      = calloc(num_plugins + 1, sizeof(*ret->multi_names));
  ret->multi_labels     = calloc(num_plugins + 1, sizeof(*ret->multi_labels));
  ret->multi_parameters = calloc(num_plugins + 1,
                                 sizeof(*ret->multi_parameters));

  ret->multi_descriptions = calloc(num_plugins + 1,
                                   sizeof(*ret->multi_descriptions));
  
  for(i = 0; i < num_plugins; i++)
    {
    info = bg_plugin_find_by_index(reg, i,
                                   type_mask, flag_mask);
    ret->multi_names[i] = bg_strdup(NULL, info->name);

    /* First plugin is the default one */
    if(!i && (ret->type != BG_PARAMETER_MULTI_CHAIN)) 
      {
      ret->val_default.val_str = bg_strdup(NULL, info->name);
      }
    
    bg_bindtextdomain(info->gettext_domain, info->gettext_directory);
    ret->multi_descriptions[i] = bg_strdup(NULL, TRD(info->description,
                                                     info->gettext_domain));
    
    ret->multi_labels[i] = bg_strdup(NULL, TRD(info->long_name,
                                               info->gettext_domain));
    
    if(info->parameters)
      {
      ret->multi_parameters[i] =
        bg_parameter_info_copy_array(info->parameters);
      }
    }
  }
