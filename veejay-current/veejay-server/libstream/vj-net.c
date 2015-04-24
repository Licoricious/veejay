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
#include <libavutil/pixfmt.h>
#include <libvjmsg/vj-msg.h>
#include <veejay/vims.h>
#include <libstream/vj-net.h>
#include <liblzo/lzo.h>
#include <time.h>
#include <libyuv/yuvconv.h>

#define        RUP8(num)(((num)+8)&~8)

typedef struct
{
	pthread_mutex_t mutex;
	pthread_t thread;
	int state;
	int have_frame;
	int error;
	int repeat;
	int w;
	int h;
	int f;
	int in_fmt;
	int in_w;
	int in_h;
	int af;
	uint8_t *buf;
	void *scaler;
	VJFrame *a;
	VJFrame *b;
} threaded_t;

#define STATE_INACTIVE 0
#define STATE_RUNNING  1

static int lock(threaded_t *t)
{
	return pthread_mutex_lock( &(t->mutex ));
}

static int unlock(threaded_t *t)
{
	return pthread_mutex_unlock( &(t->mutex ));
}

#define MS_TO_NANO(a) (a *= 1000000)
static	void	net_delay(long nsec, long sec )
{
	struct timespec ts;
	ts.tv_sec = sec;
	ts.tv_nsec = MS_TO_NANO( nsec);
	clock_nanosleep( CLOCK_REALTIME,0, &ts, NULL );
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
		goto NETTHREADEXIT;
	}

	unsigned int	optimistic = 1;
	unsigned int	optimal    = 8;
	unsigned int    bufsize    = t->w * t->h * 4;

	t->buf = (uint8_t*) vj_calloc(sizeof(uint8_t) * RUP8( bufsize ) );

	if(t->buf == NULL ) {
		veejay_msg(0, "Cannot allocate memory for network i/o buffer");
		goto NETTHREADEXIT;
	}

	for( ;; )
	{
		int error    = 0;
		int res      = 0;

		if(!error && tag->source_type == VJ_TAG_TYPE_NET && retrieve == 0) {
			ret =  vj_client_send( v, V_CMD, buf );
			if( ret <= 0 )
			{
				veejay_msg(VEEJAY_MSG_DEBUG,
								"Error sending get frame command to %s:%d",
								tag->source_name,
								tag->video_channel );

				error = 1;
			} else {
				optimistic = optimal;
			}
		}

		if(!error && tag->source_type == VJ_TAG_TYPE_NET )
		{
			res = vj_client_poll(v, V_CMD );
			if( res )
			{
				if(vj_client_link_can_read( v, V_CMD ) ) {
					retrieve = 2;
					if( optimistic < optimal && optimistic > 1 )
						optimal = (optimistic + optimal) / 2;
				}
			} 
			else if ( res < 0 ) {
				veejay_msg(VEEJAY_MSG_DEBUG,
								"Error polling connection %s:%d",
								tag->source_name,
								tag->video_channel );
				error = 1;
			} else if ( res == 0 ) {
				retrieve = 1;
				net_delay( optimistic ,0); //@ decrease wait time after each try
				optimistic --;
				if( optimistic == 0 ) {
					optimal ++;
					if(optimal > 20 )
						optimal = 20; //@ upper bound
					optimistic = 1;
				}

				continue; //@ keep waiting after SEND_FRAME until like ;; or error
			}
		}
	
	
		if(!error && (retrieve == 2))
		{
			if( res )
			{
				if(lock(t) != 0 ) //@ lock memory region to write
					goto NETTHREADEXIT;
			
				ret = vj_client_read_i ( v, t->buf,bufsize );
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
				}
				else
				{
					t->in_fmt = v->in_fmt;
					t->in_w   = v->in_width;
					t->in_h   = v->in_height;
					t->have_frame = 1;
					retrieve = 0;
				}
				if(unlock(t) != 0 )
					goto NETTHREADEXIT;
			}
		}
NETTHREADRETRY:
		if( error )
		{
			int success = 0;

			vj_client_close( v );
			vj_client_free( v);
			v = vj_client_alloc( t->w, t->h, t->af );
			if(!v) {
				goto NETTHREADEXIT;
			}

			v->lzo = lzo_new();

			veejay_msg(VEEJAY_MSG_INFO, " ZZzzzzz ... waiting for Link %s:%d to become ready", tag->source_name, tag->video_channel );
			net_delay( 0, 5 );

			if(tag->source_type == VJ_TAG_TYPE_MCAST )
				success = vj_client_connect( v,NULL,tag->source_name,tag->video_channel  );
			else
				success = vj_client_connect_dat( v, tag->source_name,tag->video_channel );
	
			if( success <= 0 )
			{
				goto NETTHREADRETRY;
			}
			else
			{
				veejay_msg(VEEJAY_MSG_INFO, "Connecton re-established with %s:%d",tag->source_name,tag->video_channel + 5);
			}
		
			retrieve = 0;
		}


		int cur_state = STATE_RUNNING;
		if(lock(t) != 0 )
			goto NETTHREADEXIT;
		cur_state = t->state;
		if(unlock(t) != 0 )
			goto NETTHREADEXIT;

		if( cur_state == 0 )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Network thread with %s: %d was told to exit",tag->source_name,tag->video_channel+5);
			goto NETTHREADEXIT;
		}
	}

NETTHREADEXIT:

	if(t->buf)
		free(t->buf);

	if(v) {
	       	vj_client_close(v);
		vj_client_free(v);
	}

	veejay_msg(VEEJAY_MSG_INFO, "Network thread with %s: %d has exited",tag->source_name,tag->video_channel+5);
	pthread_exit( &(t->thread));

	return NULL;
}

