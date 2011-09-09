/* 
 * Linux VeeJay
 *
 * Copyright(C)2002-2006 Niels Elburg <nwelburg@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License , or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */

#define THREAD_START 0
#define THREAD_STOP 1
#include <config.h>
#include <stdint.h>
#include <pthread.h>
#include <libstream/vj-tag.h>
#include <libvjnet/vj-client.h>
#include <veejay/vims.h>
#include <libyuv/yuvconv.h>
#include <libvjmem/vjmem.h>
#include <libavutil/avutil.h>
#include <libvjmsg/vj-msg.h>
#include <veejay/vims.h>
#include <libstream/vj-net.h>
#include <liblzo/lzo.h>
#include <time.h>


#ifdef STRICT_CHECKING
#include <assert.h>
#endif
typedef struct
{
	pthread_mutex_t mutex;
	pthread_t thread;
	int state;
	int have_frame;
	int error;
	int grab;
	int repeat;
	int w;
	int h;
	int f;
	int in_fmt;
	int in_w;
	int in_h;
	int af;
} threaded_t;

static void lock_(threaded_t *t, const char *f, int line)
{
//	veejay_msg(0,"lock thread by %s, line %d",f,line);
	pthread_mutex_lock( &(t->mutex ));
}

static void unlock_(threaded_t *t, const char *f, int line)
{
//	veejay_msg(0,"unlock thread by %s, line %d",f,line);
	pthread_mutex_unlock( &(t->mutex ));
}

#define lock( t ) lock_( t, __FUNCTION__, __LINE__ )
#define unlock( t ) unlock_( t, __FUNCTION__ , __LINE__ )

#define MS_TO_NANO(a) (a *= 1000000)
static	void	net_delay(long nsec, long sec )
{
	struct timespec ts;
	ts.tv_sec = sec;
	ts.tv_nsec = MS_TO_NANO( nsec);
	nanosleep( &ts, NULL );
}


void	*reader_thread(void *data)
{
	vj_tag *tag = (vj_tag*) data;
	threaded_t *t = tag->priv;
	int ret = 0;
	char buf[16];

	snprintf(buf,sizeof(buf)-1, "%03d:;", VIMS_GET_FRAME);

	int retrieve = 0;
	int success  = 0;
	
	vj_client *v = vj_client_alloc( t->w, t->h, t->af );
	v->lzo = lzo_new();

	success = vj_client_connect_dat( v, tag->source_name,tag->video_channel );
	
	if( success > 0 ) {
			veejay_msg(VEEJAY_MSG_INFO, "Connecton established with %s:%d",tag->source_name,
				tag->video_channel + 5);
	}
	else if ( tag->source_type != VJ_TAG_TYPE_MCAST ) 
	{
		veejay_msg(0, "Unable to connect to %s: %d", tag->source_name, tag->video_channel+5);
		pthread_exit(&(t->thread));
		return NULL;
	}

	for( ;; )
	{
		int error    = 0;
		int pollres  = 0;
		int res      = 0;
		if( t->state == 0 )
		{
			vj_client_close(v);
			veejay_msg(VEEJAY_MSG_INFO, "Closed connection with %s: %d",
					tag->source_name,tag->video_channel+5);
			vj_client_free(v);
			pthread_exit( &(t->thread));
			return NULL;
		}
		
		lock(t);
	
		if(!error && t->grab && tag->source_type == VJ_TAG_TYPE_NET && retrieve == 0 ) {
			ret =  vj_client_send( v, V_CMD, buf );
			if( ret <= 0 )
			{
				veejay_msg(VEEJAY_MSG_DEBUG,
								"Error sending get frame command to %s:%d",
								tag->source_name,
								tag->video_channel );

				error = 1;
			}
			else
			{
				t->grab = 0;
			}
		}
		
		if(!error && tag->source_type == VJ_TAG_TYPE_NET )
		{
			res = vj_client_poll(v, V_CMD );
			if( ret )
			{
				retrieve = 1;
			} else if ( ret < 0 ) {
				veejay_msg(VEEJAY_MSG_DEBUG,
								"Error polling connection %s:%d",
								tag->source_name,
								tag->video_channel );
				error = 1;
			}

		} else if (tag->source_type == VJ_TAG_TYPE_MCAST )
		{
			error = 0;
			retrieve = 1;
			res = 1;
		}
	
		long wait_time = 0;
	
		if(!error && retrieve)
		{
			if( res )
			{
				ret = vj_client_read_i ( v, tag->socket_frame,tag->socket_len );
				if( ret <= 0 )
				{
					if( tag->source_type == VJ_TAG_TYPE_NET )
					{
						veejay_msg(VEEJAY_MSG_DEBUG,
								"Error reading video frame from %s:%d",
								tag->source_name,
								tag->video_channel );
						error = 1;
					}
				//	else
				//	{
				//		wait_time += 10;
				//	}
					ret = 0;
				}
				else
				{
					t->in_fmt = v->in_fmt;
					t->in_w   = v->in_width;
					t->in_h   = v->in_height;
					 t->have_frame = ret;
					 t->grab = 1;
					 retrieve =0;
				}
			}
			else
			{
				if(tag->source_type == VJ_TAG_TYPE_MCAST )
					wait_time = 15;
			}
		}
		unlock(t);

		if( wait_time )
		{	
			if ( wait_time > 15 )
				wait_time = 10;
			//net_delay( wait_time,0 );
			wait_time = 0;
		}

		if( error )
		{
			int success = 0;
			vj_client_close( v );
			vj_client_free( v);
			v = vj_client_alloc( t->w, t->h, t->af );
			if(!v) {
				pthread_exit( &(t->thread));
			}
			net_delay( 0,3 );

			if(tag->source_type == VJ_TAG_TYPE_MCAST )
				success = vj_client_connect( v,NULL,tag->source_name,tag->video_channel  );
			else
				success = vj_client_connect_dat( v, tag->source_name,tag->video_channel );
	
			if( success <= 0 )
			{
				wait_time = 4000;
#ifdef STRICT_CHECKING
				veejay_msg(VEEJAY_MSG_DEBUG, "Tried to connect to %s:%d code=%d", tag->source_name,tag->video_channel,success);
#endif
			}
			else
			{
				veejay_msg(VEEJAY_MSG_INFO, "Connecton re-established with %s:%d",tag->source_name,
				tag->video_channel + 5);
			}

			t->grab = 0;
			retrieve = 0;
		}
	}
	return NULL;
}



