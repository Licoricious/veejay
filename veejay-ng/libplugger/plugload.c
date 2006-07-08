/*
 * Copyright (C) 2002-2006 Niels Elburg <nelburg@looze.net>
 * 
 * This program is free software you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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


/** \defgroup VeVoPlugin Plugin Loader
 *
 * The Plugin Loader can handle:
 *   -# Livido plugins
 *   -# Frei0r plugins
 *   -# FreeFrame plugins
 */
#include <config.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <libhash/hash.h>
#include <libvjmsg/vj-common.h>
#include <libvjmem/vjmem.h>
#include <veejay/portdef.h>
#include <libvevo/libvevo.h>
#include <libplugger/defs.h>
#include <libplugger/specs/livido.h>
#include <libyuv/yuvconv.h>
#include <ffmpeg/avcodec.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <libplugger/plugload.h>
#include <libplugger/freeframe-loader.h>
#include <libplugger/frei0r-loader.h>
#include <libplugger/livido-loader.h>
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avutil.h>

static	vevo_port_t **index_map_ = NULL;
static  vevo_port_t *illegal_plugins_ =NULL;
static  int	index_     = 0;
static	void	*buffer_	 = NULL;
static	void	*buffer2_  = NULL;
static	void	*buffer_b_ = NULL;
static	int	base_width_	=0;
static	int	base_height_	=0;
static  int	n_ff_ = 0;
static	int	n_fr_ = 0;
static  int	n_lvd_ = 0;
static	int	base_fmt_ = -1;

/*
 * port of plugins by name
 *
 * by name: get a value of a parameter, set a value of a parameter
 *
 *          get parameter description
 *	    get plugin name etc
 *
 *	    this will make libvje obsolete
 *	    this will make the fx chain structure in libsample obsolete
 *	    this will change vj performer
 *	    this will change vj event
 *
 *
 *	    merge vevo-sample to sampleadm and merge with libstream
 *	    before continuing.
 *	    
 */

static	int	select_f( const struct dirent *d )
{
	return ( strstr( d->d_name, ".so" ) != NULL );
}

int		plug_set_param_from_str( void *plugin , int p, const char *str, void *values )
{
	int type = 0;
#ifdef STRICT_CHECKING
	assert( plugin != NULL );
#endif
	return livido_set_parameter_from_string( plugin, p, str, values );
}

static	void *instantiate_plugin( void *plugin, int w , int h )
{
	int type = 0;
#ifdef STRICT_CHECKING
	assert( plugin != NULL );
#endif
	int error = vevo_property_get( plugin, "HOST_plugin_type", 0, &type);
#ifdef STRICT_CHECKING
	assert( error == VEVO_NO_ERROR );
#endif
	/*
	if( type == VEVO_FF_PORT )
	{	
		return freeframe_plug_init( plugin,w,h );
	}
	else if( type == VEVO_FR_PORT )
	{
		return frei0r_plug_init( plugin,w,h);
	}
	*/
	if ( type == VEVO_PLUG_LIVIDO )
		return livido_plug_init(plugin,w,h);

#ifdef STRICT_CHECKING
	assert( type == VEVO_PLUG_LIVIDO );
#endif
	
	return NULL;
}//@ warning: if init fails, plugin data should be freed (failed plugin)

static	void	deinstantiate_plugin( void *instance )
{
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_deinit_f	gin;
	int error = vevo_property_get( instance, "HOST_plugin_deinit_f", 0, &gin );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	(*gin)( instance );
}

