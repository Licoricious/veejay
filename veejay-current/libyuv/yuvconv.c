/* veejay - Linux VeeJay
 * 	     (C) 2002-2004 Niels Elburg <nelburg@looze.net> 
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <config.h>
#include <stdint.h>
#include <unistd.h>
#include <libyuv/yuvconv.h>
#include <libpostproc/swscale.h>
#include <libvjmsg/vj-common.h>
/* this routine is the same as frame_YUV422_to_YUV420P , unpack
 * libdv's 4:2:2-packed into 4:2:0 planar 
 * See http://mjpeg.sourceforge.net/ (MJPEG Tools) (lav-common.c)
 */



/* convert 4:2:0 to yuv 4:2:2 packed */
void yuv422p_to_yuv422(uint8_t * yuv420[3], uint8_t * dest, int width,
		       int height)
{
    unsigned int x, y;


    for (y = 0; y < height; ++y) {
	uint8_t *Y = yuv420[0] + y * width;
	uint8_t *Cb = yuv420[1] + (y / 2) * (width);
	uint8_t *Cr = yuv420[2] + (y / 2) * (width);
	for (x = 0; x < width; x +=2) {
	    *(dest + 0) = Y[0];
	    *(dest + 1) = Cb[0];
	    *(dest + 2) = Y[1];
	    *(dest + 3) = Cr[0];
	    dest += 4;
	    Y += 2;
	    ++Cb;
	    ++Cr;
	}
    }
}



/* convert 4:2:0 to yuv 4:2:2 */
void yuv420p_to_yuv422(uint8_t * yuv420[3], uint8_t * dest, int width,
		       int height)
{
    unsigned int x, y;


    for (y = 0; y < height; ++y) {
	uint8_t *Y = yuv420[0] + y * width;
	uint8_t *Cb = yuv420[1] + (y >> 1) * (width >> 1);
	uint8_t *Cr = yuv420[2] + (y >> 1) * (width >> 1);
	for (x = 0; x < width; x += 2) {
	    *(dest + 0) = Y[0];
	    *(dest + 1) = Cb[0];
	    *(dest + 2) = Y[1];
	    *(dest + 3) = Cr[0];
	    dest += 4;
	    Y += 2;
	    ++Cb;
	    ++Cr;
	}
    }
}

#ifdef HAVE_MMX 
#include "mmx_macros.h"
#include "mmx.h"

/*****************************************************************

  _yuv_yuv_mmx.c

  Copyright (c) 2001-2002 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de

  http://gmerlin.sourceforge.net

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.


	- took yuy2 -> planar 422 and 422 planar -> yuy2 mmx conversion
							     routines.
	  (Niels, 02/2005)

*****************************************************************/

static mmx_t mmx_00ffw =   { 0x00ff00ff00ff00ffLL };

#ifdef MMXEXT
#define MOVQ_R2M(reg,mem) movntq_r2m(reg, mem)
#else
#define MOVQ_R2M(reg,mem) movq_r2m(reg, mem)
#endif

