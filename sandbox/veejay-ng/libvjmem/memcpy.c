/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   Fast memcpy code was taken from xine (see below).

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * Copyright (C) 2001 the xine project
 *
 * This file is part of xine, a unix video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * These are the MMX/MMX2/SSE optimized versions of memcpy
 *
 * This code was adapted from Linux Kernel sources by Nick Kurshev to
 * the mplayer program. (http://mplayer.sourceforge.net)
 *
 * Miguel Freitas split the #ifdefs into several specialized functions that
 * are benchmarked at runtime by xine. Some original comments from Nick
 * have been preserved documenting some MMX/SSE oddities.
 * Also added kernel memcpy function that seems faster than glibc one.
 *
 */

/* Original comments from mplayer (file: aclib.c) This part of code
   was taken by me from Linux-2.4.3 and slightly modified for MMX, MMX2,
   SSE instruction set. I have done it since linux uses page aligned
   blocks but mplayer uses weakly ordered data and original sources can
   not speedup them. Only using PREFETCHNTA and MOVNTQ together have
   effect!

   From IA-32 Intel Architecture Software Developer's Manual Volume 1,

  Order Number 245470:
  "10.4.6. Cacheability Control, Prefetch, and Memory Ordering Instructions"

  Data referenced by a program can be temporal (data will be used
  again) or non-temporal (data will be referenced once and not reused
  in the immediate future). To make efficient use of the processor's
  caches, it is generally desirable to cache temporal data and not
  cache non-temporal data. Overloading the processor's caches with
  non-temporal data is sometimes referred to as "polluting the
  caches".  The non-temporal data is written to memory with
  Write-Combining semantics.

  The PREFETCHh instructions permits a program to load data into the
  processor at a suggested cache level, so that it is closer to the
  processors load and store unit when it is needed. If the data is
  already present in a level of the cache hierarchy that is closer to
  the processor, the PREFETCHh instruction will not result in any data
  movement.  But we should you PREFETCHNTA: Non-temporal data fetch
  data into location close to the processor, minimizing cache
  pollution.

  The MOVNTQ (store quadword using non-temporal hint) instruction
  stores packed integer data from an MMX register to memory, using a
  non-temporal hint.  The MOVNTPS (store packed single-precision
  floating-point values using non-temporal hint) instruction stores
  packed floating-point data from an XMM register to memory, using a
  non-temporal hint.

  The SFENCE (Store Fence) instruction controls write ordering by
  creating a fence for memory store operations. This instruction
  guarantees that the results of every store instruction that precedes
  the store fence in program order is globally visible before any
  store instruction that follows the fence. The SFENCE instruction
  provides an efficient way of ensuring ordering between procedures
  that produce weakly-ordered data and procedures that consume that
  data.

  If you have questions please contact with me: Nick Kurshev:
  nickols_k@mail.ru.
*/

/*  mmx v.1 Note: Since we added alignment of destinition it speedups
    of memory copying on PentMMX, Celeron-1 and P2 upto 12% versus
    standard (non MMX-optimized) version.
    Note: on K6-2+ it speedups memory copying upto 25% and
          on K7 and P3 about 500% (5 times).
*/

/* Additional notes on gcc assembly and processors: [MF]
   prefetch is specific for AMD processors, the intel ones should be
   prefetch0, prefetch1, prefetch2 which are not recognized by my gcc.
   prefetchnta is supported both on athlon and pentium 3.

   therefore i will take off prefetchnta instructions from the mmx1
   version to avoid problems on pentium mmx and k6-2.

   quote of the day:
    "Using prefetches efficiently is more of an art than a science"
*/
#include <config.h>
#include <libvjmem/vjmem.h>
#include <libvjmsg/vj-common.h>
#include <sys/time.h>
#include <time.h>
#include <libyuv/mmx.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUFSIZE 1024

#if defined ( HAVE_ASM_MMX2 ) || defined ( HAVE_ASM_3DNOW ) || defined( HAVE_ASM_MMX )
#undef HAVE_MMX1
#if defined ( HAVE_ASM_MMX  ) && !defined(HAVE_ASM_MMX2) && !defined( HAVE_ASM_3DNOW ) && !defined( HAVE_ASM_SSE)
/* means:  mmx v.1. Note: Since we added alignment of destinition it speedups
    of memory copying on PentMMX, Celeron-1 and P2 upto 12% versus
    standard (non MMX-optimized) version.
    Note: on K6-2+ it speedups memory copying upto 25% and
          on K7 and P3 about 500% (5 times). */
#define HAVE_MMX1
#endif
#undef HAVE_K6_2PLUS
#if !defined( HAVE_ASM_MMX2) && defined( HAVE_ASM_3DNOW )
#define HAVE_K6_2PLUS
#endif
#endif


/* definitions */
#define BLOCK_SIZE 4096
#define CONFUSION_FACTOR 0
//Feel free to fine-tune the above 2, it might be possible to get some speedup with them :)

#if defined(ARCH_X86) || defined (ARCH_X86_64)
/* for small memory blocks (<256 bytes) this version is faster */
#define small_memcpy(to,from,n)\
{\
register unsigned long int dummy;\
__asm__ __volatile__(\
  "rep; movsb"\
  :"=&D"(to), "=&S"(from), "=&c"(dummy)\
  :"0" (to), "1" (from),"2" (n)\
  : "memory");\
}

/* for small memory blocks (<256 bytes) this version is faster */
#define small_memset(to,val,n)\
{\
register unsigned long int dummy;\
__asm__ __volatile__(\
	"rep; stosb"\
	:"=&D"(to), "=&c"(dummy)\
        :"0" (to), "1" (n), "a"((char)val)\
	:"memory");\
}
static inline unsigned long long int rdtsc()
{
     unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#else
#define	small_memcpy(to,from,n) memcpy( to,from,n )
#define small_memset(to,val,n) memset(to,val,n)

static inline unsigned long long int rdtsc()
{
     struct timeval tv;
   
     gettimeofday (&tv, NULL);
     return (tv.tv_sec * 1000000 + tv.tv_usec);
}
#endif

#ifdef ARCH_X86
static inline void * __memcpy(void * to, const void * from, size_t n)
{
     int d0, d1, d2;
     if ( n < 4 ) { 
          small_memcpy(to,from,n);
     }
     else
          __asm__ __volatile__(
            "rep ; movsl\n\t"
            "testb $2,%b4\n\t"
            "je 1f\n\t"
            "movsw\n"
            "1:\ttestb $1,%b4\n\t"
            "je 2f\n\t"
            "movsb\n"
            "2:"
            : "=&c" (d0), "=&D" (d1), "=&S" (d2)
            :"0" (n/4), "q" (n),"1" ((long) to),"2" ((long) from)
            : "memory");

     return(to);
}
/*
 * memset(x,0,y) is a reasonably common thing to do, so we want to fill
 * things 32 bits at a time even when we don't know the size of the
 * area at compile-time..
 */
void mymemzero(void * s, unsigned long c ,size_t count)
{
int d0, d1;
__asm__ __volatile__(
	"rep ; stosl\n\t"
	"testb $2,%b3\n\t"
	"je 1f\n\t"
	"stosw\n"
	"1:\ttestb $1,%b3\n\t"
	"je 2f\n\t"
	"stosb\n"
	"2:"
	: "=&c" (d0), "=&D" (d1)
	:"a" (c), "q" (count), "0" (count/4), "1" ((long) s)
	:"memory");
}

#undef _MMREG_SIZE
#ifdef HAVE_ASM_SSE
#define _MMREG_SIZE 16
#else
#define _MMREG_SIZE 64 
#endif

#undef _MIN_LEN
#ifdef HAVE_MMX1
#define _MIN_LEN 0x800  /* 2K blocks */
#else
#define _MIN_LEN 0x40  /* 64-byte blocks */
#endif


#undef _EMMS
#undef _PREFETCH

#ifdef HAVE_K6_2PLUS
#define _PREFETCH "prefetch"
/* On K6 femms is faster of emms. On K7 femms is directly mapped on emms. */
#define _EMMS     "femms"
#else
#define _PREFETCH "prefetchnta"
#define _EMMS     "emms"
#endif

static void *fast_memcpy(void * to, const void * from, size_t len)
{
	void *retval;
	size_t i;
	retval = to;
	unsigned char *t = to;
	unsigned char *f = (unsigned char *)from;
#ifndef HAVE_MMX1
	/* PREFETCH has effect even for MOVSB instruction ;) */
	__asm__ __volatile__ (
		_PREFETCH" (%0)\n"
		_PREFETCH" 64(%0)\n"
		_PREFETCH" 128(%0)\n"
		_PREFETCH" 192(%0)\n"
		_PREFETCH" 256(%0)\n"
		: : "r" (f) );
#endif
	if(len >= _MIN_LEN)
	{
	  register unsigned long int delta;
	  /* Align destinition to MMREG_SIZE -boundary */
	  delta = ((unsigned long int)to)&(_MMREG_SIZE-1);
	  if(delta)
	  {
	    delta=_MMREG_SIZE-delta;
	    len -= delta;
	    small_memcpy(t, f, delta);
	  }
	  i = len >> 6; /* len/64 */
	  len&=63;
	/*
	   This algorithm is top effective when the code consequently
	   reads and writes blocks which have size of cache line.
	   Size of cache line is processor-dependent.
	   It will, however, be a minimum of 32 bytes on any processors.
	   It would be better to have a number of instructions which
	   perform reading and writing to be multiple to a number of
	   processor's decoders, but it's not always possible.
	*/
#ifdef HAVE_ASM_SSE /* Only P3 (may be Cyrix3) */
	if(((unsigned long)f) & 15)
	/* if SRC is misaligned */
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		_PREFETCH" 320(%0)\n"
		"movups (%0), %%xmm0\n"
		"movups 16(%0), %%xmm1\n"
		"movups 32(%0), %%xmm2\n"
		"movups 48(%0), %%xmm3\n"
		"movntps %%xmm0, (%1)\n"
		"movntps %%xmm1, 16(%1)\n"
		"movntps %%xmm2, 32(%1)\n"
		"movntps %%xmm3, 48(%1)\n"
		:: "r" (f), "r" (t) : "memory");
		f+=64;
		t+=64;
	}
	else
	/*
	   Only if SRC is aligned on 16-byte boundary.
	   It allows to use movaps instead of movups, which required data
	   to be aligned or a general-protection exception (#GP) is generated.
	*/
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		_PREFETCH" 320(%0)\n"
		"movaps (%0), %%xmm0\n"
		"movaps 16(%0), %%xmm1\n"
		"movaps 32(%0), %%xmm2\n"
		"movaps 48(%0), %%xmm3\n"
		"movntps %%xmm0, (%1)\n"
		"movntps %%xmm1, 16(%1)\n"
		"movntps %%xmm2, 32(%1)\n"
		"movntps %%xmm3, 48(%1)\n"
		:: "r" (f), "r" (t) : "memory");
		f+=64;
		t+=64;
	}
