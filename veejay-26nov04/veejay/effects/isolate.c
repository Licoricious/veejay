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

#include <stdlib.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "common.h"
#include "isolate.h"

vj_effect *isolate_init(int w, int h)
{
    vj_effect *ve;
    ve = (vj_effect *) malloc(sizeof(vj_effect));
    ve->num_params = 5;
    ve->defaults = (int *) malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) malloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 20;	/* angle */
    ve->defaults[1] = 0;	/* r */
    ve->defaults[2] = 0;	/* g */
    ve->defaults[3] = 0;	/* b */
    ve->defaults[4] = 150;	/* white transparency */
    ve->limits[0][0] = 5;
    ve->limits[1][0] = 900;

    ve->limits[0][1] = 0;
    ve->limits[1][1] = 255;

    ve->limits[0][2] = 0;
    ve->limits[1][2] = 255;

    ve->limits[0][3] = 0;
    ve->limits[1][3] = 255;

    ve->limits[0][4] = 0;
    ve->limits[1][4] = 255;
    ve->has_internal_data = 0;
    ve->description = "Isolate Color";
    ve->extra_frame = 0;
    ve->sub_format = 1;
    return ve;
}

/* another key derivate */

void isolate_apply( VJFrame *frame, int width,
		   int height, int i_angle, int r, int g,
		   int b, int opacity)
{

    uint8_t *fg_y, *fg_cb, *fg_cr;
    uint8_t *bg_y, *bg_cb, *bg_cr;
    int accept_angle_tg, accept_angle_ctg, one_over_kc;
    int kfgy_scale, kg;
    int cb, cr;
    float kg1, tmp, aa = 128, bb = 128, _y = 0;
    float angle = (float) i_angle / 10.0;
    unsigned int pos;
    uint8_t val;
    unsigned int op1 = (opacity > 255) ? 255: opacity;
    unsigned int op0 = 255 - op1;

	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];

    _y = ((Y_Redco * r) + (Y_Greenco * g) + (Y_Blueco * b) + 16);
    aa = ((U_Redco * r) - (U_Greenco * g) - (U_Blueco * b) + 128);
    bb = (-(V_Redco * r) - (V_Greenco * g) + (V_Blueco * b) + 128);
    tmp = sqrt(((aa * aa) + (bb * bb)));
    cb = 127 * (aa / tmp);
    cr = 127 * (bb / tmp);
    kg1 = tmp;
    /* obtain coordinate system for cb / cr */
    accept_angle_tg = 0xf * tan(M_PI * angle / 180.0);
    accept_angle_ctg = 0xf / tan(M_PI * angle / 180.0);
    
    tmp = 1 / kg1;
    one_over_kc = 0xff * 2 * tmp - 0xff;
    kfgy_scale = 0xf * (float) (_y) / kg1;
    kg = kg1;

    /* intialize pointers */
    fg_y = Y;
    fg_cb = Cb;
    fg_cr = Cr;

    bg_y = Y;
    bg_cb = Cb;
    bg_cr = Cr;

    for (pos = (width * height); pos != 0; pos--) {
	short xx, yy;
	xx = (((fg_cb[pos]) * cb) + ((fg_cr[pos]) * cr)) >> 7;
	if (xx < -128) {
	    xx = -128;
	}
	if (xx > 127) {
	    xx = 127;
	}
	yy = (((fg_cr[pos]) * cb) - ((fg_cb[pos]) * cr)) >> 7;
	if (yy < -128) {
	    yy = -128;
	}
	if (yy > 127) {
	    yy = 127;
	}
	val = (xx * accept_angle_tg) >> 4;
	if (val > 127)
	    val = 127;

	if (abs(yy) > val) {
		Y[pos]=235;
		Cb[pos]=128;
		Cr[pos]=128;
	}
	else {
		Y[pos] = (op0 * Y[pos] + (op1 * 235)) / 255;
		Cb[pos] = (op0 * Cb[pos] + (op1 * 128)) / 255;
		Cr[pos] = (op0 * Cr[pos] + (op1 * 128)) / 255;
	}
    }
}

void isolate_free(){}