#define PLANAR_TO_YUY2   movq_m2r(*src_y, mm0);/*   mm0: Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */\
                         movd_m2r(*src_u, mm1);/*   mm1: 00 00 00 00 U6 U4 U2 U0 */\
                         movd_m2r(*src_v, mm2);/*   mm2: 00 00 00 00 V6 V4 V2 V0 */\
                         pxor_r2r(mm3, mm3);/*      Zero mm3                     */\
                         movq_r2r(mm0, mm7);/*      mm7: Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */\
                         punpcklbw_r2r(mm3, mm0);/* mm0: 00 Y3 00 Y2 00 Y1 00 Y0 */\
                         punpckhbw_r2r(mm3, mm7);/* mm7: 00 Y7 00 Y6 00 Y5 00 Y4 */\
                         pxor_r2r(mm4, mm4);     /* Zero mm4                     */\
                         punpcklbw_r2r(mm1, mm4);/* mm4: U6 00 U4 00 U2 00 U0 00 */\
                         pxor_r2r(mm5, mm5);     /* Zero mm5                     */\
                         punpcklbw_r2r(mm2, mm5);/* mm5: V6 00 V4 00 V2 00 V0 00 */\
                         movq_r2r(mm4, mm6);/*      mm6: U6 00 U4 00 U2 00 U0 00 */\
                         punpcklwd_r2r(mm3, mm6);/* mm6: 00 00 U2 00 00 00 U0 00 */\
                         por_r2r(mm6, mm0);      /* mm0: 00 Y3 U2 Y2 00 Y1 U0 Y0 */\
                         punpcklwd_r2r(mm3, mm4);/* mm4: 00 00 U6 00 00 00 U4 00 */\
                         por_r2r(mm4, mm7);      /* mm7: 00 Y7 U6 Y6 00 Y5 U4 Y4 */\
                         pxor_r2r(mm6, mm6);     /* Zero mm6                     */\
                         punpcklwd_r2r(mm5, mm6);/* mm6: V2 00 00 00 V0 00 00 00 */\
                         por_r2r(mm6, mm0);      /* mm0: V2 Y3 U2 Y2 V0 Y1 U0 Y0 */\
                         punpckhwd_r2r(mm5, mm3);/* mm3: V6 00 00 00 V4 00 00 00 */\
                         por_r2r(mm3, mm7);      /* mm7: V6 Y7 U6 Y6 V4 Y5 U4 Y4 */\
                         MOVQ_R2M(mm0, *dst);\
                         MOVQ_R2M(mm7, *(dst+8));


#define YUY2_TO_YUV_PLANAR movq_m2r(*src,mm0);\
                           movq_m2r(*(src+8),mm1);\
                           movq_r2r(mm0,mm2);/*       mm2: V2 Y3 U2 Y2 V0 Y1 U0 Y0 */\
                           pand_m2r(mmx_00ffw,mm2);/* mm2: 00 Y3 00 Y2 00 Y1 00 Y0 */\
                           pxor_r2r(mm4, mm4);/*      Zero mm4 */\
                           packuswb_r2r(mm4,mm2);/*   mm2: 00 00 00 00 Y3 Y2 Y1 Y0 */\
                           movq_r2r(mm1,mm3);/*       mm3: V6 Y7 U6 Y6 V4 Y5 U4 Y4 */\
                           pand_m2r(mmx_00ffw,mm3);/* mm3: 00 Y7 00 Y6 00 Y5 00 Y4 */\
                           pxor_r2r(mm6, mm6);/*      Zero mm6 */\
                           packuswb_r2r(mm3,mm6);/*   mm6: Y7 Y6 Y5 Y4 00 00 00 00 */\
                           por_r2r(mm2,mm6);/*        mm6: Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */\
                           psrlw_i2r(8,mm0);/*        mm0: 00 V2 00 U2 00 V0 00 U0 */\
                           psrlw_i2r(8,mm1);/*        mm1: 00 V6 00 U6 00 V4 00 U4 */\
                           packuswb_r2r(mm1,mm0);/*   mm0: V6 U6 V4 U4 V2 U2 V0 U0 */\
                           movq_r2r(mm0,mm1);/*       mm1: V6 U6 V4 U4 V2 U2 V0 U0 */\
                           pand_m2r(mmx_00ffw,mm0);/* mm0: 00 U6 00 U4 00 U2 00 U0 */\
                           psrlw_i2r(8,mm1);/*        mm1: 00 V6 00 V4 00 V2 00 V0 */\
                           packuswb_r2r(mm4,mm0);/*   mm0: 00 00 00 00 U6 U4 U2 U0 */\
                           packuswb_r2r(mm4,mm1);/*   mm1: 00 00 00 00 V6 V4 V2 V0 */\
                           MOVQ_R2M(mm6, *dst_y);\
                           movd_r2m(mm0, *dst_u);\
                           movd_r2m(mm1, *dst_v);

