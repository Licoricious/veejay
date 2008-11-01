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
#ifndef VJ_SDL_H
#define VJ_SDL_H

#include <config.h>
#ifdef HAVE_SDL
#include <SDL/SDL.h>
#include <stdint.h>


typedef struct vj_sdl_t {
    SDL_Surface *screen;
    SDL_Overlay *yuv_overlay;
    SDL_Rect rectangle;
    SDL_Event event;
    uint32_t flags[2];
    int show_cursor;
    int mouse_motion;
    int use_keyboard;
    int pix_format;
    int width;
    int height;
    int sw_scale_width;
    int sw_scale_height;
    int frame_size;
    char last_error[255];
    int  custom_geo[2];
    int	fs;
    void *sampler;
} vj_sdl;

vj_sdl *vj_sdl_allocate(int width, int height, int pixel_format);
void vj_sdl_set_geometry(vj_sdl *sdl, int w, int h);
void vj_sdl_show(vj_sdl *vjsdl);
int vj_sdl_init(vj_sdl * vjsdl, int scaled_width, int scaled_height,const char *caption, int show, int fs);
int vj_sdl_lock(vj_sdl * vjsdl);
int vj_sdl_unlock(vj_sdl * vjsdl);
int vj_sdl_update_yuv_overlay(vj_sdl * vjsdl, void *frame);
int vj_sdl_direct_yuv_overlay(vj_sdl * vjsdl, uint8_t * buffer, int buflen,
			      int dataformat);
void vj_sdl_free(vj_sdl * vjsdl);
void vj_sdl_quit();

int	vj_sdl_set_fullscreen( vj_sdl *sdl, int status );
int	vj_sdl_resize_window ( vj_sdl *sdl, int w, int h, int x , int y );
#endif
#endif
