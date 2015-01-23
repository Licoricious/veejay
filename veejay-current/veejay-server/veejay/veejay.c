/* veejay - Linux VeeJay
 * 	     (C) 2002-2010 Niels Elburg <nwelburg@gmail.com> 
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
#include <config.h>
#include <string.h>
#define VJ_PROMPT "$> "
#include <stdio.h>
#include <stdint.h>
#include <libvjmem/vjmem.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <libvje/vje.h>
#include <libsubsample/subsample.h>
#include <veejay/vj-lib.h>
#include <veejay/vj-event.h>
#include <veejay/libveejay.h>
#include <libvevo/libvevo.h>
#include <libvje/vje.h>
#include <libvjmsg/vj-msg.h>
#include <veejay/vims.h>
#ifndef X_DISPLAY_MISSING
#include <veejay/x11misc.h>
#endif
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <veejay/vj-OSC.h>
#include <build.h>
extern void	veejay_init_msg_ring(); 
extern long vj_el_get_mem_size();
extern void vj_libav_ffmpeg_version();
static veejay_t *info = NULL;
static float override_fps = 0.0;
static int default_geometry_x = -1;
static int default_geometry_y = -1;
static	int	use_keyb = 1;
static	int	use_mouse = 1;
static	int	show_cursor = 0;
static int force_video_file = 0; // unused
static int override_pix_fmt = 0;
static int switch_jpeg = 0;
static char override_norm = 'p';
static int auto_loop = 0;
static int n_slots_ = 0;
static int max_mem_ = 0;
static int live =0;
static int ta = 0;
static int osl = 0;
static void CompiledWith()
{
	veejay_msg(VEEJAY_MSG_INFO,"Compilation flags:");
#ifdef ARCH_MIPS
	veejay_msg(VEEJAY_MSG_INFO, "\tCompiled for MIPS");
#endif
#ifdef ARCH_PPC
	veejay_msg(VEEJAY_MSG_INFO, "\tCompiled for PPC");
#endif
#ifdef ARCH_X86_64
	veejay_msg(VEEJAY_MSG_INFO, "\tCompiled for X86_64");
#endif
#ifdef ARCH_X86
	veejay_msg(VEEJAY_MSG_INFO, "\tCompiled for X86");
#endif
#ifdef HAVE_DARWIN
	veejay_msg(VEEJAY_MSG_INFO, "\tCompiled for Darwin");
#endif
#ifdef HAVE_PS2
	veejay_msg(VEEJAY_MSG_INFO, "\tCompiled for Sony Playstation 2 (TM)");
#endif
#ifdef HAVE_ALTIVEC
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing Altivec");
#endif
#ifdef HAVE_ASM_SSE
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing SSE instruction set");
#endif
#ifdef HAVE_CMOV
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing CMOV");
#endif
#ifdef HAVE_ASM_SSE2
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing SSE2 instruction set");
#endif
#ifdef HAVE_ASM_MMX
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing MMX instruction set");
#endif
#ifdef HAVE_ASM_MMX2	
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing MMX2 instruction set");
#endif
#ifdef HAVE_ASM_3DNOW
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing 3Dnow instruction set");
#endif

#ifdef USE_GDK_PIXBUF
	veejay_msg(VEEJAY_MSG_INFO,"\tUse GDK Pixbuf");
#endif
	veejay_msg(VEEJAY_MSG_INFO,"\tUse software scaler");
#ifdef HAVE_JACK
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing Jack audio server");
#endif
#ifdef SUPPORT_READ_DV2
	veejay_msg(VEEJAY_MSG_INFO,"\tSupport for Digital Video enabled");
#endif
#ifdef HAVE_XML2
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing XML library for Gnome");
#endif
#ifdef SUPPORT_READ_DV2
	veejay_msg(VEEJAY_MSG_INFO,"\tSupport for Digital Video enabled");
#endif
#ifdef HAVE_SDL
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing Simple Direct Media Layer");
#endif
#ifdef HAVE_JPEG
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing libjpeg");
#endif
#ifdef HAVE_LIBPTHREAD
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing libpthread");
#endif
#ifdef HAVE_V4L
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing Video4Linux");
#endif
#ifdef HAVE_V4L2
	veejay_msg(VEEJAY_MSG_INFO, "\tUsing Video4Linux II");
#endif
	/*
#ifdef HAVE_GL
	veejay_msg( VEEJAY_MSG_INFO,  "\tUsing  openGL ");
#endif
	*/