static	void	add_to_plugin_list( const char *path )
{
	if(!path)
		return;

	int i;
	char fullname[PATH_MAX+1];
	struct	dirent	**files = NULL;
	struct stat sbuf;
	int	res = 0;

	memset( &sbuf,0 ,sizeof(struct stat));
	res = stat( path, &sbuf );

	if( res != 0 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "File or directory '%s' does not exist (skip)", path);
		return;
	}
	
	if( S_ISREG( sbuf.st_mode ) )
	{
		vevo_property_set( illegal_plugins_, path, LIVIDO_ATOM_TYPE_STRING, 0, NULL );
		return;
	}

	if( !S_ISDIR( sbuf.st_mode ) )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Not a directory : '%s'", path );
		return;
	}
	int n_files = scandir( path, &files, select_f, alphasort );
	if( n_files <= 0 )
		return;

	for( i = 0 ; i < n_files; i ++ )
	{
		char *name = files[i]->d_name;

		if( vevo_property_get( illegal_plugins_, name, 0 , NULL ) == 0 )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "'%s' marked as bad", name);
			continue; 
		}
		bzero(fullname , PATH_MAX+1);

		sprintf(fullname, "%s/%s", path,name );

		void *handle = dlopen(fullname, RTLD_NOW );

		if(!handle) 
		{
			veejay_msg(0,"\tPlugin '%s' error '%s'", fullname,
				 dlerror() );
			continue;
		}

		veejay_msg(0, "\tOpened plugin '%s' in '%s'", name,path );
		
		if(dlsym( handle, "plugMain" ))
		{
			void *plugin = deal_with_ff( handle, name );
			if( plugin )
			{
				index_map_[ index_ ] = plugin;
				index_ ++;
				n_ff_ ++;
			}
			else
				dlclose( handle );	
		}
	
		if(dlsym( handle, "f0r_construct" ))
		{
			void *plugin = deal_with_fr( handle, name );
			if( plugin )
			{
				index_map_[ index_ ] = plugin;	
				index_ ++;
				n_fr_ ++;
			}
			else
				dlclose( handle );
		}

		if(dlsym( handle, "livido_setup" ))
		{
			void *plugin = deal_with_livido( handle , name );
			if( plugin )
			{
				veejay_msg(VEEJAY_MSG_INFO, "Load livido plugin '%s' to slot %d",name, index_);
			
				index_map_[index_] = plugin;
				index_ ++;
				n_lvd_ ++;
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Failed to load '%s'",name );
				dlclose(handle);
			}

		}

	}

	for( i = 0; i < n_files; i ++ )
		free( files[i] );
	free(files);
}

static	void	free_plugin(void *plugin)
{

#ifdef STRICT_CHECKING
	assert( plugin != NULL );
#endif
	
	int type = 0;
	int error = vevo_property_get( plugin, "HOST_plugin_type", 0, &type);
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif

	char *name = get_str_vevo( plugin, "name" );
	
	veejay_msg(0, "%s: Free plugin '%s' %p, Type %x",
			__FUNCTION__,name,plugin,type );
	free(name);

	int n = 0;

//		freeframe_plug_free( plugin );		
//		frei0r_plug_free( plugin );
//		livido_plug_free( plugin );
	

//	free_parameters(plugin,n);

	void *handle = NULL;
	error = vevo_property_get( plugin, "handle", 0 , &handle );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	if( handle ) dlclose( handle );
//	vevo_port_free( plugin );
/*
//	livido_port_rrfree( plugin );
	switch(type)
	{
		case VEVO_PLUG_LIVIDO: 	livido_plug_free( plugin ); break;
		case VEVO_PLUG_FR:break;
		case VEVO_PLUG_FF:break;
	}
	*/

//	livido_port_rrfree( plugin );
	if( type == VEVO_PLUG_LIVIDO )
	{
//@FIXME
	//	livido_port_free( plugin );
		vevo_port_recursive_free( plugin );
	}

}

static	void	free_plugins()
{
	int i;
	vevo_port_recursive_free( illegal_plugins_ );
	
	for( i = 0; i < index_ ; i ++ )
		free_plugin( index_map_[i]);
	free( index_map_ );
	index_ = 0;
}

#define CONFIG_FILE_LEN 65535

