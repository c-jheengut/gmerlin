/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2008 Members of the Gmerlin project
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

#include <netdb.h> /* gethostbyname */
                                                                               
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>


#include <avdec_private.h>

#define LOG_DOMAIN "udp"

int bgav_udp_open_read(const bgav_options_t * opt, int port)
  {
  int ret;
  struct sockaddr_in name;
  //  struct addrinfo * addr;
  //  addr = bgav_hostbyname(opt, (const char * hostname)0, port, SOCK_DGRAM);

  /* Create the socket */
  if((ret = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
    bgav_log(opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Cannot create socket");
    return -1;
    }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  
  if (bind(ret, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
    bgav_log(opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Cannot bind inet socket: %s", strerror(errno));
    return -1;
    }

  bgav_log(opt, BGAV_LOG_INFO, LOG_DOMAIN, "UDP Socket bound on port %d\n", port);

  //  freeaddrinfo(addr);
  return ret;
  }

int bgav_udp_read(int fd, uint8_t * data, int len)
  {
  int bytes_read;
  for(;;)
    {
    bytes_read = recv(fd, data, len, 0);
    if (bytes_read < 0)
      {
      if((errno == EAGAIN) ||
         (errno == EINTR))
        return -1;
      }
    else
      break;
    }
  return bytes_read;
  }

#if 0
int bgav_udp_open_write(int port)
  {
  int ret;
  }
#endif
