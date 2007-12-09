/* libveejayvj - a extended librarified Linux Audio Video playback/Editing
 *		Niels Elburg <nielselburg@yahoo.de>
 *
 *
 * libveejay - a librarified Linux Audio Video PLAYback
 *
 * Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>
 * Extended by:   Gernot Ziegler  <gz@lysator.liu.se>
 *                Ronald Bultje   <rbultje@ronald.bitfreak.net>
 *              & many others
 *
 * A library for playing back MJPEG video via softwastre MJPEG
 * decompression (using SDL) 
 * 
 * This program is free software you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

/** \defgroup vidplay Video Player
 *
 * The video player is multithreaded. It keeps a thread
 * for timing and synchronization purposes and another for the workload.
 */

#include <config.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <time.h>
#include "jpegutils.h"
#ifndef X_DISPLAY_MISSING
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#ifdef STRICT_CHECKING
#include <assert.h>
#endif
#include <pthread.h>
#include <mpegconsts.h>
#include <mpegtimecode.h>
#include <mjpegtools/mjpeg_types.h>
#include "mjpeg_types.h"
#include <libvjmsg/vj-common.h>
#include <libvjmem/vjmem.h>
#include <veejay/defs.h>
#include <veejay/veejay.h>
#include <veejay/performer.h>
#include <libel/lav_io.h>
//#include <libvjnet/vj-server.h>
#include <libvevo/libvevo.h>
#include <vevosample/vevosample.h>
#include <vevosample/defs.h>
#include <vevosample/uifactory.h>
#include <libplugger/plugload.h>
#include <veejay/libveejay.h>
#include <veejay/gl.h>

#include <veejay/vj-sdl.h>

/* On some systems MAP_FAILED seems to be missing */
#ifndef MAP_FAILED
#define MAP_FAILED ( (caddr_t) -1 )
#endif
#define HZ 100
#define VALUE_NOT_FILLED -10000

#define	TIMER_NSLEEP 1
#define TIMER_NONE   0
#define TIMER_RT     2

//forward
//! Software Playback thread. 
static void *veejay_software_playback_thread(void *arg);

//! Get Veejay state
int veejay_get_state(veejay_t *info) {
	video_playback_setup *settings = (video_playback_setup*)info->settings;
	return settings->state;
}

sample_video_info_t	*veejay_get_ps(void *data)
{
	veejay_t *v = (veejay_t*) data;
	return v->video_info;
}

int	veejay_get_port(void *data)
{
	veejay_t *v = (veejay_t*) data;
	return v->port_offset;
}

int	veejay_osc_verify_format( void *vevo_port, char const *types )
{
	char *format = get_str_vevo( vevo_port, "format" );
	int n = strlen(types);
	if(!format)
	{
		if( (n ==0 || types == NULL) && format == NULL )
			return 1;
		return 0;
	}
	if( strcasecmp( types,format ) == 0 )
	{
		free(format);
		return 1;
	}
	free(format);
	return 0;
}

int		veejay_osc_property_calls_event( void *instance, const char *path, char *types, void **argv[], void *raw )
{
	veejay_t *v = (veejay_t*) instance;
	void *vevo_port = v->osc_namespace;

	int atom_type = vevo_property_atom_type( vevo_port, path );
	if( atom_type == VEVO_ATOM_TYPE_PORTPTR )
	{
		void *port = NULL;
		int error = vevo_property_get( vevo_port, path,0,&port );
	
		if(error == VEVO_NO_ERROR )
		{
			vevo_event_f f;
			if( veejay_osc_verify_format( port, types ) )
			{
				error = vevo_property_get( port, "func",0,&f );
				if( error == VEVO_NO_ERROR )
				{
					(*f)( instance,path, types, argv,raw );
					return 1;
				}
			}
		}
	}

	return 0;
}

//! Change Veejay state
/**!
 \param info Veejay Object
 \param new_state State
 */
void veejay_change_state(veejay_t * info, int new_state)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;
    settings->state = new_state;
}

const	char	*veejay_get_fmt_(int fmt)
{
	if(fmt==FMT_420)
		return "Planar YUV 4:2:0";
	if(fmt==FMT_422)
		return "Planar YUV 4:2:2";
	if(fmt==FMT_444)
		return "Planar YUV 4:4:4";
	return "Invalid colorspace";
}
//! Update current sample and prepare video settings
/**!
 \param info Veejay Object
 \param id Sample ID
 */
void	veejay_load_video_settings( veejay_t *info, int id )
{
	
	info->current_sample = find_sample(id);
#ifdef STRICT_CHECKING
	assert( info->current_sample != NULL );
#endif
	sample_video_info_t *svit = info->video_info;
	video_playback_setup *settings =
		(video_playback_setup*) info->settings;
	settings->spvf = 1.0 / svit->fps;
	settings->msec_per_frame = 1000 / settings->spvf;

	if(!svit->has_audio)
		settings->spas = 0.0;
	else
		settings->spas = 1.0 / svit->rate;
	veejay_msg(0, "\tSeconds per video frame = %g",settings->spvf );
	veejay_msg(0, "\tSeconds per audio sample = %g",settings->spas);
	veejay_msg(0, "\tRate = %ld", (long) svit->rate );
}

//! Wait until Software Playback thread has finished
/**!
 \param info Veejay Object
 */
void veejay_busy(veejay_t * info)
{
    video_playback_setup *settings = info->settings;

    int ret = pthread_attr_destroy( &(settings->stattr ));
    veejay_msg(VEEJAY_MSG_INFO, "Shutting down threads");
    
    pthread_join( settings->software_playback_thread, NULL );
    if (pthread_join(settings->playback_thread, NULL)) {
	veejay_msg(VEEJAY_MSG_ERROR, 
		    "Failure deleting software playback thread");
	return;
    }
}

//! Destroy Veejay Object
/**!
 \param info Veejay Object
 */
void veejay_free(veejay_t * info)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

//    if(info->display)
//	x_display_close( info->display );
 
    if( info->use_display == 1 )
    {
   	 vj_sdl_free( info->sdl_display );
	 vj_sdl_quit();
    }

    veejay_deinit(info);
    if(info->rtc)
	    vj_rtc_close(info->rtc);
    
    free(info->video_info);	
    free(settings);
    free(info);

    vevo_report_stats();
}