#ifdef HAVE_DIRECTFB
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing DirectFB");
#endif

	vj_libav_ffmpeg_version();

	exit(0);
}

static void Usage(char *progname)
{
    fprintf(stderr, "This is Veejay %s\n\n", VERSION);
    fprintf(stderr, "Usage: %s [options] <file name> [<file name> ...]\n", progname);
    fprintf(stderr,"where options are:\n\n");
	fprintf(stderr,
			"  -v/--verbose  \t\tEnable debugging output (default off)\n");
	fprintf(stderr,
			"  -n/--no-color     \t\tDo not use colored text\n");
    fprintf(stderr,
	    	"  -u/--dump-events  \t\tDump veejay's documentation to stdout\n");
	fprintf(stderr,
			"  -B/--features  \t\tList of compiled features\n");
   	fprintf(stderr,
			"  -D/--composite \t\tDo not start with camera/projection calibration viewport \n");

	fprintf(stderr,
	   		"  -p/--port <num>\t\tTCP port to accept/send messages (default: 3490)\n");
	fprintf(stderr,
			"  -M/--multicast-osc \t\tmulticast OSC\n");
	fprintf(stderr,
			"  -T/--multicast-vims \t\tmulticast VIMS\n");
	
 	fprintf(stderr,
			"  -m/--memory <num>\t\tMaximum memory to use for cache (0=disable, default=0 max=100)\n");  
	fprintf(stderr,
			"  -j/--max_cache <num>\t\tDivide cache memory over N samples (default=0)\n");
	
	fprintf(stderr,
			"  -Y/--yuv [01]\t\t\tForce YCbCr (defaults to YUV)\n");

	fprintf(stderr,
	    	"  -t/--timer <num>\t\tspecify timer to use (none:0,normal:2,rtc:1) default is 1\n");

    fprintf(stderr,
	    	"  -P/--preserve-pathnames\tDo not 'canonicalise' pathnames in edit lists\n");
	fprintf(stderr,
            "  -e/--swap-range <num>\t\tSwap YUV range [0..255] <-> [16..235] on videofiles\n");
	fprintf(stderr,
		"\t\t\t\t 0 = YUV 4:2:0 Planar\n");
	fprintf(stderr,
		"\t\t\t\t 1 = YUV 4:2:2 Planar (default)\n");
	fprintf(stderr,
		"\t\t\t\t 2 = YUV 4:2:0 Planar full range\n");
	fprintf(stderr,
		"\t\t\t\t 3 = YUV 4:2:2 Planar full range\n");
   	fprintf(stderr,
			"  -L/--auto-loop   \t\tStart with default sample\n");
	fprintf(stderr,
			"  -b/--bezerk    \t\tBezerk (default off)   \n");
	fprintf(stderr,
	   		"  -l/--sample-file <file name>\tLoad a sample list file (none at default)\n");
	fprintf(stderr,
			"  -F/--action-file <file name>\tLoad an action file (none at default)\n");

	
	fprintf(stderr,
			"  -g/--clip-as-sample\t\tLoad every video clip as a new sample\n");	
	
	fprintf(stderr,
	    	"  -a/--audio [01]\t\tEnable (1) or disable (0) audio (default 1)\n");
	
	fprintf(stderr,
	    	"  -c/--synchronization [01]\tSync correction off/on (default on)\n");

    fprintf(stderr,
	    	"  -O/--output [0..4]\t\tOutput video\n");
#ifdef HAVE_SDL
	fprintf(stderr,
			"\t\t\t\t0 = SDL (default)\t\n");
#endif
#ifdef HAVE_DIRECTFB
	fprintf(stderr,
			"\t\t\t\t1 = DirectDB\t\n");
#ifdef HAVE_SDL
	fprintf(stderr,
			"\t\t\t\t2 = SDL and DirectDB secondary head (TV-Out) clone mode\n");
#endif
#endif
	fprintf(stderr,
			"\t\t\t\t3 = Head less (no video output)\n");	
	fprintf(stderr,
			"\t\t\t\t4 = Y4M Stream\n");
	fprintf(stderr,
			"  -o/--output-file [file]\tWrite to file (for use with Y4M Stream)\n");
#ifdef HAVE_SDL
        fprintf(stderr,
	    "  -s/--size NxN\t\t\tDisplay dimension for video window, use Width x Height\n");

	fprintf(stderr,
		"  -x/--geometry-x <num> \tTop left x offset for SDL video window\n");
	fprintf(stderr,
		"  -y/--geometry-y <num> \tTop left y offset for SDL video window\n");

	fprintf(stderr,
		"  --no-keyboard\t\t\tdisable keyboard for SDL video window\n");
	fprintf(stderr,
		"  --no-mouse\t\t\tdisable mouse for SDL video window\n");
	fprintf(stderr, 
		"  --show-cursor\t\t\tshow mouse cursor in SDL video window\n");	
#endif
	fprintf(stderr,
		"  -A/--all [num] \t\tStart with capture device <num> \n");

	fprintf(stderr,
		"  -d/--dummy	\t\tStart with no video (black frames)\n");
	fprintf(stderr,
		"  -Z/--load-generators [num]\tLoad all generator plugins as streams and start with N\n");
	fprintf(stderr,
		"  -W/--input-width <num>\tSet input video width\n");
	fprintf(stderr,
		"  -H/--input-height <num>\tSet input video height\n");
    fprintf(stderr,
		"  -f/--fps <num>\t\tOverride default frame rate (default: read from first loaded file)\n");
   	fprintf(stderr,
		"  -r/--audiorate <num>\t\tSet audio rate (defaults to 48Khz)\n");
	fprintf(stderr,
	    "  -I/--deinterlace\t\tDeinterlace video if it is interlaced\n");    
	fprintf(stderr,
		"  -N/--norm <num>\t\tSet video norm [0=PAL, 1=NTSC (defaults to PAL)]\n");

	fprintf(stderr,
		"  -w/--output-width <num>\tSet output video width (Projection)\n");
	fprintf(stderr,
		"  -h/--output-height <num>\tSet output video height (Projection)\n");

	fprintf(stderr,
		"  --benchmark NxN\t\tveejay benchmark using NxN resolution\n");

	fprintf(stderr,"  -q/--quit \t\t\tQuit at end of file\n");
	fprintf(stderr,"\n\n");
}

