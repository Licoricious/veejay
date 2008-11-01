/* 
 * LIBVJE - veejay fx library
 *
 * Copyright(C)2006 Niels Elburg <nelburg@looze.net>
 * See COPYING for software license and distribution details
 */


#define IS_LIVIDO_PLUGIN 
#include 	<livido.h>
LIVIDO_PLUGIN
#include	"utils.h"
#include	"livido-utils.c"

livido_init_f	init_instance( livido_port_t *my_instance )
{
	return LIVIDO_NO_ERROR;
}

livido_deinit_f	deinit_instance( livido_port_t *my_instance )
{
	return LIVIDO_NO_ERROR;
}

/* also from yuvdenoise */
void contrast_uv_apply(uint8_t *oCb, uint8_t *oCr, uint8_t *Cb, const int uv_len, uint8_t *Cr, int s) {
        unsigned int r;
        register int cb;
        register int cr;

        for(r=0; r < uv_len; r++) {
                cb = Cb[r];
                cb -= 128;
                cb *= s;
                cb = (cb + 50)/100;
                cb += 128;

                cr = Cr[r];
                cr -= 128;
                cr *= s;
                cr = (cr + 50)/100;
                cr += 128;

                oCb[r] = cb;
                oCr[r] = cr;
        }
}

void contrast_y_apply(uint8_t *oY, uint8_t *Y, const int len, int s) {
   unsigned int r;
   register int m;

   for(r=0; r < len; r++) {
        m = Y[r];
        m -= 128;
        m *= s;
        m = (m + 50)/100;
        m += 128;
        oY[r] = m;
    }

}


livido_process_f		process_instance( livido_port_t *my_instance, double timecode )
{
	int len =0;
	int i = 0;
	uint8_t *A[4] = {NULL,NULL,NULL,NULL};
	uint8_t *O[4]= {NULL,NULL,NULL,NULL};
	int x,y;
	int palette[3];
	int w[3];
	int h[3];

	int error = lvd_extract_channel_values( my_instance, "in_channels", 0, &w[0], &h[0], A, &palette[0] );
	if( error != LIVIDO_NO_ERROR )
		return LIVIDO_ERROR_HARDWARE; //@ error codes in livido flanky

	error	  = lvd_extract_channel_values( my_instance, "out_channels", 0, &w[1],&h[1], O,&palette[1] );
	if( error != LIVIDO_NO_ERROR )
		return LIVIDO_ERROR_HARDWARE; //@ error codes in livido flanky

#ifdef STRICT_CHECKING
	assert( w[0] == w[1] );
	assert( h[0] == h[1] );
	assert( palette[0] == palette[1] );
	assert( A[0] != NULL );
	assert( A[1] != NULL );
	assert( A[2] != NULL );
	assert( O[0] != NULL );
	assert( O[1] != NULL );
	assert( O[2] != NULL );
#endif

	int uv_len = lvd_uv_plane_len( palette[0],w[0],h[0] );
	len = ( w[0] * h[0] ) - w[0] - 1;
	const int wid = w[0];
	const int hei = w[1];

        double sharp  = lvd_extract_param_number(
                                my_instance,
                                "in_parameters",
                                0 );

	int c = (2048 * sharp );
	
   /* The sharpen comes from yuvdenoiser, we like grainy video so 512 is allowed.  */ 
      
   unsigned int r;
   register int d,e,m;
   uint8_t *Y = A[0];
   for(r=0; r < len; r++) {
        m = ( Y[r] + Y[r+1] + Y[r+wid] + Y[r+wid+1] + 2) >> 2;
        d = Y[r] - m;
        d *= c;
        d /= 100;
        m = m + d;
	if( m > 0xff ) m = 0xff ; else if ( m < 0 )m = 0;
        O[0][r] = m;
        }
   
   for(r=len; r < (wid*hei); r++) {
        m = (Y[r] + Y[r+1] + Y[r-wid] + Y[r-wid+1] + 2) >> 2;
        d = Y[r]-m;
        d *= c;
        d /= 100;
        m = m + d;
        if( m > 0xff) m = 0xff; else if (m < 0 ) m = 0;
        O[0][r] = m;
   }

	livido_memcpy( O[1], A[1], uv_len );
	livido_memcpy( O[2], A[2], uv_len );	
	
	return LIVIDO_NO_ERROR;
}

livido_port_t	*livido_setup(livido_setup_t list[], int version)

{
	LIVIDO_IMPORT(list);

	livido_port_t *port = NULL;
	livido_port_t *in_params[4];
	livido_port_t *in_chans[3];
	livido_port_t *out_chans[1];
	livido_port_t *info = NULL;
	livido_port_t *filter = NULL;

	info = livido_port_new( LIVIDO_PORT_TYPE_PLUGIN_INFO );
	port = info;

		livido_set_string_value( port, "maintainer", "Niels");
		livido_set_string_value( port, "version","1");
	
	filter = livido_port_new( LIVIDO_PORT_TYPE_FILTER_CLASS );
	livido_set_int_value( filter, "api_version", LIVIDO_API_VERSION );
	livido_set_voidptr_value( filter, "deinit_func", &deinit_instance );
	livido_set_voidptr_value( filter, "init_func", &init_instance );
	livido_set_voidptr_value( filter, "process_func", &process_instance );
	port = filter;

		livido_set_string_value( port, "name", "Sharpen");	
		livido_set_string_value( port, "description", "Software Sharpen");
		livido_set_string_value( port, "author", "Niels Elburg");
		
		livido_set_int_value( port, "flags", 0);
		livido_set_string_value( port, "license", "GPL2");
		livido_set_int_value( port, "version", 1);
	
	int palettes0[] = {
                      	LIVIDO_PALETTE_YUV420P,
                       	LIVIDO_PALETTE_YUV422P,
			LIVIDO_PALETTE_YUV444P,
               		0
	};
	
        in_chans[0] = livido_port_new( LIVIDO_PORT_TYPE_CHANNEL_TEMPLATE );
	port = in_chans[0];
            
                livido_set_string_value( port, "name", "Channel A");
           	livido_set_int_array( port, "palette_list", 3, palettes0);
		livido_set_int_value( port, "flags", 0);

	out_chans[0] = livido_port_new( LIVIDO_PORT_TYPE_CHANNEL_TEMPLATE );
	port = out_chans[0];
	
	        livido_set_string_value( port, "name", "Output Channel");
		livido_set_int_array( port, "palette_list", 3, palettes0);
		livido_set_int_value( port, "flags", 0);

	in_params[0] = livido_port_new( LIVIDO_PORT_TYPE_PARAMETER_TEMPLATE );
        port = in_params[0];

                livido_set_string_value(port, "name", "Sharpen" );
                livido_set_string_value(port, "kind", "NUMBER" );
                livido_set_double_value( port, "min", 0.0 );
                livido_set_double_value( port, "max", 1.0 );
                livido_set_double_value( port, "default",0.5);
                livido_set_string_value( port, "description" ,"Sharpen");

	livido_set_portptr_array( filter, "in_channel_templates", 1 , in_chans );
	livido_set_portptr_array( filter, "out_parameter_templates",0, NULL );
	livido_set_portptr_array( filter, "in_parameter_templates",1, in_params );
	livido_set_portptr_array( filter, "out_channel_templates", 1, out_chans );


	livido_set_portptr_value(info, "filters", filter);
	return info;
}