//! Quit Veejay
/**!
 \param info Veejay Object
 */
void veejay_quit(veejay_t * info)
{
    veejay_change_state(info, VEEJAY_STATE_STOP);
}


//! Initialize Project settings from commandline arguments
/**!
 \param info Veejay Object
 \param w Width of Video
 \param h Height of Video
 \param fps Framerate
 \param inter Interlacing
 \param norm Norm
 \param fmt Pixel Format
 \param audio Audio yes/no
 \param rate Audio rate
 \param n_chan Audio Channels
 \param bits Audio Bits
 \return Error code
 */
int veejay_init_project_from_args( veejay_t *info, int w, int h, float fps, int inter, int norm, int fmt,
		int audio, int rate, int n_chan, int bps, int display )
{
	veejay_msg(0, "%s: %dx%d, %f, inter=%d,norm=%d,fmt=%d,audio=%d,rate=%d,chan=%d,bps=%d",
			__FUNCTION__,w,h,fps, inter,norm, fmt, audio,rate,n_chan,bps );
	sample_video_info_t *svit = (sample_video_info_t*) vj_malloc(sizeof( sample_video_info_t ));
	memset( svit,0,sizeof(sample_video_info_t));
#ifdef STRICT_CHECKING
	assert( w > 0 );
	assert( h > 0 );
#endif	
	svit->w = w;
	svit->h = h;
	svit->fps = fps;
	svit->inter = inter;
	svit->norm = norm;
	svit->fmt = fmt;
	svit->audio = audio;
	svit->has_audio = (audio ? 1: 0 );
	svit->chans = n_chan;
	svit->rate = rate;
	svit->bits = 16;
	svit->bps = bps;

	avcodec_init();
	avcodec_register_all();
	
	veejay_msg(2, "Project settings:");
	veejay_msg(2, "\tvideo settings: %d x %d, @%2.2f in %s", svit->w,svit->h,svit->fps, (svit->norm ? "NTSC" :"PAL") );
	veejay_msg(2, "\taudio settings: %ld Hz, %d bits, %d channels, %d bps",
			svit->rate, svit->bits,svit->chans, svit->bps );
	veejay_msg(2, "\toutput pixel format: %d", fmt);
	
	info->video_info = (void*) svit;
#ifdef STRICT_CHECKING
	assert( info->video_info != NULL );
#endif
	info->use_display = display;
	vj_el_init(fmt);

	if(prepare_cache_line( 10, 3 ))
	{
		veejay_msg(0, "Error initializing EDL cache");
		return -1;
	}

	plug_sys_init( svit->fmt,svit->w,svit->h);
//	plug_sys_set_palette( svit->fmt );
	return 1;
}

//! Stop Playing
/**!
 \param info Veejay Object
 \return Error code
 */
int veejay_stop(veejay_t * info)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

    if (settings->state == VEEJAY_STATE_STOP)
    {
	    // stop playing samples!
	    veejay_msg(0, "FIXME: Close playing samples");
    }

    veejay_change_state(info, VEEJAY_STATE_STOP);

   // pthread_join(settings->software_playback_thread, NULL);

    return 1;
}

//!  Try to keep in sync with nominal frame rate,timestamp frame with actual completion time (after any deliberate sleeps etc)
/**!
 \param info Veejay Object
 \param frame_periods Frame Period
 */
static void veejay_mjpeg_software_frame_sync(veejay_t * info,
					      int frame_periods)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

    if (info->timer == TIMER_NSLEEP) {
	struct timeval now;
	struct timespec nsecsleep;
	int usec_since_lastframe=0;

	for (;;) {
	//fixme?	
	    gettimeofday(&now, 0);
		
	    usec_since_lastframe =
		now.tv_usec - settings->lastframe_completion.tv_usec;
	     //usec_since_lastframe = vj_get_relative_time();
	    if (usec_since_lastframe < 0)
		usec_since_lastframe += 1000000;
	    if (now.tv_sec > settings->lastframe_completion.tv_sec + 1)
		usec_since_lastframe = 1000000;

	    if (settings->first_frame ||
		(frame_periods * settings->usec_per_frame -
		usec_since_lastframe) < (1000000 / HZ))
		break;
	    	
	    nsecsleep.tv_nsec =
		(frame_periods * settings->usec_per_frame -
		 usec_since_lastframe - 1000000 / HZ) * 1000;
	    nsecsleep.tv_sec = 0;
	    nanosleep(&nsecsleep, NULL);
	  
	}
    }
    else if(info->timer == TIMER_RT )
    {
	struct timeval now;
	struct timespec nsecsleep;
		int usec_since_lastframe=0;

	gettimeofday(&now, 0);
	
	usec_since_lastframe =
		now.tv_usec - settings->lastframe_completion.tv_usec;
	if (usec_since_lastframe < 0)
		usec_since_lastframe += 1000000;
	if (now.tv_sec > settings->lastframe_completion.tv_sec + 1)
		usec_since_lastframe = 1000000;

	if (!settings->first_frame ||
		(frame_periods * settings->usec_per_frame -
		usec_since_lastframe) >= (1000000 / HZ))
	{
		vj_rtc_wait( info->rtc, 
			(frame_periods * settings->usec_per_frame -
				 usec_since_lastframe - 1000000 / HZ) 
			);
	}
    }

    settings->first_frame = 0;

    /* We are done with writing the picture - Now update all surrounding info */
	gettimeofday(&(settings->lastframe_completion), 0);
        settings->syncinfo[settings->currently_processed_frame].timestamp =
  	  settings->lastframe_completion;


}

//! Veejay Signal Loop. Catch and handle some signals.
/**!
 \param arg Veejay Object
 */
