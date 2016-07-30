/*
 * Linux VeeJay
 *
 * Copyright(C)2002-2015 Niels Elburg <nelburg@gmail.com>
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
#include <libvje/internal.h>
#include <veejay/vj-task.h>
#include "magicoverlaysalpha.h"

vj_effect *overlaymagicalpha_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 2;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 0;
    ve->defaults[1] = 0;
    ve->description = "Alpha: Overlay Magic Matte";
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 32;
    ve->limits[0][1] = 0;
    ve->limits[1][1] = 1; // clear chroma or keep
    ve->extra_frame = 1;
    ve->sub_format = -1;
    ve->has_user = 0;
    ve->parallel = 1;
	ve->param_description = vje_build_param_list( ve->num_params, "Mode", "Keep or clear chroma" );
	ve->hints = vje_init_value_hint_list( ve->num_params );

	ve->alpha = FLAG_ALPHA_IN_BLEND | FLAG_ALPHA_SRC_A | FLAG_ALPHA_SRC_B;

	vje_build_value_hint_list( ve->hints, ve->limits[1][0], 0,
		"Additive", "Subtractive","Multiply","Divide","Lighten","Hardlight",
		"Difference","Difference Negate","Exclusive","Base","Freeze",
		"Unfreeze","Relative Add","Relative Subtract","Max select", "Min select",
		"Relative Luma Add", "Relative Luma Subtract", "Min Subselect", "Max Subselect",
		"Add Subselect", "Add Average", "Experimental 1","Experimental 2", "Experimental 3",
		"Multisub", "Softburn", "Inverse Burn", "Dodge", "Distorted Add", "Distorted Subtract", "Experimental 4", "Negation Divide");



	return ve;
}

static void overlaymagicalpha_adddistorted(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	
	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
		a = Y[i];
		b = Y2[i];
		c = a + b;
		Y[i] = CLAMP_Y(c);
    }

}

static void overlaymagicalpha_add_distorted(VJFrame *frame, VJFrame *frame2)
{

    unsigned int i;
    uint8_t y1, y2;
    const unsigned int len = frame->len;
    uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
		y1 = Y[i];
		y2 = Y2[i];
		Y[i] = CLAMP_Y(y1 + y2);
    }

}

static void overlaymagicalpha_subdistorted(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    uint8_t y1, y2;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
		y1 = Y[i];
		y2 = Y2[i];
		y1 -= y2;
		Y[i] = CLAMP_Y(y1);
    }
}

/*
 static void overlaymagicalpha_sub_distorted(VJFrame *frame, VJFrame *frame2)
{

    unsigned int i ;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    uint8_t y1, y2;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
		y1 = Y[i];
		y2 = Y2[i];
		Y[i] = y1 - y2;
    }
}
*/

static void overlaymagicalpha_multiply(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
    uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
		Y[i] = (Y[i] * Y2[i]) >> 8;
	}
}

static void overlaymagicalpha_simpledivide(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
    uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		if(Y2[i] > pixel_Y_lo_ )
			Y[i] = Y[i] / Y2[i];
    }
}

static void overlaymagicalpha_divide(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    int a, b, c;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		b = Y[i] * Y[i];
		c = 255 - Y2[i];
		if (c == 0)
		    c = 1;
		a = b / c;
		Y[i] = a;
    }
}

static void overlaymagicalpha_additive(VJFrame *frame, VJFrame *frame2)
{
    
    unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a;
	while(len--) {	
		if(aA[len] == 0 || aB[len] == 0)
			continue;

		a = Y[len] + (2 * Y2[len]) - 255;
		Y[len] = CLAMP_Y(a);
	}
}

static void overlaymagicalpha_substractive(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++)  {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		Y[i] = CLAMP_Y( Y[i] - Y2[i] );
	}
}

static void overlaymagicalpha_softburn(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b, c;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];

		if ( (a + b) <= pixel_Y_hi_) {
		    if (a == pixel_Y_hi_)
			c = a;
		    else
			c = (b >> 7) / (256 - a);
		} else {
		    if (b <= pixel_Y_lo_) {
			b = 255;
		   }
		   c = 255 - (((255 - a) >> 7) / b);
		}
		Y[i] = c;
    }
}