#else
	// Align destination at BLOCK_SIZE boundary
	for(; ((int)to & (BLOCK_SIZE-1)) && i>0; i--)
	{
		__asm__ __volatile__ (
#ifndef HAVE_MMX1
		_PREFETCH" 320(%0)\n"
#endif
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movq 32(%0), %%mm4\n"
		"movq 40(%0), %%mm5\n"
		"movq 48(%0), %%mm6\n"
		"movq 56(%0), %%mm7\n"
		MOVNTQ" %%mm0, (%1)\n"
		MOVNTQ" %%mm1, 8(%1)\n"
		MOVNTQ" %%mm2, 16(%1)\n"
		MOVNTQ" %%mm3, 24(%1)\n"
		MOVNTQ" %%mm4, 32(%1)\n"
		MOVNTQ" %%mm5, 40(%1)\n"
		MOVNTQ" %%mm6, 48(%1)\n"
		MOVNTQ" %%mm7, 56(%1)\n"
		:: "r" (f), "r" (t) : "memory");
		f+=64;
		t+=64;
	}

	// Pure Assembly cuz gcc is a bit unpredictable ;)
	if(i>=BLOCK_SIZE/64)
		asm volatile(
			"xorl %%eax, %%eax	\n\t"
			".balign 16		\n\t"
			"1:			\n\t"
				"movl (%0, %%eax), %%ebx 	\n\t"
				"movl 32(%0, %%eax), %%ebx 	\n\t"
				"movl 64(%0, %%eax), %%ebx 	\n\t"
				"movl 96(%0, %%eax), %%ebx 	\n\t"
				"addl $128, %%eax		\n\t"
				"cmpl %3, %%eax			\n\t"
				" jb 1b				\n\t"

			"xorl %%eax, %%eax	\n\t"

				".balign 16		\n\t"
				"2:			\n\t"
				"movq (%0, %%eax), %%mm0\n"
				"movq 8(%0, %%eax), %%mm1\n"
				"movq 16(%0, %%eax), %%mm2\n"
				"movq 24(%0, %%eax), %%mm3\n"
				"movq 32(%0, %%eax), %%mm4\n"
				"movq 40(%0, %%eax), %%mm5\n"
				"movq 48(%0, %%eax), %%mm6\n"
				"movq 56(%0, %%eax), %%mm7\n"
				MOVNTQ" %%mm0, (%1, %%eax)\n"
				MOVNTQ" %%mm1, 8(%1, %%eax)\n"
				MOVNTQ" %%mm2, 16(%1, %%eax)\n"
				MOVNTQ" %%mm3, 24(%1, %%eax)\n"
				MOVNTQ" %%mm4, 32(%1, %%eax)\n"
				MOVNTQ" %%mm5, 40(%1, %%eax)\n"
				MOVNTQ" %%mm6, 48(%1, %%eax)\n"
				MOVNTQ" %%mm7, 56(%1, %%eax)\n"
				"addl $64, %%eax		\n\t"
				"cmpl %3, %%eax		\n\t"
				"jb 2b				\n\t"

#if CONFUSION_FACTOR > 0
	// a few percent speedup on out of order executing CPUs
			"movl %5, %%eax		\n\t"
				"2:			\n\t"
				"movl (%0), %%ebx	\n\t"
				"movl (%0), %%ebx	\n\t"
				"movl (%0), %%ebx	\n\t"
				"movl (%0), %%ebx	\n\t"
				"decl %%eax		\n\t"
				" jnz 2b		\n\t"
#endif

			"xorl %%eax, %%eax	\n\t"
			"addl %3, %0		\n\t"
			"addl %3, %1		\n\t"
			"subl %4, %2		\n\t"
			"cmpl %4, %2		\n\t"
			" jae 1b		\n\t"
				: "+r" (f), "+r" (t), "+r" (i)
				: "r" (BLOCK_SIZE), "i" (BLOCK_SIZE/64), "i" (CONFUSION_FACTOR)
				: "%eax", "%ebx"
		);

	for(; i>0; i--)
	{
		__asm__ __volatile__ (
#ifndef HAVE_MMX1
		_PREFETCH" 320(%0)\n"
#endif
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movq 32(%0), %%mm4\n"
		"movq 40(%0), %%mm5\n"
		"movq 48(%0), %%mm6\n"
		"movq 56(%0), %%mm7\n"
		MOVNTQ" %%mm0, (%1)\n"
		MOVNTQ" %%mm1, 8(%1)\n"
		MOVNTQ" %%mm2, 16(%1)\n"
		MOVNTQ" %%mm3, 24(%1)\n"
		MOVNTQ" %%mm4, 32(%1)\n"
		MOVNTQ" %%mm5, 40(%1)\n"
		MOVNTQ" %%mm6, 48(%1)\n"
		MOVNTQ" %%mm7, 56(%1)\n"
		:: "r" (f), "r" (t) : "memory");
		f+=64;
		t+=64;
	}