void veejay_signal_loop(void *arg)
{
    veejay_t *info = (veejay_t *) arg;
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;
    //struct sigaction act;
    int sig=0;
    pthread_sigmask(SIG_UNBLOCK, &(settings->signal_set), NULL);
    while (1) {
	sigwait(&(settings->signal_set), &sig);
	if (sig == SIGINT) {
	    veejay_change_state(info, VEEJAY_STATE_STOP);
	}
	if (sig == SIGSEGV  || sig == SIGBUS || sig == SIGILL )
	{
		/* save and abort */
		veejay_msg(VEEJAY_MSG_ERROR, "Catched SEGFAULT , save & abort ...");
		veejay_change_state(info,VEEJAY_STATE_STOP);
	}
    }
    pthread_exit(NULL);
}

//! Display video frame
/**!
 \param arg info Veejay Object
 \return Error Code
 */
//static int gl_ready_ = 0;
int	veejay_push_results( veejay_t *info )
{
	sample_video_info_t *vid_info = info->video_info;

	VJFrame *ref = performer_get_output_frame( info );

	switch( info->use_display )
	{
		case 2:
			x_display_push( info->display, ref );
		break;
		case 1:
			vj_sdl_update_yuv_overlay( info->sdl_display, ref );
		break;
	}
//	performer_clean_output_frame( info );

	return 1;
}


void	veejay_lock( veejay_t *info , const char *f)
{
//	veejay_msg(0, "%s locks", f);
	int n = pthread_mutex_lock( &(info->vevo_mutex) );
#ifdef STRICT_CHECKING
	assert( n == 0 );
#endif
}

void	veejay_unlock( veejay_t *info, const char *f )
{
	int n = pthread_mutex_unlock( &(info->vevo_mutex) );
#ifdef STRICT_CHECKING
	assert(n == 0 );
#endif
//	veejay_msg(0, "%s unlocks",f);
}


//! Veejay Software Playback thread
/**!
 \param arg Veejay Object
 \return NULL
 */
static void *veejay_software_playback_thread(void *arg)
{
	veejay_t *info = (veejay_t *) arg;
	video_playback_setup *settings =
		(video_playback_setup *) info->settings;
	/* Allow easy shutting down by other processes... */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	sample_video_info_t *vid_info = info->video_info;

	if( info->use_display == 2)
	{
		info->display = x_display_init();
		if(!info->display)
			return NULL;
		x_display_open( info->display, vid_info->w, vid_info->h );
	}

	
	while (settings->state != VEEJAY_STATE_STOP) {
		pthread_mutex_lock(&(settings->valid_mutex));
		while (settings->valid[settings->currently_processed_frame] == 0) {
		    pthread_cond_wait(&
			      (settings->
			       buffer_filled[settings->
					     currently_processed_frame]),
			      &(settings->valid_mutex));
		    if (settings->state == VEEJAY_STATE_STOP) {
			veejay_msg(VEEJAY_MSG_INFO,
				    "Veejay was told to exit");
			pthread_exit(NULL);
	 		return NULL;
	    		}
		}

		pthread_mutex_unlock(&(settings->valid_mutex));
      	 	if( settings->currently_processed_entry != settings->buffer_entry[settings->currently_processed_frame]  &&
			!veejay_push_results(info) )
		{
			veejay_msg(VEEJAY_MSG_ERROR,
					"Error playing frame");
			veejay_change_state( info, VEEJAY_STATE_STOP);
		}
#ifdef HAVE_SDL	
		if( info->display == 1 )
		{
			SDL_Event event;
			while( SDL_PollEvent(&event) )
				vj_sdl_event_handle( info->sdl_display, event );
		}
#endif	
		//@ Handle callbacks HERE
//		vj_event_update_remote( info );


		settings->currently_processed_entry = 
			settings->buffer_entry[settings->currently_processed_frame];

		veejay_mjpeg_software_frame_sync(info,
				  settings->valid[settings->
					  currently_processed_frame]);
		settings->syncinfo[settings->currently_processed_frame].frame =
		    settings->currently_processed_frame;


//@ Callback		
	pthread_mutex_lock(&(settings->valid_mutex));

	settings->valid[settings->currently_processed_frame] = 0;

	pthread_mutex_unlock(&(settings->valid_mutex));
	  
	/* Broadcast & wake up the waiting processes */
	pthread_cond_broadcast(&
			       (settings->
				buffer_done[settings->
					    currently_processed_frame]));

	/* Now update the internal variables */
 	// settings->previous_frame_num = settings->current_frame_num;

	settings->currently_processed_frame =
	    (settings->currently_processed_frame + 1) % 1;
    }
	
    veejay_msg(VEEJAY_MSG_INFO, 
		"Software playback thread was told to exit");

	if(info->use_display==2)
		x_display_close( info->display );
    
    return NULL;
}


//! Start Software Playback thread
/**!
 \param info Veejay Object
 \return Error Code
*/ 
int veejay_open(veejay_t * info)
{
	video_playback_setup *settings =
		(video_playback_setup *) info->settings;
	veejay_msg(VEEJAY_MSG_DEBUG, 
		"Initializing the threading system");
	memset( &(settings->lastframe_completion), 0, sizeof(struct timeval));
	pthread_mutex_init(&(settings->valid_mutex), NULL);
	pthread_mutex_init(&(settings->syncinfo_mutex), NULL);
	/* Invalidate all buffers, and initialize the conditions */
	settings->valid[0] = 0;
	settings->buffer_entry[0] = 0;
	pthread_cond_init(&(settings->buffer_filled[0]), NULL);
	pthread_cond_init(&(settings->buffer_done[0]), NULL);
	mymemset_generic(&(settings->syncinfo[0]), 0, sizeof(struct mjpeg_sync));
    	/* Now do the thread magic */
    	settings->currently_processed_frame = 0;
    	settings->currently_processed_entry = -1;

      	veejay_msg(VEEJAY_MSG_DEBUG,"Starting software playback thread"); 

	int ret = pthread_attr_init(&(settings->stattr));
	ret = pthread_attr_setdetachstate( &(settings->stattr),
			PTHREAD_CREATE_DETACHED );
	

	if( pthread_create(
			&(settings->software_playback_thread),
			&(settings->stattr),
		       veejay_software_playback_thread,
		       (void *) info)
		)
	{
		veejay_msg(VEEJAY_MSG_ERROR, 
			    "Could not create software playback thread");
		return 0;
    	}
	return 1;
}

