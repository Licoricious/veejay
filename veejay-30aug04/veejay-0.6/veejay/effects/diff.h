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

#ifndef DIFFEFFECT_H
#define DIFFEFFECT_H
#include "../vj-effect.h"
#include <sys/types.h>
#include <stdint.h>

vj_effect *diff_init(int width, int height);
void diff_free(vj_effect_data *d);
int diff_malloc(vj_effect_data* d,int w, int h);
void diff_set_background(uint8_t *map[3], int w, int h); 
void diff_apply(vj_effect_data *d , uint8_t * yuv1[3],
		uint8_t * yuv2[3], int width, int height, 
		int mode, int th, int noise, int n2);

#endif
