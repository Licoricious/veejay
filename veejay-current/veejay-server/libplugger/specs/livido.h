/*
  (C) Copyright 2005
      Gabriel "Salsaman" Finch,
      Niels Elburg,
      Dennis "Jaromil" Rojo,
      Daniel Fischer,
      Martin Bayer,
      Kentaro Fukuchi,
      Andraz Tori.
		

	  Revised by Niels, 2010 ( 101 )
                        2011 ( 102 )	  
 
   LiViDO is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   LiViDO is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


*/
#ifndef __LIVIDO_H__
#define __LIVIDO_H__

#ifdef __cplusplus
#define	LIVIDO_BEGIN_DECLS extern "C" {
#define LIVIDO_END_DECLS }
#else
#define LIVIDO_BEGIN_DECLS
#define LIVIDO_END_DECLS
#endif

#ifndef FALSE
#define FALSE   (0)
#endif

#ifndef TRUE
#define TRUE    (!FALSE)
#endif

LIVIDO_BEGIN_DECLS
#include <sys/types.h>


#ifdef IS_LIVIDO_PLUGIN
typedef void livido_port_t;
#endif
#define LIVIDO_API_VERSION 102
#define LIVIDO_PALETTE_RGB888 1
#define LIVIDO_PALETTE_RGB24 1
#define LIVIDO_PALETTE_BGR888 2
#define LIVIDO_PALETTE_BGR24 2
#define LIVIDO_PALETTE_RGBA8888 3
#define LIVIDO_PALETTE_RGBA32 3
#define LIVIDO_PALETTE_ARGB8888 4
#define LIVIDO_PALETTE_ARGB32 4

#define LIVIDO_PALETTE_RGBFLOAT 5
#define LIVIDO_PALETTE_ARGBFLOAT  6

#define LIVIDO_PALETTE_RGB48BE 7
#define LIVIDO_PALETTE_RGB48LE 8
#define LIVIDO_PALETTE_YUV444P16LE 9
#define LIVIDO_PALETTE_YUV444P16BE 10
#define LIVIDO_PALETTE_YUV422P16LE 11
#define LIVIDO_PALETTE_YUV422P16BE 12
#define LIVIDO_PALETTE_YUV422P 513
#define LIVIDO_PALETTE_YV16 513
#define LIVIDO_PALETTE_YUV420P 514
#define LIVIDO_PALETTE_YV12 514
#define LIVIDO_PALETTE_YVU420P 515
#define LIVIDO_PALETTE_I420 515

#define LIVIDO_PALETTE_YUV444P 516
#define LIVIDO_PALETTE_YUV4444P 517
#define LIVIDO_PALETTE_YUV444P16 523
#define LIVIDO_PALETTE_YUYV8888 518
#define LIVIDO_PALETTE_UYVY8888 519
#define LIVIDO_PALETTE_YUV411 520
#define	LIVIDO_PALETTE_YUV888 521
#define LIVIDO_PALETTE_YUVA8888 522
#define LIVIDO_PALETTE_A1 1025
#define LIVIDO_PALETTE_A8 1026
#define LIVIDO_PALETTE_A16 1028
#define LIVIDO_PALETTE_AFLOAT 1027

//@ do we want openmp?

#define LIVIDO_FILTER_NON_REALTIME    (1<<0)
#define LIVIDO_FILTER_CAN_DO_INPLACE  (1<<1)
#define LIVIDO_FILTER_NON_STATELESS   (1<<2)
#define LIVIDO_FILTER_IS_CONVERTOR    (1<<3)
#define LIVIDO_CHANNEL_CHANGE_UNADVISED (1<<0)
#define LIVIDO_CHANNEL_PALETTE_UNADVISED (1<<1)
#define LIVIDO_PARAMETER_CHANGE_UNADVISED (1<<0)
#define LIVIDO_PROPERTY_READONLY (1<<0)

#define LIVIDO_INTERLACE_NONE            0
#define LIVIDO_INTERLACE_TOPFIRST        1
#define LIVIDO_INTERLACE_BOTTOMFIRST     2
#define LIVIDO_INTERLACE_PROGRESSIVE     3
#define LIVIDO_NO_ERROR 0
#define LIVIDO_ERROR_MEMORY_ALLOCATION 1
#define LIVIDO_ERROR_PROPERTY_READONLY 2
#define LIVIDO_ERROR_NOSUCH_ELEMENT 3
#define LIVIDO_ERROR_NOSUCH_PROPERTY 4
#define LIVIDO_ERROR_WRONG_ATOM_TYPE 5
#define LIVIDO_ERROR_TOO_MANY_INSTANCES 6
#define LIVIDO_ERROR_HARDWARE 7
#define LIVIDO_ERROR_PORT 8 //@ error in port

#define LIVIDO_ATOM_TYPE_INT 1
#define LIVIDO_ATOM_TYPE_DOUBLE 2
#define LIVIDO_ATOM_TYPE_BOOLEAN 3
#define LIVIDO_ATOM_TYPE_STRING 4
#define LIVIDO_ATOM_TYPE_VOIDPTR 65
#define LIVIDO_ATOM_TYPE_PORTPTR 66