//! Set Playback rate (Video Framerate and Video Norm 
/**!
 \param info Veejay Object
 \param video_fps Video Framerate
 \param norm Video Norm
 \return Error Code
 */
static int veejay_set_playback_rate(veejay_t * info,
					   double video_fps, int norm)
{
    int norm_usec_per_frame = 0;
    int target_usec_per_frame;
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

    switch (norm) {
    case VIDEO_MODE_PAL:
    case VIDEO_MODE_SECAM:
	norm_usec_per_frame = 1000000 / 25;	/* 25Hz */
	break;
    case VIDEO_MODE_NTSC:
	norm_usec_per_frame = 1001000 / 30;	/* 30ish Hz */
	break;
    default:
	    veejay_msg(VEEJAY_MSG_ERROR, 
			"Unknown video norm! Use PAL , SECAM or NTSC");
	    return 0;
	}

    if (video_fps != 0.0)
	target_usec_per_frame = (int) (1000000.0 / video_fps);
    else
	target_usec_per_frame = norm_usec_per_frame;


    settings->usec_per_frame = target_usec_per_frame;
    return 1;
}

//! Queue a buffer
/**!
 \param info Veejay Object
 \param frame Frame Number
 \param frame_periods Frame Periods
 */
static int veejay_mjpeg_queue_buf(veejay_t * info, int frame,
				   int frame_periods)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;
    /* mark this buffer as playable and tell the software playback thread to wake up if it sleeps */
    pthread_mutex_lock(&(settings->valid_mutex));
    settings->valid[frame] = frame_periods;
    pthread_cond_broadcast(&(settings->buffer_filled[frame]));
    pthread_mutex_unlock(&(settings->valid_mutex));
    return 1;
}


//! Sync on a Buffer
/**!
 \param info Veejay Object
 \param mjpeg_sync Mjpeg Sync
 \return Error Code 
 */
static int veejay_mjpeg_sync_buf(veejay_t * info, struct mjpeg_sync *bs)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;
    /* Wait until this buffer has been played */
	
    pthread_mutex_lock(&(settings->valid_mutex));
    while (settings->valid[settings->currently_synced_frame] != 0) {
	pthread_cond_wait(&
			  (settings->
			   buffer_done[settings->currently_synced_frame]),
			  &(settings->valid_mutex));
    }
    pthread_mutex_unlock(&(settings->valid_mutex));
	
    memcpy(bs, &(settings->syncinfo[settings->currently_synced_frame]),
	   sizeof(struct mjpeg_sync));
    settings->currently_synced_frame =
	(settings->currently_synced_frame + 1) % 1;
	
    return 1;
}


//! Close Veejay Software Playback thread
/**!
 \param info Veejay Object
 \return Error Code
 */
int veejay_close(veejay_t * info)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

    veejay_msg(VEEJAY_MSG_INFO, 
		"Closing down the threading system ");
/*  int ret = pthread_attr_destroy( &(settings->qtattr ));


    //pthread_cancel(settings->software_playback_thread);
    if (pthread_join(settings->playback_thread, NULL)) {
	veejay_msg(VEEJAY_MSG_ERROR, 
		    "Failure deleting software playback thread");
	return 0;
    }*/
    return 1;
}

void	veejay_deinit(veejay_t *info)
{
	char **items = vevo_list_properties( info->clients );
	int i;
	if(items!=NULL)
	{
		for( i = 0; items[i] != NULL ; i ++ )
		{
			void *s = NULL;
			if(vevo_property_get(info->clients, items[i],0,&s)==VEVO_NO_ERROR)
				veejay_free_osc_sender( s );
			free(items[i]);		
		}
		free(items);
	}
	
        
        veejay_osc_del_methods( info,info->osc_namespace,info ,info);
	//vj_event_stop();
	samplebank_free();

	vevo_port_recursive_free( info->osc_namespace );

	vevo_port_free( info->clients );
	
//	vj_server_shutdown(info->status_socket);
//	vj_server_shutdown(info->command_socket);
//	vj_server_shutdown(info->frame_socket);
	
	plug_sys_free();
	if(info->performer)
		performer_destroy(info);
    	vj_el_deinit();		
}

static	void	veejay_setup_timer(veejay_t *info)
{
	switch (info->timer)
	{
		case TIMER_NONE:
			veejay_msg(VEEJAY_MSG_INFO, "\tTimer:          Nome");
			break;
  		case TIMER_RT:
			info->rtc = vj_new_rtc_clock();
			if(info->rtc)
				veejay_msg(VEEJAY_MSG_INFO, "\tTimer:          Realtime clock");

			break;
    		case TIMER_NSLEEP:
			veejay_msg(VEEJAY_MSG_INFO, "\tTimer:          Nanosleep");
		break;
    	}    
}

int		veejay_load_devices( veejay_t *info )
{
	sample_video_info_t *vid_info = info->video_info;
	
	int n = vevo_num_devices();
	int i;
	for( i = 0; i < n ; i ++ )
	{
		void *sample = sample_new( VJ_TAG_TYPE_CAPTURE );
		if( sample_open( sample, NULL, i, vid_info ) <= 0 )
		{
			if(sample) sample_delete_ptr( sample );
		}
		else
		{
			int id = samplebank_add_sample( sample );
			sample_set_user_data( sample,info, id );
		}
	}

	return 1;
}

int		veejay_load_samples( veejay_t *info, const char **argv, int n_arg )
{
	sample_video_info_t *vid_info = info->video_info;
	int id = 0;

	if( n_arg <= 0 )
	{
		veejay_msg(2, "No input files given, starting with dummy video");
		void *sample = sample_new( VJ_TAG_TYPE_COLOR );
		if( sample_open(sample, NULL, 0, vid_info ) <= 0 )
			return 0;
		//sample_set_user_data(sample,info);
		id = samplebank_add_sample( sample );
		sample_set_user_data(sample,info, id);
		
		veejay_load_video_settings(info, id );	
		return 1;
	}
	
	int n = 0;
	for ( n = 0; n < n_arg; n ++ )
	{
		void *sample = sample_new( VJ_TAG_TYPE_NONE );
		if( sample_open( sample, argv[n],1, vid_info ) <= 0 )
		{
			veejay_msg(0, "Unable to load '%s', skipped.", argv[n]);
			if(sample) sample_delete_ptr( sample );
		}
		else
		{
			id = samplebank_add_sample( sample );
			sample_set_user_data(sample,info,id);

			veejay_msg(0, "Loaded '%s' to sample %d", argv[n], id );
		}	
	}

	if( id )
	{
	       	veejay_load_video_settings(info, 1 );	
		return 1;
	}
	//veejay_load_video_settings( info, id );	

	return 0;
}

