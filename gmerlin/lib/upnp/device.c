/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include <upnp_device.h>
#include <string.h>
#include <gmerlin/http.h>

static void send_description(int fd, const char * desc)
  {
  int len;
  gavl_metadata_t res;
  len = strlen(desc);
  bg_http_response_init(&res, "HTTP/1.1", 200, "OK");
  gavl_metadata_set_int(&res, "CONTENT-LENGTH", len);
  gavl_metadata_set(&res, "CONTENT-TYPE", "text/xml; charset=UTF-8");
  gavl_metadata_set(&res, "CONNECTION", "close");

  if(bg_http_response_write(fd, &res))
    goto fail;
  
  bg_socket_write_data(fd, (const uint8_t*)desc, len);
  fail:
  gavl_metadata_free(&res);
  }

int
bg_upnp_device_handle_request(bg_upnp_device_t * dev, int fd,
                              const char * method,
                              const char * path,
                              const gavl_metadata_t * header)
  {
  int i;
  const char * pos;
  
  if(strncmp(path, "/upnp/", 6))
    return 0;

  path += 6;

  /* Check for description */

  if(!strcmp(path, "desc.xml"))
    {
    send_description(fd, dev->description);
    return 1;
    }

  pos = strchr(path, '/');
  if(!pos)
    return 0;
  
  
  for(i = 0; i < dev->num_services; i++)
    {
    if((strlen(dev->services[i].name) == (pos - path)) &&
       (!strncmp(dev->services[i].name, path, pos - path)))
      {
      /* Found service */
      path = pos + 1;

      if(!strcmp(path, "desc.xml"))
        {
        /* Send service description */
        send_description(fd, dev->services[i].description);
        return 1;
        }
      else if(!strcmp(path, "control"))
        {
        /* Service control */
        }
      else if(!strcmp(path, "event"))
        {
        /* Service events */
        }
      else
        return 0; // 404
      }
    }
  return 0;
  }

void
bg_upnp_device_destroy(bg_upnp_device_t * dev);