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
#include <config.h>
#include <string.h>
#define VJ_PROMPT "$> "
#include <libvjmem/vjmem.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
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
static int run_server = 1;
static veejay_t *info = NULL;
static float override_fps = 0.0;
static int default_geometry_x = -1;
static int default_geometry_y = -1;
static int force_video_file = 0; // unused
static int override_pix_fmt = 1;
static int full_range = 0;
static char override_norm = 'p';
static int auto_loop = 0;
static int n_slots_ = 4;
static int max_mem_ = 30;
static int live =0;
static void CompiledWith()
{
	veejay_msg(VEEJAY_MSG_INFO,"Compilation flags:");
#ifdef USE_GDK_PIXBUF
	veejay_msg(VEEJAY_MSG_INFO,"\tUse GDK Pixbuf");
#endif
	veejay_msg(VEEJAY_MSG_INFO,"\tUse software scaler");
#ifdef HAVE_JACK
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing Jack audio server");
#endif
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing Video4linux");
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
#ifdef HAVE_GL
	veejay_msg( VEEJAY_MSG_INFO,  "\tUsing  openGL ");
#endif
#ifdef HAVE_DIRECTFB
	veejay_msg(VEEJAY_MSG_INFO,"\tUsing DirectFB");
#endif
#ifdef ARCH_X86_64
	veejay_msg(VEEJAY_MSG_INFO,"\tCompiled for x86-64 architecture");
#endif
#ifdef HAVE_X86CPU
	veejay_msg(VEEJAY_MSG_INFO,"\tCompiled for x86 architecture");
#endif
#ifdef HAVE_PPCCPU
	veejay_msg(VEEJAY_MSG_INFO,"\tCompiled for PPC architecture");
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
	exit(0);
}

static void Usage(char *progname)
{
    fprintf(stderr, "This is Veejay %s\n\n", VERSION);
    fprintf(stderr, "Usage: %s [options] <file name> [<file name> ...]\n",
	    progname);
    fprintf(stderr, "where options are:\n\n");
    fprintf(stderr,
		"\t\t\t\t-w/--Projection Width \n");
    fprintf(stderr,
		"\t\t\t\t-h/--Projection Height \n");

    fprintf(stderr,
	    "  -p/--portoffset\t\tTCP port to accept/send messages (default: 3490)\n");
    fprintf(stderr,
	    "  -t/--timer num\t\tspecify timer to use (none:0,normal:2,rtc:1) default is 1\n");

	fprintf(stderr,
		"  -O/--output [012345]\t\tOutput video\n");
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
		"\t\t\t\t3 = YUV4MPEG stream (use with -o/--outstream <file name>)\n");
#ifdef HAVE_GL
	fprintf(stderr,
		"\t\t\t\t4 = OpenGL (requires openGL extension ARB fragment program)\n");
#endif
	fprintf(stderr,
		"\t\t\t\t5 = Head less (no video output)\n");	
		
    fprintf(stderr,
	    "  -o/--outstream <file name>\twhere to write the yuv4mpeg stream (use with -O3)\n");
    fprintf(stderr,
	    "  -c/--synchronization [01]\tSync correction off/on (default on)\n");
    fprintf(stderr,
		"  -f/--fps num\t\t\tOverride default frame rate (default: read from first loaded file)\n");
    fprintf(stderr,
	    "  -P/--preserve-pathnames\tDo not 'canonicalise' pathnames in edit lists\n");
    fprintf(stderr,
	    "  -a/--audio [01]\t\tEnable (1) or disable (0) audio (default 1)\n");
#ifdef HAVE_SDL
    fprintf(stderr,
	    "  -s/--size NxN\t\t\twidth X height for SDL video window\n");
#endif
#ifdef HAVE_XINERAMA
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_GL
    fprintf(stderr,
            "  -X/--xinerama N\t\tSelect Xinerama screen [0-n] (Use with -O4)\n");
