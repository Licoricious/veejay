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

#include "transcarot.h"

#include <stdlib.h>

vj_effect *transcarot_init(int width, int height)
{
    vj_effect *ve = (vj_effect *) malloc(sizeof(vj_effect));
    ve->num_params = 6;
    ve->defaults = (int *) malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) malloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 180;	/* opacity */
    ve->defaults[1] = 0;	/* type */
    ve->defaults[2] = 100;	/* point sizse */
    ve->defaults[3] = (height / 2);	/* dy */
    ve->defaults[4] = 40;	/* dye */
    ve->defaults[5] = (width / 2);	/* row */

    ve->limits[0][0] = 0;
    ve->limits[1][0] = 255;

    ve->limits[0][1] = 0;
    ve->limits[1][1] = 1;
    ve->limits[0][2] = 1;
    ve->limits[1][2] = width;
    ve->limits[0][3] = 1;
    ve->limits[1][3] = height;
    ve->limits[0][4] = 1;
    ve->limits[1][4] = height;
    ve->limits[0][5] = 1;
    ve->limits[1][5] = width;
    ve->sub_format = 0;
    ve->description = "Transition Translate Carot";
    ve->has_internal_data = 0;
    ve->extra_frame = 1;
    return ve;
}
void transcarot1_apply(uint8_t * yuv1[3], uint8_t * yuv2[3], int width,
		       int height, int point_size, int dy, int dye,
		       int row_start, int opacity)
{

    int row_length = 1;
    int reverse = 0;
    int i;
    unsigned int op0, op1;
    unsigned int uv_width = width >> 1;
    int uv_dy, uv_dye, uv_row_start, uv_row_length;


    op1 = (opacity > 255) ? 255 : opacity;
    op0 = 255 - op1;

    uv_dy = dy >> 1;
    uv_dye = dye >> 1;
    uv_row_start = row_start;
    uv_row_length = 1;

    while (dy >= dye && row_length > 0) {
	for (i = row_start; i < (row_start + row_length); i++) {
	    yuv1[0][(dy * width + i)] =
		(op0 * yuv1[0][(dy * width + i)] +
		 op1 * yuv2[0][(dy * width + i)]) / 235;

	}
	if (reverse == 1) {
	    row_length -= 2;
	    row_start++;
	} else {
	    row_length += 2;
	    row_start--;
	}

	if (row_length >= point_size)
	    reverse = 1;
	dy--;
    }
    reverse = 0;
    i = 0;
    while (uv_dy >= uv_dye && uv_row_length > 0) {
	for (i = uv_row_start; i < ((uv_row_start + uv_row_length) >> 1);
	     i++) {
	    yuv1[1][(uv_dy * uv_width + i)] =
		(op0 * yuv1[1][(uv_dy * uv_width + i)] +
		 op1 * yuv2[1][(uv_dy * uv_width + i)]) / 235;
	    yuv1[2][(uv_dy * uv_width + i)] =
		(op0 * yuv1[2][(uv_dy * uv_width + i)] +
		 op1 * yuv2[2][(uv_dy * uv_width + i)]) / 235;

	}
	if (reverse == 1) {
	    uv_row_length -= 2;
	    uv_row_start++;
	} else {
	    uv_row_length += 2;
	    uv_row_start--;
	}
	if (uv_row_length >= point_size)
	    reverse = 1;
	dy--;
    }

}

/* carot transistion. like translate, but different form and with mirroring */
void transcarot2_apply(uint8_t * yuv1[3], uint8_t * yuv2[3], int width,
		       int height, int point_size, int dy, int dye,
		       int row_start, int opacity)
{

    int row_length = 1;
    int reverse = 0;
    int i;
    unsigned int op0, op1;
    unsigned int len = (width * height);
    unsigned int uv_width = width >> 1;
    int uv_dy, uv_dye, uv_row_start, uv_row_length;


    op1 = (opacity > 235) ? 235 : opacity;
    op0 = 235 - op1;

    uv_dy = dy >> 1;
    uv_dye = dye >> 1;
    uv_row_start = row_start;
    uv_row_length = 1;

    while (dy >= dye && row_length > 0) {
	if (reverse == 0) {
	    for (i = row_start; i < (row_start + row_length); i++) {
		yuv1[0][(dy * width + i)] =
		    (op0 * yuv1[0][(dy * width + i)] +
		     op1 * yuv2[0][(dy * width + i)]) / 235;

	    }
	} else {
	    for (i = row_start; i < (row_start + row_length); i++) {
		yuv1[0][(dy * width + i)] =
		    (op0 * yuv1[0][(dy * width + i)] +
		     op1 * yuv2[0][len - (dy * width + i)]) / 235;
	    }
	}
	if (reverse == 1) {
	    row_length -= 2;
	    row_start++;
	} else {
	    row_length += 2;
	    row_start--;
	}

	if (row_length >= point_size)
	    reverse = 1;
	dy--;
    }
    reverse = 0;
    i = 0;
    len >>= 2;
    while (uv_dy >= uv_dye && uv_row_length > 0) {
	if (reverse == 0) {
	    for (i = uv_row_start;
		 i < ((uv_row_start + uv_row_length) >> 1); i++) {
		yuv1[1][(uv_dy * uv_width + i)] =
		    (op0 * yuv1[1][(uv_dy * uv_width + i)] +
		     op1 * yuv2[1][(uv_dy * uv_width + i)]) / 235;
		yuv1[2][(uv_dy * uv_width + i)] =
		    (op0 * yuv1[2][(uv_dy * uv_width + i)] +
		     op1 * yuv2[2][(uv_dy * uv_width + i)]) / 235;

	    }
	} else {
	    for (i = uv_row_start;
		 i < ((uv_row_start + uv_row_length) >> 1); i++) {
		yuv1[1][(uv_dy * uv_width + i)] =
		    (op0 * yuv1[1][(uv_dy * uv_width + i)] +
		     op1 * yuv2[1][len - (uv_dy * uv_width + i)]) / 235;
		yuv1[2][(uv_dy * uv_width + i)] =
		    (op0 * yuv1[2][(uv_dy * uv_width + i)] +
		     op1 * yuv2[2][len - (uv_dy * uv_width + i)]) / 235;

	    }


	}
	if (reverse == 1) {
	    uv_row_length -= 2;
	    uv_row_start++;
	} else {
	    uv_row_length += 2;
	    uv_row_start--;
	}
	if (uv_row_length >= point_size)
	    reverse = 1;
	dy--;
    }

}

void transcarot_apply(uint8_t * yuv1[3], uint8_t * yuv2[3], int width,
		      int height, int p, int dy, int dye, int row,
		      int opacity, int type)
{
    if (type == 1)
	transcarot1_apply(yuv1, yuv2, width, height, p, dy, dye, row,
			  opacity);
    if (type == 0)
	transcarot2_apply(yuv1, yuv2, width, height, p, dy, dye, row,
			  opacity);
}
void transcarot_free(){}