static void	net_thread_free(vj_tag *tag)
{
	threaded_t *t = (threaded_t*) tag->priv;

	if( t->scaler )
		yuv_free_swscaler( t->scaler );

	if( t->a )
		free(t->a);
	if( t->b )
		free(t->b);

	t->a = NULL;
	t->b = NULL;
	t->scaler = NULL;
	t->buf = NULL;

}	

void	*net_threader(VJFrame *frame)
{
	threaded_t *t = (threaded_t*) vj_calloc(sizeof(threaded_t));
	return (void*) t;
}

int	net_thread_get_frame( vj_tag *tag, uint8_t *buffer[3] )
{
	threaded_t *t = (threaded_t*) tag->priv;
	
	int have_frame = 0;
	int state = 0;

	/* frame ready ? */
	if(lock(t) != 0 )
		return 0;

	have_frame = t->have_frame;
	state= t->state;

	if( state == 0 || have_frame == 0 ) {
		unlock(t);
		return 1; // not active or no frame
	}

	//@ color space convert frame	
	int len = t->w * t->h;
	int b_len = t->in_w * t->in_h;
	int buvlen = b_len;

	if( t->in_fmt == PIX_FMT_RGB24 || t->in_fmt == PIX_FMT_BGR24 || t->in_fmt == PIX_FMT_RGBA || t->in_fmt == PIX_FMT_RGB32_1 ) {

		if(t->a == NULL )
			t->a = yuv_rgb_template( tag->socket_frame, t->in_w, t->in_w, t->in_fmt );
	
	} else {
	
		if( t->b == NULL ) {
			if( PIX_FMT_YUV420P == t->in_fmt || PIX_FMT_YUVJ420P == t->in_fmt )
				buvlen = b_len/4;
			else
				buvlen = b_len/2;

			t->a = yuv_yuv_template( t->buf, t->buf + b_len, t->buf+ b_len+ buvlen,t->in_w,t->in_h, t->in_fmt);
		}
	}

	if( t->b == NULL ) {
		t->b = yuv_yuv_template( buffer[0],buffer[1], buffer[2],t->w,t->h,t->f);
	}

	if( b_len + buvlen > tag->socket_len ) {
		veejay_msg(0, "Network client: buffer too small ?!");
		unlock(t);
		return 0;
	}

	if( t->scaler == NULL ) {
		sws_template sws_templ;
		memset( &sws_templ, 0, sizeof(sws_template));
		sws_templ.flags = yuv_which_scaler();
		t->scaler = yuv_init_swscaler( t->a,t->b, &sws_templ, yuv_sws_get_cpu_flags() );
	}

	yuv_convert_and_scale( t->scaler, t->a,t->b );
	
	t->have_frame = 0;
	if(unlock(t)!=0)
		return 0;

	return 1;
}

int	net_thread_get_frame_rgb( vj_tag *tag, uint8_t *buffer, int w, int h )
{
	threaded_t *t = (threaded_t*) tag->priv;
	
	int have_frame = 0;
	int state = 0;

	if(lock(t) != 0 )
		return 0;

	have_frame = t->have_frame;
	state= t->state;

	if( state == 0 || have_frame == 0 ) {
		unlock(t);
		return 1;
	}

	int len = t->w * t->h;
	int uv_len = len;
	int b_len = t->in_w * t->in_h;
	int buvlen = b_len;


	if( PIX_FMT_YUV420P == t->f || PIX_FMT_YUVJ420P == t->f )
	    uv_len = len/4;
	else
	    uv_len = len/2;


	if(t->have_frame )
	{
		if( PIX_FMT_YUV420P == t->in_fmt || PIX_FMT_YUVJ420P == t->in_fmt )
			buvlen = b_len/4;
		else
			buvlen = b_len/2;

		if( t->a == NULL ) {
			t->a = yuv_yuv_template( tag->socket_frame, tag->socket_frame + b_len, tag->socket_frame+b_len+buvlen,t->in_w,t->in_h, t->in_fmt);
		}
		if( t->b == NULL ) {
			t->b = yuv_rgb_template( buffer,w,h,PIX_FMT_RGB24);
		}

		yuv_convert_any_ac(t->a,t->b, t->a->format,t->b->format );
	}	
	
	unlock(t);

	return 1;
}


int	net_thread_start(vj_tag *tag, int wid, int height, int pixelformat)
{
	if(tag->source_type == VJ_TAG_TYPE_MCAST ) {
		veejay_msg(0, "Not in this version");
		return 0;
	}
	
	threaded_t *t = (threaded_t*)tag->priv;

	pthread_mutex_init( &(t->mutex), NULL );
	t->w = wid;
	t->h = height;
	t->af = pixelformat;
	t->f = get_ffmpeg_pixfmt(pixelformat);
	t->have_frame = 0;
	t->state = STATE_RUNNING;

	int p_err = pthread_create( &(t->thread), NULL, &reader_thread, (void*) tag );
	if( p_err ==0)

	{
		veejay_msg(VEEJAY_MSG_INFO, "Created new %s threaded stream to veejay host %s port %d",
			tag->source_type == VJ_TAG_TYPE_MCAST ? 
			"multicast" : "unicast", tag->source_name,tag->video_channel);
	}
	return 1; 
}

void	net_thread_stop(vj_tag *tag)
{
	threaded_t *t = (threaded_t*)tag->priv;
	
	if(lock(t) == 0 ) {	
		t->state = 0;
		unlock(t);
	}

	pthread_mutex_destroy( &(t->mutex));

	veejay_msg(VEEJAY_MSG_INFO, "Disconnected from Veejay host %s:%d", tag->source_name,tag->video_channel);

	net_thread_free(tag);
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