static	int	scan_plugins()
{
	char *home = getenv( "HOME" );
	char path[PATH_MAX];
	char data[CONFIG_FILE_LEN];
	if(!home) return 0;
	
	sprintf( path , "%s/.veejay/plugins" , home );

	int fd = open( path, O_RDONLY );
	if( fd < 0 )
		return 0;

	bzero( data, CONFIG_FILE_LEN );

	if( read( fd, data, CONFIG_FILE_LEN ) > 0 )
	{
		int len = strlen(data);
		int j;
		int k = 0;
		char value[PATH_MAX];
		bzero( value, PATH_MAX );
		
		char *pch = strtok( data, "\n" );
		while( pch != NULL )
		{
			veejay_msg(0, "Add '%s'",pch );
			add_to_plugin_list( pch );
			pch = strtok( NULL, "\n");
		}

/*
		for( j=0; j < len; j ++ )
		{	
			if(data[j] == '\0' )
				break;
			if( data[j] == '\n' )
			 { add_to_plugin_list( value ); bzero(value,PATH_MAX); k =0;}

			if( isascii( data[j] ) && data[j] != '\n')
			  { value[k] = data[j]; if( k < PATH_MAX) k ++; }
		
		}*/
	}
	return 1;
}

void	plug_sys_free(void)
{
	free_plugins();
	if( buffer_ )
		free( buffer_ );
	if( buffer2_ )
		free( buffer2_ );
	if( buffer_b_)
		free( buffer_b_ );
}

void	plug_sys_init( int fmt, int w, int h )
{
	buffer_ = (void*) malloc( w * h * 4);
	memset( buffer_, 0, w * h * 4);
	buffer2_ = (void*) malloc( w * h * 4);
	memset( buffer2_, 0, w * h  * 4);
	buffer_b_ = (void*) malloc( w * h * 4);
	memset( buffer_b_, 0, w * h  * 4);
	
	base_width_ = w;
	base_height_ = h;

	switch(fmt)
	{
		case FMT_420:
			base_fmt_ = PIX_FMT_YUV420P;
			break;
		case FMT_422:
			base_fmt_ = PIX_FMT_YUV422P;
			break;
		case FMT_444:
			base_fmt_ = PIX_FMT_YUV444P;
			break;
		default:
		veejay_msg(0, "%s: Unknown pixel format",__FUNCTION__);
#ifdef STRICT_CHECKING
			assert(0);
#endif	
			break;	
	}
	
	plug_sys_set_palette( base_fmt_ );
}

int	plug_sys_detect_plugins(void)
{
	index_map_ = (vevo_port_t**) malloc(sizeof(vevo_port_t*) * 256 );
	
	illegal_plugins_ = vevo_port_new( VEVO_ILLEGAL );
#ifdef STRICT_CHECKING
	assert( illegal_plugins_ != NULL );
#endif	
	if(!scan_plugins())
	{
		veejay_msg(VEEJAY_MSG_ERROR,
				"Cannot locate plugins in $HOME/.veejay/plugins" );
		return 0;
	}

	veejay_msg(VEEJAY_MSG_INFO, "Veejay plugin system initialized");
	veejay_msg(VEEJAY_MSG_INFO, "-------------------------------------------------------------------------------------------");
	//@ display copyright notice in binary form
	veejay_msg(VEEJAY_MSG_INFO, "\tFreeFrame - cross-platform real-time video effects");
	veejay_msg(VEEJAY_MSG_INFO, "\t(C) Copyright 2002 Marcus Clements www.freeframe.org. All Rights reserved.");
	veejay_msg(VEEJAY_MSG_INFO, "\thttp://freeframe.sourceforge.net");
	veejay_msg(VEEJAY_MSG_INFO, "\tFound %d FreeFrame %s",
		n_ff_ , n_ff_ == 1 ? "plugin" : "plugins" );
	veejay_msg(VEEJAY_MSG_INFO, "\tfrei0r - a minimalistic plugin API for video effects");
	veejay_msg(VEEJAY_MSG_INFO, "\t(C) Copyright 2004 Georg Seidel, Phillip Promesberger and Martin Bayer.");
	veejay_msg(VEEJAY_MSG_INFO, "\t                   Licensed as GPL");
	veejay_msg(VEEJAY_MSG_INFO, "\thttp://www.piksel.org/frei0r");
	veejay_msg(VEEJAY_MSG_INFO, "\tFound %d frei0r %s",
		n_fr_ , n_fr_ == 1 ? "plugin" : "plugins" );
	veejay_msg(VEEJAY_MSG_INFO, "\tLivido - (Linux) Video Dynamic Objects" );
	veejay_msg(VEEJAY_MSG_INFO, "\t(C) Copyright 2005 Gabriel 'Salsaman' Finch, Niels Elburg, Dennis 'Jaromil' Rojo");
	veejay_msg(VEEJAY_MSG_INFO, "\t                   Daniel Fischer, Martin Bayer, Kentaro Fukuchi and Andraz Tori");
	veejay_msg(VEEJAY_MSG_INFO, "\t                   Licensed as LGPL");
	veejay_msg(VEEJAY_MSG_INFO, "\tFound %d Livido %s",
		n_lvd_, n_lvd_ == 1 ? "plugin" :"plugins" );
	veejay_msg(VEEJAY_MSG_INFO, "-------------------------------------------------------------------------------------------");
	

	return index_;
}

