/* 
 * Linux VeeJay
 *
 * Copyright(C)2004 Niels Elburg <nwelburg@gmail.com>
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
#include <veejaycore/vjmem.h>
#include "keyselect.h"

vj_effect *keyselect_init(int w, int h)
{
    vj_effect *ve;
    ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 6;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 4500;	/* angle */
    ve->defaults[1] = 0;	/* r */
    ve->defaults[2] = 0;	/* g */
    ve->defaults[3] = 255;	/* b */
    ve->defaults[4] = 3;	/* blend type */
    ve->defaults[5] = 2400;	/* noise suppression */
    ve->limits[0][0] = 1;
    ve->limits[1][0] = 9000;

    ve->limits[0][1] = 0;
    ve->limits[1][1] = 255;

    ve->limits[0][2] = 0;
    ve->limits[1][2] = 255;

    ve->limits[0][3] = 0;
    ve->limits[1][3] = 255;

    ve->limits[0][4] = 0;
    ve->limits[1][4] = 7;

	ve->limits[0][5] = 0;
	ve->limits[1][5] = 5500;
	ve->has_user = 0;
    ve->parallel = 1;
	ve->description = "Blend by Color Key (RGB)";
    ve->extra_frame = 1;
    ve->sub_format = 1;
	ve->rgb_conv = 1;
	ve->param_description = vje_build_param_list(ve->num_params, "Angle","Red","Green","Blue","Blend mode","Noise suppression" );
    return ve;
}

typedef uint8_t(*blend_func)(uint8_t y1, uint8_t y2);
static blend_func get_blend_func(const int mode);

static uint8_t blend_func1(uint8_t a, uint8_t b) {
	return (uint8_t)  255 - (abs(255-a-b));
}

static uint8_t blend_func2(uint8_t a, uint8_t b) {
	uint8_t val;
	if( a == 0 )
	  a = 0xff;
	val = 255 -  ((255-b) * (255-b))/a;
	return CLAMP_Y(val);
}

static uint8_t blend_func3(uint8_t a , uint8_t b) {
	return (uint8_t) (a * b)>>8;
}


static uint8_t blend_func4(uint8_t a, uint8_t b) {
	uint8_t c = 0xff - b;
	if( c == 0 )
		return CLAMP_Y(c);

	return CLAMP_Y( (a*a)/c  );
}

static uint8_t blend_func5(uint8_t a, uint8_t b) {
	uint8_t val;
	uint8_t c = 0xff - b;
	if( c == 0 )
		return CLAMP_Y(b);
	val = b / c;
	return CLAMP_Y(val);
}

static uint8_t blend_func6(uint8_t a, uint8_t b) {
	uint8_t val = a + (2 * (b)) - 255;
	return CLAMP_Y(val);
}

static uint8_t blend_func7(uint8_t a, uint8_t b) {
	return (uint8_t) ( a + ( b - 255) );
}

static uint8_t blend_func8(uint8_t a, uint8_t b) {
	int c;
	if(b < 128) c = (a * b) >> 7;
	else c = 255 - ((255-b)*(255-a)>>7);
	return CLAMP_Y(c);
}

static blend_func get_blend_func(const int mode) {
	switch(mode) {
		case 0: return &blend_func1;
		case 1: return &blend_func2;
		case 2: return &blend_func3;
		case 3: return &blend_func4;
		case 4: return &blend_func5;
		case 5: return &blend_func6;
		case 6: return &blend_func7;
		case 7: return &blend_func8;
	}
	return &blend_func1;	
}

/*
Originally from:
http://www.cs.utah.edu/~michael/chroma/
*/
void keyselect_apply( void *ptr, VJFrame *frame, VJFrame *frame2, int *args ) {
    int i_angle = args[0];
    int r = args[1];
    int g = args[2];
    int b = args[3];
    int mode = args[4];
    int i_noise = args[5];

	uint8_t *fg_y, *fg_cb, *fg_cr;
    uint8_t *bg_y, *bg_cb, *bg_cr;
	const unsigned int width = frame->width;
	const unsigned int height = frame->height;
    int accept_angle_tg, accept_angle_ctg, one_over_kc;
    int kfgy_scale, kg;
    int cb, cr;
    int kbg, x1, y1;
    float kg1, tmp, aa = 255.0f, bb = 255.0f, _y = 0;
    float angle = (float) i_angle / 100.0f;
    float noise_level = (i_noise / 100.0f);
    unsigned int pos;
    uint8_t val, tmp1;
    uint8_t *Y = frame->data[0];
	uint8_t *Cb= frame->data[1];
	uint8_t *Cr= frame->data[2];
	
	int	iy=pixel_Y_lo_,iu=128,iv=128;
	_rgb2yuv( r,g,b, iy,iu,iv );
	_y = (float) iy;
	aa = (float) iu;
	bb = (float) iv;
    tmp = sqrt(((aa * aa) + (bb * bb)));
    cb =255 * (aa / tmp);
    cr =255 * (bb / tmp);
    kg1 = tmp;

    blend_func blend_pixel = get_blend_func(mode);

    /* obtain coordinate system for cb / cr */
    accept_angle_tg = (int)( 15.0f * tanf(M_PI * angle / 180.0f));
    accept_angle_ctg= (int)( 15.0f / tanf(M_PI * angle / 180.0f));


    tmp = 1 / kg1;
    one_over_kc = 0xff * 2 * tmp - 0xff;
    kfgy_scale = 0xf * (float) (_y) / kg1;
    kg = kg1;

    /* intialize pointers */
    fg_y = Y;
    fg_cb = Cb;
    fg_cr = Cr;
	/* 2005: swap these !! */
    bg_y = frame2->data[0];
    bg_cb = frame2->data[1];
    bg_cr = frame2->data[2];

    for (pos = (width * height); pos != 0; pos--) {
	short xx, yy;
	/* convert foreground to xz coordinates where x direction is
	   defined by key color */

	xx = (((fg_cb[pos]) * cb) + ((fg_cr[pos]) * cr)) >> 7;
	yy = (((fg_cr[pos]) * cb) - ((fg_cb[pos]) * cr)) >> 7;

	/* accept angle should not be > 90 degrees 
	   reasonable results between 10 and 80 degrees.
	 */

	val = (xx * accept_angle_tg) >> 4;
	if (abs(yy) < val) {
	    /* compute fg, suppress fg in xz according to kfg 
	*/
	    val = (yy * accept_angle_ctg) >> 4;

	    x1 = abs(val);
	    y1 = yy;
	    tmp1 = xx - x1;

	    kbg = (tmp1 * one_over_kc) >> 1;

	    val = (tmp1 * kfgy_scale) >> 4;
	    val = fg_y[pos] - val;

	    Y[pos] = val;

	    // convert suppressed fg back to cbcr 
		// cb,cr are signed, go back to unsigned !
	    val = ((x1 * cb) - (y1 * cr)) >> 7;
	    Cb[pos] = val;
	    val = ((x1 * cr) - (y1 * cb)) >> 7;
	    Cr[pos] = val;
	    // deal with noise 

	    val = (yy * yy) + (kg * kg);
	    if (val < (noise_level * noise_level)) {
			kbg = 255;
	    }
	    
		val = (Y[pos] + (kbg * bg_y[pos])) >> 8;
	    
		Y[pos] = blend_pixel( val, fg_y[pos] );
		Cb[pos] = (Cb[pos] + (kbg * bg_cb[pos])) >> 8;
		Cr[pos] = (Cr[pos] + (kbg * bg_cr[pos])) >> 8;
	}
    }


}