static void overlaymagicalpha_inverseburn(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if (a <= pixel_Y_lo_)
		    c = pixel_Y_lo_;
		else
		    c = 255 - (((255 - b) >> 8) / a);
		Y[i] = CLAMP_Y(c);
    }
}

static void overlaymagicalpha_colordodge(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b, c;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if (a >= pixel_Y_hi_)
		    c = pixel_Y_hi_;
		else
			c = (b >> 8) / (256 - a);

		if(c >= pixel_Y_hi_)
			c = pixel_Y_hi_;
		Y[i] = c;
    }
}

static void overlaymagicalpha_mulsub(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
		a = Y[i];
		b = 255 - Y2[i];
		if (b > pixel_Y_lo_)
		    Y[i] = a / b;
    }
}

static void overlaymagicalpha_lighten(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b, c;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if (a > b)
		    c = a;
		else
		    c = b;
		Y[i] = c;
    }
}

static void overlaymagicalpha_difference(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		Y[i] = abs(a - b);
    }
}

static void overlaymagicalpha_diffnegate(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
    int a, b;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = (255 - Y[i]);
		b = Y2[i];
		Y[i] = 255 - abs(a - b);
    }
}

static void overlaymagicalpha_exclusive(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int c;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		c = Y[i] + (2 * Y2[i]) - 255;
		Y[i] = CLAMP_Y(c - (( Y[i] * Y2[i] ) >> 8 ));	
    }
}

static void overlaymagicalpha_basecolor(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    int a, b, c, d;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
		a = Y[i];
		b = Y2[i];
		c = a * b >> 8;
		d = c + a * ((255 - (((255 - a) * (255 - b)) >> 8) - c) >> 8);	//8
		Y[i] = CLAMP_Y(d);
    }
}

static void overlaymagicalpha_freeze(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;
	
		a = Y[i];
		b = Y2[i];
		if ( b > pixel_Y_lo_ )
			Y[i] = CLAMP_Y(255 - ((( 255 - a) * ( 255 - a )) / b));
	}
}

static void overlaymagicalpha_unfreeze(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if( a > pixel_Y_lo_ )
			Y[i] = CLAMP_Y( 255 - ((( 255 - b ) * ( 255 - b )) / a));
	}
}

/*
static void overlaymagicalpha_hardlight(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];

		if (b < 128)
		    c = (a * b) >> 7;
		else
		    c = 255 - ((255 - b) * (255 - a) >> 7);
		Y[i] = c;
    }
}
*/

static void overlaymagicalpha_relativeaddlum(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    int a, b, c, d;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		c = a >> 1;
		b = Y2[i];
		d = b >> 1;
		Y[i] = CLAMP_Y(c + d);
    }
}

static void overlaymagicalpha_relativesublum(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		Y[i] = (a - b + 255) >> 1;
    }
}

static void overlaymagicalpha_relativeadd(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
    uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b, c, d;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		c = a >> 1;
		b = Y2[i];
		d = b >> 1;
		Y[i] = c + d;
    }
}

static void overlaymagicalpha_relativesub(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
    uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		Y[i] = (a - b + 255) >> 1;
    }

}

static void overlaymagicalpha_minsubselect(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if (b < a)
		    Y[i] = (b - a + 255) >> 1;
		else
		    Y[i] = (a - b + 255) >> 1;
    }
}

static void overlaymagicalpha_maxsubselect(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if (b > a)
		    Y[i] = (b - a + 255) >> 1;
		else
		    Y[i] = (a - b + 255) >> 1;
    }
}

static void overlaymagicalpha_addsubselect(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int c, a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];

		if (b < a) {
			c = (a + b) >> 1;
			Y[i] = c;
		}
    }
}

static void overlaymagicalpha_maxselect(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if (b > a)
		    Y[i] = b;
    }
}

static void overlaymagicalpha_minselect(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		if (b < a)
		    Y[i] = b;
	}
}

static void overlaymagicalpha_addtest(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int c, a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		c = (a + ((2 * b) - 255))>>1;
		Y[i] = CLAMP_Y(c);
    }
}

static void overlaymagicalpha_addtest2(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
    uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    int c, a, b;
    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		c = a + (2 * b) - 255;
		Y[i] = CLAMP_Y(c);
    }
}