#define OUT_OF_RANGE(val) ( val < 0 ? 1 : ( val > 100 ? 1 : 0) )

#define OUT_OF_RANGE_ERR(val) if(OUT_OF_RANGE(val)) { fprintf(stderr,"\tValue must be 0-100\n"); exit(1); }

#define check_val(val,msg) {\
char *v = strdup(val);\
if(v==NULL){\
fprintf(stderr, " Invalid argument given for %s\n",msg);\
}\
else\
{\
free(v);\
}\
}
static int set_option(const char *name, char *value)
{
    /* return 1 means error, return 0 means okay */
    int nerr = 0;
    if (strcmp(name, "port") == 0 || strcmp(name, "p") == 0) {
	info->uc->port = atoi(optarg);
    } else if (strcmp(name, "verbose") == 0 || strcmp(name, "v") == 0) {
	info->verbose = 1;
	veejay_set_debug_level(info->verbose);
    } else if (strcmp(name, "no-color") == 0 || strcmp(name,"n") == 0)
	{
	 veejay_set_colors(0);
    } else if (strcmp(name, "audio") == 0 || strcmp(name, "a") == 0) {
	info->audio = atoi(optarg);
    } else if ( strcmp(name, "A" ) == 0 || strcmp(name, "capture-device" ) == 0 ) {
	live = atoi(optarg);
    } else if ( strcmp(name, "Z" ) == 0 || strcmp(name, "load-generators" ) == 0 ) {
		if( sscanf( optarg, "%d",&ta ) != 1 ) {
			fprintf(stderr, "-Z/--load-generators requires an argument\n");
			nerr++;
		}
	} else if (strcmp(name, "bezerk") == 0 || strcmp(name, "b") == 0) {
	info->no_bezerk = 0;
    } else if (strcmp(name, "timer") == 0 || strcmp(name, "t") == 0) {
	info->uc->use_timer = atoi(optarg);
	if (info->uc->use_timer < 0 || info->uc->use_timer > 2) {
	    printf("Valid timers:\n\t0=none\n\t2=normal\n\t1=rtc\n");
	    nerr++;
	}
	} else if (strcmp(name, "multicast-vims") == 0 || strcmp(name,"T")==0)
	{
		check_val(optarg, name);
		info->settings->use_vims_mcast = 1;
		info->settings->vims_group_name = strdup(optarg);
	}
	else if (strcmp(name, "multicast-osc") == 0 || strcmp(name, "M") == 0 )
	{
		check_val(optarg,name);
		info->settings->use_mcast = 1;
		info->settings->group_name = strdup( optarg );
	}
	else if (strcmp(name, "max_cache" )== 0 || strcmp(name, "j" ) == 0 )
	{
		n_slots_ = atoi( optarg );
		if(n_slots_ < 0 ) n_slots_ = 0; else if (n_slots_ > 100) n_slots_ = 100;
	}
	else if (strcmp(name, "memory" ) == 0 || strcmp(name, "m" ) == 0)
	{
		max_mem_ =  atoi(optarg);
		if(max_mem_ < 0 ) max_mem_ = 0; else if (max_mem_ > 100) max_mem_ = 100;
    } else if (strcmp(name, "synchronization") == 0
	       || strcmp(name, "c") == 0) {
	info->sync_correction = atoi(optarg);
	} else if (strcmp(name, "version") == 0 )
	{ printf("Veejay %s\n", VERSION); exit(0); 
    } else if (strcmp(name, "graphics-driver") == 0
	       || strcmp(name, "G") == 0
	       || strcmp(name, "output") == 0
	       || strcmp(name, "O") == 0) {
	    info->video_out = atoi(optarg);	/* use SDL */
/*#ifndef HAVE_GL
            if(info->video_out==3)
	    {
		fprintf(stderr, "OpenGL support not enabled at compile time\n");
		exit(-1);
	    }
#endif
*/
	    if( info->video_out < 0 || info->video_out > 5 ) {
		    fprintf(stderr, "Select a valid output display driver\n");
		    exit(-1);
		   }
    } else if (strcmp(name, "B") == 0 || strcmp(name, "features")==0) {
	CompiledWith();
        nerr++;
	} else if ( strcmp(name, "output-file" ) == 0 || strcmp(name, "o") == 0 ) {
		check_val(optarg,name);
		veejay_strncpy(info->y4m_file,(char*) optarg, strlen( (char*) optarg));
    } else if (strcmp(name, "preserve-pathnames") == 0 ) {
		info->preserve_pathnames = 1;
	} else if (strcmp(name, "benchmark" ) == 0 ) {
		int w=0,h=0;
		int n = 0;
		if( value != NULL )
			n = sscanf(value, "%dx%d", &w,&h );
		if( n != 2 || value == NULL ) {
			fprintf(stderr,"  --benchmark parameter requires NxN argument\n");
		    	nerr++;
		} 
		if( n == 2 ) {
		    benchmark_veejay(w,h);
		    exit(0);
		}
    } else if (strcmp(name, "deinterlace") == 0 || strcmp(name, "I" )==0) {
		info->auto_deinterlace = 1;
    } else if (strcmp(name, "size") == 0 || strcmp(name, "s") == 0) {
	if (sscanf(value, "%dx%d", &info->bes_width, &info->bes_height) !=
	    2) {
	    fprintf(stderr,"-S/--size parameter requires NxN argument\n");
	    nerr++;
	}
     }
#ifdef HAVE_XINERAMA
#ifndef X_DISPLAY_MISSING
    else if (strcmp(name, "xinerama") == 0 || strcmp(name, "X") == 0 ) {
	x11_user_select( atoi(optarg) );
    }
#endif
#endif
    else if (strcmp(name, "action-file")==0 || strcmp(name,"F")==0) {
	check_val(optarg,name);
	veejay_strncpy(info->action_file[0],(char*) optarg, strlen( (char*) optarg));
	info->load_action_file = 1;
	}else if (strcmp(name, "sample-file")==0 || strcmp(name,"l")==0) {
	check_val(optarg,name);
	veejay_strncpy(info->action_file[1],(char*) optarg, strlen( (char*) optarg));
	info->load_action_file = 1;
	}
	else if (strcmp(name, "geometry-x") == 0 || strcmp(name, "x")==0) {
		default_geometry_x = atoi(optarg);
	}
	else if (strcmp(name, "geometry-y") == 0 || strcmp(name,"y")==0) {
		default_geometry_y = atoi(optarg);
	}
	else if (strcmp(name, "no-keyboard") == 0 ) {
		use_keyb = 0;
	}
	else if (strcmp(name, "no-mouse") == 0 ) {
		use_mouse = 0;
	}
	else if (strcmp(name, "show-cursor") == 0 ) {
		show_cursor = 1;
	}
	else if(strcmp(name,"dump-events")==0 || strcmp(name,"u")==0) {
	info->dump = 1;
	}
	else if(strcmp(name, "input-width") == 0 || strcmp(name, "W") == 0 ) {
		info->dummy->width = atoi(optarg);
	}
	else if(strcmp(name, "input-height") == 0 || strcmp(name, "H") == 0 ) {
		info->dummy->height = atoi(optarg);
	}
	else if(strcmp(name, "norm") == 0 || strcmp(name, "N") == 0 ) {
		int val = atoi(optarg);
		if(val == 1 )	
			override_norm = 'n';
	}
	else if(strcmp(name, "D") == 0 || strcmp(name, "composite") == 0)
	{
		info->settings->composite = 0;
	}
	else if(strcmp(name, "output-width") == 0 || strcmp(name, "w") == 0) {
		info->video_output_width = atoi(optarg);
	}
	else if(strcmp(name, "output-height") == 0 || strcmp(name, "h") == 0) {
		info->video_output_height = atoi(optarg);
	}
	else if(strcmp(name, "audiorate") == 0 || strcmp(name, "r") == 0 )
	{
		info->dummy->arate = atoi(optarg);
	}
    	else if (strcmp(name,"fps")==0 || strcmp(name, "f")==0) {
		override_fps = atof(optarg);
	}
	else if(strcmp(name,"yuv")==0 || strcmp(name,"Y")==0)
	{
		override_pix_fmt = atoi(optarg);
		if( override_pix_fmt < 0 || override_pix_fmt > 2 )
			override_pix_fmt = 0;
	}
	else if(strcmp(name, "swap-range") == 0 || strcmp(name, "e") == 0 )
	{
		switch_jpeg = 1;
	}
	else if( strcmp(name,"auto-loop")==0 || strcmp(name,"L") == 0)
	{
		auto_loop = 1;
	}
	else if (strcmp(name, "quit") == 0 || strcmp(name, "q") == 0 )
	{
		info->continuous = 0;
	}
	else if (strcmp(name, "clip-as-sample") == 0 || strcmp(name, "g") == 0 )
	{	
		info->uc->file_as_sample = 1;
	}
	else if (strcmp(name, "dummy") == 0 || strcmp(name, "d" ) == 0 )
	{
		info->dummy->active = 1; // enable DUMMY MODE
	}
    	else
		nerr++;			/* unknown option - error */

    return nerr;
}