void	*net_threader( )
{
	threaded_t *t = (threaded_t*) vj_calloc(sizeof(threaded_t));
	return (void*) t;
}

int	net_thread_get_frame( vj_tag *tag, uint8_t *buffer[3] )
{
	threaded_t *t = (threaded_t*) tag->priv;
	const uint8_t *buf = tag->socket_frame;
	
	lock(t);
	if( t->state == 0 || t->error  )
	{
		if(t->repeat < 0)
			veejay_msg(VEEJAY_MSG_INFO, "Connection closed with remote host");
		t->repeat++;
		unlock(t);
		return 0;
	}

	//@ color space convert frame	
	int len = t->w * t->h;
	int uv_len = len;
	switch(t->f)
	{
		case PIX_FMT_YUV420P:
		case PIX_FMT_YUVJ420P:
		uv_len=len/4;
		break;
		default:
			uv_len=len/2;
		break;
	}

	if(t->have_frame == 1 )
	{
		veejay_memcpy(buffer[0], tag->socket_frame, len );
		veejay_memcpy(buffer[1], tag->socket_frame+len, uv_len );
		veejay_memcpy(buffer[2], tag->socket_frame+len+uv_len, uv_len );
		t->grab = 1;
		unlock(t);
		return 1;
	}
	else if(t->have_frame == 2 )
	{
		VJFrame *a = NULL;
		VJFrame *b = NULL;

		if( t->in_fmt == PIX_FMT_RGB24 || t->in_fmt == PIX_FMT_BGR24 || t->in_fmt == PIX_FMT_RGBA ||
				t->in_fmt == PIX_FMT_RGB32_1 ) {
		
			a = yuv_rgb_template( tag->socket_frame, t->in_w, t->in_w, t->in_fmt );
		
		} else {
			int b_len = t->in_w * t->in_h;
			int buvlen = b_len;
		
			switch(t->in_fmt)
			{
				case PIX_FMT_YUV420P:
				case PIX_FMT_YUVJ420P:
					buvlen = b_len/4;
					break;
				default:
					buvlen = b_len/2;
					break;
			}


			a =yuv_yuv_template( tag->socket_frame, tag->socket_frame + b_len, tag->socket_frame+b_len+buvlen,
						t->in_w,t->in_h, t->in_fmt);
		}

		b = yuv_yuv_template( buffer[0],buffer[1], buffer[2],t->w,t->h,t->f);
		yuv_convert_any_ac(a,b, a->format,b->format );
		free(a);
		free(b);
	}	
	t->grab = 1;
	
	unlock(t);
	return 1;
}