//! Initialize Veejay
int veejay_init(veejay_t * info)
{
	video_playback_setup *settings = info->settings;
	sample_video_info_t *vid_info = info->video_info;
#ifdef STRICT_CHECKING
	assert( vid_info != NULL );
	assert( settings != NULL );
#endif
	veejay_setup_timer( info );

	info->performer = performer_init( info, 32 );

	veejay_msg(VEEJAY_MSG_INFO, "Loading plugins");
	int n = plug_sys_detect_plugins();
	veejay_msg(VEEJAY_MSG_INFO, "\tPlugin(s) loaded:   %d", n);

	if(info->itu601)
		veejay_msg(VEEJAY_MSG_INFO, "\tKeep video data in ITU601");
	vj_el_set_itu601_preference( info->itu601 );

//	pthread_mutex_init( &(info->display_mutex), 0 );

	if(info->use_display==1)
	{
		info->sdl_display = vj_sdl_allocate(
				vid_info->w,
				vid_info->h,
				vid_info->fmt );
		vj_sdl_init( info->sdl_display, vid_info->w,vid_info->h, "Veejay", 1, 0 );
	}

	if (seteuid(getuid()) < 0)
	{
		/* fixme: get rid of sys_errlist and use sys_strerror */
		veejay_msg(VEEJAY_MSG_ERROR, 
			"Can't set effective user-id: %s", sys_errlist[errno]);
		return -1;
	}
       
	if (!veejay_set_playback_rate(info, vid_info->fps,
				 vid_info->norm )) {
		veejay_msg(VEEJAY_MSG_ERROR,
				"Cannot set video playback rate");
		return -1;
	}

	if(veejay_open(info) != 1)
	{
		veejay_msg(VEEJAY_MSG_ERROR, 
	    		"Unable to initialize the threading system");
		return -1;   
      	}

	veejay_change_state( info, VEEJAY_STATE_PLAYING );  

	
	return 0;
}

void	veejay_playback_status( veejay_t *info )
{
/*	char status_message[300];

	int len = strlen( info->message );
	
	sprintf( status_message, "V%03dS%s", len, info->message);

	veejay_msg(0, "Status: '%s'", info->message );
	
	int res = vj_server_send(
			info->status_socket,
			info->current_link,
			status_message,
			strlen(status_message)
			);

	if( res <= 0 )
	{
		veejay_msg(VEEJAY_MSG_INFO, 
				"Remote closed connection");
		_vj_server_del_client( info->status_socket, info->current_link );
		_vj_server_del_client( info->command_socket,info->current_link );
		_vj_server_del_client( info->frame_socket, info->current_link );
	}
	//sample_sprintf_port( info->current_sample);*/
}


/******************************************************
 * veejay_playback_cycle()
 *   the playback cycle
 ******************************************************/