#endif /* Have SSE */
#ifdef HAVE_ASM_MMX2
		/* since movntq is weakly-ordered, a "sfence"
		 * is needed to become ordered again. */
		__asm__ __volatile__ ("sfence":::"memory");
#endif
#ifndef HAVE_ASM_SSE
		/* enables to use FPU */
		__asm__ __volatile__ (_EMMS:::"memory");
#endif
	}
	/*
	 *	Now do the tail of the block
	 */
	if(len) small_memcpy(t, f, len);
	return retval;
}
/* Fast memory set. See comments for fast_memcpy */
void * fast_memset(void * to, int val, size_t len)
{
	void *retval;
	size_t i;
	unsigned char mm_reg[_MMREG_SIZE], *pmm_reg;
	unsigned char *t = to;
  	retval = to;
        if(len >= _MIN_LEN)
	{
	  register unsigned long int delta;
          delta = ((unsigned long int)to)&(_MMREG_SIZE-1);
          if(delta)
	  {
	    delta=_MMREG_SIZE-delta;
	    len -= delta;
	    small_memset(t, val, delta);
	  }
	  i = len >> 7; /* len/128 */
	  len&=127;
	  pmm_reg = mm_reg;
	  small_memset(pmm_reg,val,sizeof(mm_reg));
#ifdef HAVE_ASM_SSE /* Only P3 (may be Cyrix3) */
	__asm__ __volatile__(
		"movups (%0), %%xmm0\n"
		:: "r"(mm_reg):"memory");
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		"movntps %%xmm0, (%0)\n"
		"movntps %%xmm0, 16(%0)\n"
		"movntps %%xmm0, 32(%0)\n"
		"movntps %%xmm0, 48(%0)\n"
		"movntps %%xmm0, 64(%0)\n"
		"movntps %%xmm0, 80(%0)\n"
		"movntps %%xmm0, 96(%0)\n"
		"movntps %%xmm0, 112(%0)\n"
		:: "r" (t) : "memory");
		t+=128;
	}
