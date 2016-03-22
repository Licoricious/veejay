/* 
 * Linux VeeJay
 *
 * Copyright(C)2007 Niels Elburg <nwelburg@gmail.com>
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

#ifndef MOTIONMAP_H
#define MOTIONMAP_H
#include <libvje/vje.h>
#include <sys/types.h>
#include <stdint.h>

vj_effect *motionmap_init(int w, int h);
void motionmap_apply( VJFrame *frame, int width, int height, int t, int n, int draw, int histo, int op, int ip, int la);
int	motionmap_malloc(int w,int h);
void	motionmap_free(void);
int	motionmap_prepare( uint8_t *map[4], int w, int h );
int	motionmap_active();
int	motionmap_is_locked();
uint8_t	*motionmap_interpolate_buffer();
uint8_t *motionmap_bgmap();
void	motionmap_store_frame( VJFrame *fx );
void	motionmap_interpolate_frame( VJFrame *fx, int N, int n );
#endif