static void veejay_playback_cycle(veejay_t * info)
{
    video_playback_stats stats;
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;
    struct mjpeg_sync bs;
    struct timeval time_now;
    int n;
    int first_free, skipv, skipa, skipi;
    uint8_t frame = 0;
    int nvcorr = 0;
    uint64_t frame_number[2];	/* Must be at least as big as the number of buffers used */

    double tdiff1 = 0.0;
    double tdiff2 = 0.0;
    long ts, te;

    sample_video_info_t *vid_info = (sample_video_info_t*) info->video_info;

    if( sample_has_audio( info->current_sample ))
	{
		
		long rate = performer_audio_start(info);
		if( rate > 0 )
		{
			vid_info->audio = 1;
			vid_info->has_audio = 1;
			settings->spas = 1.0 / (double) rate;
			stats.audio =1;
		}
		else
		{
			info->audio = NO_AUDIO;
			vid_info->audio = 0;
			stats.audio = 0;
		}
	}

    performer_queue_audio_frame( info, 0 );

    if (performer_queue_frame(info, 0) != 0)
    {
	   veejay_msg(VEEJAY_MSG_ERROR,"Unable to queue frame");
           return;
    }

    memset( &stats,0,sizeof(video_playback_stats)); 
    stats.norm = vid_info->norm;
    nvcorr = 0;
    
    frame_number[0] = sample_get_current_pos( info->current_sample );

    veejay_mjpeg_queue_buf(info, 0, 1);
    
    stats.nqueue = 1;
    int last_id = sample_get_key_ptr( info->current_sample);
    while (settings->state != VEEJAY_STATE_STOP)
    {
	first_free = stats.nsync;

    	do {
	    if (settings->state == VEEJAY_STATE_STOP)
	    {
		goto FINISH;
	    }
	    if (!veejay_mjpeg_sync_buf(info, &bs))
	    {
		veejay_change_state(info, VEEJAY_STATE_STOP);
		goto FINISH;
	    }
	
	    frame = bs.frame;

	    // This should never happen
	    if (frame != stats.nsync % 1)
	    {
		veejay_msg(VEEJAY_MSG_ERROR, 
			    "**INTERNAL ERROR: Bad frame order on sync: frame = %d, nsync = %d, br.count = %ld sould be %ld",
			    frame, stats.nsync, 1, (stats.nsync % 1));
		veejay_change_state(info, VEEJAY_STATE_STOP);
		goto FINISH;
	    }
	   
	    stats.nsync++;
	    /* Lock on clock */
	    gettimeofday(&time_now, 0);
	    stats.tdiff = time_now.tv_sec - bs.timestamp.tv_sec +
		(time_now.tv_usec - bs.timestamp.tv_usec)*1.e-6;

	//vj_event_update_remote( info );

	} while (stats.tdiff > settings->spvf && (stats.nsync - first_free) < (1 - 1));

#ifdef HAVE_JACK
	if (stats.audio ) {
	   struct timeval audio_tmstmp;	
	   long int sec=0;
	   long int usec=0;
	   long num_audio_bytes_written = vj_jack_get_status( &sec,&usec);
	   audio_tmstmp.tv_sec = sec;
	   audio_tmstmp.tv_usec = usec;
	  if (audio_tmstmp.tv_sec)
          {
     		  tdiff2 = (bs.timestamp.tv_sec - audio_tmstmp.tv_sec) +
                (bs.timestamp.tv_usec - audio_tmstmp.tv_usec) * 1.e-6;

	      tdiff1 = settings->spvf * (stats.nsync - nvcorr) -
                settings->spas * num_audio_bytes_written;

 	  }
	
	}
#endif
	stats.tdiff = tdiff1 - tdiff2;
	/* Fill and queue free buffers again */
	for (n = first_free; n < stats.nsync;) {
	    /* Audio/Video sync correction */
	    skipv = 0;
	    skipa = 0;
	    skipi = 0;
	    if (info->sync_correction) {
		if (stats.tdiff > settings->spvf) {
		    /* Video is ahead audio */
		    skipa = 1;
		    if (info->sync_ins_frames)
			skipi = 1;
		    nvcorr++;
		    stats.num_corrs_a++;
		    stats.tdiff -= settings->spvf;
		    stats.stats_changed = 1;
		}
		if (stats.tdiff < -settings->spvf) {
		    /* Video is behind audio */
		    skipv = 1;
   		    if (!info->sync_skip_frames)
			skipi = 1;

		    nvcorr--;
		    stats.num_corrs_b++;
		    stats.tdiff += settings->spvf;
		    stats.stats_changed = 1;
		}
	    }

	    /* Read one frame, break if EOF is reached */
	    // actually measure duration of render in ms */
	    frame = n % 1;
	    frame_number[frame] = sample_get_current_pos( info->current_sample );

	    settings->buffer_entry[frame] = 
		    sample_get_current_pos( info->current_sample );
	  
	    sample_cache_data( info->current_sample );
#ifdef HAVE_JACK
	    vj_jack_continue( sample_get_speed(info->current_sample) );
#endif     
	    performer_queue_audio_frame(info,skipa);	    
		
	    performer_queue_frame(info,skipi);
	
	    sample_save_cache_data( info->current_sample );
	
	    performer_save_frame( info );

	    int cur_id = sample_get_key_ptr( info->current_sample);
	    if( cur_id != last_id )
	    {
		last_id = cur_id;
		performer_audio_restart(info);
	    }	

	    sample_format_status( info->current_sample, info->message );
	
		//@ and restart performer on sample switch!
	    
	    if(skipv) continue;
	    
	    if (!veejay_mjpeg_queue_buf(info, frame, 1))
	    {
		veejay_msg(VEEJAY_MSG_ERROR ,"Error queuing a frame");
		veejay_change_state(info, VEEJAY_STATE_STOP);
		goto FINISH;
	    }
	    stats.nqueue++;
	    n++;
	}
	if (vid_info->has_audio)
	    stats.audio = settings->audio_mute ? 0 : 1;
	stats.stats_changed = 0;
    }

  FINISH:

	performer_audio_stop(info);
}

/******************************************************
 * veejay_playback_thread()
 *   The main playback thread
 ******************************************************/

static void Welcome(veejay_t *info)
{
	veejay_msg(2, "Starting Veejay playback thread");
}

static void *veejay_playback_thread(void *data)
{
    veejay_t *info = (veejay_t *) data;
    int i;
    

    Welcome(info);
    veejay_change_state( info, VEEJAY_STATE_PLAYING );

    
    
    veejay_playback_cycle(info);
    veejay_close(info); 

    pthread_exit(NULL);
    return NULL;
}


void	veejay_create_sample_ui( veejay_t *info, int id )
{
	void *sample = find_sample(id);
	if(sample)
	{
		char *window = vevosample_construct_ui( sample );
		vevosample_construct_ui_fx_chain( sample );
		free(window);
	}
}

void	veejay_add_client( veejay_t *info, char *uri )
{
	char server_port_name[10];
#ifdef STRICT_CHECKING
	assert( uri != NULL );
#endif
	char *server 	 = veejay_osc_server_get_addr( info->osc_server );
	int   server_port= veejay_osc_server_get_port( info->osc_server );	

	sprintf( server_port_name, "%d", server_port );
	
	veejay_msg(0, "Veejay server '%s' communicates with %s" ,server,uri );

	void *ui = veejay_new_osc_sender_uri( uri );
	veejay_send_osc( ui, "/veejay", "sx", server );
	int error = vevo_property_set( info->clients, uri, VEVO_ATOM_TYPE_VOIDPTR,1,&ui );
#ifdef STRICT_CHECKING
	assert( error == VEVO_NO_ERROR );
#endif
	samplebank_send_all_samples( ui );

	samplebank_tick_ui_client( uri );
}

void	*veejay_get_osc_sender(veejay_t * info )
{
	if(!info)
		return NULL;
	void *sender = NULL;
	if(!info->current_client)
		return NULL;
#ifdef STRICT_CHECKING
	assert( info->current_client != NULL );
	assert( info->clients != NULL );
#endif
	int error = vevo_property_get( info->clients, info->current_client, 0, &sender );
	if( error == VEVO_NO_ERROR )
		return sender;
	return NULL;
}

void	*veejay_get_osc_sender_by_uri( veejay_t *info , const char *uri )
{
	void *sender = NULL;
	int error = vevo_property_get( info->clients, uri, 0, &sender );
	if( error == VEVO_NO_ERROR )
		return sender;
	return NULL;

}

void	veejay_set_current_client( veejay_t *info, char *uri )
{
	if(info->current_client)
		free(info->current_client);
	info->current_client = strdup(uri);
//	veejay_msg(0, "current client is '%s'", info->current_client );
}