#else
	__asm__ __volatile__(
		"movq (%0), %%mm0\n"
		:: "r"(mm_reg):"memory");
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		MOVNTQ" %%mm0, (%0)\n"
		MOVNTQ" %%mm0, 8(%0)\n"
		MOVNTQ" %%mm0, 16(%0)\n"
		MOVNTQ" %%mm0, 24(%0)\n"
		MOVNTQ" %%mm0, 32(%0)\n"
		MOVNTQ" %%mm0, 40(%0)\n"
		MOVNTQ" %%mm0, 48(%0)\n"
		MOVNTQ" %%mm0, 56(%0)\n"
		MOVNTQ" %%mm0, 64(%0)\n"
		MOVNTQ" %%mm0, 72(%0)\n"
		MOVNTQ" %%mm0, 80(%0)\n"
		MOVNTQ" %%mm0, 88(%0)\n"
		MOVNTQ" %%mm0, 96(%0)\n"
		MOVNTQ" %%mm0, 104(%0)\n"
		MOVNTQ" %%mm0, 112(%0)\n"
		MOVNTQ" %%mm0, 120(%0)\n"
		:: "r" (t) : "memory");
		t+=128;
	}
#endif /* Have SSE */
#ifdef HAVE_ASM_MMX2
                /* since movntq is weakly-ordered, a "sfence"
		 * is needed to become ordered again. */
		__asm__ __volatile__ ("sfence":::"memory");