void	yuv422_to_yuyv(uint8_t *src[3], uint8_t *dstI, int w, int h)
{
	int j,jmax,imax,i;
	uint8_t *dst = dstI;
	uint8_t *src_y = src[0];
	uint8_t *src_u = src[1];
	uint8_t *src_v = src[2];
	
	jmax = w / 8;
	imax = h;

	for( i = 0; i < imax ;i ++ )
	{
		for( j = 0; j < jmax ; j ++ )
		{
			PLANAR_TO_YUY2
			dst += 16;
			src_y += 8;
			src_u += 4;
			src_v += 4;
		}
	}

	emms();
}


void	yuy2toyv16(uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v, uint8_t *srcI, int w, int h )
{
	int j,jmax,imax,i;
	uint8_t *src = srcI;
	
	jmax = w / 8;
	imax = h;

	for( i = 0; i < imax ;i ++ )
	{
		for( j = 0; j < jmax ; j ++ )
		{
			YUY2_TO_YUV_PLANAR;
			src += 16;
			dst_y += 8;
			dst_u += 4;
			dst_v += 4;
		}
	}
	
}

void yuy2toyv12(uint8_t * _y, uint8_t * _u, uint8_t * _v, uint8_t * input,
		int width, int height)
{
	int j,jmax,imax,i;
	uint8_t *src = input;
	uint8_t *dst_y = _y;
	uint8_t *dst_u = _u; 
	uint8_t *dst_v = _v;
	jmax = width / 8;
	imax = height;

	for( i = 0; i < imax ;i ++ )
	{
		for( j = 0; j < jmax ; j ++ )
		{
			YUY2_TO_YUV_PLANAR;
			src += 16;
			dst_y += 8;
			dst_u += 4;
			dst_v += 4;
		}
		dst_u += width;
		dst_v += width;
	}
}
#else
// non mmx functions

