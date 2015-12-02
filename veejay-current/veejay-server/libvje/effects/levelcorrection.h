/* 
 * Linux VeeJay
 *
 * Copyright(C)2015 Niels Elburg <nweburg@gmail.com>
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

#ifndef ALEVELCORRECTION_H
#define ALEVELCORRECTION_H
#include <sys/types.h>
#include <stdint.h>
#include <libvje/vje.h>
void levelcorrection_apply(VJFrame *frame, int min, int max, int bmin, int bmax);
vj_effect *levelcorrection_init(int w,int h);
#endif