static int check_command_line_options(int argc, char *argv[])
{
    int nerr, n, option_index = 0;
    char option[2];
#ifdef HAVE_GETOPT_LONG
    /* getopt_long options */
    static struct option long_options[] = {
	{"verbose", 0, 0, 0},	/* -v/--verbose         */
	{"skip", 1, 0, 0},	/* -s/--skip            */
	{"synchronization", 1, 0, 0},	/* -c/--synchronization */
	{"preserve-pathnames", 0, 0, 0},	/* -P/--preserve-pathnames    */
	{"audio", 1, 0, 0},	/* -a/--audio num       */
	{"size", 1, 0, 0},	/* -S/--size            */
	{"benchmark", 1, 0, 0}, /* --benchmark	 */
/*#ifdef HAVE_XINERAMA
#ifndef X_DISPLAY_MISSING
	{"xinerama",1,0,0},
#endif
#endif
*/
	{"graphics-driver", 1, 0, 0},
	{"timer", 1, 0, 0},	/* timer */
	{"dump-events",0,0,0},
	{"bezerk",0,0,0},
	{"sample-file",1,0,0},
	{"action-file",1,0,0},
	{"features",0,0,0},
	{"deinterlace",0,0,0},
	{"clip-as-sample",0,0,0},
	{"port", 1, 0, 0},
	{"sample-mode",1,0,0},
	{"dummy",0,0,0},
	{"geometry-x",1,0,0},
	{"geometry-y",1,0,0},
	{"no-keyboard",0,0,0},
	{"no-mouse",0,0,0},
	{"show-cursor",0,0,0},
	{"auto-loop",0,0,0},
	{"fps",1,0,0},
	{"no-color",0,0,0},
	{"version",0,0,0},
	{"input-width",1,0,0},
	{"input-height",1,0,0},
	{"output-width", 1,0,0 },
	{"output",1,0,0},
	{"output-file",1,0,0},
	{"output-height", 1,0,0 },
	{"norm",1,0,0},
	{"audiorate",1,0,0},
	{"yuv",1,0,0},
	{"multicast-osc",1,0,0},
	{"multicast-vims",1,0,0},
	{"composite",0,0,0},
	{"quit",0,0,0},
	{"memory",1,0,0},
	{"max_cache",1,0,0},
	{"capture-device",1,0,0},
	{"swap-range",0,0,0},
	{"load-generators",1,0,0},
	{0, 0, 0, 0}
    };
#endif
    if (argc < 2) {
	Usage(argv[0]);
	return 0;
    }
    
/* Get options */
    nerr = 0;
#ifdef HAVE_GETOPT_LONG
    while ((n =
	    getopt_long(argc, argv,
			"o:G:O:a:H:s:c:t:j:l:p:m:h:w:x:y:r:f:Y:A:N:H:W:M:T:F:nILPVDugvBdibjqeZ:",
			long_options, &option_index)) != EOF)
#else
    while ((n =
	    getopt(argc, argv,
		   	"o:G:O:a:H:s:c:t:j:l:p:m:h:w:x:y:r:f:Y:A:N:H:W:M:T:F:nILPVDugvBdibjqeZ:"
						   )) != EOF)
#endif
    {
	switch (n) {
#ifdef HAVE_GETOPT_LONG
	    /* getopt_long values */
	case 0:
	    nerr += set_option(long_options[option_index].name, optarg);
	    break;
#endif

	    /* These are the old getopt-values (non-long) */
	default:
	    sprintf(option, "%c", n);
#ifdef STRICT_CHECKING
	    {
		int tmpn = set_option( option,optarg );
		if(tmpn)
			veejay_msg(VEEJAY_MSG_ERROR, "Error setting option '%c'", option );
		nerr += tmpn;
	    }
#else
	    nerr += set_option(option, optarg);
#endif	    
	    break;
	}
	
    }
    if (optind > argc)
	nerr++;

    if (nerr)
	Usage(argv[0]);

    if(!info->dump)
	{
       if(veejay_open_files(
			info,
			argv + optind,
			argc - optind,
			override_fps,
			force_video_file,
			override_pix_fmt,
			override_norm,
			switch_jpeg )<=0)
       {
			vj_el_show_formats();
			veejay_msg(VEEJAY_MSG_ERROR, "Unable to open video file(s)");
			nerr++;
       }
	}

    if(!nerr) 
		return 1;
	return 0;
}