void	veejay_init_ui(veejay_t * info , const char *uri)
{
#ifdef STRICT_CHECKING
	assert( info->current_sample != NULL );
#endif
	char veejaystr[100];
	sprintf(veejaystr, "Veejay-NG %s", VERSION );
//veejay_msg(0, "%s : %s", __FUNCTION__, uri );
//	veejay_set_current_client( info,uri );
	void *sender = veejay_get_osc_sender( info );
#ifdef STRICT_CHECKING
	assert( sender != NULL );
#endif

/*	char *window = vevosample_construct_ui(info->current_sample );
	vevosample_construct_ui_fx_all( info->current_sample );
	free(window);*/
	veejay_osc_set_window( sender , "MainWindow" );

	veejay_ui_bundle_add( sender, 
			"/create/window","ssx", "MainWindow", veejaystr );
	veejay_ui_bundle_add( sender,
			"/create/frame", "sssx", "MainWindow", "VeejayPanel",
			" " );

	double row = (double) samplebank_guess_row_sequence( info->current_sample );

/*	void *msg = veejay_message_new_pulldown( sender,
			"MainWindow","VeejayPanel","combobox_playlist","Play",
			"/veejay/select",
			row, "Click a sample to play" );
	
	samplebank_push_pulldown_items( sender, msg );

	veejay_message_pulldown_done( sender, msg );

	void *msg2 = veejay_message_new_pulldown( sender,
			"MainWindow","VeejayPanel","combobox_editlist","Edit",
			"/veejay/show",
		        0.0, "Click a sample to edit"	);
	
	samplebank_push_pulldown_items( sender, msg2 );
	veejay_message_pulldown_done( sender, msg2 );*/

	
	char tmp[128];
	sprintf(tmp, "%s", "/veejay/fullscreen" );
	vevosample_ui_new_switch( info->current_sample, "MainWindow" , "VeejayPanel","Check", "Fullscreen",0, tmp, "Toggle between window and fullscreen" );
	sprintf(tmp, "%s","/veejay/quit");
	vevosample_ui_new_button( info->current_sample,"MainWindow" , "VeejayPanel", "Quit Veejay", tmp, "Request veejay to stop processing" );
	
	
	veejay_ui_bundle_add( sender,
			"/show/window", "sx", "MainWindow" );
//	samplebank_send_all_samples( sender );

}

int vj_server_setup(veejay_t * info)
{
	int port = info->port_offset;

	/*info->command_socket = vj_server_alloc(port, NULL, V_CMD);

	if(!info->command_socket)
		return 0;

	info->status_socket = vj_server_alloc(port, NULL, V_STATUS);
	if(!info->status_socket)
		return 0;

	info->frame_socket = vj_server_alloc(port, NULL, V_MSG );
	if(!info->frame_socket)
		return 0;
*/
	const char port_str[50];
	sprintf(port_str, "%d",port );
	info->osc_server = veejay_new_osc_server( (void*)info, port_str );
	info->osc_namespace = vpn( VEVO_ANONYMOUS_PORT );
	veejay_osc_namespace_events( (void*) info, "/veejay");
	
	
//	info->mcast_socket =
//			vj_server_alloc(port, info->settings->vims_group_name, V_CMD );
//	GoMultiCast( info->settings->group_name );

	return 1;
}
#define VALID_RESOLUTION(w,h,x,y) (( (w >=64 && w<=2048) && (h >= 64 && h <= 2048) && (x >= 0 && x <2048) && (y>=0 && y <2048)) ? 1: 0 )


void	veejay_resize_screen( void *info, int x, int y, int w, int h )
{
	veejay_t *v = (veejay_t*) info;
	if(!VALID_RESOLUTION(w,h,x,y))
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Invalid dimensions for video window. Width and Height must be >= 64 and <= 2048");
                return;

	}
	if( v->use_display == 2)
        {
                veejay_msg(VEEJAY_MSG_INFO, "OpenGL video window is %gx%g+%gx%g",
                                w,h,x,y );
                x_display_resize( w,h,x,y ); 
        }
        if( v->use_display == 1 )
        {
                 if(vj_sdl_resize_window(v->display,w,h,x,y))
                         veejay_msg(VEEJAY_MSG_INFO, "SDL video window is %dx%d+%dx%d",
                                        w,h,x,y); 
        }
}

void	veejay_fullscreen(void *info, int value)
{
	veejay_t *v = (veejay_t*) info;
 	if( v->use_display == 2 )
        {
                if(x_display_set_fullscreen( v->display, value ))
                        veejay_msg(VEEJAY_MSG_INFO,"OpenGL display window Fullscreen %s",
                                        value == 0 ? "disabled" : "enabled");
        }
        if( v->use_display == 1 )
        {
                if(vj_sdl_set_fullscreen( v->display, value ))
                        veejay_msg(VEEJAY_MSG_INFO,"SDL display window fullsceen %s",
                                        value == 0 ? "disabled" : "enabled");
        }       
}

/******************************************************
 * veejay_malloc()
 *   malloc the pointer and set default options
 *
 * return value: a pointer to an allocated veejay_t
 ******************************************************/


veejay_t *veejay_malloc()
{
	veejay_t *info;
	int i;

	info = (veejay_t *) vj_malloc(sizeof(veejay_t));
	if (!info)
		return NULL;
	memset(info,0,sizeof(veejay_t));

	info->settings = (video_playback_setup *) vj_malloc(sizeof(video_playback_setup));
	if (!(info->settings)) 
		return NULL;
   	memset( info->settings, 0, sizeof(video_playback_setup));
	info->continuous = 1;
	info->sync_correction = 1;
	info->sync_ins_frames = 1;
	info->sync_skip_frames = 1;
	info->no_bezerk = 1;
	info->settings->currently_processed_entry = -1;
	info->settings->first_frame = 1;
	info->settings->state = VEEJAY_STATE_STOP;
	info->port_offset = 3490;// VJ_PORT;
	info->clients = vpn( VEVO_ANONYMOUS_PORT);

	samplebank_init();
	available_diskspace();	

	//vj_event_init();

	if(!vj_server_setup(info))
    	{
	    veejay_msg(0, "Cannot setup service");
	    return -1;
    	}
	else
	{	
		veejay_msg(VEEJAY_MSG_INFO, "TCP/UDP service (port range %d-%d)",
				info->port_offset, info->port_offset+5);
	}

	return info;
}


