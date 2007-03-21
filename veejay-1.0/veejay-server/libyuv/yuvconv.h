#ifndef YUVCONF_H
#define YUVCONF_H
/*  Veejay -  A visual instrument and realtime video sampler
 *            Copyright (C)    2004 Niels Elburg <nelburg@looze.net>
 *
 *  YUV library for veejay.
 *
 *  Mjpegtools, (C) The Mjpegtools Development Team (http://mjpeg.sourceforge.net)
 *  	      Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
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

void *subsample_init(int buf_len);
void subsample_free(void *sampler);

void chroma_subsample(subsample_mode_t mode, void *sampler, uint8_t * ycbcr[],
		      int width, int height);
void chroma_subsample_cp(subsample_mode_t mode, void *data, uint8_t *ycbcr[], uint8_t *dcbcr[],
		      int width, int height);

void chroma_supersample(subsample_mode_t mode, void *sampler, uint8_t * ycbcr[],
			int width, int height);

// yuv 4:2:2 packed to yuv 4:2:0 planar 
void vj_yuy2toyv12( uint8_t *y, uint8_t *u, uint8_t *v,  uint8_t *in, int w, int h);
// yuv 4:2:2 packet to yuv 4:2:2 planar
void yuy2toyv16( uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *in, int w, int h);
// yuv 4:2:2 planar to yuv 4:2:2 packed
void yuv422p_to_yuv422( uint8_t *yuv422[3], uint8_t *dst, int w, int h );

// yuv 4:2:2 planar to yuv 4:2:0 planar
void yuv420p_to_yuv422( uint8_t *yuv420[3], uint8_t *dst, int w, int h );

// yuv 4:2:2 planar to YUYV
void yuv422_to_yuyv( uint8_t *yuv422[3], uint8_t *dst, int w, int h );

// scene detection
int luminance_mean(uint8_t * frame[], int w, int h);

/* software scaler from ffmpeg project: */

typedef struct
{
	float lumaGBlur;
	float chromaGBlur;
	float lumaSarpen;
	float chromaSharpen;
	float chromaHShift;
	float chromaVShift;
	int	verbose;
	int	flags;
	int	use_filter;
} sws_template;

void	yuv_init_lib();

void*	yuv_init_swscaler(VJFrame *src, VJFrame *dst, sws_template *templ, int cpu_flags);

void	yuv_convert_and_scale( void *sws, VJFrame *src, VJFrame *dst );

void	yuv_convert_and_scale_rgb( void *sws, VJFrame *src, VJFrame *dst );

void	yuv_convert_and_scale_gray_rgb(void *sws,VJFrame *src, VJFrame *dst);

int	yuv_sws_get_cpu_flags(void);

void	yuv_free_swscaler(void *sws);

void  	yuv_crop(VJFrame *src, VJFrame *dst, VJRectangle *rect );

VJFrame	*yuv_allocate_crop_image( VJFrame *src, VJRectangle *rect );

void	yuv_deinterlace(
		uint8_t *data[3],
		const int width,
		const int height,
		int out_pix_fmt,
		int shift,
		uint8_t *Y,uint8_t *U, uint8_t *V );


void	yuv_init_lib();

void	yuv_free_lib();

void	yuv_convert_ac( VJFrame *src, VJFrame *dst, int a, int b );

void	yuv_convert_any( VJFrame *src, VJFrame *dst, int a, int b );

void	yuv_convert_any3( VJFrame *src,int strides[], VJFrame *dst, int a, int b );


VJFrame *yuv_rgb_template( uint8_t *rgb_buffer, int w, int h, int fmt );

VJFrame *yuv_yuv_template( uint8_t *Y, uint8_t *U, uint8_t *V, int w, int h, int fmt );

char	*yuv_get_scaler_name(int id);

void	yuv_convert_any_ac( VJFrame *src, VJFrame *dst, int src_fmt, int dst_fmt );

void    *yuv_fx_context_create( VJFrame *src, VJFrame *dst, int src_fmt, int dst_fmt );

void    yuv_fx_context_process( void *ctx, VJFrame *src, VJFrame *dst );

void    yuv_fx_context_destroy( void *ctx );


#endif
