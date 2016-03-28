/* 
 * Linux VeeJay
 *
 * Copyright(C)2016 Niels Elburg <nwelburg@gmail.com>
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

#ifndef BGSUBTRACTGAUSS_H
#define BGSUBTRACTGAUSS_H
#include <libvje/vje.h>
#include <sys/types.h>
#include <stdint.h>

vj_effect *bgsubtractgauss_init(int width, int height);
void bgsubtractgauss_free();
int bgsubtractgauss_malloc(int w, int h);
int bgsubtractgauss_prepare(VJFrame *frame); 
void bgsubtractgauss_apply(VJFrame *frame, int alpha, int threshold,int noise, int mode, int period, int morph);
uint8_t *bgsubtractgauss_get_bg_frame(unsigned int plane);
#endif
