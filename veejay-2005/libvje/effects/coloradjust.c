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

#include "coloradjust.h"
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include "common.h"
vj_effect *coloradjust_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_malloc(sizeof(vj_effect));
    ve->num_params = 2;
    ve->defaults = (int *) vj_malloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_malloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_malloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = -235;
    ve->limits[1][0] = 235;
    ve->limits[0][1] = 0;
    ve->limits[1][1] = 36000;
    ve->defaults[0] = 116;
    ve->defaults[1] = 4500;
    ve->description = "Saturation";
    ve->extra_frame = 0;
    ve->sub_format = 0;
	ve->has_user = 0;
    return ve;
}

/* these methods were derived from yuv-subtitler */
inline uint8_t ccolor_adjust_u(double dcolor, double dsaturation)
{
    return (sin(dcolor) * dsaturation) + 128;
}
inline uint8_t ccolor_adjust_v(double dcolor, double dsaturation)
{
    return (cos(dcolor) * dsaturation) + 128;
}


inline double ccolor_sqrt(double u, double v)
{
//    return sqrt((u * u) + (v * v));
     double r;
     fast_sqrt( r,(u*u)+(v*v));
     return r;
}
inline double ccolor_sine(int u, double dsaturation)
{
    return asin((u / dsaturation));
}


void coloradjust_apply(VJFrame *frame, int width, int height, int val,
		       int _degrees)
{
    unsigned int i;
	const int len = frame->uv_len;
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];

    int cb, cr;
    double dsaturation, dcolor;
    const double degrees = (_degrees / 100.0);
    double co, si;
    const double dsat = val / 100.0;

    for (i = 0; i < len; i++)
	{
		cb = Cb[i] - 128;
		cr = Cr[i] - 128;
		if (cb != 0 && cr != 0)
		{
		    dsaturation = ccolor_sqrt((double) cb, (double) cr);
		    dcolor = ccolor_sine(cb, dsaturation);
		    if (cr < 0)
				dcolor = M_PI - dcolor;

			dcolor += (degrees * M_PI) / 180.0;
			dsaturation *= dsat;
			sin_cos(co,si, dcolor );
			Cb[i] = si * dsaturation + 128;
			Cr[i] = co * dsaturation + 128;
		}
    }

}
void coloradjust_free(){}