static void overlaymagicalpha_addtest4(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
	uint8_t *Y = frame->data[0];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];
    int a, b;
    
	for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		a = Y[i];
		b = Y2[i];
		b = b - 255;
		if (b <= pixel_Y_lo_)
		    Y[i] = a;
		else
		    Y[i] = (a * a) / b;
    }

}

static void overlaymagicalpha_try(VJFrame *frame, VJFrame *frame2)
{
    unsigned int i;
    const unsigned int len = frame->len;
    int a, b, p, q;
  	uint8_t *Y = frame->data[0];
    uint8_t *Y2 = frame2->data[0];
	uint8_t *aA = frame->data[3];
	uint8_t *aB = frame2->data[3];

    for (i = 0; i < len; i++) {
		if(aA[i] == 0 || aB[i] == 0)
			continue;

		/* calc p */
		a = Y[i];
		b = Y[i];

		if (b <= pixel_Y_lo_)
		    p = pixel_Y_lo_;
		else
		    p = 255 - ((256 - a) * (256 - a)) / b;
		if (p <= pixel_Y_lo_)
		    p = pixel_Y_lo_;

		/* calc q */
		a = Y2[i];
		b = Y2[i];
		if (b <= pixel_Y_lo_)
		    q = pixel_Y_lo_;
		else
		    q = 255 - ((256 - a) * (256 - a)) / b;
		if (b <= pixel_Y_lo_)
		    q = pixel_Y_lo_;

		/* calc pixel */
		if (q <= pixel_Y_lo_)
		    q = pixel_Y_lo_;
		else
		    q = 255 - ((256 - p) * (256 - a)) / q;

		Y[i] = q;
    }
}

void overlaymagicalpha_apply(VJFrame *frame, VJFrame *frame2, int n, int clearchroma)
{
    switch (n) {
    case VJ_EFFECT_BLEND_ADDITIVE:
	overlaymagicalpha_additive(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_SUBSTRACTIVE:
	overlaymagicalpha_substractive(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_MULTIPLY:
	overlaymagicalpha_multiply(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_DIVIDE:
	overlaymagicalpha_simpledivide(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_LIGHTEN:
	overlaymagicalpha_lighten(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_DIFFERENCE:
	overlaymagicalpha_difference(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_DIFFNEGATE:
	overlaymagicalpha_diffnegate(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_EXCLUSIVE:
	overlaymagicalpha_exclusive(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_BASECOLOR:
	overlaymagicalpha_basecolor(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_FREEZE:
	overlaymagicalpha_freeze(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_UNFREEZE:
	overlaymagicalpha_unfreeze(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_RELADD:
	overlaymagicalpha_relativeadd(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_RELSUB:
	overlaymagicalpha_relativesub(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_RELADDLUM:
	overlaymagicalpha_relativeaddlum(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_RELSUBLUM:
	overlaymagicalpha_relativesublum(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_MAXSEL:
	overlaymagicalpha_maxselect(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_MINSEL:
	overlaymagicalpha_minselect(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_MINSUBSEL:
	overlaymagicalpha_minsubselect(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_MAXSUBSEL:
	overlaymagicalpha_maxsubselect(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_ADDSUBSEL:
	overlaymagicalpha_addsubselect(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_ADDAVG:
	overlaymagicalpha_add_distorted(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_ADDTEST2:
	overlaymagicalpha_addtest(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_ADDTEST3:
	overlaymagicalpha_addtest2(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_ADDTEST4:
	overlaymagicalpha_addtest4(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_MULSUB:
	overlaymagicalpha_mulsub(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_SOFTBURN:
	overlaymagicalpha_softburn(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_INVERSEBURN:
	overlaymagicalpha_inverseburn(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_COLORDODGE:
	overlaymagicalpha_colordodge(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_ADDDISTORT:
	overlaymagicalpha_adddistorted(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_SUBDISTORT:
	overlaymagicalpha_subdistorted(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_ADDTEST5:
	overlaymagicalpha_try(frame, frame2);
	break;
    case VJ_EFFECT_BLEND_NEGDIV:
 	overlaymagicalpha_divide(frame,frame2);
	break;

    }
    if(clearchroma) {
		veejay_memset( frame->data[1], 128, (frame->ssm ? frame->len : frame->uv_len) );
		veejay_memset( frame->data[2], 128, (frame->ssm ? frame->len : frame->uv_len) );
    }
}