#endif
#ifndef HAVE_ASM_SSE
		/* enables to use FPU */
		__asm__ __volatile__ (EMMS:::"memory");
#endif
	}
	/*
	 *	Now do the tail of the block
	 */
	if(len) small_memset(t, val, len);
	return retval;
}

static void *linux_kernel_memcpy(void *to, const void *from, size_t len) {
     return __memcpy(to,from,len);
}

#endif

static struct {
     char                 *name;
     void               *(*function)(void *to, const void *from, size_t len);
     unsigned long long    time;
} memcpy_method[] =
{
     { NULL, NULL, 0},
     { "glibc memcpy()",            memcpy, 0},
#ifdef ARCH_X86
     { "linux kernel memcpy()",     linux_kernel_memcpy, 0},
     { "MMX/MMX2/SSE optimized memcpy()",    fast_memcpy, 0},
#endif
    { NULL, NULL, 0},
};

static struct {
     char                 *name;
     void                *(*function)(void *to, uint8_t c, size_t len);
     unsigned long long    time;
} memset_method[] =
{
     { NULL, NULL, 0},
     { "glibc memset()",            memset, 0},
#ifdef ARCH_X86
     { "MMX/MMX2/SSE optimized memset()",    fast_memset, 0},
#endif
     { NULL, NULL, 0},
};