#endif
#endif
#endif
    fprintf(stderr,
	    "  -l/--action-file <file name>\tLoad a sample list/action list file (none at default)\n");
	fprintf(stderr,
	    "  -u/--dump-events  \t\tDump event information to screen\n");
	fprintf(stderr,
	    "  -I/--deinterlace\t\tDeinterlace video if it is interlaced\n");    
	fprintf(stderr,
		"  -x/--geometryx <num> \t\tTop left x offset for SDL video window\n");
	fprintf(stderr,
		"  -y/--geometryy <num> \t\tTop left y offset for SDL video window\n");
 	fprintf(stderr,
		"  -F/--features  \t\tList of compiled features\n");
	fprintf(stderr,
		"  -v/--verbose  \t\tEnable debugging output (default off)\n");
	fprintf(stderr,
		"  -b/--bezerk    \t\tBezerk (default off)   \n");
 	fprintf(stderr,
		"  -L/--auto-loop   \t\tStart with default sample\n");
	fprintf(stderr,
		"  -g/--clip-as-sample\t\tLoad every video clip as a new sample\n");	
	fprintf(stderr,
		"  -n/--no-color     \t\tDont use colored text\n");
	fprintf(stderr,
		"  -r/--audiorate\t\tDummy audio rate\n");
	fprintf(stderr,
		"  -m/--memory	\t\tMaximum memory to use for cache (0=disable, default=30 max=100)\n");  
	fprintf(stderr,
		"  -j/--max_cache \t\tDivide cache memory over N samples (default=4)\n");
	fprintf(stderr,
		"  -Y/--yuv [0123]\t\tForce pixel format if you get a corrupted image. Load one videofile only.\n");
	fprintf(stderr,
		"\t\t\t\t 0 = YUV 4:2:0 Planar\n");
	fprintf(stderr,
		"\t\t\t\t 1 = YUV 4:2:2 Planar (default)\n");
	fprintf(stderr,
		"\t\t\t\t 2 = YUV 4:2:0 Planar full range\n");
	fprintf(stderr,
		"\t\t\t\t 3 = YUV 4:2:2 Planar full range\n");
	fprintf(stderr,
		"  -d/--dummy	\t\tDummy playback\n");
	fprintf(stderr,
		"  -W/--width <num>\t\tdummy width\n");
	fprintf(stderr,
		"  -H/--height <num>\t\tdummy height\n");

	fprintf(stderr,
		"  -N/--norm [0=PAL, 1=NTSC (defaults to PAL)]\n");
	fprintf(stderr,
		"  -R/--framerate <num>\t\tdummy frame rate\n");
	fprintf(stderr,
		"  -M/--multicast-osc \t\tmulticast OSC\n");
	fprintf(stderr,
		"  -T/--multicast-vims \t\tmulticast VIMS\n");
	fprintf(stderr,
		"     --map-from-file <num>\tmap N frames to memory\n");
	fprintf(stderr,
		"  -z/--zoom [1-11]\t\tScale output video\n");
	fprintf(stderr,
		"\t\t\t\tsoftware scaler type (also use -w/--projection-width, -h/--projection-height ). \n");
	fprintf(stderr,
		"\t\t\t\available types are:\n");         
	fprintf(stderr,
		"\t\t\t\t1\tFast bilinear (default)\n");
	fprintf(stderr,
		"\t\t\t\t2\tBilinear\n");
	fprintf(stderr,
		"\t\t\t\t3\tBicubic (good quality)\n");
	fprintf(stderr,
		"\t\t\t\t4\tExperimental\n");
	fprintf(stderr,
		"\t\t\t\t5\tNearest Neighbor (bad quality)\n");
	fprintf(stderr,
		"\t\t\t\t6\tArea\n");
	fprintf(stderr,
		"\t\t\t\t7\tLuma bi cubic / chroma bilinear\n");
	fprintf(stderr,
		"\t\t\t\t9\tGauss\n");
	fprintf(stderr,
		"\t\t\t\t9\tsincR\n");
	fprintf(stderr,
		"\t\t\t\t10\tLanczos\n");
	fprintf(stderr,
		"\t\t\t\t11\tNatural bi cubic spline\n");
	fprintf(stderr,
		"\n\t\t\t\tsoftware scaler options:\n");
	fprintf(stderr,
		"\t\t\t\t--lgb=<0-100>\tGaussian blur filter (luma)\n");
	fprintf(stderr,
		"\t\t\t\t--cgb=<0-100>\tGuassian blur filter (chroma)\n");
	fprintf(stderr,
		"\t\t\t\t--ls=<0-100>\tSharpen filter (luma)\n");
	fprintf(stderr,
		"\t\t\t\t--cs=<0-100>\tSharpen filter (chroma)\n");
	fprintf(stderr,
		"\t\t\t\t--chs=<h>\tChroma horizontal shifting\n");
	fprintf(stderr,
		"\t\t\t\t--cvs=<v>\tChroma vertical shifting\n");
	fprintf(stderr,
		"\t\t\t\t-C/--zoomcrop [top:bottom:left:right] (crop source before scaling)\n");
	fprintf(stderr,
		"  -D/--composite \t\tDo not start with camera/projection calibration viewport \n");
