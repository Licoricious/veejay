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

#include "scratcher.h"
#include <config.h>
#include <stdlib.h>
#include "common.h"
#include "vj-common.h"

static uint8_t *frame[3];
static  int nframe = 0;
static int nreverse = 0;
static int cycle_check = 0;
static int plane = 0;
static unsigned int *sum;
extern void *(* veejay_memcpy)(void *to, const void *from, size_t len);

vj_effect *scratcher_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) malloc(sizeof(vj_effect));
    ve->num_params = 3;
    ve->defaults = (int *) malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) malloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 255;
    ve->limits[0][1] = 1;
    ve->limits[1][1] = (MAX_SCRATCH_FRAMES-1);
    ve->limits[0][2] = 0;
    ve->limits[1][2] = 1;
    ve->defaults[0] = 150;
    ve->defaults[1] = 8;
    ve->defaults[2] = 1;
    ve->description = "Overlay Scratcher";
    ve->sub_format = 0;
    ve->extra_frame = 1;
    ve->has_internal_data = 1;
    return ve;

}

void scratcher_free() {
   if(frame[0]) free(frame[0]);
   if(frame[1]) free(frame[1]);
   if(frame[2]) free(frame[2]);

}

int scratcher_malloc(int w, int h)
{
	/* need memory for bounce mode ... */
    frame[0] =
	(uint8_t *) vj_malloc(w * h * sizeof(uint8_t) * MAX_SCRATCH_FRAMES);
	if(!frame[0]) return 0;
    memset( frame[0], 16, w * h * MAX_SCRATCH_FRAMES );
    frame[1] =
	(uint8_t *) vj_malloc( ((w * h)/4) * sizeof(uint8_t) * MAX_SCRATCH_FRAMES);
	if(!frame[1]) return 0;
	memset( frame[1], 128, ((w * h) / 4 ) * MAX_SCRATCH_FRAMES);
    frame[2] =
	(uint8_t *) vj_malloc( ((w * h)/4) * sizeof(uint8_t) * MAX_SCRATCH_FRAMES);
	if(!frame[2]) return 0;
	memset( frame[2], 128, ((w * h)/4) * MAX_SCRATCH_FRAMES);
	return 1;
}


void store_frame(uint8_t * yuv1[3], int w, int h, int n, int no_reverse)
{
    int uv_len = (w * h) / 4;

    if (!nreverse) {
	//printf("copy from buffer at pos %d to display", (w*h*nframe));
	memcpy(frame[0] + (w * h * nframe), yuv1[0], (w * h));
	memcpy(frame[1] + (uv_len * nframe), yuv1[1], uv_len);
	memcpy(frame[2] + (uv_len * nframe), yuv1[2], uv_len);
    } else {
	//printf("copy frame to buffer at pos %d", (w*h*nframe)); 
	memcpy(yuv1[0], frame[0] + (w * h * nframe), (w * h));
	memcpy(yuv1[1], frame[1] + (uv_len * nframe), uv_len);
	memcpy(yuv1[2], frame[2] + (uv_len * nframe), uv_len);
    }

   if (nreverse)
	nframe--;
   else
	nframe++;



  if (nframe >= n) {
	if (no_reverse == 0) {
	    nreverse = 1;
	    nframe = n - 1;
	} else {
	    nframe = 0;
	}
    }

   if (nframe == 0)
	nreverse = 0;

 //  printf("nframe=%d, n=%d, nreverse=%d. no_reverse=%d\n", nframe,n,nreverse,no_reverse);
}


void scratcher_apply(uint8_t * yuv1[3],
		     int width, int height, int opacity, int n,
		     int no_reverse)
{

    unsigned int x,len = width * height;
    unsigned int op1 = (opacity > 255) ? 255 : opacity;
    unsigned int op0 = 255 - op1;
    int offset = len * nframe;
    int uv_len = len / 4;
    int uv_offset = uv_len * nframe;

    if (nframe== 0) {
	veejay_memcpy(frame[0] + (len * nframe), yuv1[0], len);
	veejay_memcpy(frame[1] + (uv_len * nframe), yuv1[1], uv_len);
	veejay_memcpy(frame[2] + (uv_len * nframe), yuv1[2], uv_len);
        return;
    }

    for (x = 0; x < len; x++) {
	yuv1[0][x] =
	    ((op0 * yuv1[0][x]) + (op1 * frame[0][offset + x])) >> 8;
	}

    for(x=0; x < uv_len; x++) {
	yuv1[2][x] =
	    ((op0 * yuv1[2][x]) + (op1 * frame[2][uv_offset + x])) >> 8;

	yuv1[1][x] =
    	     ((op0 * yuv1[1][x]) + (op1 * frame[1][uv_offset + x])) >> 8;
    }

    store_frame(yuv1, width, height, n, no_reverse);
  /*  if (nframe > 0 && no_reverse == 1) {
	memset(frame[0] + (len * (nframe - 1)), 16, len);
	memset(frame[1] + (uv_len * (nframe - 1)), 128, uv_len);
	memset(frame[2] + (uv_len * (nframe - 1)), 128, uv_len);
  }*/  

}
