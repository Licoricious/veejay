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

#include "common.h"
#include <libvjmem/vjmem.h>
#include "flip.h"

vj_effect *flip_init(int w, int h)
{

	vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
	ve->num_params = 2;
	ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
	ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
	ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
	ve->defaults[0] = 0;
	ve->defaults[1] = 0;

	ve->limits[0][0] = 0;
	ve->limits[1][0] = 1;
	ve->limits[0][1] = 0;
	ve->limits[1][1] = 1;

	ve->description = "Flip Frame";
	ve->sub_format = 0;
	ve->extra_frame = 0;
	ve->has_user = 0;
	ve->param_description = vje_build_param_list(ve->num_params, "Horizontal", "Vertical");

	ve->hints = vje_init_value_hint_list (ve->num_params);

	vje_build_value_hint_list( ve->hints, ve->limits[1][0], 0,
	                          "Normal", "Flip Horizontal"
	);
	vje_build_value_hint_list( ve->hints, ve->limits[1][1], 1,
	                          "Normal", "Flip Vertical"
	);

	return ve;
}
/**********************************************************************************************
 * flips the image verticaly, derived from SDLcam-0.7.0 (flip_y.c)
 * added uv routine to cope with Cb and Cr data
 *
 * \param frame         Pointer to the actual VJFrame to flip
 **********************************************************************************************/
static void flip_x_yuvdata(VJFrame *frame)
{
	unsigned int y = frame->height, x, x2;
	unsigned int pos = 0;
	const unsigned int w2 = frame->width >> 1;
    const unsigned int w3 = frame->width - 1;
	uint8_t temp;
	const unsigned int uv_width = frame->uv_width -1;
	unsigned int uy = frame->uv_height;
	const unsigned int uw2 = w2 >> frame->shift_h;
	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];

	/* Luminance */
	do {
		x = w2;
        x2 = w3;
		do {
			temp = Y[pos + x2];
			Y[pos + x2] = Y[pos + w3 - x2];
			Y[pos + w3 - x2] = temp;
            --x2;
		} while (x--);
//        Y[pos]=255;
		pos += frame->width;
	} while (--y);

	pos = 0;

	/* Chrominance */
	do {
		x = uw2;
        x2 = uv_width;
		do {
			temp = Cb[pos + x2];
			Cb[pos + x2] = Cb[pos + uv_width - x2];
			Cb[pos + uv_width - x2] = temp;
			temp = Cr[pos + x2];
			Cr[pos + x2] = Cr[pos + uv_width - x2];
			Cr[pos + uv_width - x2] = temp;
            --x2;
		} while (x--);

		pos += frame->uv_width;
	} while (--uy);

}

/**********************************************************************************************
 * flips the image horizontal, derived from SDLcam-0.7.0 (flip_y.c)
 * added uv routine to cope with Cb and Cr data
 *
 * \param frame         Pointer to the actual VJFrame to flip
 **********************************************************************************************/
static void flip_y_yuvdata(VJFrame *frame)
{
	unsigned int x, pos_a = 0, pos_b;
	uint8_t temp;
	unsigned int w1 = frame->width - 1;
	unsigned int y = frame->height >> 1;
	unsigned int uy = y >> frame->shift_v;
	const unsigned int uv_height = frame->uv_height;
	const unsigned int uv_width = frame->uv_width;
	const unsigned int uw1 = ( frame->width >> frame->shift_h ) - 1;
	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];

	/* Luminance */
	pos_b = (frame->height ) * frame->width;
	do {
		x = w1;
		do {
			temp = Y[pos_a + x];
			Y[pos_a + x] = Y[pos_b + x];
			Y[pos_b + x] = temp;
		} while (x--);
		pos_a += frame->width;
		pos_b -= frame->width;
	} while (--y);

	/* Chrominance */
	pos_a = 0;
	pos_b = (uv_height ) * uv_width;
	do {
		x = uw1;
		do {
			temp = Cb[pos_a + x];
			Cb[pos_a + x] = Cb[pos_b + x];
			Cb[pos_b + x] = temp;

			temp = Cr[pos_a + x];
			Cr[pos_a + x] = Cr[pos_b + x];
			Cr[pos_b + x] = temp;
	
		} while (x--);
		pos_a += uv_width;
		pos_b -= uv_width;
	} while (--uy);
}

/**********************************************************************************************
 * flips the image both verticaly and horizontaly, derived from derived
 *
 * \param frame         Pointer to the actual VJFrame to flip
 **********************************************************************************************/
void flip_both_yuvdata(VJFrame *frame)
{
	unsigned int x, x2, pos_a = 0, pos_b;
	uint8_t temp;
	unsigned int w1 = frame->width - 1;
	unsigned int y = frame->height >> 1;
	unsigned int uy = y >> frame->shift_v;
	const unsigned int uv_height = frame->uv_height;
	const unsigned int uv_width = frame->uv_width;
	const unsigned int uw1 = ( frame->width >> frame->shift_h ) - 1;
	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];

	/* Luminance */
	pos_b = (frame->height ) * frame->width;
	do {
		x = w1;
		do {
			temp = Y[pos_a + x];
			Y[pos_a + x] = Y[pos_b + w1 - x];
			Y[pos_b + w1 - x] = temp;
		} while (x--);
		pos_a += frame->width;
		pos_b -= frame->width;
	} while (--y);

	/* Chrominance */
	pos_a = 0;
	pos_b = (uv_height ) * uv_width;
	do {
		x = uw1;
		do {
			temp = Cb[pos_a + x];
			Cb[pos_a + x] = Cb[pos_b + uw1 - x];
			Cb[pos_b + uw1 - x] = temp;

			temp = Cr[pos_a + x];
			Cr[pos_a + x] = Cr[pos_b + uw1 - x];
			Cr[pos_b + uw1 - x] = temp;
	
		} while (x--);
		pos_a += uv_width;
		pos_b -= uv_width;
	} while (--uy);
}

void flip_apply(VJFrame *frame, int h, int v)
{
    if ((h == 0) && (v == 0))
        return;
    if ((h == 1) && (v == 1))
    {
        flip_both_yuvdata(frame);
        return;
    }
    if (h == 1){
        flip_y_yuvdata(frame);
        return;
    }
    if (v == 1)
        flip_x_yuvdata(frame);
}