void yuy2toyv12(uint8_t * _y, uint8_t * _u, uint8_t * _v, uint8_t * input,
		int width, int height)
{
  int i, j, w2;
    uint8_t *y, *u, *v;

    w2 = width / 2;

    //I420
    y = _y;
    v = _v;
    u = _u;

    for (i = 0; i < height; i += 4) {
	/* top field scanline */
	for (j = 0; j < w2; j++) {
	    /* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
	    *(y++) = *(input++);
	    *(u++) = *(input++);
	    *(y++) = *(input++);
	    *(v++) = *(input++);
	}
	for (j = 0; j < w2; j++)
	{
	    *(y++) = *(input++);
	    *(u++) = *(input++);
	    *(y++) = *(input++);
	    *(v++) = *(input++);
	
	}

	/* next two scanlines, one frome each field , interleaved */
	for (j = 0; j < w2; j++) {
	    /* skip every second line for U and V */
	    *(y++) = *(input++);
	    input++;
	    *(y++) = *(input++);
	    input++;
	}
	/* bottom field scanline*/
	for (j = 0; j < w2; j++) {
	    /* skip every second line for U and V */
	    *(y++) = *(input++);
	    input++;
	    *(y++) = *(input++);
	    input++;
	}

    }
}

void yuy2toyv16(uint8_t * _y, uint8_t * _u, uint8_t * _v, uint8_t * input,
		int width, int height)
{

    int i, j, w2;
    uint8_t *y, *u, *v;

    w2 = width / 2;

    //YV16
    y = _y;
    v = _v;
    u = _u;

    for (i = 0; i < height; i ++ )
    {
	for (j = 0; j < w2; j++) {
	    /* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
	    *(y++) = *(input++);
	    *(u++) = *(input++);
	    *(y++) = *(input++);
	    *(v++) = *(input++);
	}
    }
}

void yuv422_to_yuyv(uint8_t *yuv422[3], uint8_t *pixels, int w, int h)
{
    int x,y;
    uint8_t *Y = yuv422[0];
    uint8_t *U = yuv422[1];
    uint8_t *V = yuv422[2]; // U Y V Y
	for(y = 0; y < h; y ++ )
	{
		Y = yuv422[0] + y * w;
		U = yuv422[1] + (y>>1) * w;
		V = yuv422[2] + (y>>1) * w;
		for( x = 0 ; x < w ; x += 4 )
		{
			*(pixels + 0) = Y[0];
			*(pixels + 1) = U[0];
			*(pixels + 2) = Y[1];
			*(pixels + 3) = V[0];
			*(pixels + 4) = Y[2];
			*(pixels + 5) = U[1];
			*(pixels + 6) = Y[3];
			*(pixels + 7) = V[1];
			pixels += 8;
			Y+=4;
			U+=2;
			V+=2;
		}
    }
}
#endif


/* lav_common - some general utility functionality used by multiple
	lavtool utilities. */

/* Copyright (C) 2000, Rainer Johanni, Andrew Stevens */
/* - added scene change detection code 2001, pHilipp Zabel */
/* - broke some code out to lav_common.h and lav_common.c 
 *   July 2001, Shawn Sulma <lavtools@athos.cx>.  In doing this,
 *   I replaced the large number of globals with a handful of structs
 *   that are passed into the appropriate methods.  Check lav_common.h
 *   for the structs.  I'm sure some of what I've done is inefficient,
 *   subtly incorrect or just plain Wrong.  Feedback is welcome.
 */


int luminance_mean(uint8_t * frame[], int w, int h)
{
    uint8_t *p;
    uint8_t *lim;
    int sum = 0;
    int count = 0;
    p = frame[0];
    lim = frame[0] + w * (h - 1);
    while (p < lim) {
	sum += (p[0] + p[1]) + (p[w - 3] + p[w - 2]);
	p += 31;
	count += 4;
    }

    w = w / 2;
    h = h / 2;

    p = frame[1];
    lim = frame[1] + w * (h - 1);
    while (p < lim) {
	sum += (p[0] + p[1]) + (p[w - 3] + p[w - 2]);
	p += 31;
	count += 4;
    }
    p = frame[2];
    lim = frame[2] + w * (h - 1);
    while (p < lim) {
	sum += (p[0] + p[1]) + (p[w - 3] + p[w - 2]);
	p += 31;
	count += 4;
    }
    return sum / count;
}

typedef struct
{
	struct SwsContext *sws;
	SwsFilter	  *src_filter;
	SwsFilter	  *dst_filter;
} vj_sws;
#include <libpostproc/img_format.h>
void*	yuv_init_swscaler(VJFrame *src, VJFrame *dst, sws_template *tmpl, int cpu_flags)
{
	vj_sws *s = (vj_sws*) vj_malloc(sizeof(vj_sws));
	if(!s)
		return NULL;

	int	sws_type = 0;

	memset( s, 0, sizeof(vj_sws) );

	if( tmpl->use_filter )
	{
		s->src_filter = sws_getDefaultFilter(
			tmpl->lumaGBlur,
			tmpl->chromaGBlur,
			tmpl->lumaSarpen,
			tmpl->chromaSharpen,
			tmpl->chromaHShift,
			tmpl->chromaVShift,
			tmpl->verbose );
	}

	switch(tmpl->flags)
	{
		case 1:
			sws_type = SWS_FAST_BILINEAR;
			break;
		case 2:
			sws_type = SWS_BILINEAR;
			break;
		case 3:
			sws_type = SWS_BICUBIC;
			break;
		case 4:
			sws_type = SWS_POINT;
			break;
		case 5:
			sws_type = SWS_X;
			break;
		case 6:
			sws_type = SWS_AREA;
			break;
		case 7:
			sws_type = SWS_BICUBLIN;
			break;
		case 8: 
			sws_type = SWS_GAUSS;
			break;
		case 9:
			sws_type = SWS_SINC;
			break;
		case 10:
			sws_type = SWS_LANCZOS;
			break;
		case 11:
			sws_type = SWS_SPLINE;
			break;
	}	

	s->sws = sws_getContext(
			src->width,
			src->height,
			src->format,
			dst->width,
			dst->height,
			dst->format,
			sws_type | cpu_flags,
			s->src_filter,
			s->dst_filter,
			NULL
		);

	if(!s->sws)
	{
		if(s)free(s);
		return NULL;
	}	

	sws_rgb2rgb_init( cpu_flags );

	return ((void*)s);

}

void  yuv_crop(VJFrame *src, VJFrame *dst, VJRectangle *rect )
{
	int x;
	int y;
	uint8_t *sy = src->data[0];
	uint8_t *su = src->data[1];
	uint8_t *sv = src->data[2];

	uint8_t *dstY = dst->data[0];	
	uint8_t *dstU = dst->data[1];
	uint8_t *dstV = dst->data[2];
	int i = 0;

	for( i = 0 ; i < 3 ; i ++ )
	{
		int j = 0;
		uint8_t *srcPlane = src->data[i];
		uint8_t *dstPlane = dst->data[i];
		for( y = rect->top ; y < ( src->height - rect->bottom ); y ++ )
		{
			for ( x = rect->left ; x < ( src->width - rect->right ); x ++ )
			{
				dstPlane[j] = srcPlane[ y * src->width + x ];
				j++;
			}
		}
	}

}

VJFrame	*yuv_allocate_crop_image( VJFrame *src, VJRectangle *rect )
{
	int w = src->width - rect->left - rect->right;
	int h = src->height - rect->top - rect->bottom;

	if( w <= 0 )
		return NULL;
	if( h <= 0 )
		return NULL;

	VJFrame *new = (VJFrame*) vj_malloc(sizeof(VJFrame));
	if(!new)	
		return NULL;

	new->width = w;
	new->height = h;	
	new->uv_len = (w >> src->shift_h) * (h >> src->shift_v );
	new->len = w * h;
	new->uv_width  = (w >> src->shift_h );
	new->uv_height = (h >> src->shift_v );
	new->shift_v = src->shift_v;
	new->shift_h = src->shift_h;

	return new;
}


void	yuv_free_swscaler(void *sws)
{
	if(sws)
	{
		vj_sws *s = (vj_sws*) sws;
		if(s->src_filter)
			sws_freeFilter(s->src_filter);
		if(s->dst_filter)
			sws_freeFilter(s->dst_filter);
		if(s->sws)
			sws_freeContext( s->sws );
		if(s) free(s);
	}
}

void	yuv_convert_and_scale(void *sws , VJFrame *src, VJFrame *dst)
{
	vj_sws *s = (vj_sws*) sws;
	int src_stride[3] = { src->width,src->uv_width,src->uv_width };
	int dst_stride[3] = { dst->width,dst->uv_width,dst->uv_width };

	sws_scale( s->sws, src->data, src_stride, 0, src->height,
		dst->data, dst_stride );

#ifdef 	HAVE_ASM_MMX
	asm volatile ("emms\n\t");
#endif
	
}

int	yuv_sws_get_cpu_flags(void)
{
	int cpu_flags = 0;
#ifdef HAVE_ASM_MMX
	cpu_flags = cpu_flags | SWS_CPU_CAPS_MMX;
#endif
#ifdef HAVE_ASM_3DNOW
	cpu_flags = cpu_flags | SWS_CPU_CAPS_3DNOW;
#endif
#ifdef HAVE_ASM_MMX2
	cpu_flags = cpu_flags | SWS_CPU_CAPS_MMX2;
#endif
#ifdef HAVE_ALTIVEC
	cpu_flags = cpu_flags | SWS_CPU_CAPS_ALTIVEC;
#endif
	return cpu_flags;
}