void	plug_clone_from_parameters(void *instance, void *fx_values)
{
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_reverse_clone_parameter_f	grc;
	int error = vevo_property_get( instance, "HOST_plugin_param_reverse_f", 0, &grc );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	(*grc)( instance ,0, fx_values );
	
}

void	plug_clone_parameters( void *instance, void *fx_values )
{
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_clone_parameter_f	gcc;
	int error = vevo_property_get( instance, "HOST_plugin_param_clone_f", 0, &gcc );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	(*gcc)( instance, 0, fx_values );
}

void	plug_set_parameter( void *instance, int seq_num,int n_elements,void *value )
{
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_push_parameter_f	gpp;
	int error = vevo_property_get( instance, "HOST_plugin_param_f", 0, &gpp );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	(*gpp)( instance, seq_num, value );
}

void	plug_get_defaults( void *instance, void *fx_values )
{
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_default_values_f	gdv;
	int error = vevo_property_get( instance, "HOST_plugin_defaults_f", 0, &gdv );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	(*gdv)( instance, fx_values );
}
void	plug_set_defaults( void *instance, void *fx_values )
{
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_clone_parameter_f	gcp;	
	int error = vevo_property_get( instance, "HOST_plugin_param_clone_f", 0, &gcp );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	(*gcp)( instance, 0,fx_values );
}
void	plug_deactivate( void *instance )
{
	deinstantiate_plugin( instance );	
}

void	*plug_activate( int fx_id )
{
	if(!index_map_[fx_id] )
	{
		veejay_msg(0,"Plugin %d is not loaded",fx_id);
		return NULL;
	}
	return instantiate_plugin( index_map_[fx_id], base_width_,base_height_);
}

char	*plug_get_name( int fx_id )
{
	if(!index_map_[fx_id] )
		return NULL;
	char *name = get_str_vevo( index_map_[fx_id], "name" );
	return name;
}

int	plug_get_fx_id_by_name( const char *name )
{
	int n = 0;
	for(n = 0; n < index_; n ++ )
	{
		char *plugname = plug_get_name( n );
		if(plugname)
		{
			if( strncasecmp(name,plugname,strlen(plugname)) == 0 )
			{
				free(plugname);
				return n;
			}
			free(plugname);
		}
	}
	return -1;
}

int	plug_get_num_input_channels( int fx_id )
{
	if(!index_map_[fx_id] )
		return NULL;

	int res = 0;
	int error = vevo_property_get( index_map_[fx_id], "num_inputs",0,&res);

#ifdef STRICT_CHECKING
	assert( error == VEVO_NO_ERROR );
#endif
	return res;
}

int	plug_get_num_parameters( int fx_id )
{
	if(!index_map_[fx_id] )
		return NULL;

	int res = 0;
	int error = vevo_property_get( index_map_[fx_id], "num_params",0,&res);

#ifdef STRICT_CHECKING
	assert( error == VEVO_NO_ERROR );
#endif
	return res;
}	

