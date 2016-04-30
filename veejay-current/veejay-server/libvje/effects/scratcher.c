/* 
 * Linux VeeJay
 *
 * Copyright(C)2002-2016 Niels Elburg <nwelburg@gmail.com>
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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <libvje/vje.h>
#include <libvjmem/vjmem.h>
#include "scratcher.h"
#include "common.h"
#include "opacity.h" 

static uint8_t *frame[4] = { NULL,NULL,NULL,NULL };
static int nframe = 0;
static int nreverse = 0;
static int last_reverse = 1;
static int last_n = 8;

vj_effect *scratcher_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 3;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 255;
    ve->limits[0][1] = 1;
    ve->limits[1][1] = (MAX_SCRATCH_FRAMES-1);
    ve->limits[0][2] = 0;
    ve->limits[1][2] = 1;
    ve->defaults[0] = 150;
    ve->defaults[1] = 8;
    ve->defaults[2] = 1;
    ve->description = "Overlay Scratcher";
    ve->sub_format = -1;
    ve->extra_frame = 0;
	ve->has_user = 0;
	ve->param_description = vje_build_param_list(ve->num_params, "Opacity", "Scratch buffer", "PingPong");
    return ve;

}

void scratcher_free() {
   if(frame[0])
	  free(frame[0]);
	frame[0] = NULL;
	frame[1] = NULL;
	frame[2] = NULL;
}

int scratcher_malloc(int w, int h)
{
	/* need memory for bounce mode ... */
    frame[0] = (uint8_t *) vj_malloc(w * h * 3 * sizeof(uint8_t) * MAX_SCRATCH_FRAMES);
	if(!frame[0]) 
		return 0;
	
    veejay_memset( frame[0], pixel_Y_lo_, w * h * MAX_SCRATCH_FRAMES );

    frame[1] =
	    frame[0] + ( w * h * MAX_SCRATCH_FRAMES );
    frame[2] =
	    frame[1] + ( w * h  * MAX_SCRATCH_FRAMES );

    veejay_memset( frame[1], 128, (w * h * MAX_SCRATCH_FRAMES) );
    veejay_memset( frame[2], 128, (w * h * MAX_SCRATCH_FRAMES) );
    return 1;
}


static void store_frame(VJFrame *src, int n, int no_reverse)
{
	const int len = src->len;
	int uv_len = src->uv_len;
	int strides[4] = { len, uv_len, uv_len , 0 };

	uint8_t *dest[4] = {
		frame[0] + (len*nframe),
		frame[1] + (uv_len*nframe),
		frame[2] + (uv_len*nframe),
       	NULL
	};

	if (!nreverse) {
		vj_frame_copy( src->data, dest, strides ); 
    }
	else {
		vj_frame_copy( dest, src->data, strides );
    }

	if (nreverse)
		nframe--;
	else
		nframe++;

	if (nframe >= n) {
		if (no_reverse == 0) {
		    nreverse = 1;
		    nframe = n - 1;
			if(nframe < 0)
				nframe = 0;
		} else {
		    nframe = 0;
		}
    }

   	if (nframe == 0)
		nreverse = 0;

}


void scratcher_apply(VJFrame *src,int opacity, int n, int no_reverse)
{
    const unsigned int len = src->len;
    const int offset = len * nframe;
    const int uv_len = src->uv_len;
    const int uv_offset = uv_len * nframe;

	VJFrame tmp;
	veejay_memcpy( &tmp, src, sizeof(VJFrame) );
	
	tmp.data[0] = frame[0] + offset;
	tmp.data[1] = frame[1] + uv_offset;
	tmp.data[2] = frame[2] + uv_offset;

	if( no_reverse != last_reverse || n != last_n )
	{
		last_reverse = no_reverse;
		nframe = n;
		last_n = n;
	}		

	if( nframe == 0 ) {
		tmp.data[0] = src->data[0];
		tmp.data[1] = src->data[1];
		tmp.data[2] = src->data[2];
	}

	opacity_applyN( src, &tmp, opacity );
	
	store_frame( src, n, no_reverse);
}