#ifdef HAVE_UNICAP
	fprintf(stderr,
		"  -A/--all [num] \t\tStart with all capture devices, start with device <num> \n");
#endif
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
    if (strcmp(name, "portoffset") == 0 || strcmp(name, "p") == 0) {
	info->uc->port = atoi(optarg);
    } else if (strcmp(name, "verbose") == 0 || strcmp(name, "v") == 0) {
	info->verbose = 1;
    } else if (strcmp(name, "no-color") == 0 || strcmp(name,"n") == 0)
	{
	 veejay_set_colors(0);
    } else if (strcmp(name, "audio") == 0 || strcmp(name, "a") == 0) {
	info->audio = atoi(optarg);
    } else if ( strcmp(name, "A" ) == 0 || strcmp(name, "capture-device" ) == 0 ) {
	live = atoi(optarg);
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
	       || strcmp(name, "output-driver") == 0
	       || strcmp(name, "O") == 0) {
	    info->video_out = atoi(optarg);	/* use SDL */
#ifndef HAVE_GL
            if(info->video_out==4)
	    {
		fprintf(stderr, "OpenGL support not enabled at compile time\n");
		exit(-1);
	    }
#endif
    } else if (strcmp(name, "F") == 0 || strcmp(name, "features")==0) {
	CompiledWith();
        nerr++;
    } else if (strcmp(name, "preserve-pathnames") == 0
	       || strcmp(name, "P") == 0) {
	info->preserve_pathnames = 1;
    } else if (strcmp(name, "deinterlace") == 0 || strcmp(name, "I" )==0) {
	info->auto_deinterlace = 1;
    } else if (strcmp(name, "size") == 0 || strcmp(name, "s") == 0) {
	if (sscanf(value, "%dx%d", &info->bes_width, &info->bes_height) !=
	    2) {
	    mjpeg_error("--size parameter requires NxN argument");
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
    else if (strcmp(name, "outstream") == 0 || strcmp(name, "o") == 0) {
	check_val(optarg,name);
	snprintf(info->stream_outname,256,"%s", (char*) optarg);
#ifdef HAVE_XML2
    } else if (strcmp(name, "action-file")==0 || strcmp(name,"l")==0) {
	check_val(optarg,name);
	veejay_strncpy(info->action_file,(char*) optarg, strlen( (char*) optarg));
	info->load_action_file = 1;
#endif
	}
	else if(strcmp(name,"map-from-file") == 0 ) {
		info->seek_cache = atoi(optarg);
	}
	else if (strcmp(name, "geometry-x") == 0 || strcmp(name, "x")==0) {
		default_geometry_x = atoi(optarg);
	}
	else if (strcmp(name, "geometry-y") == 0 || strcmp(name,"y")==0) {
		default_geometry_y = atoi(optarg);
	}
	else if(strcmp(name,"dump-events")==0 || strcmp(name,"u")==0) {
	info->dump = 1;
	}
	else if(strcmp(name, "width") == 0 || strcmp(name, "W") == 0 ) {
		info->dummy->width = atoi(optarg);
	}
	else if(strcmp(name, "height") == 0 || strcmp(name, "H") == 0 ) {
		info->dummy->height = atoi(optarg);
	}
	else if(strcmp(name, "norm") == 0 || strcmp(name, "N") == 0 ) {
		info->dummy->norm = optarg[0];
		if(info->dummy->norm == 1 )	
			override_norm = 'n';
	}
	else if(strcmp(name, "D") == 0 || strcmp(name, "composite") == 0)
	{
		info->settings->composite = 0;
	}
	else if(strcmp(name, "projection-width") == 0 || strcmp(name, "w") == 0) {
		info->video_output_width = atoi(optarg);
	}
	else if(strcmp(name, "projection-height") == 0 || strcmp(name, "h") == 0) {
		info->video_output_height = atoi(optarg);
	}
	else if(strcmp(name, "audiorate") == 0 || strcmp(name, "r") == 0 )
	{
		info->dummy->arate = atoi(optarg);
	}
	else if(strcmp(name, "framerate") == 0 || strcmp(name, "R" ) == 0 ) {
		info->dummy->fps = atof(optarg);
	}
    	else if (strcmp(name,"fps")==0 || strcmp(name, "f")==0) {
		override_fps = atof(optarg);
	}
	else if(strcmp(name,"yuv")==0 || strcmp(name,"Y")==0)
	{
		override_pix_fmt = atoi(optarg);
		if(override_pix_fmt > 1 )
		{
			full_range = 1;
		}
		if( override_pix_fmt < 0 || override_pix_fmt > 3 )
			override_pix_fmt = 1;
	}
	else if( strcmp(name,"auto-loop")==0 || strcmp(name,"L") == 0)
	{
		auto_loop = 1;
	}
	else if (strcmp(name, "zoom") == 0 || strcmp(name, "z" ) == 0)
	{
		if(info->settings->composite) {
			fprintf(stderr, "Use --zoom or -z with -D or --composite");
			nerr++;
		}
		info->settings->zoom = atoi(optarg);
		if(info->settings->zoom < 1 || info->settings->zoom > 11)
		{
			fprintf(stderr, "Use --zoom [1-11] or -z [1-11]\n");
			nerr++;
		}
	}
	else if (strcmp(name, "lgb") == 0) 	
	{
		info->settings->sws_templ.lumaGBlur = (float)atof(optarg);
		OUT_OF_RANGE_ERR( info->settings->sws_templ.lumaGBlur);
		info->settings->sws_templ.use_filter = 1;
	}
	else if (strcmp(name, "cgb") == 0)
	{
		info->settings->sws_templ.chromaGBlur = (float)atof(optarg);
		OUT_OF_RANGE_ERR( info->settings->sws_templ.chromaGBlur );
		info->settings->sws_templ.use_filter = 1;

	}
	else if (strcmp(name, "ls") == 0)
	{
		info->settings->sws_templ.lumaSarpen = (float) atof(optarg);
		OUT_OF_RANGE_ERR( info->settings->sws_templ.lumaSarpen);
		info->settings->sws_templ.use_filter = 1;

	}
	else if (strcmp(name, "cs") == 0)
	{
		info->settings->sws_templ.chromaSharpen = (float) atof(optarg);
		OUT_OF_RANGE_ERR(info->settings->sws_templ.chromaSharpen);
		info->settings->sws_templ.use_filter = 1;

	}	
	else if (strcmp(name, "chs") == 0)
	{
		info->settings->sws_templ.chromaHShift = (float) atof(optarg);
		OUT_OF_RANGE_ERR(info->settings->sws_templ.chromaHShift );
		info->settings->sws_templ.use_filter = 1;

	}
	else if (strcmp(name, "cvs") == 0)
	{
		info->settings->sws_templ.chromaVShift = (float) atof(optarg);
		OUT_OF_RANGE_ERR(info->settings->sws_templ.chromaVShift );
		info->settings->sws_templ.use_filter = 1;

	}	
	else if (strcmp(name, "C") == 0 || strcmp(name, "zoomcrop") == 0 )
	{
		if (sscanf(value, "%d:%d:%d:%d", &(info->settings->viewport.top),
						 &(info->settings->viewport.bottom),
						 &(info->settings->viewport.left),
						 &(info->settings->viewport.right)) < 4)
		{
			fprintf(stderr, "Crop requires top:bottom:left:right\n");
			nerr++;
		}
		info->settings->crop = 1;
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
#ifdef HAVE_XINERAMA
#ifndef X_DISPLAY_MISSING
	{"xinerama",1,0,0},	/* -X/--Xinerama	*/
#endif
#endif
	{"graphics-driver", 1, 0, 0},
	{"timer", 1, 0, 0},	/* timer */
	{"dump-events",0,0,0},
	{"bezerk",0,0,0},
	{"outstream", 1, 0, 0},
	{"action-file",1,0,0},
	{"features",0,0,0},
	{"deinterlace",0,0,0},
	{"zoom",1,0,0},
	{"clip-as-sample",0,0,0},
	{"portoffset", 1, 0, 0},
	{"sample-mode",1,0,0},
	{"dummy",0,0,0},
	{"geometry-x",1,0,0},
	{"geometry-y",1,0,0},
	{"auto-loop",0,0,0},
	{"fps",1,0,0},
	{"no-color",0,0,0},
	{"version",0,0,0},
	{"width",1,0,0},
	{"height",1,0,0},
	{"projection-width", 1,0,0 },
	{"projection-height", 1,0,0 },
	{"norm",1,0,0},
	{"framerate",1,0,0},
	{"audiorate",1,0,0},
	{"yuv",1,0,0},
	{"multicast-osc",1,0,0},
	{"multicast-vims",1,0,0},
	{"map-from-file",1,0,0},
	{"zoomcrop",1,0,0},
	{"lgb",1,0,0},
	{"cgb",1,0,0},
	{"ls",1,0,0},
	{"cs",1,0,0},
	{"chs",1,0,0},
	{"cvs",1,0,0},
	{"composite",0,0,0},
	{"quit",0,0,0},
	{"memory",1,0,0},
	{"max_cache",1,0,0},
	{"capture-device",1,0,0},
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
			"o:G:O:a:H:s:c:t:j:l:p:m:w:h:x:y:r:z:f:Y:A:N:H:W:R:M:C:T:nILFPVDugvdibjq",
			long_options, &option_index)) != EOF)