/*
void	plug_control( int fx_id, void *instance, int *args )
{
	vevo_port_t *port = index_map_[ fx_id ];
#ifdef STRICT_CHECKING
	assert ( port != NULL );
#endif		
	int type = 0;
	int error= 0;
	error = vevo_property_get( port, "type", 0, &type);
#ifdef STRICT_CHECKING
	assert( error == LIVIDO_NO_ERROR );
#endif	
	if( type == VEVO_FF_PORT )
	{
		freeframe_plug_control( port, args );	
	}
	else if ( type == VEVO_FR_PORT )
	{
		frei0r_plug_control( port, args );
	}
	else if ( type == VEVO_LIVIDO_PORT )
	{
	//	livido_plug_control( port, args );
		livido_set_parameters_scaled( port, args );  
	}
}*/

void	plug_sys_set_palette( int pref_palette )
{
	base_fmt_ = pref_palette;
	livido_set_pref_palette( base_fmt_ );
}

/*
static void	plug_process_mix( VJFrame *frame, VJFrame *frame_b,int fx_id )
{
	void	*plugin = index_map_[fx_id];
#ifdef STRICT_CHECKING	
	assert( plugin != NULL );
	assert( frame != NULL );
	assert( frame_b != NULL );
#endif
	util_convertrgba32( frame->data, base_width_, base_height_, base_format_, frame->shift_v, buffer_ );

	util_convertrgba32( frame_b->data,base_width_,base_height_, base_format_, frame_b->shift_v, buffer_b_ );	

	process_mix_plugin( plugin, buffer_, buffer_b_, buffer2_ );
#ifdef STRICT_CHECKING
	assert( buffer2_ != NULL);
#endif

#ifdef STRICT_CHECKING
	if( base_fmt_ == 0 )
	{
		assert( (frame->width * frame->height) == frame->len );
		assert( (frame->uv_width * frame->uv_height) == frame->uv_len );

		assert( (frame->width / 2 ) == frame->uv_width );
	        assert( (frame->height /2 ) == frame->uv_height );	
		
		assert( frame->shift_v == 1 );
		assert( frame->shift_h == 1 );

	}
	if( base_fmt_ == 1 )
	{
		assert( (frame->width * frame->height) == frame->len );
		assert( (frame->uv_width * frame->uv_height) == frame->uv_len );
		assert( frame->width /2  == frame->uv_width );
	        assert( (frame->height ) == frame->uv_height );	
		assert( frame->shift_v == 0 );
		assert( frame->shift_h == 1 );
		
	}
	assert( base_fmt_ == 0 || base_fmt_ == 1 );
#endif
	
	util_convertsrc( buffer2_, base_width_, base_height_, base_format_, frame->data );
}*/

void	plug_push_frame( void *instance, int out, int seq_num, void *frame_info )
{
	VJFrame *frame = (VJFrame*) frame_info;
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_push_channel_f	gpu;
	int error = vevo_property_get( instance, "HOST_plugin_push_f", 0, &gpu );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	(*gpu)( instance, (out ? "out_channels" : "in_channels" ), seq_num, frame );
}

void	plug_process( void *instance )
{
#ifdef STRICT_CHECKING
	assert( instance != NULL );
#endif
	generic_process_f	gpf;
	int error = vevo_property_get( instance, "HOST_plugin_process_f", 0, &gpf );
#ifdef STRICT_CHECKING
	assert( error == 0 );
#endif
	
	(*gpf)( instance,0.0 );
	
	/*
	// it is frei0r or freeframe mixing plugin
	int is_mix = 0;
	error = vevo_property_get( plugin, "mixer", 0, &is_mix );
#ifdef HAVE_STRICT_
	assert( error == LIVIDO_NO_ERROR );
#endif
	if( is_mix ) 
	{
		plug_process_mix( frame, b, fx_id );
		return;
	}

	util_convertrgba32( frame->data, base_width_, base_height_, base_format_, frame->shift_v, buffer_ );

	void *res_frame = process_plug_plugin( plugin, buffer_, buffer2_ );
#ifdef STRICT_CHECKING
	assert( res_frame != NULL );
#endif

	util_convertsrc( res_frame, base_width_, base_height_, base_format_, frame->data );
	*/
}

