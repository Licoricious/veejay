/* libvjnet - Linux VeeJay
 * 	     (C) 2002-2007 Niels Elburg <nelburg@looze.net> 
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

#include <config.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdio.h>
#include <libvjnet/vj-client.h>
#include <veejay/vims.h>
#include <libvjmsg/vj-msg.h>
#include <libvjmem/vjmem.h>
#include <libvjnet/cmd.h>
#include <libvjnet/mcastreceiver.h>
#include <libvjnet/mcastsender.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <liblzo/lzo.h>
#ifdef STRICT_CHECKING
#include <assert.h>
#endif
#define VJC_OK 0
#define VJC_NO_MEM 1
#define VJC_SOCKET 2
#define VJC_BAD_HOST 3

#define PACKET_LEN (65535*32)

vj_client *vj_client_alloc( int w, int h, int f )
{
	vj_client *v = (vj_client*) malloc(sizeof(vj_client));
	memset(v, 0, sizeof(vj_client));
	if(!v)
	{
		return NULL;
	}
	v->cur_width = w;
	v->cur_height = h;
	v->cur_fmt = f;
	v->space = NULL;
//	v->lzo = lzo_new();
	v->c = (conn_type_t**) malloc(sizeof(conn_type_t*) * 3);
	v->c[0] = (conn_type_t*) malloc(sizeof(conn_type_t));
	v->c[1] = (conn_type_t*) malloc(sizeof(conn_type_t));
	v->blob = (unsigned char*) malloc(sizeof(unsigned char) * PACKET_LEN ); 
	v->mcast = 0;
	if( w > 0 && h > 0 )
		v->space = (uint8_t*) malloc( sizeof(uint8_t) * SOCKETFRAMELEN );
	return v;
}

void		vj_client_free(vj_client *v)
{
	if(v)
	{
		if(v->c[0])
			free(v->c[0]);
		if(v->c[1])
			free(v->c[1]);
		if(v->c) 
			free(v->c);
		if(v->blob)
			free(v->blob);
		if(v->lzo)
			free(v->lzo);
		if(v->space)
			free(v->space);
		free(v);
	}
}

int	vj_client_window_sizes( int socket_fd, int *r, int *s )
{
	int tmp = sizeof(int);
	if( getsockopt( socket_fd, SOL_SOCKET, SO_SNDBUF,(unsigned char*) s, &tmp) == -1 )
        {
                veejay_msg(0, "Cannot read socket buffer size: %s", strerror(errno));
                return 0;
        }
        if( getsockopt( socket_fd, SOL_SOCKET, SO_RCVBUF, (unsigned char*) r, &tmp) == -1 )
        {
                veejay_msg(0, "Cannot read socket buffer receive size %s" , strerror(errno));
                return 0;
        }
	return 1;
}

void	vj_client_flush(vj_client *v, int num_frames)
{
	if(vj_client_poll(v, V_STATUS ))
	{
		char status[100];
		int bytes = 100;
		int n = vj_client_read( v, V_STATUS,status,bytes);
		if( n > 0 )
		{	num_frames --;	
		}
	
	}
}

int vj_client_connect_dat(vj_client *v, char *host, int port_id  )
{
	int error = 0;
	if(host == NULL)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid host name");
		return 0;
	}
	if(port_id < 1 || port_id > 65535)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid port number. Use [1-65535]");
		return 0;
	}


	v->c[0]->type = VSOCK_C;
	v->c[0]->fd   = alloc_sock_t();
	  free(v->c[1]);
	v->c[1] = NULL;
	if( sock_t_connect( v->c[0]->fd, host, port_id + 5  ) )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Connect to DAT port %d", port_id + 5);	
		return 1;
	}
	return error;
}

int vj_client_connect(vj_client *v, char *host, char *group_name, int port_id  )
{
	int error = 0;
	
	if( group_name == NULL )
	{

		if(host == NULL)
			return 0;
		if(port_id <= 0 || port_id > 65535)
			return 0;

		v->c[0]->type = VSOCK_C;
		v->c[0]->fd   = alloc_sock_t();
		v->c[1]->type = VSOCK_S;
		v->c[1]->fd   = alloc_sock_t();
		if(!v->c[0]->fd || !v->c[1]->fd )
		{
			return 0;
		}
		if( sock_t_connect( v->c[0]->fd, host, port_id + VJ_CMD_PORT ) )
		{
			if( sock_t_connect( v->c[1]->fd, host, port_id + VJ_STA_PORT ) )
			{		
				return 1;
			}
		}
	}
	else
	{
		if(port_id <= 0 || port_id > 65535 )
			return 0;

		v->c[0]->type = VMCAST_C;
		v->c[0]->r    = mcast_new_receiver( group_name, port_id + VJ_CMD_MCAST ); 
		v->c[0]->s    = mcast_new_sender( group_name );
		v->ports[0] = port_id + VJ_CMD_MCAST;
		v->ports[1] = port_id + VJ_CMD_MCAST_IN;
fprintf(stderr, "%s: receiver at port %d (v->ports[0] = %d, v->ports[1] = %d)",
		__FUNCTION__, port_id + VJ_CMD_MCAST, v->ports[0], v->ports[1] );

		mcast_sender_set_peer( v->c[0]->s , group_name );
		v->mcast = 1;
//		mcast_receiver_set_peer( v->c[0]->r, group_name);
//		veejay_msg(VEEJAY_MSG_DEBUG, "Added peer");
		veejay_msg(VEEJAY_MSG_DEBUG, "Client is interested in packets from group %s : %d, send to %d",
			group_name, port_id + VJ_CMD_MCAST , port_id + VJ_CMD_MCAST_IN);

		return 1;
	}
	return error;
}

int	vj_client_poll_w( vj_client *v, int sock_type )
{
	if(sock_type == V_STATUS )
	{
		if(v->c[1]->type == VSOCK_S)
			return ( sock_t_poll_w(v->c[1]->fd ) );
	}
	if(sock_type == V_CMD )
	{
		if(v->c[0]->type == VSOCK_C)
			return	( sock_t_poll_w( v->c[0]->fd ));
	}
	return 0;
}

int	vj_client_poll( vj_client *v, int sock_type )
{
	if(sock_type == V_STATUS )
	{
		if(v->c[1]->type == VSOCK_S)
			return ( sock_t_poll(v->c[1]->fd ) );
	}
	if(sock_type == V_CMD )
	{
		if(v->c[0]->type == VSOCK_C)
			return	( sock_t_poll( v->c[0]->fd ));
		if(v->c[0]->type == VMCAST_C )
			return  ( mcast_poll( v->c[0]->r ));
	}
	return 0;
}

static	void	vj_client_decompress( vj_client *t, uint8_t *out, int len, int Y, int UV)
{
	uint8_t *d[3] = {
			out,
			out + Y,
			out + Y + UV };
#ifdef STRICT_CHECKING
	assert( len > 0 );
	assert( Y > 0 );
	assert( UV > 0 );
#endif
	lzo_decompress( t->lzo, t->space, len, d );
}

int	vj_client_read_i( vj_client *v, uint8_t *dst, int len )
{
	char line[32];
	int p[4] = {0, 0,0,0 };
	int n = 0;
	int plen = 0;
	int conv = 1;
	int y_len = 0;
	int uv_len = 0;

	if( v->c[0]->type == VMCAST_C )
	{
		plen = mcast_recv_frame( v->c[0]->r, v->space, 0, v->cur_width,v->cur_height,v->cur_fmt,
				&p[0],&p[1],&p[2] );
		if(plen <= 0)
			return 0;

		v->in_width = p[0];
		v->in_height = p[1];
		v->in_fmt = p[2];

		uv_len = 0;
		y_len = p[0]  * p[1];
		switch(v->in_fmt )
		{
			case FMT_420F:
			case FMT_420:
				uv_len = y_len/4; break;
			default:
				uv_len = y_len/2;break;
		}
			
		if(plen)
			vj_client_decompress( v, dst,plen,y_len,uv_len );

		if( p[0] != v->cur_width || p[1] != v->cur_height || p[2] != v->cur_fmt )
			return 2;
		return 1;
	} else if ( v->c[0]->type == VSOCK_C )
	{
		veejay_memset( line,0, sizeof(line));

		if( sock_t_poll( v->c[0]->fd ) <= 0)
		{
			veejay_msg(VEEJAY_MSG_DEBUG, "Nothing has arrived yet");
			return -1;
		}
		if( v->c[0]->type == VSOCK_C )
			plen = sock_t_recv_w( v->c[0]->fd, line, 20 );	

		if( plen <= 0 )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Read error: %s", strerror(errno));
			return plen;
		}
		n = sscanf( line, "%d %d %d %d", &p[0],&p[1],&p[2],&p[3] );
		if( n != 4)
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Frame header invalid, received garbage" );
			return -1;
		}

		if( v->cur_width != p[0] || v->cur_height != p[1] || v->cur_fmt != p[2])
			conv = 2;

		v->in_width = p[0];
		v->in_height = p[1];
		v->in_fmt = p[2];
		uv_len = 0;
		y_len = p[0]  * p[1];
		switch(v->in_fmt )
		{
			case FMT_420F:
			case FMT_420:
				uv_len = y_len/4; break;
			default:
				uv_len = y_len/2;break;
		}

		if( sock_t_poll( v->c[0]->fd ) <= 0 )
		{
			veejay_msg(VEEJAY_MSG_DEBUG, "Nothing has arrived yet but we already got some");
			return -1;
		}
		if( v->c[0]->type == VSOCK_C) 
		{
			int n = sock_t_recv_w( v->c[0]->fd, v->space, p[3] );
			if(n)
				vj_client_decompress( v, dst, n, y_len, uv_len );
		}
		if(n > 0 )
			plen += n;


		return conv;
	}
veejay_msg(VEEJAY_MSG_DEBUG, "socket is not used for this");
	return 0;
}

int	vj_client_get_status_fd(vj_client *v, int sock_type )
{	
	if(sock_type == V_STATUS)
	{
		vj_sock_t *c = v->c[1]->fd;
		return c->sock_fd;
	}
	if(sock_type == V_CMD )
	{
		if(!v->mcast)
		{
			vj_sock_t *c = v->c[0]->fd;
			return	c->sock_fd;
		}
		else
		{
			mcast_receiver *c = v->c[0]->r;
			return  c->sock_fd;
		}
	}
	return 0;	
}
int	vj_client_read_no_wait(vj_client *v, int sock_type, uint8_t *dst, int bytes )
{
	if( sock_type == V_STATUS )
	{
		if(v->c[1]->type == VSOCK_S)
			return( sock_t_recv( v->c[1]->fd, dst, bytes ) );
	}
	if( sock_type == V_CMD )
	{
		if(v->c[0]->type == VSOCK_C)
			return ( sock_t_recv( v->c[0]->fd, dst, bytes ) );
	}
	return 0;
}

int	vj_client_read(vj_client *v, int sock_type, uint8_t *dst, int bytes )
{
	if( sock_type == V_STATUS )
	{
		if(v->c[1]->type == VSOCK_S)
			return( sock_t_recv_w( v->c[1]->fd, dst, bytes ) );
	}
	if( sock_type == V_CMD )
	{
		if(v->c[0]->type == VSOCK_C)
			return ( sock_t_recv_w( v->c[0]->fd, dst, bytes ) );
	}
	return 0;
}

int vj_client_send(vj_client *v, int sock_type,char *buf )
{
	if( sock_type == V_CMD )
	{

		// format msg
		int len = strlen( buf );
		sprintf(v->blob, "V%03dD%s", len, buf);
		if(v->c[0]->type == VSOCK_C)
			return ( sock_t_send( v->c[0]->fd, v->blob, len + 5 ));
		if(v->c[0]->type == VMCAST_C)
			return ( mcast_send( v->c[0]->s, (void*) v->blob, len + 5,
					v->ports[1] ));
	}
   	return 1;
}

int vj_client_send_buf(vj_client *v, int sock_type,unsigned char *buf, int len )
{
	if( sock_type == V_CMD )
	{
		// format msg
		sprintf(v->blob, "V%03dD", len);
		veejay_memcpy( v->blob+5, buf, len );
		if(v->c[0]->type == VSOCK_C)
			return ( sock_t_send( v->c[0]->fd, v->blob, len + 5 ));
		if(v->c[0]->type == VMCAST_C)
			return ( mcast_send( v->c[0]->s, (void*) v->blob, len + 5,
					v->ports[1] ));
	}
   	return 1;
}

int vj_client_send_bufX(vj_client *v, int sock_type,unsigned char *buf, int len )
{
	if( sock_type == V_CMD )
	{

		// format msg
		sprintf(v->blob, "K%08d", len);
		veejay_memcpy( v->blob+9, buf, len );
		if(v->c[0]->type == VSOCK_C)
			return ( sock_t_send( v->c[0]->fd, v->blob, len + 9 ));
		if(v->c[0]->type == VMCAST_C)
			return ( mcast_send( v->c[0]->s, (void*) v->blob, len + 9,
					v->ports[1] ));
	}
   	return 1;
}

int vj_client_close( vj_client *v )
{
	if(v)
	{
		if(v->c[0])
		{
			if(v->c[0]->type == VSOCK_C)
				sock_t_close(v->c[0]->fd );
			else if ( v->c[0]->type == VMCAST_C )
			{
				mcast_close_receiver( v->c[0]->r );
				mcast_close_sender( v->c[0]->s );
			}
		}
		if(v->c[1])
		{
			if(v->c[1]->type == VSOCK_S)
				sock_t_close(v->c[1]->fd );
		}

		return 1;
	}
	return 0;
}

int	vj_client_test(char *host, int port)
{
	if( h_errno == HOST_NOT_FOUND )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Specified host '%s':'%d' is unknown", host,port );
		return 0;
	}

	if( h_errno == NO_ADDRESS || h_errno == NO_DATA )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Specified host '%s' is valid but does not have IP address",
			host );
		return 0;
	}
	if( h_errno == NO_RECOVERY )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Non recoverable name server error occured");
		return 0;
	}
	if( h_errno == TRY_AGAIN )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Temporary error occurred on an authoritative name. Try again later");
		return 0;
	}
	return 1;
}