/******************************************************
 * veejay_main()
 *   the whole video-playback cycle
 *
 * Basic setup:
 *   * this function initializes the devices,
 *       sets up the whole thing and then forks
 *       the main task and returns control to the
 *       main app. It can then start playing by
 *       setting playback speed and such. Stop
 *       by calling veejay_stop()
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int veejay_main(veejay_t * info)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;
	veejay_msg(VEEJAY_MSG_INFO, "Flushing buffers");

    /* Flush the Linux File buffers to disk */
    sync();

    veejay_msg(VEEJAY_MSG_INFO, "Starting playback thread. Giving control to main app");
/*	int ret = pthread_attr_init(&(settings->qtattr));
	ret = pthread_attr_setdetachstate( &(settings->qtattr),
			PTHREAD_CREATE_DETACHED );*/
	
	pthread_mutex_init( &(info->vevo_mutex), NULL );


    /* fork ourselves to return control to the main app */
    if (pthread_create(&(settings->playback_thread), NULL, //&(settings->qtattr),
		       veejay_playback_thread, (void *) info)) {
	veejay_msg(VEEJAY_MSG_ERROR, "Failed to create thread");
	return -1;
    }

    return 1;
}



/*** Methods for simple video editing (cut/paste) ***/

/******************************************************
 * veejay_edit_copy()
 *   copy a number of frames into a buffer
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

static void	veejay_reset_el_buffer( veejay_t *info )
{
	
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

    if (settings->save_list)
	free(settings->save_list);

    settings->save_list = NULL;
    settings->save_list_len = 0;

}
/*
int veejay_edit_copy(veejay_t * info, editlist *el, long start, long end)
{
	uint64_t *res = NULL;
	uint64_t k, i;
	uint64_t n1 = (uint64_t) start;
	uint64_t n2 = (uint64_t) end;

	long len = 0;
	   video_playback_setup *settings =
	(video_playback_setup *) info->settings;

	res = vj_el_edit_copy( el, start, end, &len );

	if (settings->save_list)
		free(settings->save_list);

	settings->save_list = res;
    	if (!settings->save_list)
	{
		veejay_msg(VEEJAY_MSG_ERROR, 
		    "Malloc error, you\'re probably out of memory");
		veejay_change_state(info, VEEJAY_STATE_STOP);
		return 0;
    	}
    	settings->save_list_len = len;

    	veejay_msg(VEEJAY_MSG_DEBUG, "Copied frames %d - %d to buffer (of size %d)",n1,n2,k );

	return 1;
}

int veejay_edit_delete(veejay_t * info, editlist *el, long start, long end)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

	vj_el_edit_del( el, start, end );
    
    return 1;
}

int veejay_edit_cut(veejay_t * info, editlist *el, long start, long end)
{
	if( el->is_empty )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Nothing to cut in EDL");
		return 0;
	}
    if (!veejay_edit_copy(info, el,start, end))
	return 0;
    if (!veejay_edit_delete(info, el,start, end))
	return 0;

    return 1;
}

int veejay_edit_paste(veejay_t * info, editlist *el, long destination)
{
	video_playback_setup *settings =
		(video_playback_setup *) info->settings;
	uint64_t i, k;

	if (!settings->save_list_len || !settings->save_list)
	{
		veejay_msg(VEEJAY_MSG_ERROR, 
			    "No frames in the buffer to paste");
		return 0;
	 }

	vj_el_edit_paste( el, destination, settings->save_list, settings->save_list_len );
	
	return 1;
}



int veejay_edit_move(veejay_t * info,editlist *el, long start, long end,
		      long destination)
{
    long dest_real;
    if( el->is_empty )
		return 0;
	
    if (destination >= el->video_frames || destination < 0
		|| start < 0 || end < 0 || start >= el->video_frames
		|| end >= el->video_frames || end < start)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid parameters for moving video from %ld - %ld to position %ld",
			start,end,destination);
		veejay_msg(VEEJAY_MSG_ERROR, "Range is 0 - %ld", el->video_frames);   
		return 0;
    }

    if (destination < start)
		dest_real = destination;
    else if (destination > end)
		dest_real = destination - (end - start + 1);
    else
		dest_real = start;

    if (!veejay_edit_cut(info, el, start, end))
		return 0;

    if (!veejay_edit_paste(info, el,dest_real))
		return 0;


	return 1;
}

*/
/******************************************************
 * veejay_toggle_audio()
 *   mutes or unmutes audio (1 = on, 0 = off)
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/
int veejay_toggle_audio(veejay_t * info, int audio)
{
    video_playback_setup *settings =
	(video_playback_setup *) info->settings;

    if(!sample_has_audio( info->current_sample ) )
   {
	veejay_msg(VEEJAY_MSG_WARNING, 
		    "Audio playback has not been enabled");
	//info->audio = 0;
	return 0;
    }

    settings->audio_mute = !settings->audio_mute;

    veejay_msg(VEEJAY_MSG_DEBUG, 
		"Audio playback was %s", audio == 0 ? "muted" : "unmuted");
    
    return 1;
}



/*** Methods for saving the currently played movie to editlists or open new movies */

/******************************************************
 * veejay_save_all()
 *   save the whole current movie to an editlist
 *
 * return value: 1 on succes, 0 on error
int veejay_save_all(veejay_t * info, char *filename, long n1, long n2)
{
	if( info->edit_list->num_video_files <= 0 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "EditList has no contents!");
		return 0;
	}
	if(n1 == 0 && n2 == 0 )
		n2 = info->edit_list->video_frames - 1;
	if( vj_el_write_editlist( filename, n1,n2, info->edit_list ) )
		veejay_msg(VEEJAY_MSG_INFO, "Saved editlist to file [%s]", filename);
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error while saving editlist!");
		return 0;
	}	

    return 1;
}
*/

