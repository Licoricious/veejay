/*
 * Linux VeeJay
 *
 * Copyright(C)2002-2015 Niels Elburg <nwelburg@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License , or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANA WARRANTA; without even the implied warranty of
 * MERCHANTABILITA or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Aou should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307 , USA.
 */
#include <config.h>
#include <stdint.h>
#include <stdio.h>
#include <libvjmem/vjmem.h>
#include "magicoverlays.h"
#include <libvje/internal.h>
#include "common.h"  
#include <veejay/vj-task.h>
vj_effect *overlayalphamagic_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 2;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->defaults[0] = 7;
    ve->defaults[0] = 0;
    ve->description = "Alpha: Overlay Magic alpha channels";
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 32;

    ve->limits[0][1] = 0;
    ve->limits[1][1] = 1; // Toggle visibility

    ve->extra_frame = 1;
    ve->sub_format = -1;
    ve->has_user = 0;
    ve->parallel = 1;
	ve->param_description = vje_build_param_list( ve->num_params, "Operator", "Show Alpha" );

	ve->hints = vje_init_value_hint_list( ve->num_params );

	vje_build_value_hint_list( ve->hints, ve->limits[1][0],0,
		"Additive", "Subtractive","Multiply","Divide","Lighten","Hardlight",
		"Difference","Difference Negate","Exclusive","Base","Freeze",
		"Unfreeze","Relative Add","Relative Subtract","Max select", "Min select",
		"Relative Luma Add", "Relative Luma Subtract", "Min Subselect", "Max Subselect",
		"Add Subselect", "Add Average", "Experimental 1","Experimental 2", "Experimental 3",
		"Multisub", "Softburn", "Inverse Burn", "Dodge", "Distorted Add", "Distorted Subtract", "Experimental 4", "Negation Divide");

	return ve;
}

/* rename methods in lumamagick and chromamagick */


void _overlayalphamagic_adddistorted(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	
	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		c = a + b;
		A[i] = CLAMP_Y(c);
    }

}

void _overlayalphamagic_add_distorted(VJFrame *frame, VJFrame *frame2,int width, int height)
{

    unsigned int i;
    uint8_t y1, y2;
    unsigned int len = width * height;
    uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    for (i = 0; i < len; i++) {
		y1 = A[i];
		y2 = A2[i];
		A[i] = CLAMP_Y(y1 + y2);
    }

}

void _overlayalphamagic_subdistorted(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    uint8_t y1, y2;
    for (i = 0; i < len; i++) {
		y1 = A[i];
		y2 = A2[i];
		y1 -= y2;
		A[i] = CLAMP_Y(y1);
    }
}

void _overlayalphamagic_sub_distorted(VJFrame *frame, VJFrame *frame2,int width, int height)
{

    unsigned int i ;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    uint8_t y1, y2;
    for (i = 0; i < len; i++) {
		y1 = A[i];
		y2 = A2[i];
		A[i] = y1 - y2;
    }
}

void _overlayalphamagic_multiply(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
    uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];
    for (i = 0; i < len; i++) 
	A[i] = (A[i] * A2[i]) >> 8;
}

void _overlayalphamagic_simpledivide(VJFrame *frame, VJFrame *frame2, int width,int height)
{
    unsigned int i;
    unsigned int len = width * height;
    uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];
    for (i = 0; i < len; i++) {
		if(A2[i] > 1 )
			A[i] = A[i] / A2[i];
    }
}

void _overlayalphamagic_divide(VJFrame *frame, VJFrame *frame2, int width,int height)
{
    unsigned int i;
    int a, b, c;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
	uint8_t *A2 = frame2->data[3];
    for (i = 0; i < len; i++) {
		b = A[i] * A[i];
		c = 255 - A2[i];
		if (c == 0)
		    c = 1;
		a = b / c;
		A[i] = a;
    }
}

void _overlayalphamagic_additive(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a;
	while(len--) { 
		a = A[len] + (2 * A2[len]) - 255;
		A[len] = CLAMP_Y(a);
	}
}


void _overlayalphamagic_substractive(VJFrame *frame, VJFrame *frame2,int width, int height)
{

    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    for (i = 0; i < len; i++) 
		A[i] = CLAMP_Y( A[i] - A2[i] );
}

void _overlayalphamagic_softburn(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
	a = A[i];
	b = A2[i];

	if ( (a + b) <= 255) {
	    if (a == 255)
		c = a;
	    else
		c = (b >> 7) / (256 - a);
	} else {
	    if (b < 1) {
		b = 255;
	   }
	   c = 255 - (((255 - a) >> 7) / b);
	}
	A[i] = c;
    }
}