#define LIVIDO_PORT_TYPE_PLUGIN_INFO 1
#define LIVIDO_PORT_TYPE_FILTER_CLASS 2
#define LIVIDO_PORT_TYPE_FILTER_INSTANCE 3
#define LIVIDO_PORT_TYPE_CHANNEL_TEMPLATE 4
#define LIVIDO_PORT_TYPE_PARAMETER_TEMPLATE 5
#define LIVIDO_PORT_TYPE_CHANNEL 6
#define LIVIDO_PORT_TYPE_PARAMETER 7
#define LIVIDO_PORT_TYPE_GUI 8

//#define FUNCSTRUCT
/*
	Uncomment the #define above and recompile all
 */

typedef int (*livido_init_f) (livido_port_t * filter_instance);
typedef int (*livido_process_f) (livido_port_t * filter_instance,double timestamp);
typedef int (*livido_deinit_f) (livido_port_t * filter_instance);


typedef void *(*livido_malloc_f) (size_t size);
typedef void (*livido_free_f) (void *ptr);
typedef void *(*livido_memset_f) (void *s, int c, size_t n);
typedef void *(*livido_memcpy_f) (void *dest, const void *src, size_t n);
typedef livido_port_t *(*livido_port_new_f) (int);
typedef void (*livido_port_free_f) (livido_port_t * port);
typedef int (*livido_property_set_f) (livido_port_t *, const char *, int, int,  void *);
typedef int (*livido_property_get_f) (livido_port_t *, const char *, int,  void *);
typedef int (*livido_property_num_elements_f) (livido_port_t *, const char *);
typedef int (*livido_property_atom_type_f) (livido_port_t *, const char *);
typedef size_t(*livido_property_element_size_f) (livido_port_t *, const char *, const int);
typedef char **(*livido_list_properties_f) (livido_port_t *);
typedef int (*livido_keyframe_get_f)(livido_port_t *port, long pos, int dir );
typedef int (*livido_keyframe_put_f)(livido_port_t *port, long pos, int dir );

//@ what about openmp
//@ automatic top/bottom half processing, using 2 threads.
//@ maybe with preprocessor directives
//typedef int (*livido_set_max_threads)(livido_port_t *port, int max );

typedef struct
{
	void (*f)();
//	void *f;
} livido_setup_t;

typedef livido_port_t *(*livido_setup_f) (const livido_setup_t list[], int );

#define	LIVIDO_PLUGIN \
static livido_port_t *(*livido_port_new) (int) = 0;\
static void (*livido_port_free) (livido_port_t * port) = 0;\
static int (*livido_property_set) (livido_port_t * port,const char *key, int atom_type, int num_elems, void *value) = 0;\
static int (*livido_property_get) (livido_port_t * port,const char *key, int idx, void *value) = 0;\
static int (*livido_property_num_elements) (livido_port_t * port,const char *key) = 0;\
static int (*livido_property_atom_type) (livido_port_t * port,const char *key) = 0;\
static size_t(*livido_property_element_size) (livido_port_t * port,const char *key, const int idx) = 0;\
static char **(*livido_list_properties) (livido_port_t * port) = 0;\
static void *(*livido_malloc) (size_t size) = 0;\
static void (*livido_free) (void *ptr) = 0;\
static void *(*livido_memset) (void *s, int c, size_t n) = 0;\
static void *(*livido_memcpy) (void *dest, const void *src, size_t n) = 0;\
static	int (*livido_keyframe_get)(livido_port_t *port, long pos, int dir) = 0;\
static  int (*livido_keyframe_put)(livido_port_t *port, long pos, int dir) = 0; \

/* Using void* to pass base address of function, needs explicit typecast and host
   must match ordering */
#define	LIVIDO_IMPORT(list) \
{\
	livido_malloc		= (livido_malloc_f) list[0].f;\
	livido_free			= (livido_free_f) list[1].f;\
	livido_memset		= (livido_memset_f) list[2].f;\
	livido_memcpy		= (livido_memcpy_f) list[3].f;\
	livido_port_new		= (livido_port_new_f) list[4].f;\
	livido_port_free	= (livido_port_free_f) list[5].f;\
	livido_property_set = (livido_property_set_f) list[6].f;\
	livido_property_get	= (livido_property_get_f) list[7].f;\
	livido_property_num_elements = (livido_property_num_elements_f) list[8].f;\
	livido_property_atom_type = (livido_property_atom_type_f) list[9].f;\
	livido_property_element_size = (livido_property_element_size_f) list[10].f;\
	livido_list_properties = (livido_list_properties_f) list[11].f;\
	livido_keyframe_get    = (livido_keyframe_get_f) list[12].f;\
	livido_keyframe_put    = (livido_keyframe_put_f) list[13].f;\
}

LIVIDO_END_DECLS
#endif				// #ifndef __LIVIDO_H_