static void print_license()
{
	veejay_msg(VEEJAY_MSG_INFO,
	    "Veejay -<|Classic +|>- %s Copyright (C) Niels Elburg and others",VERSION);
	veejay_msg(VEEJAY_MSG_INFO,    
		"Build for %s/%s arch %s on %s",
	    BUILD_OS,
	    BUILD_KERNEL,
	    BUILD_MACHINE,
	    BUILD_DATE );

	veejay_msg(VEEJAY_MSG_INFO,
	    "This software is subject to the GNU GENERAL PUBLIC LICENSE");

	veejay_msg(VEEJAY_MSG_INFO,    
		"Veejay comes with ABSOLUTELY NO WARRANTY; this is free software and");
	
	veejay_msg(VEEJAY_MSG_INFO,
		"you are welcome to redistribute it under certain conditions.");
	
	veejay_msg(VEEJAY_MSG_INFO,
	    "The license must be included in the (source) package (COPYING)");

	veejay_msg(VEEJAY_MSG_INFO,
        "Veejay's BTC donation address: 1PUNRsv8vDt1upTx9tTpY5sH8mHW1DTrKJ");
}

static void donothing(int sig)
{
	vj_lock(info); //FIXME
	veejay_handle_signal( info, sig );	
	vj_unlock(info);
}

