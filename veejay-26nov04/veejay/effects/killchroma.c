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
#include <config.h>
#include "killchroma.h"
#include "common.h"
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
vj_effect *killchroma_init(int w, int h)
{

    vj_effect *ve = (vj_effect *) malloc(sizeof(vj_effect));
    ve->num_params = 1;
    ve->defaults = (int *) malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) malloc(sizeof(int) * ve->num_params);	/* max */
    ve->sub_format = 0;
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 2;
    ve->defaults[0] = 0;
    ve->has_internal_data = 0;
    ve->description = "Filter out chroma channels";
    ve->extra_frame = 0;
    return ve;
}


void killchroma_apply(VJFrame *frame, int width, int height, int n)
{
	if(n==0)
	{
#ifdef HAVE_ASM_MMX
		memset_ycbcr( frame->data[1], frame->data[1], 128, frame->uv_width,frame->uv_height );
		memset_ycbcr( frame->data[2], frame->data[2], 128, frame->uv_width,frame->uv_height );
#else
		memset( frame->data[1], 128, frame->uv_len);
		memset( frame->data[2], 128, frame->uv_len);
#endif
	}
	else
	{
#ifdef HAVE_ASM_MMX
		memset_ycbcr( frame->data[n], frame->data[n], 128,frame->uv_width, frame->uv_height );
#else
		memset( frame->data[n], 128,  frame->uv_len );
#endif
	}
}
void killchroma_free(){}
