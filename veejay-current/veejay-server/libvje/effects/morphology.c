/* 
 * Linux VeeJay
 *
 * Copyright(C)2004-2016 Niels Elburg <nwelburg@gmail.com>
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
#include "common.h"
#include <libvjmem/vjmem.h>
#include "morphology.h"

typedef uint8_t (*morph_func)(uint8_t *kernel, uint8_t *mt );

static  uint8_t kernels[8][9] ={
		 { 1,1,1, 1,1,1 ,1,1,1 },//0
		 { 0,1,0, 1,1,1, 0,1,0 },//1
		 { 0,0,0, 1,1,1, 0,0,0 },//2
		 { 0,1,0, 0,1,0, 0,1,0 },//3
		 { 0,0,1, 0,1,0, 1,0,0 },//4
		 { 1,0,0, 0,1,0, 0,0,1 },
		 { 1,1,1, 0,0,0, 0,0,0 },
		 { 0,0,0, 0,0,0, 1,1,1 }
		};


vj_effect *morphology_init(int w, int h)
{
	vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
	ve->num_params = 4;

	ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
	ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
	ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
	ve->limits[0][0] = 0;  // threshold
	ve->limits[1][0] = 255;
	ve->limits[0][1] = 0; 
	ve->limits[1][1] = 7; // convolution kernel
	ve->limits[0][2] = 0;
	ve->limits[1][2] = 1; // morphology operator
	ve->limits[0][3] = 0;
	ve->limits[1][3] = 1; // luma or alpha
    	ve->defaults[0] = 140;
	ve->defaults[1] = 0;
	ve->defaults[2] = 0;
	ve->defaults[3] = 0;

	ve->description = "Morphology (Erosion/Dilation)";

	ve->sub_format = -1;
	ve->extra_frame = 0;
	ve->has_user = 0;
	
	ve->param_description = vje_build_param_list( ve->num_params,"Threshold", "Convolution Kernel", "Mode", "Channel");

	ve->hints = vje_init_value_hint_list( ve->num_params );

	vje_build_value_hint_list(ve->hints, ve->limits[1][1], 1,
				  "[1,1,1],[1,1,1],[1,1,1]",
				  "[0,1,0],[1,1,1],[0,1,0]",
				  "[0,0,0],[1,1,1],[0,0,0]",
				  "[0,1,0],[0,1,0],[0,1,0]",
				  "[0,0,1],[0,1,0],[1,0,0]",
				  "[1,0,0],[0,1,0],[0,0,1]",
				  "[1,1,1],[0,0,0],[0,0,0]",
				  "[0,0,0],[0,0,0],[1,1,1]" );

	vje_build_value_hint_list(ve->hints, ve->limits[1][2], 2,
				  "Dilate",
				  "Erode" );

	vje_build_value_hint_list(ve->hints, ve->limits[1][3], 3,
				  "Luminance",
				  "Alpha"  );
	
	return ve;
}


static uint8_t *binary_img = NULL;

int	morphology_malloc(int w, int h )
{
	binary_img = (uint8_t*) vj_malloc(sizeof(uint8_t) * w * h );
	if(!binary_img) return 0;
	return 1;
}

void		morphology_free(void)
{
	if(binary_img)
		free(binary_img);
	binary_img = NULL;
}

static uint8_t _dilate_kernel3x3( uint8_t *kernel, uint8_t img[9])
{
	register int x;
	/* if one of the neighbouring pixels is up, return 0xff */	
	for(x = 0; x < 9; x ++ )
		if((kernel[x] * img[x]) > 0 )
			return 0xff;
	return 0;
}


static uint8_t _erode_kernel3x3( uint8_t *kernel, uint8_t img[9])
{
	register int x;
	/* if one of the neighbouring pixels is down, return 0 */
	for(x = 0; x < 9; x ++ )
		if(kernel[x] && img[x] == 0 )
			return 0;
	return 0xff;
}

static morph_func	_morphology_function(int i)
{
	if( i == 0 )
		return _dilate_kernel3x3;
	return _erode_kernel3x3;
}

static void morph_threshold_image( const uint8_t *I, const int len, const int threshold, uint8_t *O )
{
	unsigned int i;
	for( i = 0; i < len; i ++ )
	{
		binary_img[i] = (  I[i] < threshold ? 0: 0xff );
	}
}

void morphology_apply( VJFrame *frame, int threshold, int convolution_kernel, int mode, int channel )
{
	unsigned int x,y;
	int len = frame->len;
	int width = frame->width;

	const unsigned int uv_len = (frame->ssm ? frame->len : frame->uv_len);
	
	uint8_t *I = frame->data[0];
	
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];

	switch( channel ) {
		case 1: I = frame->data[3];
		break;
		default:
			I = frame->data[0];
		break;
	}
	
	morph_func	p = _morphology_function(mode);

	if( threshold == 0 ) {
		/* assume image is binary thresholded already */
		veejay_memcpy( binary_img, I, len );
	}
	else {
		morph_threshold_image( I, len, threshold, binary_img );
	}

	if( channel == 0 ) { /* other channel is alpha */
		veejay_memset( Cb, 128, uv_len );
		veejay_memset( Cr, 128, uv_len );
	}	

	len -= width;

	if( mode == 0 ) {
		for(y = width; y < len; y += width  )
		{	
			for(x = 1; x < width-1; x ++)
			{	
				if(binary_img[x+y] == 0)
				{
					uint8_t mt[9] = {
						binary_img[x-1+y-width], binary_img[x+y-width], binary_img[x+1+y-width],
						binary_img[x-1+y],binary_img[x+y], binary_img[x+1+y],
						binary_img[x-1+y+width], binary_img[x+y+width], binary_img[x+1+y+width]
						};
					I[x+y] = p( kernels[convolution_kernel], mt );
				}
				else
				{
					I[x+y] = 0xff;
				}
			}
		}
	}
	else {
		for(y = width; y < len; y += width  )
		{	
			for(x = 1; x < width-1; x ++)
			{	
				if(binary_img[x+y] == 0xff)
				{
					uint8_t mt[9] = {
						binary_img[x-1+y-width], binary_img[x+y-width], binary_img[x+1+y-width],
						binary_img[x-1+y], binary_img[x+y],binary_img[x+1+y],
						binary_img[x-1+y+width], binary_img[x+y+width], binary_img[x+1+y+width]
						};
					I[x+y] = p( kernels[convolution_kernel], mt );
				}
				else 
				{
					I[x+y] = 0;
				}
			}
		}

	}
}