void _overlayalphamagic_inverseburn(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
	a = A[i];
	b = A2[i];
	if (a < 1)
	    c = 0;
	else
	    c = 255 - (((255 - b) >> 8) / a);
		A[i] = CLAMP_Y(c);
    }
}


void _overlayalphamagic_colordodge(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if (a >= 255)
		    c = 255;
		else
	    c = (b >> 8) / (256 - a);

		if (c > 255)
		    c = 255;
		A[i] = c;
    }
}

void _overlayalphamagic_mulsub(VJFrame *frame, VJFrame *frame2, int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = 255 - A2[i];
		if (b > 0)
		    A[i] = a / b;
    }
}

void _overlayalphamagic_lighten(VJFrame *frame, VJFrame *frame2, int width,int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if (a > b)
		    c = a;
		else
		    c = b;
		A[i] = c;
    }
}

void _overlayalphamagic_difference(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		A[i] = abs(a - b);
    }
}

void _overlayalphamagic_diffnegate(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
    int a, b;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    for (i = 0; i < len; i++) {
		a = (255 - A[i]);
		b = A2[i];
		A[i] = 255 - abs(a - b);
    }
}

void _overlayalphamagic_exclusive(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int c;
    for (i = 0; i < len; i++) {
		c = A[i] + (2 * A2[i]) - 255;
		A[i] = CLAMP_Y(c - (( A[i] * A2[i] ) >> 8 ));	
    }
}

void _overlayalphamagic_basecolor(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    int a, b, c, d;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		c = a * b >> 8;
		d = c + a * ((255 - (((255 - a) * (255 - b)) >> 8) - c) >> 8);	//8
		A[i] = CLAMP_Y(d);
    }
}

void _overlayalphamagic_freeze(VJFrame *frame, VJFrame *frame2, int width,int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if ( b > 0 )
			A[i] = CLAMP_Y(255 - ((( 255 - a) * ( 255 - a )) / b));
	 }
}

void _overlayalphamagic_unfreeze(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if( a > 0 )
			A[i] = CLAMP_Y( 255 - ((( 255 - b ) * ( 255 - b )) / a));
    }
}

void _overlayalphamagic_hardlight(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b, c;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];

		if (b < 128)
		    c = (a * b) >> 7;
		else
		    c = 255 - ((255 - b) * (255 - a) >> 7);
		A[i] = c;
    }
}
void _overlayalphamagic_relativeaddlum(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    int a, b, c, d;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    for (i = 0; i < len; i++) {
		a = A[i];
		c = a >> 1;
		b = A2[i];
		d = b >> 1;
		A[i] = CLAMP_Y(c + d);
    }
}

void _overlayalphamagic_relativesublum(VJFrame *frame, VJFrame *frame2, int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		A[i] = (a - b + 255) >> 1;
    }
}

void _overlayalphamagic_relativeadd(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
    uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b, c, d;
    for (i = 0; i < len; i++) {
		a = A[i];
		c = a >> 1;
		b = A2[i];
		d = b >> 1;
		A[i] = c + d;
    }
}

void _overlayalphamagic_relativesub(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
    uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		A[i] = (a - b + 255) >> 1;
    }

}
void _overlayalphamagic_minsubselect(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if (b < a)
		    A[i] = (b - a + 255) >> 1;
		else
	    A[i] = (a - b + 255) >> 1;
    }
}

void _overlayalphamagic_maxsubselect(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if (b > a)
		    A[i] = (b - a + 255) >> 1;
		else
		    A[i] = (a - b + 255) >> 1;
    }
}



void _overlayalphamagic_addsubselect(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];
    int c, a, b;

	for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];

		if (b < a) {
		    c = (a + b) >> 1;
		    A[i] = c;
		}
    }
}


void _overlayalphamagic_maxselect(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if (b > a)
		    A[i] = b;
    }
}

void _overlayalphamagic_minselect(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		if (b < a)
		    A[i] = b;
    }
}