static void	sigsegfault_handler(void) {
	struct sigaction sigst;
	sigst.sa_sigaction = veejay_backtrace_handler;
	sigemptyset(&sigst.sa_mask);
	sigaddset(&sigst.sa_mask, SIGSEGV );
	sigst.sa_flags = SA_SIGINFO | SA_ONESHOT;
	if( sigaction(SIGSEGV, &sigst, NULL) == - 1) 
		veejay_msg(0,"sigaction");
}

int main(int argc, char **argv)
{
	video_playback_setup *settings;
	 
    	sigset_t allsignals;
    	struct sigaction action;
	struct timespec req;
	int i;
	fflush(stdout);

	vj_mem_init();

	vevo_strict_init();
	
	info = veejay_malloc();
	if (!info) {
		vj_mem_threaded_stop();
		return 1;
	}
	
   	settings = (video_playback_setup *) info->settings;

	if(!check_command_line_options(argc, argv))
	{
		veejay_free(info);
		return 0;
    	}

	
   	if(info->dump)
 	{
		veejay_set_colors(0);
		vj_event_init();
		vj_effect_initialize(720,576,0);
		vj_osc_allocate(VJ_PORT+2);	
		vj_event_dump();
		vj_effect_dump();
			fprintf(stdout, "Environment variables:\n\tSDL_VIDEO_HWACCEL\t\tSet to 1 to use SDL video hardware accel (default=on)\n\tVEEJAY_PERFORMANCE\t\tSet to \"quality\" or \"fastest\" (default is fastest)\n\tVEEJAY_AUTO_SCALE_PIXELS\tSet to 1 to convert between CCIR 601 and JPEG automatically (default=dont care)\n\tVEEJAY_INTERPOLATE_CHROMA\tSet to 1 if you wish to interpolate every chroma sample when scaling (default=0)\n\tVEEJAY_CAPTURE_DRIVER\t\tSet to \"unicap\" or \"v4lutils\" (default=v4lutils)\n\tVEEJAY_SDL_KEY_REPEAT_INTERVAL\tinterval of key pressed to repeat while pressed down.\n\tVEEJAY_PLAYBACK_CACHE\t\tSample cache size in MB - by default, veejay takes 30 percent of total RAM\n\tVEEJAY_SDL_KEY_REPEAT_DELAY\tDelay key repeat in ms\n\tVEEJAY_FULLSCREEN\t\tStart in fullscreen (1) or windowed (0) mode\n\tVEEJAY_SCREEN_GEOMETRY\t\tSpecifiy a geometry for veejay to position the video window.\n\tVEEJAY_SCREEN_SIZE\t\tSize of video window, defaults to full screen size.\n\tVEEJAY_RUN_MODE\t\t\tRun in \"classic\" (352x288 Dummy) or default (720x576). \n");
			fprintf(stdout, "\n\n\tExample for bash:\n\t\t\t$ export VEEJAY_AUTO_SCALE_PIXEL=1\n");


		veejay_free(info);
		return 0;
	}

	if( vj_el_get_mem_size() == 0 )
		prepare_cache_line( max_mem_, n_slots_ );

	veejay_check_homedir( info );

	sigsegfault_handler();

    	sigemptyset(&(settings->signal_set));
	sigaddset(&(settings->signal_set), SIGINT);
	sigaddset(&(settings->signal_set), SIGPIPE);
	sigaddset(&(settings->signal_set), SIGILL);
//	sigaddset(&(settings->signal_set), SIGSEGV);
	sigaddset(&(settings->signal_set), SIGFPE );
	sigaddset(&(settings->signal_set), SIGTERM );
	sigaddset(&(settings->signal_set), SIGABRT);
	sigaddset(&(settings->signal_set), SIGPWR );
	sigaddset(&(settings->signal_set), SIGQUIT );

	sigfillset( &allsignals );
	action.sa_handler = donothing;
	action.sa_mask = allsignals;
	action.sa_flags = SA_SIGINFO | SA_ONESHOT ; //SA_RESTART | SA_RESETHAND;

	signal( SIGPIPE, SIG_IGN );

	for( i = 1; i < NSIG; i ++ )
		if( sigismember( &(settings->signal_set), i ))
			sigaction( i, &action, 0 );
	
	char *mem_func = get_memcpy_descr();
	if(mem_func)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Using SIMD %s", mem_func);
		free(mem_func);
	}
	info->use_keyb = use_keyb;
	info->use_mouse = use_mouse;
	info->show_cursor = show_cursor;


	if(veejay_init(
		info,
		default_geometry_x,
		default_geometry_y,
		NULL,
		live,
	    ta	)< 0)
	{	
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot start veejay");
		return 0;
	}

	if(auto_loop)
		veejay_auto_loop(info);


	print_license();

	veejay_init_msg_ring();  // rest of logging to screen

    	if(!veejay_main(info))
	{
	    veejay_msg(VEEJAY_MSG_ERROR, "Cannot start main playback cycle");
		veejay_free(info);
		return 1;
	}

	
	veejay_msg(VEEJAY_MSG_DEBUG, "Started playback");


	int current_state = LAVPLAY_STATE_PLAYING;

	req.tv_sec = 0;
	req.tv_nsec = 4000 * 1000; 

	while( 1 ) { //@ until your PC stops working
		
		nanosleep( &req, NULL );

		current_state = veejay_get_state(info);
		
		if( current_state == LAVPLAY_STATE_STOP )
			break;
	}

	veejay_busy(info);			
	veejay_free(info);

	veejay_msg(VEEJAY_MSG_INFO, "Thank you for using Veejay");

#ifdef STRICT_CHECKING
	vevo_report_stats();
#endif
	return 0;
}
