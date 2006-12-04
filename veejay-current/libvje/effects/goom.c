/* 
 * Linux VeeJay
 *
 * Copyright(C)2004 Niels Elburg <elburg@hio.hen.nl>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307 , USA.
 */
#include <config.h>
#include <ffmpeg/avutil.h>

#include <libyuv/yuvconv.h>
#include <libgoom/goom.h>
#include <stdlib.h>

static	PluginInfo *goom_ = NULL;
static uint8_t *goom_buffer_ = NULL;
static  int last_= 0;

vj_effect *goomfx_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 2;

    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 10;
    ve->limits[0][1] = 100;
    ve->limits[1][1] = 5000;
    ve->defaults[0] = 2;
    ve->defaults[1] = 2500;
    ve->description = "Goom";
    ve->sub_format = 0;
    ve->extra_frame = 0;
	ve->has_user = 0;
    return ve;
}

int goomfx_malloc(int width, int height)
{
	goom_ = goom_init( width,height );
	if(!goom_ )
		return 0;
	goom_buffer_ = vj_malloc( width * height * 4 );
	goom_set_screenbuffer( goom_, goom_buffer_);
	return 1;
}
void goomfx_free()
{
	goom_close( goom_ );
	if(goom_buffer_)
		free(goom_buffer_);
	goom_buffer_ = NULL;
	goom_ = NULL;
}

void goomfx_apply( VJFrame *frame, int width, int height, int val, int val2)
{
    unsigned int i;
    uint8_t *Y = frame->data[0];
    int chunks = frame->len / 1024;
    int16_t data[2][1024];
    float fps = (float)val2 * 0.01f;
    int j;

	if( last_ >= chunks )
		last_ = 0;
	i = last_;
   	last_ ++; 
	for( j = 0; j < 512; j ++ )
	{
		data[0][j] = (Y[i+j]-128) * 256;
		data[1][j] = (Y[i+j]-128) * 256;
	}
	    
    	goom_update( goom_, 
		    data,
		    val,
		    fps,
		    NULL,
		    NULL );
    
    int pix_fmt = 0;
    if( frame->shift_v == 0 )
	    pix_fmt = PIX_FMT_YUV422P;
    else
	    pix_fmt = PIX_FMT_YUV420P;
    util_convertsrc( goom_buffer_,
		    width,
		    height,
		    pix_fmt,
		    1,
		    frame->data,
		    1 );
		    
}
