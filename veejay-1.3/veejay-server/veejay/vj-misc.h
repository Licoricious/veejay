/* veejay - Linux VeeJay
 * 	     (C) 2002-2004 Niels Elburg <nwelburg@gmail.com> 
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
#ifndef VJ_MISC_H
#define VJ_MISC_H
#include <config.h>
#include <stdarg.h>
#include "vj-lib.h"

#define VEEJAY_FILE_LIMIT (1048576 * 16000)
int   available_diskspace(void);

int vj_perform_screenshot2(veejay_t * info, uint8_t ** src);

#ifdef ARCH_X86
int	veejay_sprintf( char *s, size_t size, const char *format, ... );
#define	veejay_snprintf	veejay_sprintf
#else
#define	veejay_snprintf snprintf
#endif

unsigned int	vj_get_relative_time(void);

void	vj_stamp_clear();
unsigned int vj_stamp();

int	vj_perform_take_bg( veejay_t *info, uint8_t **src, int ssm);

int	veejay_create_temp_file(const char *prefix, char *dst);

void	vj_get_yuv_template(VJFrame *src, int w, int h, int fmt);

void	vj_get_rgb_template(VJFrame *src, int w, int h );

void	vj_get_yuvgrey_template(VJFrame *src, int w, int h);

void	vj_get_yuv444_template(VJFrame *src, int w, int h);

int	verify_working_dir();

#endif