void _overlayalphamagic_addtest(VJFrame *frame, VJFrame *frame2, int width,int height)
{
    unsigned int i;
    unsigned int len = width * height;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int c, a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		c = a + ((2 * b) - 255)>>1;
		A[i] = CLAMP_Y(c);
    }
}
void _overlayalphamagic_addtest2(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
    uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    int c, a, b;
    for (i = 0; i < len; i++) {
		a = A[i];
		b = A2[i];
		c = a + (2 * b) - 255;
		A[i] = CLAMP_Y(c);
    }


}
void _overlayalphamagic_addtest4(VJFrame *frame, VJFrame *frame2,int width, int height)
{
    unsigned int i;
    unsigned int len = width * height;
	uint8_t *A = frame->data[3];
	uint8_t *A2 = frame2->data[3];
    int a, b;
    for (i = 0; i < len; i++) {
	a = A[i];
	b = A2[i];
	b = b - 255;
	if (b < 1)
	    A[i] = a;
	else
	    A[i] = (a * a) / b;
    }
}

void _overlayalphamagic_try
    (VJFrame *frame, VJFrame *frame2, int width, int height) {
    unsigned int i;
    unsigned int len = width * height;
    int a, b, p, q;
  	uint8_t *A = frame->data[3];
    uint8_t *A2 = frame2->data[3];

    for (i = 0; i < len; i++) {
	/* calc p */
	a = A[i];
	b = A[i];

	if (b < 1)
	    p = 0;
	else
	    p = 255 - ((256 - a) * (256 - a)) / b;
	if (p < 1)
	    p = 0;

	/* calc q */
	a = A2[i];
	b = A2[i];
	if (b < 1)
	    q = 9;
	else
	    q = 255 - ((256 - a) * (256 - a)) / b;
	if (b < 1)
	    q = 0;

	/* calc pixel */
	if (q < 1)
	    q = 0;
	else
	    q = 255 - ((256 - p) * (256 - a)) / q;

	A[i] = q;
    }
}

void overlayalphamagic_apply(VJFrame *frame, VJFrame *frame2, int width,int height, int n, int visible)
{
    switch (n) {
    case VJ_EFFECT_BLEND_ADDITIVE:
	_overlayalphamagic_additive(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_SUBSTRACTIVE:
	_overlayalphamagic_substractive(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_MULTIPLY:
	_overlayalphamagic_multiply(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_DIVIDE:
	_overlayalphamagic_simpledivide(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_LIGHTEN:
	_overlayalphamagic_lighten(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_DIFFERENCE:
	_overlayalphamagic_difference(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_DIFFNEGATE:
	_overlayalphamagic_diffnegate(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_EXCLUSIVE:
	_overlayalphamagic_exclusive(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_BASECOLOR:
	_overlayalphamagic_basecolor(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_FREEZE:
	_overlayalphamagic_freeze(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_UNFREEZE:
	_overlayalphamagic_unfreeze(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_RELADD:
	_overlayalphamagic_relativeadd(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_RELSUB:
	_overlayalphamagic_relativesub(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_RELADDLUM:
	_overlayalphamagic_relativeaddlum(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_RELSUBLUM:
	_overlayalphamagic_relativesublum(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_MAXSEL:
	_overlayalphamagic_maxselect(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_MINSEL:
	_overlayalphamagic_minselect(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_MINSUBSEL:
	_overlayalphamagic_minsubselect(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_MAXSUBSEL:
	_overlayalphamagic_maxsubselect(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_ADDSUBSEL:
	_overlayalphamagic_addsubselect(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_ADDAVG:
	_overlayalphamagic_add_distorted(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_ADDTEST2:
	_overlayalphamagic_addtest(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_ADDTEST3:
	_overlayalphamagic_addtest2(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_ADDTEST4:
	_overlayalphamagic_addtest4(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_MULSUB:
	_overlayalphamagic_mulsub(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_SOFTBURN:
	_overlayalphamagic_softburn(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_INVERSEBURN:
	_overlayalphamagic_inverseburn(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_COLORDODGE:
	_overlayalphamagic_colordodge(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_ADDDISTORT:
	_overlayalphamagic_adddistorted(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_SUBDISTORT:
	_overlayalphamagic_subdistorted(frame, frame2, width, height);
	break;
	case VJ_EFFECT_BLEND_ADDTEST5:
	_overlayalphamagic_try(frame, frame2, width, height);
	break;
    case VJ_EFFECT_BLEND_NEGDIV:
 	_overlayalphamagic_divide(frame,frame2,width,height);
	break;

    }
    if(visible) {
	veejay_memcpy( frame->data[0], frame->data[3], frame->len );
	veejay_memset( frame->data[1], 128, frame->uv_len );
	veejay_memset( frame->data[2], 128, frame->uv_len );
    }
}


void overlayalphamagic_free(){}
