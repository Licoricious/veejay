/*
 * subsample.h:  Routines to do chroma subsampling.  ("Work In Progress")
 *
 *
 *  Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
 *                2004 Niels Elburg <nelburg@looze.net>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef SUBSAMPLE_H
#define SUBSAMPLE_H



typedef enum subsample_mode {
    SSM_UNKNOWN = 0,
    SSM_420_JPEG_TR = 1,
    SSM_420_JPEG_BOX = 2,
    SSM_420_MPEG2 = 3,
    SSM_422_444 = 4, 
    SSM_420_422 = 5, 
    SSM_COUNT = 6,
} subsample_mode_t;



extern const char *ssm_id[SSM_COUNT];
extern const char *ssm_description[SSM_COUNT];

void	subsample_init(void);

void chroma_subsample(subsample_mode_t mode, uint8_t * ycbcr[],
		      int width, int height);

void chroma_supersample(subsample_mode_t mode, uint8_t * ycbcr[],
			int width, int height);
#endif
