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

#include "dither.h"
#include <stdlib.h>
vj_effect *dither_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) malloc(sizeof(vj_effect));
    ve->num_params = 2;
    ve->defaults = (int *) malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) malloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 2;
    ve->defaults[1] = 0;

    ve->limits[0][0] = 2;
    ve->limits[1][0] = 255;
    ve->limits[0][1] = 0;
    ve->limits[1][1] = 1;

    ve->description = "Matrix Dithering";
    ve->sub_format = 0;
    ve->extra_frame = 0;
    ve->has_internal_data = 0;
    return ve;
}

void dither_apply(uint8_t * yuv[3], int width, int height, int size,
		  int random_on)
{
    long int w_, h_;
    long int dith[size][size];
    long int i, j, d, v, l, m;

    for (l = 0; l < size; l++) {
	for (m = 0; m < size; m++) {
	    dith[l][m] =
		1 + (int) ((double) size * rand() / (RAND_MAX + 1.0));
	}
    }

    for (h_ = 0; h_ < height; h_++) {
	j = h_ % size;
	for (w_ = 0; w_ < width; w_++) {
	    i = w_ % size;
	    d = dith[i][j] << 4;
	    /* Luminance , dither image. Do
	       this for U and V too, see what happens hehe */
	    v = ((long) yuv[0][((h_ * width) + w_)] + d);
	    if (v > 255)
		v = 216;
	    yuv[0][(h_ * width) + w_] = (uint8_t) ((v >> 7) << 7);
	}
    }
}
void dither_free(){}
