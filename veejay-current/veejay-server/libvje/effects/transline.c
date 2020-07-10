/* 
 * Linux VeeJay
 *
 * Copyright(C)2002 Niels Elburg <nwelburg@gmail.com>
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

#include <libvje/effects/common.h>
#include <veejaycore/vjmem.h>
#include "transline.h"

vj_effect *transline_init(int width, int height)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 4;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 150;	/* opacity */
    ve->defaults[1] = 10;	/* line width */
    ve->defaults[2] = 3;	/* distance */
    ve->defaults[3] = 0;	/* type */
    
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 255;

    ve->limits[0][1] = 1;
    ve->limits[1][1] = width;
    ve->limits[0][2] = 2;
    ve->limits[1][2] = width;
    ve->limits[0][3] = 0;
    ve->limits[1][3] = 1;
    ve->sub_format = 1;
    ve->description = "Transition Line";
    ve->extra_frame = 1;
	ve->has_user = 0;
	ve->param_description = vje_build_param_list(ve->num_params, "Opacity", "Line width", "Distance" , "Mode");
    return ve;
}

static void transline1_apply(uint8_t *yuv1[4], uint8_t *yuv2[4], int width, int height, int distance, int line_width)
{
    int x, y, z;
    int step;

    for (y = 0; y < height; y++) {
		for (x = 0; x < width; x += distance) {
	    	step = line_width;
	    	if (distance < step)
				step = distance - 1;
	    	for (z = 0; z < step; z++) {
				yuv1[0][(y * width + x + z)] = yuv2[0][(y * width + x + z)];
				yuv1[1][(y * width + x + z)] = yuv2[1][(y * width + x + z)];
				yuv1[2][(y * width + x + z)] = yuv2[2][(y * width + x + z)];
	    	}
		}
    }
}

static void transline2_apply(uint8_t *yuv1[4], uint8_t *yuv2[4], int width, int height, int distance, int line_width, int opacity)
{
    unsigned int op0, op1;
    int x, y, z;
    int step;

    op1 = (opacity > 255) ? 255 : opacity;
    op0 = 255 - op1;
    for (y = 0; y < height; y++) {
		for (x = 0; x < width; x += distance) {
	    	step = line_width;
	    	if (distance < step)
				step = distance - 1;
	    	for (z = 0; z < step; z++) {
				yuv1[0][(y * width + x + z)] =
		   		 	(op0 * yuv1[0][(y * width + x + z)] +
		   	 		 op1 * yuv2[0][(y * width + x + z)]) >> 8;
				yuv1[1][(y * width + x + z)] =
		   			(op0 * yuv1[1][(y * width + x + z)] +
		   		  	op1 * yuv2[1][(y * width + x + z)]) >> 8;
				yuv1[2][(y * width + x + z)] =
				    (op0 * yuv1[2][(y * width + x + z)] +
				     op1 * yuv2[2][(y * width + x + z)]) >> 8;
	    	}
		}
    }
}

void transline_apply( void *ptr, VJFrame *frame, VJFrame *frame2, int *args ) {
    int distance = args[0];
    int line_width = args[1];
    int opacity = args[2];
    int type = args[3];

    if (type == 1)
		transline1_apply(frame->data, frame2->data, frame->width, frame->height, distance, line_width);
    if (type == 0)
		transline2_apply(frame->data, frame2->data, frame->width, frame->height, distance, line_width, opacity);
}
