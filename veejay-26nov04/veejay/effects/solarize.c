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

#include "solarize.h"
#include <stdlib.h>
vj_effect *solarize_init(int w,int h)
{
    vj_effect *ve = (vj_effect *) malloc(sizeof(vj_effect));
    ve->num_params = 1;
    ve->defaults = (int *) malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) malloc(sizeof(int) * ve->num_params);
    ve->defaults[0] = 16;

    ve->limits[0][0] = 1;	/* threshold */
    ve->limits[1][0] = 255;
	// negation by threshold 
    ve->description = "Solarize";
    ve->sub_format = 1;
    ve->extra_frame = 0;
    ve->has_internal_data = 0;
    return ve;
}

void solarize_apply( VJFrame *frame, int width, int height, int threshold)
{
    int i, len;
    uint8_t val;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb= frame->data[1];
	uint8_t *Cr= frame->data[2];
	len = frame->len;
    for (i = 0; i < len; i++) {
		val = Y[i];
		if (val > threshold)
		{
			Y[i] = 255 - val;
			Cb[i] = 255 - Cb[i]; 
            Cr[i] = 255 - Cr[i];
    	}
    }
}
void solarize_free(){}
