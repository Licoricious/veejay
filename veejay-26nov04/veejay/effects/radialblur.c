/* 
 * Linux VeeJay
 *
 * Copyright(C)2002 Niels Elburg <elburg@hio.hen.nl>
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
/*
 * Copyright (C) 2000-2004 the xine project
 * 
 * This file is part of xine, a free video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: radialblur.c,v 1.1 2004/10/27 23:49:01 niels Exp $
 *
 * mplayer's boxblur
 * Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>
 */


#include "radialblur.h"
#include <stdlib.h>
#include "common.h"
#include <veejay/vj-common.h>
extern void *(* veejay_memcpy)(void *to, const void *from, size_t len);

static uint8_t *radial_src[3];

vj_effect *radialblur_init(int w,int h)
{
    vj_effect *ve = (vj_effect *) malloc(sizeof(vj_effect));
    ve->num_params = 3;
    ve->defaults = (int *) malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) malloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 15;
    ve->defaults[1] = 0;
    ve->defaults[2] = 2;

    ve->limits[0][0] = 0;
    ve->limits[1][0] = 90; // radius
    ve->limits[0][1] = 0; // power
    ve->limits[1][1] = 100;
    ve->limits[0][2] = 0; // direction
    ve->limits[1][2] = 2; // 2 = both
    ve->description = "Radial Blur";
    ve->sub_format = 0;
    ve->extra_frame = 0;
    ve->has_internal_data = 0;
    return ve;
}

int	radialblur_malloc(int w, int h)
{
	radial_src[0] = (uint8_t*) vj_malloc(sizeof(uint8_t) * w * h );
	if(!radial_src[0]) return 0;
	radial_src[1] = (uint8_t*) vj_malloc(sizeof(uint8_t) * ((w*h)));
	if(!radial_src[1]) return 0;
	radial_src[2] = (uint8_t*) vj_malloc(sizeof(uint8_t) * ((w*h)));
	if(!radial_src[2]) return 0;
	return 1;
}

//copied from xine
static inline void blur(uint8_t *dst, uint8_t *src, int w, int radius, int dstStep, int srcStep){
	int x;
	const int length= radius*2 + 1;
	const int inv= ((1<<16) + length/2)/length;

	int sum= 0;

	for(x=0; x<radius; x++){
		sum+= src[x*srcStep]<<1;
	}
	sum+= src[radius*srcStep];

	for(x=0; x<=radius; x++){
		sum+= src[(radius+x)*srcStep] - src[(radius-x)*srcStep];
		dst[x*dstStep]= (sum*inv + (1<<15))>>16;
	}

	for(; x<w-radius; x++){
		sum+= src[(radius+x)*srcStep] - src[(x-radius-1)*srcStep];
		dst[x*dstStep]= (sum*inv + (1<<15))>>16;
	}

	for(; x<w; x++){
		sum+= src[(2*w-radius-x-1)*srcStep] - src[(x-radius-1)*srcStep];
		dst[x*dstStep]= (sum*inv + (1<<15))>>16;
	}
}

//copied from xine
static inline void blur2(uint8_t *dst, uint8_t *src, int w, int radius, int power, int dstStep, int srcStep){
	uint8_t temp[2][4096];
	uint8_t *a= temp[0], *b=temp[1];
	
	if(radius){
		blur(a, src, w, radius, 1, srcStep);
		for(; power>2; power--){
			uint8_t *c;
			blur(b, a, w, radius, 1, 1);
			c=a; a=b; b=c;
		}
		if(power>1)
			blur(dst, a, w, radius, dstStep, 1);
		else{
			int i;
			for(i=0; i<w; i++)
				dst[i*dstStep]= a[i];
		}
	}else{
		int i;
		for(i=0; i<w; i++)
			dst[i*dstStep]= src[i*srcStep];
	}
}

static void rhblur_apply( uint8_t *dst , uint8_t *src, int w, int h, int r , int p)
{
	int y;
	for(y = 0; y < h ; y ++ )
	{
		blur2( dst + y * w, src + y *w , w, r,p, 1, 1);
	}	

}
static void rvblur_apply( uint8_t *dst, uint8_t *src, int w, int h, int r , int p)
{
	int x;
	for(x=0; x < w; x++)
	{
		blur2( dst + x, src + x , h, r, p, w, w );
	}
}


void radialblur_apply(VJFrame *frame, int width, int height, int radius, int power, int direction)
{
	const int len = frame->len;
	const int uv_len = frame->uv_len;

	uint8_t *Y = frame->data[0];
	uint8_t *Cb= frame->data[1];
	uint8_t *Cr= frame->data[2];

	if(radius == 0) return;
	// inplace

	veejay_memcpy( radial_src[0] , Y, len);
	veejay_memcpy( radial_src[1] , Cb, uv_len); 
	veejay_memcpy( radial_src[2] , Cr, uv_len);	

	switch(direction)
	{
		case 0: rhblur_apply( Y, radial_src[0],width, height, radius, power );
			rhblur_apply( Cb, radial_src[1],frame->uv_width, frame->uv_height, radius, power );
			rhblur_apply( Cr, radial_src[2],frame->uv_width, frame->uv_height, radius, power );
			break;
		case 1: rvblur_apply( Y, radial_src[0],width, height, radius, power ); 
			rvblur_apply( Cb, radial_src[1],frame->uv_width, frame->uv_height, radius, power );
			rvblur_apply( Cr, radial_src[2],frame->uv_width, frame->uv_height, radius, power );
			break;
		case 2:
			rhblur_apply( Y, radial_src[0],width, height, radius, power );
			rhblur_apply( Cb, radial_src[1],frame->uv_width, frame->uv_height, radius, power );
			rhblur_apply( Cr, radial_src[2],frame->uv_width, frame->uv_height, radius, power );
			rvblur_apply( Y, radial_src[0],width, height, radius, power ); 
			rvblur_apply( Cb, radial_src[1],frame->uv_width, frame->uv_height, radius, power );
			rvblur_apply( Cr, radial_src[2],frame->uv_width, frame->uv_height, radius, power );
			break;
		
	}

}


void radialblur_free()
{
	if( radial_src[0] ) free(radial_src[0]);
	if( radial_src[1] ) free(radial_src[1]);
	if( radial_src[2] ) free(radial_src[2]);
	radial_src[0] = NULL;
	radial_src[1] = NULL;
	radial_src[2] = NULL;
}
