/* vjnet - low level network I/O for VeeJay
 *
 *           (C) 2005-2007 Niels Elburg <nwelburg@gmail.com> 
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef MCASTRECEIVER_H
#define MCASTRECEIVER_H
typedef struct
{
	char *group;
	int	addr_len;
	struct sockaddr_in addr;	
	int	port;
	int	sock_fd;
	uint8_t	*space;
	int space_len;
	void *next;
} mcast_receiver;

mcast_receiver *mcast_new_receiver( const char *group_name, int port );
int	mcast_poll( mcast_receiver *v );
int	mcast_recv( mcast_receiver *v, void *dst, int len );
uint8_t	*mcast_recv_frame( mcast_receiver *v, int *len, int *hdrlen, uint8_t *recvbuf );
void mcast_close_receiver( mcast_receiver *v );
#endif