void *(* veejay_memcpy)(void *to, const void *from, size_t len) = 0;

void *(* veejay_memset)(void *what, uint8_t val, size_t len ) = 0;

char *get_memcpy_descr( void )
{
	int i = 1;
	int best = 1;
	for (i=1; memcpy_method[i].name; i++)
	{
		if( memcpy_method[i].time <= memcpy_method[best].time )
			best = i;	
     	}
	char *res = strdup( memcpy_method[best].name );
	return res;
}

void find_best_memcpy()
{
     /* save library size on platforms without special memcpy impl. */
     unsigned long long t;
     char *buf1, *buf2;
     int i, best = 0;

     if (!(buf1 = (char*) malloc( BUFSIZE * 2000 * sizeof(char) )))
          return;

     if (!(buf2 = (char*) malloc( BUFSIZE * 2000 * sizeof(char) ))) {
          free( buf1 );
          return;
     }
	
     memset(buf1,0, BUFSIZE*2000);
     memset(buf2,0, BUFSIZE*2000);

     /* make sure buffers are present on physical memory */
     memcpy( buf1, buf2, BUFSIZE * 2000 );
     memcpy( buf2, buf1, BUFSIZE * 2000 );
     for (i=1; memcpy_method[i].name; i++) {
          t = rdtsc();

          memcpy_method[i].function( buf1 , buf2 , 2000 * BUFSIZE );

          t = rdtsc() - t;
          memcpy_method[i].time = t;

          if (best == 0 || t < memcpy_method[best].time)
               best = i;
     }
     if (best) {
          veejay_memcpy = memcpy_method[best].function;
     }

     free( buf1 );
     free( buf2 );
}




void find_best_memset()
{
     /* save library size on platforms without special memcpy impl. */
     unsigned long long t;
     char *buf1, *buf2;
     int i, best = 0;

     if (!(buf1 = (char*) malloc( BUFSIZE * 2000 * sizeof(char) )))
          return;

     if (!(buf2 = (char*) malloc( BUFSIZE * 2000 * sizeof(char) ))) {
          free( buf1 );
          return;
     }
	
     for( i = 0; i < (BUFSIZE*2000); i ++ )
     {
	     buf1[i] = 0;
	     buf2[i] = 0;
     }

     for (i=1; memset_method[i].name; i++)
     {
          t = rdtsc();

          memset_method[i].function( buf1 , 0 , 2000 * BUFSIZE );

          t = rdtsc() - t;
	  
          memset_method[i].time = t;

          if (best == 0 || t < memset_method[best].time)
               best = i;
     }

     if (best) {
          veejay_memset = memset_method[best].function;
     }

     free( buf1 );
     free( buf2 );
}