int	net_thread_get_frame_rgb( vj_tag *tag, uint8_t *buffer, int w, int h )
{
	threaded_t *t = (threaded_t*) tag->priv;
	const uint8_t *buf = tag->socket_frame;
	
	lock(t);
	if( t->state == 0 || t->error  )
	{
		if(t->repeat < 0)
			veejay_msg(VEEJAY_MSG_INFO, "Connection closed with remote host");
		t->repeat++;
		unlock(t);
		return 0;
	}

	//@ color space convert frame	
	int len = t->w * t->h;
	int uv_len = len;
	switch(t->f)
	{
		case FMT_420:
		case FMT_420F:
			uv_len=len/4;
		break;
		default:
			uv_len=len/2;
		break;
	}

	if(t->have_frame )
	{
		int b_len = t->in_w * t->in_h;
		int buvlen = b_len;
		switch(t->in_fmt)
		{
			case FMT_420:
			case FMT_420F:
				buvlen = b_len/4;
				break;
			default:
				buvlen = b_len/2;
				break;
		}

		int tmp_fmt = get_ffmpeg_pixfmt( t->in_fmt );
 
		VJFrame *a = yuv_yuv_template( tag->socket_frame, tag->socket_frame + b_len, tag->socket_frame+b_len+buvlen,
						t->in_w,t->in_h, tmp_fmt);
		VJFrame *b = yuv_rgb_template( buffer,w,h,PIX_FMT_RGB24);
		yuv_convert_any_ac(a,b, a->format,b->format );
		free(a);
		free(b);
	}	
	t->grab = 1;
	
	unlock(t);
	return 1;
}


int	net_thread_start(vj_tag *tag, int wid, int height, int pixelformat)
{
	int success = 0;
	int res = 0;

	if(tag->source_type == VJ_TAG_TYPE_MCAST ) {
		veejay_msg(0, "MCAST: fixme!");
	}
	//@ 
	//	success = vj_client_connect( v,NULL,tag->source_name,tag->video_channel  );
/*	else
		success = vj_client_connect_dat( v, tag->source_name,tag->video_channel );
*/	
	
	threaded_t *t = (threaded_t*)tag->priv;

	pthread_mutex_init( &(t->mutex), NULL );
	t->repeat = 0;
	t->w = wid;
	t->h = height;
	t->af = pixelformat;
	t->f = get_ffmpeg_pixfmt(pixelformat);
	t->have_frame = 0;
	t->error = 0;
	t->state = 1;
	t->grab = 1;

	int p_err = pthread_create( &(t->thread), NULL, &reader_thread, (void*) tag );
	if( p_err ==0)

	{

		veejay_msg(VEEJAY_MSG_INFO, "Created new %s threaded stream to veejay host %s port %d",
			tag->source_type == VJ_TAG_TYPE_MCAST ? 
			"multicast" : "unicast", tag->source_name,tag->video_channel);
	}
	return 1; 
	/*
	
	if( tag->source_type == VJ_TAG_TYPE_MCAST )
	{
		char start_mcast[8];
		
		int  gs = 0;
		char *gs_str = getenv( "VEEJAY_MCAST_GREYSCALE" );
		if( gs_str ) {
			gs = atoi(gs_str);
			if(gs < 0 || gs > 1 )
				gs = 0;
		}
		
		snprintf(start_mcast,sizeof(start_mcast), "%03d:%d;", VIMS_VIDEO_MCAST_START,gs);
		
		veejay_msg(VEEJAY_MSG_DEBUG, "Request mcast stream from %s port %d",
				tag->source_name, tag->video_channel);
		
		res = vj_client_send( v, V_CMD, start_mcast );
		if( res <= 0 )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Unable to send to %s port %d",
					tag->source_name, tag->video_channel );
			return 0;
		}
		else
			veejay_msg(VEEJAY_MSG_INFO, "Requested mcast stream from Veejay group %s port %d",
					tag->source_name, tag->video_channel );	
	}
	int p_err = pthread_create( &(t->thread), NULL, &reader_thread, (void*) tag );
	if( p_err ==0)

	{
		veejay_msg(VEEJAY_MSG_INFO, "Created new %s threaded stream to veejay host %s port %d",
			tag->source_type == VJ_TAG_TYPE_MCAST ? 
			"multicast" : "unicast", tag->source_name,tag->video_channel);
		return 1;
	}
	return 0;		
*/
}

void	net_thread_stop(vj_tag *tag)
{
	char mcast_stop[6];
	threaded_t *t = (threaded_t*)tag->priv;
	int ret = 0;
	
	lock(t);
	
	t->state = 0;

	unlock(t);
	
	pthread_mutex_destroy( &(t->mutex));

	veejay_msg(VEEJAY_MSG_INFO, "Disconnected from Veejay host %s:%d", tag->source_name,
		tag->video_channel);
}

int	net_already_opened(const char *filename, int n, int channel)
{
	char sourcename[255];
	int i;
	for (i = 1; i < n; i++)
	{	
		if (vj_tag_exists(i) )
		{
		    vj_tag_get_source_name(i, sourcename);
		    if (strcasecmp(sourcename, filename) == 0)
		    {
			vj_tag *tt = vj_tag_get( i );
			if( tt->source_type == VJ_TAG_TYPE_NET || tt->source_type == VJ_TAG_TYPE_MCAST )
			{
				if( tt->video_channel == channel )
				{
					veejay_msg(VEEJAY_MSG_WARNING, "Already streaming from %s:%p in stream %d",
						filename,channel, tt->id);
					return 1;
				}
			}
	    	    }
		}
    	}
	return 0;
}