#else
    while ((n =
	    getopt(argc, argv,
		   	"o:G:O:a:H:s:c:t:j:l:p:m:w:h:x:y:r:z:f:Y:A:N:H:W:R:M:C:T:nILFPVDugvdibjq"
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

    veejay_set_debug_level(info->verbose);

    if(info->video_out == 3)
    {
	veejay_silent();
	mjpeg_default_handler_verbosity( 0 );	
    }
    else
    {
    	mjpeg_default_handler_verbosity( (info->verbose ? 1:0) );
    }
    if(!info->dump)
	{
       if(veejay_open_files(
			info,
			argv + optind,
			argc - optind,
			override_fps,
			force_video_file,
			override_pix_fmt,
			override_norm )<=0)
       {
			vj_el_show_formats();
			veejay_msg(VEEJAY_MSG_ERROR, "Unable to open video file(s), codec/format not supported)");
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
	    "This software is subject to the GNU GENERAL PUBLIC LICENSE");

	veejay_msg(VEEJAY_MSG_INFO,
	    "Veejay comes with ABSOLUTELY NO WARRANTY; this is free software and");
	veejay_msg(VEEJAY_MSG_INFO,
		"you are welcome to redistribute it under certain conditions.");
	veejay_msg(VEEJAY_MSG_INFO,
	    "The license must be included in the (source) package (COPYING)");
}

static void donothing(int sig)
{
	veejay_msg(VEEJAY_MSG_INFO,"Received signal %x ",sig);
	veejay_handle_signal( info, sig );	
}

int main(int argc, char **argv)
{
	video_playback_setup *settings;
	char *dont_use; 
    	sigset_t allsignals;
    	struct sigaction action;
	struct sched_param schp;
	int i;
	fflush(stdout);
	vj_mem_init();
	vevo_strict_init();

	info = veejay_malloc();
	if (!info)
		return 1;

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
		veejay_free(info);
		return 0;
	}

	print_license();
	prepare_cache_line( max_mem_, n_slots_ );
	veejay_check_homedir( info );

    	sigemptyset(&(settings->signal_set));
	sigaddset(&(settings->signal_set), SIGINT);
	sigaddset(&(settings->signal_set), SIGPIPE);
	sigaddset(&(settings->signal_set), SIGILL);
	sigaddset(&(settings->signal_set), SIGSEGV);
	sigaddset(&(settings->signal_set), SIGFPE );
	sigaddset(&(settings->signal_set), SIGTERM );
	sigaddset(&(settings->signal_set), SIGABRT);
	sigaddset(&(settings->signal_set), SIGPWR );
	sigaddset(&(settings->signal_set), SIGQUIT );

	sigfillset( &allsignals );
	action.sa_handler = donothing;
	action.sa_mask = allsignals;
	action.sa_flags = SA_RESTART | SA_RESETHAND;

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

	if(veejay_init(
		info,
		default_geometry_x,
		default_geometry_y,
		NULL,
		live,
		full_range ) < 0 )
	{	
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot start Vveejay");
		return 0;
	}

	if(auto_loop)
		veejay_auto_loop(info);

    	if(!veejay_main(info))
	{
	    veejay_msg(VEEJAY_MSG_ERROR, "Cannot start main playback cycle");
		return 1;
	}


	veejay_msg(VEEJAY_MSG_DEBUG, "Starting playback");

	veejay_set_frame(info, 0);
	veejay_set_speed(info, 1);
	
	int sig;
	
   	while (veejay_get_state(info) != LAVPLAY_STATE_STOP) 
    	{
		usleep( 25000 );
    	}

	veejay_quit(info);
	veejay_busy(info);		/* wait for all the nice goodies to shut down */
	veejay_free(info);

	veejay_msg(VEEJAY_MSG_INFO, "Thank you for using Veejay");

	return 0;
}
