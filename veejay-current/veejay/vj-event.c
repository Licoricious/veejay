/*
 * Linux VeeJay
 *
 * Copyright(C)2002-2004 Niels Elburg <nelburg@looze.net>
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
 */


#include <config.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#ifdef HAVE_SDL
#include <SDL/SDL.h>
#endif
#include <stdarg.h>
#include <libhash/hash.h>
#include <libvje/vje.h>
#include <libvjmem/vjmem.h>
#include <libvjmsg/vj-common.h>
#include <veejay/vj-lib.h>
#include <veejay/vj-perform.h>
#include <veejay/libveejay.h>
#include <libel/vj-avcodec.h>
#include <libsamplerec/samplerecord.h>
#include <utils/mpegconsts.h>
#include <utils/mpegtimecode.h>
#include <veejay/vims.h>
#include <veejay/vj-event.h>
#include <libstream/vj-tag.h>
#include <veejay/vj-plugin.h>

/* Highest possible SDL Key identifier */
#define MAX_SDL_KEY	(3 * SDLK_LAST) + 1  
#define MSG_MIN_LEN	  4 /* stripped ';' */



static int _last_known_num_args = 0;
static hash_t *BundleHash;

static int vj_event_valid_mode(int mode) {
	switch(mode) {
	 case VJ_PLAYBACK_MODE_CLIP:
	 case VJ_PLAYBACK_MODE_TAG:
	 case VJ_PLAYBACK_MODE_PLAIN:
	 return 1;
	}

	return 0;
}

/* define the function pointer to any event */
typedef void (*vj_event)(void *ptr, const char format[], va_list ap);

void vj_event_create_effect_bundle(veejay_t * v,char *buf, int key_id, int key_mod );

/* struct for runtime initialization of event handlers */
typedef struct {
	int list_id;			// VIMS id
	vj_event act;			// function pointer
} vj_events;

static	vj_events	net_list[VIMS_MAX];

#ifdef HAVE_SDL
typedef struct
{
	vj_events	vims;
	int		key_symbol;
	int		key_mod;
	char		*arguments;
} vj_keyboard_event;

static vj_keyboard_event keyboard_events[MAX_SDL_KEY];
#endif

static int _recorder_format = ENCODER_YUV420;



#define SEND_BUF 125000
static char _print_buf[SEND_BUF];
static char _s_print_buf[SEND_BUF];

// forward decl
int vj_event_get_video_format(void)
{
	return _recorder_format;
}

enum {
	VJ_ERROR_NONE=0,	
	VJ_ERROR_MODE=1,
	VJ_ERROR_EXISTS=2,
	VJ_ERROR_VIMS=3,
	VJ_ERROR_DIMEN=4,
	VJ_ERROR_MEM=5,
	VJ_ERROR_INVALID_MODE = 6,
};

#ifdef HAVE_SDL
#define	VIMS_MOD_SHIFT	3
#define VIMS_MOD_NONE	0
#define VIMS_MOD_CTRL	2
#define VIMS_MOD_ALT	1

static struct {					/* hardcoded keyboard layout (the default keys) */
	int event_id;			
	int key_sym;			
	int key_mod;
	const char *value;
} vj_event_default_sdl_keys[] = {

	{ 0,0,0,NULL },
	{ VIMS_EFFECT_SET_BG,			SDLK_b,		VIMS_MOD_ALT,	NULL	},
	{ VIMS_VIDEO_PLAY_FORWARD,		SDLK_KP6,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_PLAY_BACKWARD,		SDLK_KP4, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_PLAY_STOP,			SDLK_KP5, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_SKIP_FRAME,		SDLK_KP9, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_PREV_FRAME,		SDLK_KP7, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_SKIP_SECOND,		SDLK_KP8, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_PREV_SECOND,		SDLK_KP2, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_GOTO_START,		SDLK_KP1, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_GOTO_END,			SDLK_KP3, 	VIMS_MOD_NONE,	NULL	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_a,		VIMS_MOD_NONE,	"1"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_s,		VIMS_MOD_NONE,	"2"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_d,		VIMS_MOD_NONE,	"3"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_f,		VIMS_MOD_NONE,	"4"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_g,		VIMS_MOD_NONE,	"5"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_h,		VIMS_MOD_NONE,	"6"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_j,		VIMS_MOD_NONE,	"7"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_k,		VIMS_MOD_NONE,	"8"	},
	{ VIMS_VIDEO_SET_SPEED,			SDLK_l,		VIMS_MOD_NONE,	"9"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_a,		VIMS_MOD_ALT,	"1"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_s,		VIMS_MOD_ALT,	"2"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_d,		VIMS_MOD_ALT,	"3"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_e,		VIMS_MOD_ALT,	"4"	},	
	{ VIMS_VIDEO_SET_SLOW,			SDLK_f,		VIMS_MOD_ALT,	"5"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_g,		VIMS_MOD_ALT,	"6"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_h,		VIMS_MOD_ALT,	"7"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_j,		VIMS_MOD_ALT,	"8"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_k,		VIMS_MOD_ALT,	"9"	},
	{ VIMS_VIDEO_SET_SLOW,			SDLK_l,		VIMS_MOD_ALT,	"10"	},
#ifdef HAVE_SDL
	{ VIMS_FULLSCREEN,			SDLK_f,		VIMS_MOD_CTRL,	NULL	},
#endif
	{ VIMS_CHAIN_ENTRY_DOWN,		SDLK_KP_MINUS,	VIMS_MOD_NONE,	"1"	},
	{ VIMS_CHAIN_ENTRY_UP,			SDLK_KP_PLUS,	VIMS_MOD_NONE,	"1"	},
	{ VIMS_CHAIN_ENTRY_CHANNEL_INC,		SDLK_EQUALS,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_CHAIN_ENTRY_CHANNEL_DEC,		SDLK_MINUS,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_CHAIN_ENTRY_SOURCE_TOGGLE,	SDLK_SLASH,	VIMS_MOD_NONE,	NULL	}, // stream/clip
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_PAGEUP,	VIMS_MOD_NONE,	"0 1"	},
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_KP_PERIOD,	VIMS_MOD_NONE,	"1 1"	},
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_PERIOD,	VIMS_MOD_NONE,	"2 1"	},
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_w,		VIMS_MOD_NONE,	"3 1"	},
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_r,		VIMS_MOD_NONE,	"4 1"	},
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_y,		VIMS_MOD_NONE,	"5 1"	},
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_i,		VIMS_MOD_NONE,	"6 1"	},
	{ VIMS_CHAIN_ENTRY_INC_ARG,		SDLK_p,		VIMS_MOD_NONE,	"7 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_PAGEDOWN,	VIMS_MOD_NONE,	"0 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_KP0,	VIMS_MOD_NONE,	"1 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_COMMA,	VIMS_MOD_NONE,	"2 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_q,		VIMS_MOD_NONE,	"3 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_e,		VIMS_MOD_NONE,	"4 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_t,		VIMS_MOD_NONE,	"5 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_u,		VIMS_MOD_NONE,	"6 1"	},
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		SDLK_o,		VIMS_MOD_NONE,	"7 1"	},
	{ VIMS_SELECT_BANK,			SDLK_1,		VIMS_MOD_NONE,	"1"	},
	{ VIMS_SELECT_BANK,			SDLK_2,		VIMS_MOD_NONE,	"2"	},
	{ VIMS_SELECT_BANK,			SDLK_3,		VIMS_MOD_NONE,	"3"	},
	{ VIMS_SELECT_BANK,			SDLK_4,		VIMS_MOD_NONE,	"4"	},
	{ VIMS_SELECT_BANK,			SDLK_5,		VIMS_MOD_NONE,	"5"	},
	{ VIMS_SELECT_BANK,			SDLK_6,		VIMS_MOD_NONE,	"6"	},
	{ VIMS_SELECT_BANK,			SDLK_7,		VIMS_MOD_NONE,	"7"	},
	{ VIMS_SELECT_BANK,			SDLK_8,		VIMS_MOD_NONE,	"8"	},
	{ VIMS_SELECT_BANK,			SDLK_9,		VIMS_MOD_NONE,	"9"	},
	{ VIMS_SELECT_ID,			SDLK_F1,	VIMS_MOD_NONE,	"1"	},
	{ VIMS_SELECT_ID,			SDLK_F2,	VIMS_MOD_NONE,	"2"	},
	{ VIMS_SELECT_ID,			SDLK_F3,	VIMS_MOD_NONE,	"3"	},
	{ VIMS_SELECT_ID,			SDLK_F4,	VIMS_MOD_NONE,	"4"	},
	{ VIMS_SELECT_ID,			SDLK_F5,	VIMS_MOD_NONE,	"5"	},
	{ VIMS_SELECT_ID,			SDLK_F6,	VIMS_MOD_NONE,	"6"	},
	{ VIMS_SELECT_ID,			SDLK_F7,	VIMS_MOD_NONE,	"7"	},
	{ VIMS_SELECT_ID,			SDLK_F8,	VIMS_MOD_NONE,	"8"	},
	{ VIMS_SELECT_ID,			SDLK_F9,	VIMS_MOD_NONE,	"9"	},
	{ VIMS_SELECT_ID,			SDLK_F10,	VIMS_MOD_NONE,	"10"	},
	{ VIMS_SELECT_ID,			SDLK_F11, 	VIMS_MOD_NONE,	"11"	},
	{ VIMS_SELECT_ID,			SDLK_F12,	VIMS_MOD_NONE,	"12"	},
	{ VIMS_SET_PLAIN_MODE,			SDLK_KP_DIVIDE,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_REC_AUTO_START,			SDLK_e,		VIMS_MOD_CTRL,	"100"	},
	{ VIMS_REC_STOP,			SDLK_t,		VIMS_MOD_CTRL,	NULL	},
	{ VIMS_REC_START,			SDLK_r,		VIMS_MOD_CTRL,	NULL	},
	{ VIMS_CHAIN_TOGGLE,			SDLK_END,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_CHAIN_ENTRY_SET_STATE,		SDLK_END,	VIMS_MOD_ALT,	NULL	},	
	{ VIMS_CHAIN_ENTRY_CLEAR,		SDLK_DELETE,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_FXLIST_INC,			SDLK_UP,	VIMS_MOD_NONE,	"1"	},
	{ VIMS_FXLIST_DEC,			SDLK_DOWN,	VIMS_MOD_NONE,	"1"	},
	{ VIMS_FXLIST_ADD,			SDLK_RETURN,	VIMS_MOD_NONE,	"NULL"	},
	{ VIMS_SET_CLIP_START,			SDLK_LEFTBRACKET,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_SET_CLIP_END,			SDLK_RIGHTBRACKET,	VIMS_MOD_NONE,	NULL	},
	{ VIMS_CLIP_SET_MARKER_START,		SDLK_LEFTBRACKET,	VIMS_MOD_ALT,	NULL	},
	{ VIMS_CLIP_SET_MARKER_END,		SDLK_RIGHTBRACKET,	VIMS_MOD_ALT,	NULL	},
	{ VIMS_CLIP_TOGGLE_LOOP,		SDLK_KP_MULTIPLY,	VIMS_MOD_NONE,NULL	},
	{ VIMS_SWITCH_CLIP_STREAM,		SDLK_ESCAPE,		VIMS_MOD_NONE, NULL	},
	{ VIMS_PRINT_INFO,			SDLK_HOME,		VIMS_MOD_NONE, NULL	},
	{ VIMS_CLIP_CLEAR_MARKER,		SDLK_BACKSPACE,		VIMS_MOD_NONE, NULL },
	{ 0,0,0,NULL },
};
#endif

#define VIMS_REQUIRE_ALL_PARAMS (1<<0)			/* all params needed */
#define VIMS_DONT_PARSE_PARAMS (1<<1)		/* dont parse arguments */
#define VIMS_LONG_PARAMS (1<<3)				/* long string arguments (bundle, plugin) */
#define VIMS_ALLOW_ANY (1<<4)				/* use defaults when optional arguments are not given */			
/* the main event list */
static struct {

	const int event_id;			// VIMS id 
	const char *name;			// english text description 
	void (*function)(void *ptr, const char format[], va_list ap);
	const int num_params;			// number of arguments 
	const char *format;
	const int args[2];
	const int flags;   
} vj_event_list[] = {
	{ 0,NULL,0,0,NULL, {0,0} },
	{ VIMS_VIDEO_PLAY_FORWARD,		 "Video: play forward",
		vj_event_play_forward,		0 ,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_PLAY_BACKWARD,		 "Video: play backward",
		vj_event_play_reverse,		0 ,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_PLAY_STOP,			 "Video: play/stop",
		vj_event_play_stop,		0 ,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_SKIP_FRAME,		 "Video: skip frame forward",
		vj_event_inc_frame,		1 ,	"%d",		{1,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_PREV_FRAME,		 "Video: skip frame backward",
		vj_event_dec_frame,		1 ,	"%d",		{1,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_SKIP_SECOND,		 "Video: skip one second forward",
		vj_event_next_second,		1 ,	"%d",		{1,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_PREV_SECOND,		 "Video: skip one second backward",
		vj_event_prev_second,		1 ,	"%d",		{1,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_GOTO_START,		 "Video: go to starting position",
		vj_event_goto_start,		0 ,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_GOTO_END,			 "Video: go to ending position",
		vj_event_goto_end,		0 ,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_SET_SPEED,			 "Video: change speed to N",
		vj_event_play_speed,		1 , 	"%d",		{1,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_SET_SLOW,			 "Video: duplicate every frame N times",
		vj_event_play_slow,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_VIDEO_SET_FRAME,			 "Video: set frame",
		vj_event_set_frame,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS },

	{ VIMS_CHAIN_ENTRY_UP,	 		 "ChainEntry: select next entry",
		vj_event_entry_up,    		1,	"%d",		{1,0}, VIMS_ALLOW_ANY },
	{ VIMS_CHAIN_ENTRY_DOWN,		 "ChainEntry: select previous entry",
		vj_event_entry_down,  		1,	"%d",		{1,0}, VIMS_ALLOW_ANY },
	{ VIMS_CHAIN_ENTRY_CHANNEL_INC,		 "ChainEntry: increment mixing channel with N",
		vj_event_chain_entry_channel_inc,1,	"%d",		{1,0}, VIMS_ALLOW_ANY }, /* uses default value if none given */
	{ VIMS_CHAIN_ENTRY_CHANNEL_DEC,		 "ChainEntry: decrement mixing channel with N",
		vj_event_chain_entry_channel_dec,1,	"%d",		{1,0}, VIMS_ALLOW_ANY }, /* uses default value if none given */
	{ VIMS_CHAIN_ENTRY_SOURCE_TOGGLE,		 "ChainEntry: set mixing source of entry N1 to source type N2",
		vj_event_chain_entry_src_toggle,2,	"%d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_CHAIN_ENTRY_INC_ARG,		 "ChainEntry: increment current value N2 of parameter N1",
		vj_event_chain_arg_inc,		2,	"%d %d",	{0,1},	VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_CHAIN_ENTRY_DEC_ARG,		 "Chainentry: decrement current value N2 of parameter N1",
		vj_event_chain_arg_inc,  	2,	"%d %d",	{9,-1}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_CHAIN_ENTRY_SET_STATE,		 "ChainEntry: toggle effect on/off current entry",
		vj_event_chain_entry_video_toggle,0,	NULL,		{0,0}, VIMS_ALLOW_ANY },

	{ VIMS_CHAIN_TOGGLE,			 "Chain: Toggle effect chain on/off",
		vj_event_chain_toggle,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY },

	{ VIMS_SET_CLIP_START,			 "Clip: set starting frame at current position",
		vj_event_clip_start,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY   },
	{ VIMS_SET_CLIP_END,			 "Clip: set ending frame at current position and create new clip",
		vj_event_clip_end,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY   },
	{ VIMS_CLIP_SET_MARKER_START,		 "Clip: set clip N1 marker start to N2",
		vj_event_clip_set_marker_start,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_SET_MARKER_END,		 "Clip: set clip N1 marker end to N2",
		vj_event_clip_set_marker_end,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},

	{ VIMS_FXLIST_INC,		 	"EffectList: select next effect",
		 vj_event_effect_inc,		1,	"%d",		{1,0}, VIMS_ALLOW_ANY	},
	{ VIMS_FXLIST_DEC,	   	 	"EffectList: select previous effect",
		vj_event_effect_dec,		1,	"%d",		{1,0}, VIMS_ALLOW_ANY	},
	{ VIMS_FXLIST_ADD,		 	"EffectList: add selected effect on current chain enry",
		 vj_event_effect_add,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_SELECT_BANK,			 "Set clip/stream bank N",
		vj_event_select_bank,		1,	"%d",		{1,0}, VIMS_ALLOW_ANY	},
	{ VIMS_SELECT_ID,			 "Set clip/stream N of current bank",
		vj_event_select_id,		1,	"%d",		{2,0}, VIMS_ALLOW_ANY	},
	{ VIMS_CLIP_TOGGLE_LOOP,		 "Toggle looptype to normal or pingpong",
		vj_event_clip_set_loop_type,	2,	"%d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_RECORD_DATAFORMAT,		 "Set dataformat for stream/clip record",
		vj_event_tag_set_format,	1,	"%s",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	 },
	{ VIMS_REC_AUTO_START,			 "Record clip/stream and auto play after recording",
		 vj_event_misc_start_rec_auto,	0,	NULL,		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_REC_START,			 "Record clip/stream start",
		vj_event_misc_start_rec,	0,	NULL,		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_REC_STOP,			 "Record clip/stream stop",
		vj_event_misc_stop_rec,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_CLIP_NEW,			 "Clip: create new",
		vj_event_clip_new,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_PRINT_INFO,			 "Info: output clip/stream details",
		vj_event_print_info,		1,	"%d",		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_SET_PLAIN_MODE,			 "Video: set plain video mode",
		vj_event_set_play_mode,		1,	"%d",		{2,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_SET_LOOPTYPE,		 "Clip: set looptype",
		vj_event_clip_set_loop_type, 	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS  }, 
	{ VIMS_CLIP_SET_SPEED,			 "Clip: set speed",
		vj_event_clip_set_speed,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_SET_DESCRIPTION,		"Clip: set description",
		vj_event_clip_set_descr,	2,	"%d %s",	{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_SET_END,			"Clip: set ending position",
		vj_event_clip_set_end, 		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_SET_START,			"Clip: set starting position",
		vj_event_clip_set_start,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_SET_DUP,			"Clip: set frame duplication",
		vj_event_clip_set_dup,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_SET_MARKER_START,		"Clip: set marker starting position",
		vj_event_clip_set_marker_start,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_SET_MARKER_END,		"Clip: set marker ending position",
		vj_event_clip_set_marker_end,  	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_SET_MARKER,			"Clip: set marker starting and ending position",
		vj_event_clip_set_marker,	3,	"%d %d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_CLEAR_MARKER,		"Clip: clear marker",
		vj_event_clip_set_marker_clear,	1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
#ifdef HAVE_XML2
	{ VIMS_CLIP_LOAD_CLIPLIST,		"Clip: load clips from file",
		vj_event_clip_load_list,	1,	"%s",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_SAVE_CLIPLIST,		"Clip: save clips to file",
		vj_event_clip_save_list,	1,	"%s",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
#endif
	{ VIMS_CLIP_CHAIN_ENABLE,		"Clip: enable effect chain",
		vj_event_clip_chain_enable,	1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_CHAIN_DISABLE,		"Clip: disable effect chain",
		vj_event_clip_chain_disable,	1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_REC_START,			"Clip: record this clip to new",
		vj_event_clip_rec_start,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_REC_STOP,			"Clip: stop recording this clip",
		vj_event_clip_rec_stop,	  	0,	NULL,		{0,0}, VIMS_ALLOW_ANY  },
	{ VIMS_CLIP_RENDERLIST,			"Clip: send renderlist",
		vj_event_send_clip_history_list,1,	"%d",		{0,0}, VIMS_ALLOW_ANY	}, 
	{ VIMS_CLIP_RENDER_TO,			"Clip: render to inlined entry",
		vj_event_clip_ren_start,	2,	"%d %d",    	{0,1}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_RENDER_MOVE,		"Clip: move rendered entry to edl",
		vj_event_clip_move_render,	2,	"%d %d",	{0, 0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_RENDER_SELECT,		"Clip: select and play inlined entry",
		vj_event_clip_sel_render,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_DEL,			"Clip: delete",
		vj_event_clip_del,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_DEL_ALL,			"Clip: delete all",
		vj_event_clip_clear_all,	0,    	NULL,		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_CLIP_COPY,			"Clip: copy clip <num>",
		vj_event_clip_copy,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_CLIP_SELECT,			"Clip: select and play a clip",
		vj_event_clip_select,		1,	"%d",		{0,0}, VIMS_ALLOW_ANY	}, // use default 
	{ VIMS_STREAM_SELECT,			"Stream: select and play a stream",
		vj_event_tag_select,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_STREAM_DELETE,			"Stream: delete",
		vj_event_tag_del,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},		
#ifdef HAVE_V4L
	{ VIMS_STREAM_NEW_V4L,			"Stream: open video4linux device (hw)",
		vj_event_tag_new_v4l,		2,	"%d %d",	{0,1}, VIMS_REQUIRE_ALL_PARAMS	},	
#endif
#ifdef SUPPORT_READ_DV2
	{ VIMS_STREAM_NEW_DV1394,		"Stream: open dv1394 device <channel>",
		vj_event_tag_new_dv1394,	 1, 	"%d",		{63,0}, VIMS_ALLOW_ANY	},
#endif
	{ VIMS_STREAM_NEW_Y4M,			"Stream: open y4m stream by name (file)",
		vj_event_tag_new_y4m,		 1,	"%s",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	}, 
	{ VIMS_STREAM_NEW_COLOR,		"Stream: new color stream by RGB color",
		vj_event_tag_new_color,		 3,     "%d %d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_RGB_PARAMETER_TYPE,		"Effect: RGB parameter type",
		vj_event_set_rgb_parameter_type, 1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	}, 
	{ VIMS_STREAM_COLOR,			"Stream: set color of a solid stream",
		vj_event_set_stream_color,	 4,	"%d %d %d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_STREAM_NEW_UNICAST,		"Stream: open network stream ",
		vj_event_tag_new_net, 		 2, 	"%d %s", 	{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_STREAM_NEW_MCAST,		"Stream: open multicast stream",
		vj_event_tag_new_mcast, 	 2, 	"%d %s", 	{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS },	
	{ VIMS_STREAM_NEW_AVFORMAT,		"Stream: open file as stream with FFmpeg",
		vj_event_tag_new_avformat,	 1,	"%s",		{0,0}, VIMS_LONG_PARAMS	| VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_STREAM_OFFLINE_REC_START,	"Stream: start record from an invisible stream",
		vj_event_tag_rec_offline_start,  3,	"%d %d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	}, 
	{ VIMS_STREAM_OFFLINE_REC_STOP,		"Stream: stop record from an invisible stream",
		vj_event_tag_rec_offline_stop,   0,	NULL,		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_STREAM_SET_DESCRIPTION,		"Stream: set title ",
		vj_event_tag_set_descr,		 2,	"%d %s",	{0,0}, VIMS_LONG_PARAMS  | VIMS_REQUIRE_ALL_PARAMS},
	{ VIMS_STREAM_REC_START,		"Stream: start recording from stream",
		vj_event_tag_rec_start,	 	 2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_STREAM_REC_STOP,			"Stream: stop recording from stream",
		vj_event_tag_rec_stop,		 0,	NULL,		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_STREAM_CHAIN_ENABLE,		"Stream: enable effect chain",
		vj_event_tag_chain_enable,	 1,	"%d",		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_STREAM_CHAIN_DISABLE,		"Stream: disable effect chain",
		vj_event_tag_chain_disable,	 1,	"%d",		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_CHAIN_ENTRY_SET_EFFECT,		"ChainEntry: set effect with defaults",
		vj_event_chain_entry_set,	3,	"%d %d %d" ,	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_SET_PRESET,		"ChainEntry: preset effect",	
		 vj_event_chain_entry_preset,   15,	"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_CHAIN_ENTRY_SET_ARG_VAL,	"ChainEntry: set parameter x value y",
		vj_event_chain_entry_set_arg_val,4,	"%d %d %d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_SET_VIDEO_ON,	"ChainEntry: set video on entry on",
		vj_event_chain_entry_enable_video,2,	"%d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_SET_VIDEO_OFF,	"ChainEntry: set video on entry off",
		vj_event_chain_entry_disable_video,2,	"%d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_SET_DEFAULTS,	"ChainEntry: set effect on entry to defaults",
		vj_event_chain_entry_set_defaults,2,	"%d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_SET_CHANNEL,	"ChainEntry: set mixing channel on entry to",
		vj_event_chain_entry_channel,	3,	"%d %d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_SET_SOURCE,		"ChainEntry: set mixing source on entry to",
		vj_event_chain_entry_source,	3,	"%d %d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_SET_SOURCE_CHANNEL,"ChainEntry: set mixing source and channel on",
		vj_event_chain_entry_srccha,	4,	"%d %d %d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENTRY_CLEAR,		"ChainEntry: clear entry",
		vj_event_chain_entry_del,	2,	"%d %d",	{0,-1}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CHAIN_ENABLE,			"Chain: enable chain",
		vj_event_chain_enable,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_CHAIN_DISABLE,			"Chain: disable chain",
		vj_event_chain_disable,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_CHAIN_CLEAR,			"Chain: clear chain",
		vj_event_chain_clear,		1,	"%d",		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_CHAIN_FADE_IN,			"Chain: fade in",
		vj_event_chain_fade_in,		2,	"%d %d",	{0,0}, VIMS_ALLOW_ANY	}, 
	{ VIMS_CHAIN_FADE_OUT,			"Chain: fade out",
		vj_event_chain_fade_out,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS		},
	{ VIMS_CHAIN_MANUAL_FADE,		"Chain: set opacity",
		vj_event_manual_chain_fade,	2, 	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS 	},
	{ VIMS_CHAIN_SET_ENTRY,		"Chain: select entry ",
		vj_event_chain_entry_select,	1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_OUTPUT_Y4M_START,		"Output: Yuv4Mpeg start writing to file",
		vj_event_output_y4m_start,	1,	"%s",		{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_OUTPUT_Y4M_STOP,		"Output: Yuv4Mpeg stop writing to file",
		vj_event_output_y4m_stop,	0,	NULL,		{0,0}, VIMS_ALLOW_ANY },
#ifdef HAVE_SDL
	{ VIMS_RESIZE_SDL_SCREEN,		"Output: Re(initialize) SDL video screen",
		vj_event_set_screen_size,	4,	"%d %d %d %d", 	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
#endif
	{ VIMS_SET_PLAY_MODE,			"Playback: switch playmode clip/tag/plain",
		vj_event_set_play_mode,		1,	"%d",		{2,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_SET_MODE_AND_GO,		"Playback: set playmode (and fire clip/tag)",
		vj_event_set_play_mode_go,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_SWITCH_CLIP_STREAM,		"Playback: switch between clips/tags",
		vj_event_switch_clip_tag,	0,	NULL,		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_AUDIO_DISABLE,			"Playback: disable audio",
		vj_event_disable_audio,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY   },
	{ VIMS_AUDIO_ENABLE,			"Playback: enable audio",
		vj_event_enable_audio,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY	},
	{ VIMS_EDITLIST_PASTE_AT,		"EditList: Paste frames from buffer at frame",
		vj_event_el_paste_at,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_EDITLIST_CUT,			"EditList: Cut frames n1-n2 to buffer",
		vj_event_el_cut,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS  },
	{ VIMS_EDITLIST_COPY,			"EditList: Copy frames n1-n2 to buffer",
		vj_event_el_copy,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_EDITLIST_CROP,			"EditList: Crop frames n1-n2",
		vj_event_el_crop,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS   },
	{ VIMS_EDITLIST_DEL,			"EditList: Del frames n1-n2",
		vj_event_el_del,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	}, 
	{ VIMS_EDITLIST_SAVE,			"EditList: save EditList to new file",
		vj_event_el_save_editlist,	3,	"%d %d %s",	{0,0}, VIMS_LONG_PARAMS  | VIMS_ALLOW_ANY },
	{ VIMS_EDITLIST_LOAD,			"EditList: load EditList into veejay",
		vj_event_el_load_editlist,	1,	"%s",		{0,0}, VIMS_LONG_PARAMS	| VIMS_REQUIRE_ALL_PARAMS},
	{ VIMS_EDITLIST_ADD,			"EditList: add video file to editlist",
		vj_event_el_add_video,		1,	"%s",		{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_EDITLIST_ADD_CLIP,		"EditList: add video file to editlist as clip",
		vj_event_el_add_video_clip,	1,	"%s",		{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_STREAM_LIST,			"Stream: send list of all streams",
		vj_event_send_tag_list,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_CLIP_LIST,			"Clip: send list of Clips",
		vj_event_send_clip_list,	1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
	{ VIMS_EDITLIST_LIST,			"EditList: send list of all files",
		vj_event_send_editlist,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY   },
	{ VIMS_BUNDLE,				"Bundle: execute collection of messages",
		vj_event_do_bundled_msg,	1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
#ifdef HAVE_XML2
	{ VIMS_BUNDLE_FILE,			"Veejay load configuartion file",
		vj_event_read_file,		1,	"%s",		{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS	},
#endif
	{ VIMS_BUNDLE_DEL,			"Bundle: delete a bundle of messages",
		vj_event_bundled_msg_del,	1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS	},
#ifdef HAVE_XML2
	{ VIMS_BUNDLE_SAVE,			"Veejay save configuration file",
		vj_event_write_actionfile,	2,	"%d %s",	{0,0}, VIMS_LONG_PARAMS | VIMS_ALLOW_ANY  },
#endif
	{ VIMS_BUNDLE_LIST,			"Bundle: get all contents",
		vj_event_send_bundles,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_VIMS_LIST,			"VIMS: send whole event list",
		vj_event_send_vimslist,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_BUNDLE_ADD,			"Bundle: add a new bundle to event system",
		vj_event_bundled_msg_add,	2,	"%d %s",	{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS		},
	{ VIMS_BUNDLE_CAPTURE,			"Bundle: capture effect chain",
		vj_event_quick_bundle,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_CHAIN_LIST,			"Chain: get all contents",
		vj_event_send_chain_list,	1,	"%d",		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_CHAIN_GET_ENTRY,			"Chain: get entry contents",
		vj_event_send_chain_entry,	2,	"%d %d",	{0,-1}, VIMS_ALLOW_ANY		},		
	{ VIMS_EFFECT_LIST,			"EffectList: list all effects",
		vj_event_send_effect_list,	0,	NULL,		{0,0}, VIMS_ALLOW_ANY		},
	{ VIMS_VIDEO_INFORMATION,		"Video: send properties",
		vj_event_send_video_information,0,	NULL,		{0,0}, VIMS_ALLOW_ANY	},
#ifdef HAVE_SDL
	{ VIMS_BUNDLE_ATTACH_KEY,		"Attach/Detach a Key to VIMS event",
		vj_event_attach_detach_key,	4,	"%d %d %d %s",	{0,0}, VIMS_ALLOW_ANY 	},
#endif
#ifdef HAVE_JPEG
	{ VIMS_SCREENSHOT,			"Various: Save frame to jpeg",
		vj_event_screenshot,		1,	"%s",		{0,0}, VIMS_LONG_PARAMS | VIMS_ALLOW_ANY  },
#endif
	{ VIMS_CHAIN_TOGGLE_ALL,		"Toggle Effect Chain on all clips or streams",
		vj_event_all_clips_chain_toggle,1,	"%d",		{0,0} , VIMS_REQUIRE_ALL_PARAMS  },
	{ VIMS_CLIP_UPDATE,			"Clip: Update starting and ending position by offset",
		vj_event_clip_rel_start,	3,	"%d %d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS	},	 
#ifdef HAVE_V4L
	{ VIMS_STREAM_SET_BRIGHTNESS,		"Video4Linux: set v4l brightness value",
		vj_event_v4l_set_brightness,	2,	"%d %d",	{0,0}, 	VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_STREAM_SET_CONTRAST,		"Video4Linux: set v4l contrast value",
		vj_event_v4l_set_contrast,	2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_STREAM_SET_HUE,			"Video4Linux: set v4l hue value",
		vj_event_v4l_set_hue,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_STREAM_SET_COLOR,			"Video4Linux: set v4l color value",
		vj_event_v4l_set_color,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_STREAM_SET_WHITE,			"Video4Linux: set v4l white value",
		vj_event_v4l_set_white,		2,	"%d %d",	{0,0}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_STREAM_GET_V4L,			"Video4Linux: get properties",
		vj_event_v4l_get_info,		2,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS },
#endif
	{ VIMS_EFFECT_SET_BG,			"Effect: set background (if supported)",
		vj_event_effect_set_bg,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_QUIT,				"Quit veejay",
		vj_event_quit,			0, 	NULL, 		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_SET_VOLUME,			"Veejay set audio volume",
		vj_event_set_volume,		1,	"%d",		{0,0}, VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_SUSPEND,				"Suspend veejay",
		vj_event_suspend,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_DEBUG_LEVEL,			"Toggle more/less debugging information",
		vj_event_debug_level,		0,	 NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_BEZERK,				"Toggle bezerk mode",
		vj_event_bezerk,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_SAMPLE_MODE,			"Toggle between box or triangle filter for 2x2 -> 1x1 sampling",
		vj_event_sample_mode, 		0, 	NULL, 		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_CMD_PLUGIN,			"Send a command to the plugin",
		vj_event_plugin_command,	1, 	"%s", 		{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS },
	{ VIMS_LOAD_PLUGIN,			"Load a plugin from disk",
		vj_event_load_plugin, 		1, 	"%s", 		{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS},
	{ VIMS_UNLOAD_PLUGIN,			"Unload a plugin from disk",
		vj_event_unload_plugin, 	1, 	"%s", 		{0,0}, VIMS_LONG_PARAMS | VIMS_REQUIRE_ALL_PARAMS},

	{ VIMS_VIDEO_MCAST_START,		"Start mcast frame sender",
		vj_event_mcast_start,		0,	NULL,		{0,0}, VIMS_ALLOW_ANY },
	{ VIMS_VIDEO_MCAST_STOP,		"Stop mcast frame sender",
		vj_event_mcast_stop,		0,	NULL,		{0,0},	VIMS_ALLOW_ANY },
	{ VIMS_GET_FRAME,			"Send a frame to the client",
		vj_event_send_frame, 		0,	NULL, 		{0,0}, VIMS_ALLOW_ANY },
#ifdef HAVE_SDL
	{ VIMS_FULLSCREEN,			"Fullscreen video toggle",
		vj_event_fullscreen,		1, 	"%d" , 		{0,0}, VIMS_ALLOW_ANY},
#endif
	{ 0,NULL,0,0,NULL,{0,0}},
};


#define FORMAT_MSG(dst,str) sprintf(dst,"%03d%s",strlen(str),str)
#define APPEND_MSG(dst,str) strncat(dst,str,strlen(str))
#define SEND_MSG_DEBUG(v,str) veejay_msg(VEEJAY_MSG_INFO, "[%d][%s]",strlen(str),str)
#define SEND_MSG(v,str)\
{\
vj_server_send(v->vjs[0], v->uc->current_link, str, strlen(str));\
}
#define RAW_SEND_MSG(v,str,len)\
{\
vj_server_send(v->vjs[0],v->uc->current_link, str, len );\
}	

/* some macros for commonly used checks */

#define CLIP_PLAYING(v) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_CLIP) )
#define STREAM_PLAYING(v) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_TAG) )
#define PLAIN_PLAYING(v) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_PLAIN) )

#define p_no_clip(a) {  veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exist",a); }
#define p_no_tag(a)    {  veejay_msg(VEEJAY_MSG_ERROR, "Stream %d does not exist",a); }
#define p_invalid_mode() {  veejay_msg(VEEJAY_MSG_DEBUG, "Invalid playback mode for this action"); }
#define v_chi(v) ( (v < 0  || v >= CLIP_MAX_EFFECTS ) ) 

/* P_A: Parse Arguments. This macro is used in many functions */
#define P_A(a,b,c,d)\
{\
int __z = 0;\
if(a!=NULL){\
unsigned int __rp;\
unsigned int __rplen = (sizeof(a) / sizeof(int) );\
for(__rp = 0; __rp < __rplen; __rp++) a[__rp] = 0;\
}\
while(*c) { \
if(__z > _last_known_num_args )  break; \
switch(*c++) {\
 case 's': sprintf( b,"%s",va_arg(d,char*) ); __z++ ;\
 break;\
 case 'd': a[__z] = *( va_arg(d, int*)); __z++ ;\
 break; }\
 }\
}

/* P_A16: Parse 16 integer arguments. This macro is used in 1 function */
#define P_A16(a,c,d)\
{\
int __z = 0;\
while(*c) { \
if(__z > 15 )  break; \
switch(*c++) { case 'd': a[__z] = va_arg(d, int); __z++ ; break; }\
}}\


#define DUMP_ARG(a)\
if(sizeof(a)>0){\
int __l = sizeof(a)/sizeof(int);\
int __i; for(__i=0; __i < __l; __i++) veejay_msg(VEEJAY_MSG_DEBUG,"[%02d]=[%06d], ",__i,a[__i]);}\
else { veejay_msg(VEEJAY_MSG_DEBUG,"arg has size of 0x0");}


#define CLAMPVAL(a) { if(a<0)a=0; else if(a >255) a =255; }


static inline hash_val_t int_bundle_hash(const void *key)
{
	return (hash_val_t) key;
}

static inline int int_bundle_compare(const void *key1,const void *key2)
{
	return ((int)key1 < (int) key2 ? -1 : 
		((int) key1 > (int) key2 ? +1 : 0));
}

typedef struct {
	int event_id;
	int accelerator;
	int modifier;
	char *bundle;
} vj_msg_bundle;

/* forward declarations (former console clip/tag print info) */
void vj_event_print_plain_info(void *ptr, int x);
void vj_event_print_clip_info(veejay_t *v, int id); 
void vj_event_print_tag_info(veejay_t *v, int id); 
int vj_event_bundle_update( vj_msg_bundle *bundle, int bundle_id );
vj_msg_bundle *vj_event_bundle_get(int event_id);
int vj_event_bundle_exists(int event_id);
int vj_event_suggest_bundle_id(void);
int vj_event_load_bundles(char *bundle_file);
int vj_event_bundle_store( vj_msg_bundle *m );
int vj_event_bundle_del( int event_id );
vj_msg_bundle *vj_event_bundle_new(char *bundle_msg, int event_id);
void vj_event_trigger_function(void *ptr, vj_event f, int max_args, const char format[], ...); 
vj_event	vj_event_function_by_id(int id);
const int vj_event_get_id(int event_id);
const char  *vj_event_name_by_id(int id);
void  vj_event_parse_bundle(veejay_t *v, char *msg );
int	vj_has_video(veejay_t *v);
void vj_event_fire_net_event(veejay_t *v, int net_id, char *str_arg, int *args, int arglen);

void    vj_event_commit_bundle( veejay_t *v, int key_num, int key_mod);
#ifdef HAVE_SDL
static void vj_event_get_key( int event_id, int *key_id, int *key_mod );
void vj_event_single_fire(void *ptr , SDL_Event event, int pressed);
int vj_event_register_keyb_event(int event_id, int key_id, int key_mod, const char *args);
void vj_event_unregister_keyb_event(int key_id, int key_mod);
#endif

#ifdef HAVE_XML2
void    vj_event_format_xml_event( xmlNodePtr node, int event_id );
#endif
void	vj_event_init(void);

int	vj_has_video(veejay_t *v)
{
	if(v->edit_list->num_video_files <= 0 )
		return 0;
	return 1;
}

int vj_event_bundle_update( vj_msg_bundle *bundle, int bundle_id )
{
	if(bundle) {
		hnode_t *n = hnode_create(bundle);
		if(!n) return 0;
		hnode_put( n, (void*) bundle_id);
		hnode_destroy(n);
		return 1;
	}
	return 0;
}

vj_msg_bundle *vj_event_bundle_get(int event_id)
{
	vj_msg_bundle *m;
	hnode_t *n = hash_lookup(BundleHash, (void*) event_id);
	if(n) 
	{
		m = (vj_msg_bundle*) hnode_get(n);
		if(m)
		{
			return m;
		}
	}
	return NULL;
}

int vj_event_bundle_exists(int event_id)
{
	hnode_t *n = hash_lookup( BundleHash, (void*) event_id );
	if(!n)
		return 0;
	return ( vj_event_bundle_get(event_id) == NULL ? 0 : 1);
}

int vj_event_suggest_bundle_id(void)
{
	int i;
	for(i=VIMS_BUNDLE_START ; i < VIMS_BUNDLE_END; i++)
	{
		if ( vj_event_bundle_exists(i ) == 0 ) return i;
	}

	return -1;
}

int vj_event_bundle_store( vj_msg_bundle *m )
{
	hnode_t *n;
	if(!m) return 0;
	n = hnode_create(m);
	if(!n) return 0;
	if(!vj_event_bundle_exists(m->event_id))
	{
		hash_insert( BundleHash, n, (void*) m->event_id);
	}
	else
	{
		hnode_put( n, (void*) m->event_id);
		hnode_destroy( n );
	}

	// add bundle to VIMS list
	veejay_msg(VEEJAY_MSG_DEBUG,
		"Added Bundle VIMS %d to net_list", m->event_id );
 
	net_list[ m->event_id ].list_id = m->event_id;
	net_list[ m->event_id ].act = vj_event_none;


	return 1;
}
int vj_event_bundle_del( int event_id )
{
	hnode_t *n;
	vj_msg_bundle *m = vj_event_bundle_get( event_id );
	if(!m) return -1;

	n = hash_lookup( BundleHash, (void*) event_id );
	if(!n)
		return -1;

	net_list[ m->event_id ].list_id = 0;
	net_list[ m->event_id ].act = vj_event_none;

#ifdef HAVE_SDL
	vj_event_unregister_keyb_event( m->accelerator, m->modifier );
#endif	

	if( m->bundle )
		free(m->bundle);
	if(m)
		free(m);
	m = NULL;

	hash_delete( BundleHash, n );


	return 0;
}

vj_msg_bundle *vj_event_bundle_new(char *bundle_msg, int event_id)
{
	vj_msg_bundle *m;
	int len = 0;
	if(!bundle_msg || strlen(bundle_msg) < 1)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Doesnt make sense to store empty bundles in memory");
		return NULL;
	}	

	len = strlen(bundle_msg);
	m = (vj_msg_bundle*) malloc(sizeof(vj_msg_bundle));
	if(!m) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error allocating memory for bundled message");
		return NULL;
	}
	memset(m, 0, sizeof(m) );
	m->bundle = (char*) malloc(sizeof(char) * len+1);
	bzero(m->bundle, len+1);
	m->accelerator = 0;
	m->modifier = 0;
	if(!m->bundle)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error allocating memory for bundled message context");
		return NULL;
	}
	strncpy(m->bundle, bundle_msg, len);
	
	m->event_id = event_id;

	veejay_msg(VEEJAY_MSG_DEBUG, 
		"New VIMS Bundle %d [%s]",
			event_id, m->bundle );

	return m;
}


void vj_event_trigger_function(void *ptr, vj_event f, int max_args, const char *format, ...) 
{
	va_list ap;
	va_start(ap,format);
	f(ptr, format, ap);	
	va_end(ap);
}

vj_event vj_event_function_by_id(int id) 
{
	vj_event function;
	int i;
	for(i=1; vj_event_list[i].name; i++)
	{
		if( id == vj_event_list[i].event_id ) 	
		{
			function = vj_event_list[i].function;
			return function;
		}
	}
	function = vj_event_none;
	return function;
}

const int vj_event_get_id(int event_id)
{
	int i;
	if( event_id >= VIMS_BUNDLE_START && event_id < VIMS_BUNDLE_END )
		return vj_event_bundle_exists(event_id);

	for(i=1; vj_event_list[i].name != NULL; i++)
	{
		if( vj_event_list[i].event_id == event_id ) return i;
	}
	return 0;
}

const char  *vj_event_name_by_id(int id)
{
	int i;
	for(i=1; vj_event_list[i].name != NULL; i++)
	{
		if(id == vj_event_list[i].event_id)
		{
			return vj_event_list[i].name;
		}
	}
	return NULL;
}

/* error statistics struct */


int vj_event_parse_msg(veejay_t *v, char *msg);

/* parse a message received from network */
void vj_event_parse_bundle(veejay_t *v, char *msg )
{

	int num_msg = 0;
	int offset = 3;
	int i = 0;
	
	
	if ( msg[offset] == ':' )
	{
		int j = 0;
		offset += 1; /* skip ':' */
		if( sscanf(msg+offset, "%03d", &num_msg )<= 0 )
		{
			veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Invalid number of messages. Skipping message [%s] ",msg);
		}
		if ( num_msg <= 0 ) 
		{
			veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Invalid number of message given to execute. Skipping message [%s]",msg);
			return;
		}

		offset += 3;

		if ( msg[offset] != '{' )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) '{' expected. Skipping message [%s]",msg);
			return;
		}	

		offset+=1;	/* skip # */

		for( i = 1; i <= num_msg ; i ++ ) /* iterate through message bundle and invoke parse_msg */
		{				
			char atomic_msg[256];
			int found_end_of_msg = 0;
			int total_msg_len = strlen(msg);
			bzero(atomic_msg,256);
			while( (offset+j) < total_msg_len)
			{
				if(msg[offset+j] == '}')
				{
					return; /* dont care about semicolon here */
				}	
				else
				if(msg[offset+j] == ';')
				{
					found_end_of_msg = offset+j+1;
					strncpy(atomic_msg, msg+offset, (found_end_of_msg-offset));
					atomic_msg[ (found_end_of_msg-offset) ] ='\0';
					offset += j + 1;
					j = 0;
					vj_event_parse_msg( v, atomic_msg );
				}
				j++;
			}
		}
	}
}

void vj_event_dump()
{

	int i;
	for(i=0; i < VIMS_MAX; i++)
	{
	   if(net_list[i].list_id>0)
	   {	
	   int id = net_list[i].list_id;
	   //int id = i;
	   printf("\t%s\n", vj_event_list[id].name);
	   printf("\t%03d\t\t%s\n\n", i , vj_event_list[id].format);
	   }
	}
	vj_osc_dump();
}

void vj_event_print_range(int n1, int n2)
{
	int i;
	for(i=n1; i < n2; i++)
	{
		if(net_list[i].list_id > 0)
		{
			int id = net_list[i].list_id;
			if(vj_event_list[id].name != NULL )
			{
				veejay_msg(VEEJAY_MSG_INFO,"%03d\t\t%s\t\t%s",
					i, vj_event_list[id].format,vj_event_list[id].name);
			}
		}
	}
}


int vj_event_get_num_args(int net_id)
{
	int id = net_list[ net_id ].list_id;
	return (int) vj_event_list[id].num_params;	
}

typedef struct
{
	void *value;
} vims_arg_t; 

void vj_event_fire_net_event(veejay_t *v, int net_id, char *str_arg, int *args, int arglen)
{
	int id = net_list[ net_id ].list_id;
	
	int		argument_list[16];
	vims_arg_t	vims_arguments[16];

	if(id <= 0)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) VIMS %d is not known", net_id );
		return;
	}
	
	memset( argument_list, 0, 16 );
	memset( vims_arguments,0, 16 );

	if( vj_event_list[id].num_params <= 0 )
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) %s single fire %d", __FUNCTION__, net_id);
		_last_known_num_args = 0;
		vj_event_trigger_function( (void*) v, net_list[net_id].act,
			vj_event_list[id].num_params, vj_event_list[id].format,
			&(vj_event_list[id].args[0]), &(vj_event_list[id].args[1]));
		return;
	}
	else
	{
		// order arguments
		char *f = (char*) vj_event_list[id].format;
		int i;
		int fmt_offset = 1;
		int flags = vj_event_list[id].flags;
		if( arglen >  vj_event_list[id].num_params)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) %d too many arguments : %d (only %d)",
				net_id, arglen, vj_event_list[id].num_params );
			return;
		}

		for( i = 0; i < arglen; i ++ )
		{
			if(f[fmt_offset] == 'd' )
				vims_arguments[i].value = &(args[i]);
			if(f[fmt_offset] == 's' )
			{
				if( str_arg == NULL && (flags & VIMS_REQUIRE_ALL_PARAMS))
				{
					veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) %d requires string",
						net_id);
					return;
				}
				int len = strlen( str_arg );
				vims_arguments[i].value = strndup( str_arg, len );
			}	
			fmt_offset += 3;
		}
		if( arglen < vj_event_list[id].num_params)
		{
			if(flags & VIMS_ALLOW_ANY)
			{
				int n;
				veejay_msg(VEEJAY_MSG_WARNING, "(VIMS) %d uses default values", net_id);
				for(n = arglen; n < vj_event_list[id].num_params; n ++ )
				{
					if(n < 2 )
						vims_arguments[i].value = (void*) &(vj_event_list[id].args[n]);
					else
						vims_arguments[i].value = &(args[n]);
				} 
			}
			if(flags & VIMS_REQUIRE_ALL_PARAMS)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) %d requires all parameters",
					net_id);
				return;
			}
		}

		for( i = 0; i < 16; i ++ )
		{
			veejay_msg(VEEJAY_MSG_DEBUG, "vims argument [%p]", vims_arguments[i].value );
		}

		vj_event_trigger_function( (void*) v, net_list[net_id].act,
			vj_event_list[id].num_params, vj_event_list[id].format,
			vims_arguments[0].value,
			vims_arguments[1].value,
			vims_arguments[2].value,
			vims_arguments[3].value,
			vims_arguments[4].value,
			vims_arguments[5].value,
			vims_arguments[6].value,
			vims_arguments[7].value,
			vims_arguments[8].value,
			vims_arguments[9].value,
			vims_arguments[10].value,
			vims_arguments[11].value,
			vims_arguments[12].value,
			vims_arguments[13].value,
			vims_arguments[14].value,
			vims_arguments[15].value ); 
	
		fmt_offset =1 ;
		for( i = 0; i < vj_event_list[id].num_params; i ++ )
		{
			if( vims_arguments[i].value != NULL &&
				f[fmt_offset] == 's' )
				free(vims_arguments[i].value);
			fmt_offset += 3;
		}
	
	}	
}

int vj_event_parse_msg(veejay_t *v, char *msg)
{
	char args[150];
	int net_id=0;
	
	
	char *tmp = NULL;
	int msg_len = strlen(msg);
	int id = 0;
	bzero(args,150);  
	/* message is at least 5 bytes in length */
	if( msg == NULL || msg_len < MSG_MIN_LEN)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) Message [%s] too small (only %d bytes, need at least %d)",
			(msg==NULL? "(Empty)": msg), msg_len, MSG_MIN_LEN);
		return 0;
	}

	/* Sometimes, messages can have a trailing sequence of characters (like newline or spaces)*/
	while( msg[msg_len] != ';' )
	{
		msg[msg_len] = '\0';
		msg_len --;
		if(msg_len < MSG_MIN_LEN)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) Syntax error: Message does not end with ';'");
			return 0;
		}	
	}
	veejay_msg(VEEJAY_MSG_DEBUG, "Remaining: [%s]", msg );
	tmp = strndup( msg, 3 );
	if( strncasecmp( tmp, "bun", 3) == 0 )
	{
		vj_event_parse_bundle(v,msg);
		return 1;
	} 
	
	if( sscanf(tmp, "%03d", &net_id ) != 1 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) Selector '%s' is not a number!", tmp);
		return 0;
	}
	free(tmp);

	if( net_id < 0 || net_id >= VIMS_MAX)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) '%d' is not a valid VIMS selector", net_id);
		return 0;
	}

	/* 4th position is ':' */
	if( msg[3] != 0x3a || msg[msg_len] != ';' )
	{
		veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Syntax error, use \"<VIMS selector>:<arguments>;\" ");
		veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) [%s] : '%c' , '%c' ", msg,
			msg[3], msg[msg_len] );
		return 0;
	}

	msg[msg_len] = '\0'; // null terminate (uses semicolon position)

	if( net_list[ net_id ].list_id == 0 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) Event %d not known", net_id );
		return 0;
	}
	
	id =  net_list[net_id].list_id;

	if( id >= VIMS_BUNDLE_START && id < VIMS_BUNDLE_END )
	{
		vj_msg_bundle *bun = vj_event_bundle_get(id );
		if(!bun)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) internal error: Bundle %d was registered but is not present in Hash");
			return 0;
		}
		vj_event_parse_bundle( v, bun->bundle );
		return 1;
	}


	if( msg_len <= MSG_MIN_LEN )
	{	
		veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) Single fire %d", net_id);
		_last_known_num_args = vj_event_list[id].num_params; 

		vj_event_trigger_function(
			(void*)v,net_list[net_id].act,
			vj_event_list[id].num_params,
			vj_event_list[id].format,
			&(vj_event_list[id].args[0]),
			&(vj_event_list[id].args[1])
		);	
	}
	veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) '%s' -> %s", msg,
			vj_event_list[id].name );
	if( msg_len > MSG_MIN_LEN)
	{
		const char *fmt = vj_event_list[id].format;
		const int   np  = vj_event_list[id].num_params;
		int		fmt_offset = 1; // fmt offset
		int 		i;
		int		offset = 0;  // arguments offset
		int		num_array[16];
		char		*arguments;

	
		arguments = strndup( (msg + 4) , (msg_len -4) );
		veejay_msg(VEEJAY_MSG_DEBUG, "Arguments: [%s]",arguments);

		if( arguments == NULL )
		{	
			veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) %d requires %d arguments but none were given",
				net_id, np );
			return 0;
		} 
		if( np <= 0 )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) %d accepts no arguments", net_id); 
			if(arguments)
				free( (void*) arguments);
			return 0; // Ffree mem
		}
	
		vims_arg_t	vims_arguments[16];
		memset( vims_arguments, 0, sizeof(vims_arguments) );
		memset( num_array, 0, sizeof(num_array));
		int flags = vj_event_list[id].flags;

		for( i = 0; i < np; i ++ )
		{
			int failed_arg = 0;
			int type = 0;

			if(offset >= (msg_len - 4))
			{
				break;
			}

			if( fmt[fmt_offset] == 's' )
			{		
				type = 1;
				if( (flags & VIMS_LONG_PARAMS) ) /* copy rest of message */
				{
					int str_len = 0;
					vims_arguments[i].value = (void*) strndup( arguments+offset, msg_len - offset );
					str_len = strlen( (char*) vims_arguments[i].value );
					if(str_len < 1 )
						failed_arg ++;
					else 
						offset += str_len;
				}
				if( !(flags & VIMS_LONG_PARAMS) )
				{
					char tmp_str[256];
					bzero(tmp_str,256);
					int n = sscanf( arguments + offset, "%s", tmp_str );
					if( n <= 0 )
						failed_arg ++;
					else
					{
						int str_len = strlen( tmp_str );
						offset += str_len;
						vims_arguments[i].value = (void*) strndup( tmp_str, str_len );
					}
				}
			}
			if( fmt[fmt_offset] == 'd' )
			{
				char longest_num[16];
				bzero(longest_num,16);
				long tmp_val =0;
				type = 2;
				if(sscanf( arguments+offset, "%ld", &tmp_val) <= 0 )
					failed_arg ++;
				else
				{
					sprintf(longest_num, "%ld", tmp_val );
					offset += strlen( longest_num ); 
					if( arguments[offset] != 0x20 && arguments[offset] != 0x0)
						failed_arg++;
					else
					{
						num_array[i] = (int)tmp_val;
						offset ++;
						vims_arguments[i].value = &(num_array[i]); 
					}
				}
			}
		
			if( flags & VIMS_REQUIRE_ALL_PARAMS && failed_arg > 0)
			{
				if(type == 0 )
					veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) %d argument %d is of invalid type",
						net_id,i);
				if(type == 2)  
					veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) %d argument %d must be a number",
						net_id,i);
				if(type == 1)
					veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) %d argument %d must be a string",
						net_id,i);
				if(arguments)
					free( (void*)arguments);
				return 0;
			}

			if( failed_arg > 0 )
			{
				veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) %d argument %d not specified, using default %d",
					net_id, i , (i < 2 ? vj_event_list[id].args[i]: num_array[i]));
				if(i<2)
				{	
					vims_arguments[i].value = (void*) &(vj_event_list[id].args[i]);
				}
				else
				{
					int zero = 0;
					if( fmt[fmt_offset] == 'd' )
						vims_arguments[i].value = (void*) &zero;
					else
						vims_arguments[i].value = NULL;
				}
			}
			fmt_offset += 3;	
		}

		while( i < np)
		{
			int zero = 0;
			if( fmt[fmt_offset] == 'd' )
				vims_arguments[i].value = (void*) &zero;
			else
				vims_arguments[i].value = NULL;
			i++;
			fmt_offset += 3;
		}


		// should be 'I' ?
		_last_known_num_args = i;

		vj_event_trigger_function(
					(void*)v,
					net_list[net_id].act,
					np,
					vj_event_list[id].format,
					vims_arguments[0].value,
					vims_arguments[1].value,
					vims_arguments[2].value,
					vims_arguments[3].value,
					vims_arguments[4].value,
					vims_arguments[5].value,
					vims_arguments[6].value,		
					vims_arguments[7].value,
					vims_arguments[8].value,
					vims_arguments[9].value,
					vims_arguments[10].value,
					vims_arguments[11].value,
					vims_arguments[12].value,
					vims_arguments[13].value,
					vims_arguments[14].value,
					vims_arguments[15].value);

		fmt_offset = 1;
		for ( i = 0; i < np ; i ++ )
		{
			if( vims_arguments[i].value &&
				fmt[fmt_offset] == 's' )
				free( vims_arguments[i].value );
			fmt_offset += 3;
		}
		if(arguments)
			free( (void*)arguments);
	}
	else
		_last_known_num_args = 0;
	return 1;
}	

/*
	update connections
 */
void vj_event_update_remote(void *ptr)
{
	veejay_t *v = (veejay_t*)ptr;
	int cmd_poll = 0;	// command port
	int sta_poll = 0;	// status port
	int new_link = -1;
	int sta_link = -1;
	int i;
	cmd_poll = vj_server_poll(v->vjs[0]);
	sta_poll = vj_server_poll(v->vjs[1]);
	// accept connection command socket    

	if( cmd_poll > 0)
	{
		new_link = vj_server_new_connection ( v->vjs[0] );
	}
	// accept connection on status socket
	if( sta_poll > 0) 
	{
		sta_link = vj_server_new_connection ( v->vjs[1] );
	}
	// see if there is any link interested in status information
	for( i = 0; i < v->vjs[1]->nr_of_links; i ++ )
		veejay_pipe_write_status( v, i );

	if( v->settings->use_vims_mcast )
	{
		int res = vj_server_update(v->vjs[2],0 );
		if(res > 0)
		{
			v->uc->current_link = 0;
			char buf[MESSAGE_SIZE];
			bzero(buf, MESSAGE_SIZE);
			while( vj_server_retrieve_msg( v->vjs[2], 0, buf ) )
			{
				vj_event_parse_msg( v, buf );
				bzero( buf, MESSAGE_SIZE );
			}
		}
		
	}

	// see if there is anything to read from the command socket
	for( i = 0; i < v->vjs[0]->nr_of_links; i ++ )
	{
		int res = vj_server_update(v->vjs[0],i );
		if(res > 0)
		{
			v->uc->current_link = i;
			char buf[MESSAGE_SIZE];
			bzero(buf, MESSAGE_SIZE);
			while( vj_server_retrieve_msg( v->vjs[0], i, buf ) )
			{
				vj_event_parse_msg( v, buf );
				bzero( buf, MESSAGE_SIZE );
			}
		}
		if(res==-1)
		{
			_vj_server_del_client( v->vjs[0], i );
			_vj_server_del_client( v->vjs[1], i );
		}
	}


	

}

void	vj_event_commit_bundle( veejay_t *v, int key_num, int key_mod)
{
	char bundle[4096];
	bzero(bundle,4096);
	vj_event_create_effect_bundle(v, bundle, key_num, key_mod );
}

#ifdef HAVE_SDL
void vj_event_single_fire(void *ptr , SDL_Event event, int pressed)
{
	
	SDL_KeyboardEvent *key = &event.key;
	SDLMod mod = key->keysym.mod;
	
	int vims_mod = 0;

	if( (mod & KMOD_LSHIFT) || (mod & KMOD_RSHIFT ))
		vims_mod = VIMS_MOD_SHIFT;
	if( (mod & KMOD_LALT) || (mod & KMOD_ALT) )
		vims_mod = VIMS_MOD_ALT;
	if( (mod & KMOD_CTRL) || (mod & KMOD_CTRL) )
		vims_mod = VIMS_MOD_CTRL;

	int vims_key = key->keysym.sym;
	int index = vims_mod * SDLK_LAST + vims_key;

	vj_keyboard_event *ev = &keyboard_events[index];

	int event_id = ev->vims.list_id;

	if(event_id == 0)
	{
		return;
	}

	if( event_id >= VIMS_BUNDLE_START && event_id < VIMS_BUNDLE_END )
	{
		vj_msg_bundle *bun = vj_event_bundle_get(event_id );
		if(!bun)
		{
			veejay_msg(VEEJAY_MSG_DEBUG, "Requested BUNDLE %d does not exist", event_id);
			return;
		}
		veejay_msg(VEEJAY_MSG_DEBUG, "Keyboard fires Bundle %d", event_id);
		/*
			parse_bundle calls vj_event_parse_msg :)
			it should be possible to trigger a set of bundles with a bundle,
			but this needs testing.
		*/

		vj_event_parse_bundle( (veejay_t*) ptr, bun->bundle );
	}
	else
	{
		char msg[100];
		if( ev->arguments != NULL)
		{
			sprintf(msg,"%03d:%s;", event_id, ev->arguments );
		}
		else
			sprintf(msg,"%03d:;", event_id );
		veejay_msg(VEEJAY_MSG_DEBUG, "Keyboard fires Event %d [%s]", event_id,
			msg );
		vj_event_parse_msg( (veejay_t*) ptr, msg );
	}

}

#endif


void vj_event_none(void *ptr, const char format[], va_list ap)
{
	veejay_msg(VEEJAY_MSG_DEBUG, "No event attached on this key");
}

#ifdef HAVE_XML2

// parsing of XML files can be handled in a general way
static	int	get_cstr( xmlDocPtr doc, xmlNodePtr cur, const xmlChar *what, char *dst )
{
	xmlChar *tmp = NULL;
	char *t = NULL;
	if(! xmlStrcmp( cur->name, what ))
	{
		tmp = xmlNodeListGetString( doc, cur->xmlChildrenNode,1 );
		t   = UTF8toLAT1(tmp);
		if(!t)
			return 0;
		strncpy( dst, t, strlen(t) );	
		free(t);
		xmlFree(tmp);
		return 1;
	}
	return 0;
}
static	int	get_fstr( xmlDocPtr doc, xmlNodePtr cur, const xmlChar *what, float *dst )
{
	xmlChar *tmp = NULL;
	char *t = NULL;
	float tmp_f = 0;
	int n = 0;
	if(! xmlStrcmp( cur->name, what ))
	{
		tmp = xmlNodeListGetString( doc, cur->xmlChildrenNode,1 );
		t   = UTF8toLAT1(tmp);
		if(!t)
			return 0;
		
		n = sscanf( t, "%f", &tmp_f );
		free(t);
		xmlFree(tmp);

		if( n )
			*dst = tmp_f;
		else
			return 0;

		return 1;
	}
	return 0;
}

static	int	get_istr( xmlDocPtr doc, xmlNodePtr cur, const xmlChar *what, int *dst )
{
	xmlChar *tmp = NULL;
	char *t = NULL;
	int tmp_i = 0;
	int n = 0;
	if(! xmlStrcmp( cur->name, what ))
	{
		tmp = xmlNodeListGetString( doc, cur->xmlChildrenNode,1 );
		t   = UTF8toLAT1(tmp);
		if(!t)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Input not in UTF8 format!");
			return 0;
		}

		n = sscanf( t, "%d", &tmp_i );
		free(t);
		xmlFree(tmp);

		if( n )
			*dst = tmp_i;
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot convert value '%s'to number", t);
			return 0;
		}
		return 1;
	}
	return 0;
}
#define XML_CONFIG_KEY_SYM "key_symbol"
#define XML_CONFIG_KEY_MOD "key_modifier"
#define XML_CONFIG_KEY_VIMS "vims_id"
#define XML_CONFIG_KEY_EXTRA "extra"
#define XML_CONFIG_EVENT "event"
#define XML_CONFIG_FILE "config"
#define XML_CONFIG_SETTINGS	   "run_settings"
#define XML_CONFIG_SETTING_PORTNUM "port_num"
#define XML_CONFIG_SETTING_HOSTNAME "hostname"
#define XML_CONFIG_SETTING_PRIOUTPUT "primary_output"
#define XML_CONFIG_SETTING_PRINAME   "primary_output_destination"
#define XML_CONFIG_SETTING_SDLSIZEX   "SDLwidth"
#define XML_CONFIG_SETTING_SDLSIZEY   "SDLheight"
#define XML_CONFIG_SETTING_AUDIO     "audio"
#define XML_CONFIG_SETTING_SYNC	     "sync"
#define XML_CONFIG_SETTING_TIMER     "timer"
#define XML_CONFIG_SETTING_FPS	     "output_fps"
#define XML_CONFIG_SETTING_GEOX	     "Xgeom_x"
#define XML_CONFIG_SETTING_GEOY	     "Xgeom_y"
#define XML_CONFIG_SETTING_BEZERK    "bezerk"
#define XML_CONFIG_SETTING_COLOR     "nocolor"
#define XML_CONFIG_SETTING_YCBCR     "chrominance_level"
#define XML_CONFIG_SETTING_WIDTH     "output_width"
#define XML_CONFIG_SETTING_HEIGHT    "output_height"
#define XML_CONFIG_SETTING_DFPS	     "dummy_fps"
#define XML_CONFIG_SETTING_DUMMY	"dummy"
#define XML_CONFIG_SETTING_NORM	     "video_norm"
#define XML_CONFIG_SETTING_MCASTOSC  "mcast_osc"
#define XML_CONFIG_SETTING_MCASTVIMS "mcast_vims"
#define XML_CONFIG_SETTING_SCALE     "output_scaler"	

#define __xml_cint( buf, var , node, name )\
{\
sprintf(buf,"%d", var);\
xmlNewChild(node, NULL, (const xmlChar*) name, (const xmlChar*) buf );\
}

#define __xml_cfloat( buf, var , node, name )\
{\
sprintf(buf,"%f", var);\
xmlNewChild(node, NULL, (const xmlChar*) name, (const xmlChar*) buf );\
}

#define __xml_cstr( buf, var , node, name )\
{\
if(var != NULL){\
strncpy(buf,var,strlen(var));\
xmlNewChild(node, NULL, (const xmlChar*) name, (const xmlChar*) buf );}\
}

void	vj_event_format_xml_settings( veejay_t *v, xmlNodePtr node  )
{
	char buf[4069];
	bzero(buf,4069);
	int c = veejay_is_colored();
	veejay_msg(VEEJAY_MSG_DEBUG, "Format XML settings %s", __FUNCTION__ );	
	__xml_cint( buf, v->uc->port, node, 	XML_CONFIG_SETTING_PORTNUM );
	__xml_cint( buf, v->video_out, node, 	XML_CONFIG_SETTING_PRIOUTPUT);
	__xml_cstr( buf, v->stream_outname,node,XML_CONFIG_SETTING_PRINAME );
	__xml_cint( buf, v->bes_width,node,	XML_CONFIG_SETTING_SDLSIZEX );
	__xml_cint( buf, v->bes_height,node,	XML_CONFIG_SETTING_SDLSIZEY );
	__xml_cint( buf, v->audio,node,		XML_CONFIG_SETTING_AUDIO );
	__xml_cint( buf, v->sync_correction,node,	XML_CONFIG_SETTING_SYNC );
	__xml_cint( buf, v->uc->use_timer,node,		XML_CONFIG_SETTING_TIMER );
	__xml_cint( buf, v->real_fps,node,		XML_CONFIG_SETTING_FPS );
	__xml_cint( buf, v->uc->geox,node,		XML_CONFIG_SETTING_GEOX );
	__xml_cint( buf, v->uc->geoy,node,		XML_CONFIG_SETTING_GEOY );
	__xml_cint( buf, v->no_bezerk,node,		XML_CONFIG_SETTING_BEZERK );
	__xml_cint( buf, c,node, XML_CONFIG_SETTING_COLOR );
	__xml_cint( buf, v->pixel_format,node,	XML_CONFIG_SETTING_YCBCR );
	__xml_cint( buf, v->video_output_width,node, XML_CONFIG_SETTING_WIDTH );
	__xml_cint( buf, v->video_output_height,node, XML_CONFIG_SETTING_HEIGHT );
	__xml_cfloat( buf,v->dummy->fps,node,	XML_CONFIG_SETTING_DFPS ); 
	__xml_cint( buf, v->dummy->norm,node,	XML_CONFIG_SETTING_NORM );
	__xml_cint( buf, v->dummy->active,node,	XML_CONFIG_SETTING_DUMMY );
	__xml_cint( buf, v->settings->use_mcast,node, XML_CONFIG_SETTING_MCASTOSC );
	__xml_cint( buf, v->settings->use_vims_mcast,node, XML_CONFIG_SETTING_MCASTVIMS );
	__xml_cint( buf, v->settings->zoom ,node,	XML_CONFIG_SETTING_SCALE );
#ifdef HAVE_SDL
#endif


}

void	vj_event_xml_parse_config( veejay_t *v, xmlDocPtr doc, xmlNodePtr cur )
{
	// should be called at malloc, dont change setttings when it runs!
	if( veejay_get_state(v) != LAVPLAY_STATE_STOP)
		return;

	int c = 0;

	while( cur != NULL )
	{
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_PORTNUM, &(v->uc->port) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_PRIOUTPUT, &(v->video_out) );
		get_cstr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_PRINAME, v->stream_outname);
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_SDLSIZEX, &(v->bes_width) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_SDLSIZEY, &(v->bes_height) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_AUDIO, &(v->audio) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_SYNC, &(v->sync_correction) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_TIMER, &(v->uc->use_timer) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_FPS, &(v->real_fps) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_GEOX, &(v->uc->geox) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_GEOY, &(v->uc->geoy) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_BEZERK, &(v->no_bezerk) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_COLOR, &c );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_YCBCR, &(v->pixel_format) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_WIDTH, &(v->video_output_width) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_HEIGHT,&(v->video_output_height) );
		get_fstr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_DFPS, &(v->dummy->fps ) );
		get_cstr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_NORM, &(v->dummy->norm) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_DUMMY, &(v->dummy->active) ); 
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_MCASTOSC, &(v->settings->use_mcast) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_MCASTVIMS, &(v->settings->use_vims_mcast) );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_SETTING_SCALE, &(v->settings->zoom) );

		cur = cur->next;
	}

	veejay_set_colors( c );


}

// not only for keyboard, also check if events in the list exist
void vj_event_xml_new_keyb_event( void *ptr, xmlDocPtr doc, xmlNodePtr cur )
{
	int key = 0;
	int key_mod = 0;
	int event_id = 0;	
	
	if( veejay_get_state( (veejay_t*) ptr ) == LAVPLAY_STATE_STOP )
	{
		return;
	}
	char msg[4096];
	bzero( msg,4096 );

	while( cur != NULL )
	{
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_KEY_VIMS, &event_id );
		get_cstr( doc, cur, (const xmlChar*) XML_CONFIG_KEY_EXTRA, msg );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_KEY_SYM, &key );
		get_istr( doc, cur, (const xmlChar*) XML_CONFIG_KEY_MOD, &key_mod );		
		cur = cur->next;
	}

	if( event_id <= 0 )
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "Invalid event_id in configuration file used ?!");
		return;
	}

	if( event_id >= VIMS_BUNDLE_START && event_id < VIMS_BUNDLE_END )
	{
		if( vj_event_bundle_exists(event_id))
		{
			veejay_msg(VEEJAY_MSG_WARNING, "Bundle %d already exists in VIMS system! (Bundle in configfile was ignored)",event_id);
			return;
		}

		vj_msg_bundle *m = vj_event_bundle_new( msg, event_id);
		if(!msg)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Failed to create new Bundle %d - [%s]", event_id, msg );
			return;
		}
		if(!vj_event_bundle_store(m))
		{
			veejay_msg(VEEJAY_MSG_DEBUG, "%s Error storing newly created bundle?!", __FUNCTION__);
			return;
		}
		veejay_msg(VEEJAY_MSG_DEBUG, "Added bundle %d", event_id);
	}

	if ( !vj_event_get_id( event_id ) )
	{
		if( event_id >= VIMS_BUNDLE_START && event_id < VIMS_BUNDLE_END )
		{
			 veejay_msg(VEEJAY_MSG_DEBUG, "Bundle %d in configuration file is never defined",
				event_id );
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "VIMS %d is invalid or missing in veejay (The event may depend on packages found at compile time)");
			veejay_msg(VEEJAY_MSG_ERROR, "For a hint about this event, look in veejay/vims.h (may the source be with you)");
		}
		return; // and exit, no need for nonsense bindings.
	}

#ifdef HAVE_SDL
	if( key > 0 && key_mod >= 0)
	{
		if( vj_event_register_keyb_event( event_id, key, key_mod, NULL ))
			veejay_msg(VEEJAY_MSG_INFO, "Attached key %d + %d to Bundle %d ", key,key_mod,event_id);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "No keyboard binding for VIMS %d", event_id);
	}
#endif
}

int  veejay_load_action_file( void *ptr, char *file_name )
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	veejay_t *v = (veejay_t*) ptr;
	if(!file_name)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Invalid filename");
			return 0;
		}

	doc = xmlParseFile( file_name );

	if(doc==NULL)	
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot read file %s", file_name );
		return 0;
	}
	cur = xmlDocGetRootElement( doc );
	if( cur == NULL)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "This is not a XML document");
		xmlFreeDoc(doc);
		return 0;
	}
	if( xmlStrcmp( cur->name, (const xmlChar *) XML_CONFIG_FILE))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "This is not a Veejay Configuration File");
		xmlFreeDoc(doc);
		return 0;
	}

	cur = cur->xmlChildrenNode;
	while( cur != NULL )
	{
		if( !xmlStrcmp( cur->name, (const xmlChar*) XML_CONFIG_SETTINGS ) )
		{
			vj_event_xml_parse_config( v, doc, cur->xmlChildrenNode  );
		}
		if( !xmlStrcmp( cur->name, (const xmlChar *) XML_CONFIG_EVENT ))
		{
			vj_event_xml_new_keyb_event( (void*)v, doc, cur->xmlChildrenNode );
		}
		cur = cur->next;
	}
	xmlFreeDoc(doc);	
	return 1;
}

void	vj_event_format_xml_event( xmlNodePtr node, int event_id )
{
	char buffer[4096];
	int key_id=0;
	int key_mod=-1;

	bzero( buffer, 4096 );

	if( event_id >= VIMS_BUNDLE_START && event_id < VIMS_BUNDLE_END)
	{ /* its a Bundle !*/
		vj_msg_bundle *m = vj_event_bundle_get( event_id );
		if(!m) 
		{	
			veejay_msg(VEEJAY_MSG_ERROR, "bundle %d does not exist", event_id);
			return;
		}

		strncpy(buffer, m->bundle, strlen(m->bundle) );
		xmlNewChild(node, NULL, (const xmlChar*) XML_CONFIG_KEY_EXTRA ,
			(const xmlChar*) buffer);
			// m->event_id and event_id should be equal
	}

	/* Put VIMS keybinding and Event ID */
#ifdef HAVE_SDL
	vj_event_get_key( event_id, &key_id, &key_mod );
 #endif

	/* Put all known VIMS so we can detect differences in runtime
           some Events will not exist if SDL, Jack, DV, Video4Linux would be missing */

	sprintf(buffer, "%d", event_id);
	xmlNewChild(node, NULL, (const xmlChar*) XML_CONFIG_KEY_VIMS , 
		(const xmlChar*) buffer);

#ifdef HAVE_SDL
	if(key_id > 0 && key_mod >= 0 )
	{
		sprintf(buffer, "%d", key_id );
		xmlNewChild(node, NULL, (const xmlChar*) XML_CONFIG_KEY_SYM ,
			(const xmlChar*) buffer);
		sprintf(buffer, "%d", key_mod );
		xmlNewChild(node, NULL, (const xmlChar*) XML_CONFIG_KEY_MOD ,
			(const xmlChar*) buffer);
	}
#endif
}

void vj_event_write_actionfile(void *ptr, const char format[], va_list ap)
{
	char file_name[512];
	int args[2] = {0,0};
	int i;
	//veejay_t *v = (veejay_t*) ptr;
	xmlDocPtr doc;
	xmlNodePtr rootnode,childnode;	
	P_A(args,file_name,format,ap);
	doc = xmlNewDoc( "1.0" );
	rootnode = xmlNewDocNode( doc, NULL, (const xmlChar*) XML_CONFIG_FILE,NULL);
	xmlDocSetRootElement( doc, rootnode );
	/* Here, we can save the cliplist, editlist as it is now */
	if(args[0]==1 || args[1]==1)
	{
		/* write cliplist into XML bundle */	
		childnode = xmlNewChild( rootnode, NULL, (const xmlChar*) XML_CONFIG_SETTINGS, NULL );
		vj_event_format_xml_settings( (veejay_t*) ptr, childnode );
	}

	for( i = 0; i < VIMS_MAX; i ++ )
	{
		if( net_list[i].list_id > 0 )
		{	
			childnode = xmlNewChild( rootnode,NULL,(const xmlChar*) XML_CONFIG_EVENT ,NULL);
			vj_event_format_xml_event( childnode, i );
		}
	}
	

	xmlSaveFormatFile( file_name, doc, 1);
	xmlFreeDoc(doc);	
}

#endif  // XML2

void	vj_event_read_file( void *ptr, 	const char format[], va_list ap )
{
	char file_name[512];
	int args[1];

	P_A(args,file_name,format,ap);

#ifdef HAVE_XML2
	veejay_load_action_file( ptr, file_name );
#endif
}

#ifdef HAVE_SDL

static void vj_event_get_key( int event_id, int *key_id, int *key_mod )
{
	if ( event_id >= VIMS_BUNDLE_START && event_id < VIMS_BUNDLE_END )
	{
		if( vj_event_bundle_exists( event_id ))
		{
			vj_msg_bundle *bun = vj_event_bundle_get( event_id );
			if( bun )
			{
				*key_id = bun->accelerator;
				*key_mod = bun->modifier;
			}
		}
	}
	else
	{
		int i;
		for ( i = 0; i < MAX_SDL_KEY ; i ++ )
		{
			if (keyboard_events[i].vims.list_id == event_id )
			{
				*key_id =  keyboard_events[ i ].key_symbol;
  				*key_mod=  keyboard_events[ i ].key_mod;
				return;
			}
		}
		// see if binding is in 
	}
	

	*key_id  = 0;
	*key_mod = 0;
}

void	vj_event_unregister_keyb_event( int sdl_key, int modifier )
{
	int sdl_index = modifier * SDLK_LAST;
	vj_keyboard_event *ev = &keyboard_events[ sdl_index + sdl_key ];
	ev->vims.act = vj_event_none;
	ev->vims.list_id = 0;
	ev->key_symbol = 0;
	ev->key_mod = 0;
	if(ev->arguments)
		free(ev->arguments);
	ev->arguments = NULL;
}
int 	vj_event_register_keyb_event(int event_id, int symbol, int modifier, const char *value)
{
	int offset = SDLK_LAST * modifier;
	if( symbol > 0 )
	{
		/* If an event has been attached to some other event, abort registring and let the
                   user explicitly remove the binding */
		if( keyboard_events[offset + symbol].vims.list_id > 0 && event_id != keyboard_events[offset+symbol].vims.list_id )
		{
			veejay_msg(VEEJAY_MSG_DEBUG, "Key %d + %d already exists ", symbol, modifier );
			return 0;
		}
	}
/*	else 
	{	// finds only 1st occurence 
		if(symbol == 0)
		{
			int i;
			for( i = 0; i < MAX_SDL_KEY; i ++ )
			{
				if( keyboard_events[i].vims.list_id == event_id )
				{
					symbol = keyboard_events[i].key_symbol;
					modifier = keyboard_events[i].key_mod;
					break;
				}
			}              
			// find symbol / mod!
			if( i == MAX_SDL_KEY )
			{
				veejay_msg(VEEJAY_MSG_ERROR,
					"VIMS  %d is not bound to any key !", event_id);
				return 0;
			}
		}
	}*/

	int id = vj_event_get_id( event_id );
	if( id <= 0 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Event %d does not exist (%s)", event_id, __FUNCTION__);
		return 0;
	}

	keyboard_events[offset + symbol].vims.act     = vj_event_list[ id ].function;
	keyboard_events[offset + symbol].vims.list_id = event_id;
	keyboard_events[offset + symbol].key_symbol   = symbol;
	keyboard_events[offset + symbol].key_mod      = modifier;

	char *args = NULL;
	if( value != NULL && vj_event_list[ id].format != NULL )
		args = strndup( value, 30 ); 

	if( keyboard_events[offset+symbol].arguments != NULL)
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "going to free existing arguments %s",
			keyboard_events[offset+symbol].arguments );
		free( keyboard_events[offset+symbol].arguments );
		keyboard_events[offset+symbol].arguments = NULL;
	}

	if( value )
		keyboard_events[offset + symbol].arguments    =	args;
	else
		keyboard_events[offset + symbol].arguments    = args;  

	return 1;
}


#endif

void	vj_event_init_network_events()
{
	int i;
	int net_id = 0;
	for( i = 1; vj_event_list[i].event_id != 0; i ++ )
	{
		net_id = vj_event_list[i].event_id;
		net_list[ net_id ].act = vj_event_list[i].function;
		net_list[ net_id ].list_id = i;
	}

	veejay_msg(VEEJAY_MSG_DEBUG, "Registered %d VIMS events", net_id );
}



#ifdef HAVE_SDL
void	vj_event_init_keyboard_defaults()
{
	int i;
	int keyb_events = 0;
	for( i = 1; vj_event_default_sdl_keys[i].event_id != 0; i ++ )
	{
		if( vj_event_register_keyb_event(
				vj_event_default_sdl_keys[i].event_id,
				vj_event_default_sdl_keys[i].key_sym,
				vj_event_default_sdl_keys[i].key_mod,
				vj_event_default_sdl_keys[i].value ))
		{
			keyb_events++;
		}
		else
		{

			veejay_msg(VEEJAY_MSG_ERROR,
			  "VIMS event %d not yet implemented", vj_event_default_sdl_keys[i].event_id );
		}
	}
}
#endif

void vj_event_init()
{
	
	
	int i;
#ifdef HAVE_SDL
	for(i=0; i < MAX_SDL_KEY; i++)
	{
		keyboard_events[i].vims.act = vj_event_none;
		keyboard_events[i].vims.list_id  = 0;
		keyboard_events[i].key_symbol = 0;
		keyboard_events[i].key_mod = 0;
		keyboard_events[i].arguments = NULL;
	}
#endif

	/* clear Network bindings */
	for(i=0; i < VIMS_MAX; i++)
	{
		net_list[i].act = vj_event_none;
		net_list[i].list_id = 0;
	}

	if( !(BundleHash = hash_create(HASHCOUNT_T_MAX, int_bundle_compare, int_bundle_hash)))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot initialize hashtable for message bundles");
		return;
	}

	vj_event_init_network_events();
	vj_event_init_keyboard_defaults();
	
}

void vj_event_quit(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	veejay_msg(VEEJAY_MSG_INFO, "Remote requested session-end.");
	veejay_change_state(v, LAVPLAY_STATE_STOP);         
}

void  vj_event_sample_mode(void *ptr,	const char format[],	va_list ap)
{
	veejay_t *v = (veejay_t *) ptr;
	if(v->settings->sample_mode == SSM_420_JPEG_BOX)
		veejay_set_sampling( v, SSM_420_JPEG_TR );
	else
		veejay_set_sampling( v, SSM_420_JPEG_BOX );
	veejay_msg(VEEJAY_MSG_WARNING, "Sampling of 2x2 -> 1x1 is set to [%s]",
		(v->settings->sample_mode == SSM_420_JPEG_BOX ? "lame box filter" : "triangle linear filter")); 
}

void vj_event_bezerk(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(v->no_bezerk) v->no_bezerk = 0; else v->no_bezerk = 1;
	veejay_msg(VEEJAY_MSG_INFO, "Bezerk mode is %s", (v->no_bezerk==0? "enabled" : "disabled"));
	if(v->no_bezerk==1)
		veejay_msg(VEEJAY_MSG_DEBUG,"Bezerk On  :No clip-restart when changing input channels");
	else
		veejay_msg(VEEJAY_MSG_DEBUG,"Bezerk Off :Clip-restart when changing input channels"); 
}

void vj_event_debug_level(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(v->verbose) v->verbose = 0; else v->verbose = 1;
	if(v->verbose==0) veejay_msg(VEEJAY_MSG_INFO,"Not displaying debug information");  
	veejay_set_debug_level( v->verbose );

}

void vj_event_suspend(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	veejay_change_state(v, LAVPLAY_STATE_PAUSED);
}

void vj_event_set_play_mode_go(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;

	P_A(args,s,format,ap);
	if(vj_event_valid_mode(args[0]))
	{
		if(args[0] == VJ_PLAYBACK_MODE_PLAIN) 
		{
			if( vj_has_video(v) )
				veejay_change_playback_mode(v, args[0], 0);
			else
				veejay_msg(VEEJAY_MSG_ERROR,
				"There are no video files in the editlist");
			return;
		}
	
		if(args[0] == VJ_PLAYBACK_MODE_CLIP) 
		{
			if(args[1]==0) args[1] = v->uc->clip_id;
			if(args[1]==-1) args[1] = clip_size()-1;
			if(clip_exists(args[1]))
			{
				veejay_change_playback_mode(v,args[0] ,args[1]);
			}
			else
			{	
				p_no_clip(args[1]);
			}
		}
		if(args[0] == VJ_PLAYBACK_MODE_TAG)
		{
			if(args[1]==0) args[1] = v->uc->clip_id;
			if(args[1]==-1) args[1] = clip_size()-1;
			if(vj_tag_exists(args[1]))
			{
				veejay_change_playback_mode(v,args[0],args[1]);
			}
			else
			{
				p_no_tag(args[1]);
			}
		}
	}
}



void	vj_event_set_rgb_parameter_type(void *ptr, const char format[], va_list ap)
{	
	
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(args[0] >= 0 && args[0] < 3 )
	{
		rgb_parameter_conversion_type_ = args[0];
		if(args[0] == 0)
			veejay_msg(VEEJAY_MSG_INFO,"GIMP's RGB -> YUV");
		if(args[1] == 1)
			veejay_msg(VEEJAY_MSG_INFO,"CCIR601 RGB -> YUV");
		if(args[2] == 2)
			veejay_msg(VEEJAY_MSG_INFO,"Broken RGB -> YUV");
	}

}

void vj_event_effect_set_bg(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	v->uc->take_bg = 1;
	veejay_msg(VEEJAY_MSG_INFO, "Next frame will be taken for static background\n");
}

void	vj_event_send_bundles(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	vj_msg_bundle *m;
	int i;
	int len = 0;
	const int token_len = 14;

	for( i =0 ; i < MAX_SDL_KEY ; i ++ )
	{
		vj_keyboard_event *ev = &keyboard_events[i];
		if( ev->key_symbol > 0 )
		{
			len += token_len;
			if(ev->arguments != NULL)
				len += strlen(ev->arguments);
		}
	}

	len ++;

	for( i = VIMS_BUNDLE_START; i < VIMS_BUNDLE_END; i ++ )
	{
		if( vj_event_bundle_exists(i))
		{
			m = vj_event_bundle_get( i );
			if(m)
			{
				len += strlen( m->bundle );
				len += token_len;
			}
		}
	}
	
	// token len too small !!
	if(len > 0)
	{
		char *buf = (char*) vj_malloc(sizeof(char) * (len+5) );
		bzero(buf, len+5 );
		sprintf(buf, "%05d", len ); 
		for ( i = 0; i < MAX_SDL_KEY; i ++ )
		{
			vj_keyboard_event *ev = &keyboard_events[i];
			if( ev->key_symbol > 0 )
			{
				int id = ev->vims.list_id;
				int arg_len = (ev->arguments == NULL ? 0 : strlen(ev->arguments));
				char tmp[token_len];
				bzero(tmp,token_len); 

				sprintf(tmp, "%04d%03d%03d%04d", id, ev->key_symbol, ev->key_mod, arg_len );
				strncat(buf,tmp,token_len);
				if( arg_len > 0 )
				{
					strncat( buf, ev->arguments, arg_len );
				}
			}
		}

		for( i = VIMS_BUNDLE_START; i < VIMS_BUNDLE_END; i ++ )
		{
			if( vj_event_bundle_exists(i))
			{
				m = vj_event_bundle_get( i );
				if(m)
				{
					int key_id = 0;
					int key_mod = 0;
					int bun_len = strlen(m->bundle)-1;	
					char tmp[token_len];
					bzero(tmp,token_len);
					vj_event_get_key( i, &key_id, &key_mod );
	
					sprintf(tmp, "%04d%03d%03d%04d",
						i,key_id,key_mod, bun_len );

					strncat( buf, tmp, token_len );
					strncat( buf, m->bundle, bun_len );
				}
			}
		}
		SEND_MSG(v,buf);
		if(buf) free(buf);
	}	
	else
	{
		char *buf = "0000";
		SEND_MSG(v,buf);
	}
}

void	vj_event_send_vimslist(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	
	int i;

	int len = 1;
	// send event list seperatly

	for( i = 1; vj_event_list[i].name != NULL ; i ++ )
	{
		// dont include bundles or query messages
		if( vj_event_list[i].event_id < 400 || vj_event_list[i].event_id > 599 )
		{
			len += strlen( vj_event_list[i].name );
			len += (vj_event_list[i].format == NULL ? 0 : strlen(vj_event_list[i].format) );
			len += 12; /* event_id: 4, num_params: 2 , format:3 (strlen), descr: 3 (strlen) */
		}
	}

	if(len > 1)
	{
		char *buf = (char*) vj_malloc(sizeof(char) * (len+5) );
		bzero(buf, len+5 );
		sprintf(buf, "%05d", len ); 

		

		for( i = 1; vj_event_list[i].name != NULL ; i ++ )
		{
			if( vj_event_list[i].event_id < 400 || vj_event_list[i].event_id > 599 )
			{
				char tmp[12];
				bzero(tmp,12);
				int event_id = vj_event_list[i].event_id;
				char *description = (char*) vj_event_list[i].name;
				char *format	  = (char*) vj_event_list[i].format;
				int   format_len  = ( vj_event_list[i].format == NULL ? 0:strlen( vj_event_list[i].format ));
				int   descr_len   = strlen(description);
				
				sprintf(tmp, "%04d%02d%03d%03d",
						event_id, vj_event_list[i].num_params , format_len, descr_len );

				strncat( buf, tmp, 12 );
				if(format != NULL) 
					strncat( buf, format,	   format_len		 );
				strncat( buf, description, descr_len );
			}
		}

		SEND_MSG(v,buf);
		if(buf) free(buf);
	}	
	else
	{
		char *buf = "0000";
		SEND_MSG(v,buf);
	}
}



void vj_event_clip_select(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;	
	P_A( args, s , format, ap);

	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1)
	{
		args[0] = clip_size()-1;
	}
	if(clip_exists(args[0]))
	{
		veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP,args[0] );
	}
	else
	{
		p_no_clip(args[0]);
	}
}

void vj_event_tag_select(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;	
	P_A( args, s , format, ap);

	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0]==-1)
	{
		args[0] = vj_tag_size()-1;
	}

	if(vj_tag_exists(args[0]))
	{
		veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG,args[0]);
	}
	else
	{
		p_no_tag(args[0]);
	}
}


void vj_event_switch_clip_tag(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;

	if(!STREAM_PLAYING(v) && !CLIP_PLAYING(v))
	{
		if(clip_exists(v->last_clip_id)) 
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP, v->last_clip_id);
			return;
		}
		if(vj_tag_exists(v->last_tag_id))
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG, v->last_tag_id);
			return;
		}
		if(clip_size()-1 <= 0)
		{
			if(vj_tag_exists( vj_tag_size()-1 ))
			{
				veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_TAG, vj_tag_size()-1);
				return;
			}
		}	
	}

	if(CLIP_PLAYING(v))
	{
		if(vj_tag_exists(v->last_tag_id))
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG, v->last_tag_id);
		}
		else
		{
			int id = vj_tag_size() - 1;
			if(id)
			{
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG,id);
			}
			else
			{
				p_no_tag(id);
			}
		}
	}
	else
	if(STREAM_PLAYING(v))
	{
		if(clip_exists(v->last_clip_id) )
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP, v->last_clip_id);
		}
		else
		{
			int id = clip_size() - 1;
			if(id)
			{
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP,id);
			}
			else
			{
				p_no_clip(id);
			}
		}
	}
}

void	vj_event_set_volume(void *ptr, const char format[], va_list ap)
{
	int args[1];	
	char *s = NULL;
	P_A(args,s,format,ap)
	if(args[0] >= 0 && args[0] <= 100)
	{
#ifdef HAVE_JACK
		if(vj_jack_set_volume(args[0]))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Volume set to %d", args[0]);
		}
#endif
	}
}
void vj_event_set_play_mode(void *ptr, const char format[], va_list ap)
{
	int args[1];
	char *s = NULL;
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,s,format,ap);

	if(vj_event_valid_mode(args[0]))
	{
		int mode = args[0];
		/* check if current playing ID is valid for this mode */
		if(mode == VJ_PLAYBACK_MODE_CLIP)
		{
			int last_id = clip_size()-1;
			if(last_id == 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "There are no clips. Cannot switch to clip mode");
				return;
			}
			if(!clip_exists(v->last_clip_id))
			{
				v->uc->clip_id = last_id;
			}
			if(clip_exists(v->uc->clip_id))
			{
				veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_CLIP, v->uc->clip_id );
			}
		}
		if(mode == VJ_PLAYBACK_MODE_TAG)
		{
			int last_id = vj_tag_size()-1;
			if(last_id == 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "There are no streams. Cannot switch to stream mode");
				return;
			}
			
			if(!vj_tag_exists(v->last_tag_id)) /* jump to last used Tag if ok */
			{
				v->uc->clip_id = last_id;
			}
			if(vj_tag_exists(v->uc->clip_id))
			{
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG, v->uc->clip_id);
			}
		}
		if(mode == VJ_PLAYBACK_MODE_PLAIN)
		{
			if(vj_has_video(v) )
				veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_PLAIN, 0);
			else
				veejay_msg(VEEJAY_MSG_ERROR,
				 "There are no video files in the editlist");
		}
	}
}

void vj_event_clip_new(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(CLIP_PLAYING(v) || PLAIN_PLAYING(v)) 
	{
		int args[2];
		char *s = NULL;
		int num_frames = v->edit_list->video_frames-1;
		P_A(args,s,format,ap);

		if(args[0] < 0)
		{
			/* count from last frame */
			int nframe = args[0];
			args[0] = v->edit_list->video_frames - 1 + nframe;
		}
		//if(args[0] == 0)
		//	args[0] = 1;

		if(args[1] == 0)
		{
			args[1] = v->edit_list->video_frames - 1;
		}

		if(args[0] >= 0 && args[1] > 0 && args[0] <= args[1] && args[0] <= num_frames &&
			args[1] <= num_frames ) 
		{
			clip_info *skel = clip_skeleton_new(args[0],args[1]);
			if(clip_store(skel)==0) 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Created new clip [%d]", skel->clip_id);
				clip_set_looptype(skel->clip_id,1);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Invalid frame range given : %d - %d , range is %d - %d",
				args[0],args[1], 1,num_frames);
		}
	}
	else 
	{
		p_invalid_mode();
	}
}
#ifdef HAVE_SDL
void	vj_event_fullscreen(void *ptr, const char format[], va_list ap )
{
	veejay_t *v = (veejay_t*) ptr;
	const char *caption = "Veejay";
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);
	// parsed display num!! -> index of SDL array

	//int id = args[0];
	int id = 0;
	int status = args[0];

	if( status < 0 || status > 1 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid argument passed to FULLSCREEN");
		return;
	}
	
	if( status != v->settings->full_screen[id] )
	{
		v->settings->full_screen[id] = status;

		vj_sdl_free(v->sdl[id]);
		vj_sdl_init(v->sdl[id],
			v->edit_list->video_width,
			v->edit_list->video_height,
			caption,
			1,
			v->settings->full_screen[id]
		);
	}
	veejay_msg(VEEJAY_MSG_INFO,"Video screen is %s", (v->settings->full_screen[id] ? "full screen" : "windowed"));
	
}

void vj_event_set_screen_size(void *ptr, const char format[], va_list ap) 
{
	int args[5];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);

	int id = 0;
	int w  = args[0];
	int h  = args[1];
        int x  = args[2];
        int y  = args[3];

	// multiple sdl screen needs fixing
	const char *title = "Veejay";
	
	if( w < 0 || w > 4096 || h < 0 || h > 4096 || x < 0 || y < 0 )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid parameters");
		return;
	}

	if( v->sdl[id] )
	{
		vj_sdl_free( v->sdl[id] );
		free(v->sdl[id]);
		v->sdl[id] = NULL;
	}

	if(v->sdl[id]==NULL)	
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "Allocate screen %d: %d x %d", id, v->video_output_width,
				v->video_output_height );
		v->sdl[id] = vj_sdl_allocate( v->video_output_width,
					      v->video_output_height,
					      v->pixel_format );
	}

	vj_sdl_init( v->sdl[id],w, h, title, 1, v->settings->full_screen[id] );
	if(x > 0 && y > 0 )
		vj_sdl_set_geometry(v->sdl[id],x,y);

}
#endif

void vj_event_play_stop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*) ptr;
	if(!STREAM_PLAYING(v))
	{
		int speed = v->settings->current_playback_speed;
		veejay_set_speed(v, (speed == 0 ? 1 : 0  ));
		veejay_msg(VEEJAY_MSG_INFO,"Video is %s", (speed==0 ? "paused" : "playing"));
	}
}


void vj_event_play_reverse(void *ptr,const char format[],va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(!STREAM_PLAYING(v))
	{
		int speed = v->settings->current_playback_speed;
		if( speed == 0 ) speed = -1;
		else 
			if(speed > 0) speed = -(speed);
		veejay_set_speed(v,
				speed );

		veejay_msg(VEEJAY_MSG_INFO, "Video is playing in reverse at speed %d.", speed);
	}
}

void vj_event_play_forward(void *ptr, const char format[],va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(!STREAM_PLAYING(v))
	{
		int speed = v->settings->current_playback_speed;
		if(speed == 0) speed = 1;
		else if(speed < 0 ) speed = -1 * speed;

	 	veejay_set_speed(v,
			speed );  

		veejay_msg(VEEJAY_MSG_INFO, "Video is playing forward at speed %d" ,speed);
	}
}

void vj_event_play_speed(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	if(!STREAM_PLAYING(v))
	{
		char *s = NULL;
		int speed = 0;
		P_A(args,s,format,ap);
		veejay_set_speed(v, args[0] );
		speed = v->settings->current_playback_speed;
		veejay_msg(VEEJAY_MSG_INFO, "Video is playing at speed %d now (%s)",
			speed, speed == 0 ? "paused" : speed < 0 ? "reverse" : "forward" );
	}
}

void vj_event_play_slow(void *ptr, const char format[],va_list ap)
{
	int args[1];
	veejay_t *v = (veejay_t*)ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	
	if(PLAIN_PLAYING(v) || CLIP_PLAYING(v))
	{
		if(veejay_set_framedup(v, args[0]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Video frame will be duplicated %d to output",args[0]);
		}
	}
	
}


void vj_event_set_frame(void *ptr, const char format[], va_list ap)
{
	int args[1];
	veejay_t *v = (veejay_t*) ptr;
	if(!STREAM_PLAYING(v))
	{
		video_playback_setup *s = v->settings;
		char *str = NULL;
		P_A(args,str,format,ap);
		if(args[0] == -1 )
			args[0] = v->edit_list->video_frames - 1;
		veejay_set_frame(v, args[0]);
		veejay_msg(VEEJAY_MSG_INFO, "Video frame %d set",s->current_frame_num);
	}
}

void vj_event_inc_frame(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A( args,s,format, ap );
	if(!STREAM_PLAYING(v))
	{
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num + args[0]));
	veejay_msg(VEEJAY_MSG_INFO, "Skipped frame");
	}
}

void vj_event_dec_frame(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *) ptr;
	int args[1];
	char *s = NULL;
	P_A( args,s,format, ap );
	if(!STREAM_PLAYING(v))
	{
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num - args[0]));
	veejay_msg(VEEJAY_MSG_INFO, "Previous frame");
	}
}

void vj_event_prev_second(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;	
	int args[1];
	char *s = NULL;
	P_A( args,s,format, ap );
	if(!STREAM_PLAYING(v))
	{
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num - (int) 
			    (args[0] * v->edit_list->video_fps)));
	
	}
}

void vj_event_next_second(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[1];
	char *str = NULL;
	P_A( args,str,format, ap );
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num + (int)
			     ( args[0] * v->edit_list->video_fps)));
}


void vj_event_clip_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	video_playback_setup *s = v->settings;
	if(CLIP_PLAYING(v) || PLAIN_PLAYING(v)) 
	{
		v->uc->clip_start = s->current_frame_num;
		veejay_msg(VEEJAY_MSG_INFO, "Clip starting position set to frame %ld", v->uc->clip_start);
	}	
	else
	{
		p_invalid_mode();
	}
}



void vj_event_clip_end(void *ptr, const char format[] , va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	video_playback_setup *s = v->settings;
	if(CLIP_PLAYING(v) || PLAIN_PLAYING(v))
	{
		v->uc->clip_end = s->current_frame_num;
		if( v->uc->clip_end > v->uc->clip_start) {
			clip_info *skel = clip_skeleton_new(v->uc->clip_start,v->uc->clip_end);
			if(clip_store(skel)==0) {
				veejay_msg(VEEJAY_MSG_INFO,"Created new clip [%d]", skel->clip_id);
				clip_set_looptype(skel->clip_id, 1);	
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR,"%s %d: Cannot store new clip!",__FILE__,__LINE__);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip ending position before starting position. Cannot create new clip");
		}
	}
	else
	{
		p_invalid_mode();
	}		
}
 
void vj_event_goto_end(void *ptr, const char format[], va_list ap)
{
  	veejay_t *v = (veejay_t*) ptr;
	if(STREAM_PLAYING(v))
	{
		p_invalid_mode();
		return;
	} 
 	if(CLIP_PLAYING(v))
  	{	
		veejay_set_frame(v, clip_get_endFrame(v->uc->clip_id));
  	}
  	if(PLAIN_PLAYING(v)) 
 	{
		veejay_set_frame(v,(v->edit_list->video_frames-1));
  	}
}

void vj_event_goto_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(STREAM_PLAYING(v))
	{
		p_invalid_mode();
		return;
	}
  	if( CLIP_PLAYING(v))
	{
		veejay_set_frame(v, clip_get_startFrame(v->uc->clip_id));
  	}
  	if ( PLAIN_PLAYING(v))
	{
		veejay_set_frame(v,0);
  	}
}

void vj_event_clip_set_loop_type(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);

	if(!CLIP_PLAYING(v)) return;

	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if(args[1] == -1)
	{
		if(clip_exists(args[0]))
		{
			if(clip_get_looptype(args[0])==2)
			{
				int lp;
				clip_set_looptype(args[0],1);
				lp = clip_get_looptype(args[0]);
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d loop type is now %s",args[0],
		  			( lp==1 ? "Normal Looping" : (lp==2 ? "Pingpong Looping" : "No Looping" ) ) );
				return;
			}
			else
			{
				int lp;
				clip_set_looptype(args[0],2);
				lp = clip_get_looptype(args[0]);
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d loop type is now %s",args[0],
		  			( lp==1 ? "Normal Looping" : lp==2 ? "Pingpong Looping" : "No Looping" ) );
				return;
			}
		}
		else
		{
			p_no_clip(args[0]);
			return;
		}
	}

	if(args[1] >= 0 && args[1] <= 2) 
	{
		if(clip_exists(args[0]))
		{
			int lp;
			clip_set_looptype( args[0] , args[1]);
			lp = clip_get_looptype(args[0]);
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d loop type is now %s",args[0],
			  ( args[1]==1 ? "Normal Looping" : lp==2 ? "Pingpong Looping" : "No Looping" ) );
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exist or invalid looptype %d",args[1],args[0]);
	}
}

void vj_event_clip_set_speed(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args, s, format, ap);

	if(args[0] == -1)
	{
		args[0] = clip_size() - 1;
	}

	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}

	if(clip_exists(args[0]))
	{	
		if( CLIP_PLAYING(v))
		{
			veejay_set_speed(v, args[1] );
		}
		else
		{
			if( clip_set_speed(args[0], args[1]) != -1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d speed set to %d",args[0],args[1]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Speed %d it too high to set on clip %d !",
					args[1],args[0]); 
			}

		}
	}
	else
	{
		p_no_clip(args[0]);
	}
}

void vj_event_clip_set_marker_start(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	veejay_t *v = (veejay_t*)ptr;
	
	char *str = NULL;
	P_A(args,str,format,ap);
	
	if( args[0] == 0) 
	{
		if(CLIP_PLAYING(v))
			args[0] = v->uc->clip_id;
	}

	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		int start = 0; int end = 0;
		if ( clip_get_el_position( args[0], &start, &end ) )
		{	// marker in relative positions given !
			args[1] += start; // add sample's start position
			if( clip_set_marker_start( args[0], args[1] ) )
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker starting position at %d", args[0],args[1]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set marker position %d for clip %d (limits are %d - %d)",args[1],args[0],start,end);
			}
		}
	}	
}


void vj_event_clip_set_marker_end(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	
	char *str = NULL;
	P_A(args,str,format,ap);
	
	if( args[0] == 0 ) 
	{
		if(CLIP_PLAYING(v))
			args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		int start = 0; int end = 0;
		if ( clip_get_el_position( args[0], &start, &end ) )
		{
			args[1] = end - args[1]; // add sample's ending position
			if( clip_set_marker_end( args[0], args[1] ) )
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker ending position at position %d", args[0],args[1]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_INFO, "Marker position out side of clip boundaries");
			}
		}	
	}
}


void vj_event_clip_set_marker(void *ptr, const char format[], va_list ap) 
{
	int args[3];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	
	if( args[0] == 0) 
	{
		if(CLIP_PLAYING(v))
			args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		int start = 0;
		int end = 0;
		if( clip_get_el_position( args[0], &start, &end ) )
		{
			args[1] += start;
			args[2] = end - args[2];
			if( clip_set_marker( args[0], args[1],args[2] ) )
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker starting position at %d, ending position at %d", args[0],args[1],args[2]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set marker %d-%d for clip %d",args[1],args[2],args[0]);
			}
		}
	}	
}


void vj_event_clip_set_marker_clear(void *ptr, const char format[],va_list ap) 
{
	int args[1];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	
	if( args[0] == 0) 
	{
		if(CLIP_PLAYING(v))
			args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		if( clip_marker_clear( args[0] ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker cleared", args[0]);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot set marker %d-%d for clip %d",args[1],args[2],args[0]);
		}
	}	
}

void vj_event_clip_set_dup(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);

	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0])) 
	{
		if( clip_set_framedup( args[0], args[1] ) != -1) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d frame duplicator set to %d", args[0],args[1]);
			if( args[0] == v->uc->clip_id)
			{
			    if(veejay_set_framedup(v, args[1]))
               		    {
                       		 veejay_msg(VEEJAY_MSG_INFO,
			"Video frame will be duplicated %d to output",args[1]);
                	    }
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Cannot set frame duplicator to %d for clip %d",args[0],args[1]);
		}
	}
	else
	{
		p_no_clip(args[0]);
	}
}

void	vj_event_tag_set_descr( void *ptr, const char format[], va_list ap)
{
	char str[TAG_MAX_DESCR_LEN];
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,str,format,ap);
	if(args[0] == 0 && STREAM_PLAYING(v))
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1)
		args[0] = vj_tag_size()-1;
	if( vj_tag_set_description(args[0],str) == 1)
		veejay_msg(VEEJAY_MSG_INFO, "Streamd %d description [%s]", args[0], str );
}


void vj_event_clip_set_descr(void *ptr, const char format[], va_list ap)
{
	char str[CLIP_MAX_DESCR_LEN];
	int args[5];
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,str,format,ap);

	if( args[0] == 0 && CLIP_PLAYING(v)) 
	{
		args[0] = v->uc->clip_id;
	}

	if(args[0] == -1) args[0] = clip_size()-1;

	if(clip_set_description(args[0],str) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Clip %d description [%s]",args[0],str);
	}
}

#ifdef HAVE_XML2
void vj_event_clip_save_list(void *ptr, const char format[], va_list ap)
{
	char str[255];
	int *args = NULL;
	P_A(args,str,format,ap);
	if(clip_size()-1 < 1) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "No clips to save");
		return;
	}
	if(clip_writeToFile( str) )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Wrote %d clips to file %s", clip_size()-1, str);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error saving clips to file %s", str);
	}
}

void vj_event_clip_load_list(void *ptr, const char format[], va_list ap)
{
	char str[255];
	int *args = NULL;
	P_A( args, str, format, ap);

	if( clip_readFromFile( str ) ) 
	{
		veejay_msg(VEEJAY_MSG_INFO, "Loaded clip list [%s]", str);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error loading clip list [%s]", str);
	}
}
#endif

void 	vj_event_clip_ren_start			( 	void *ptr, 	const char format[], 	va_list ap	)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	int entry;
	int s1;
	
	char tmp[255];
	char prefix[150];
	P_A( args,str, format, ap );
	bzero(tmp,255);
	bzero(prefix,150);
	s1 = args[0];
	entry = args[1];
	if(entry < 0 || entry > CLIP_MAX_RENDER)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid renderlist entry given. Use %d - %d", 0,CLIP_MAX_RENDER);
		return;
	}
	if(s1 == 0)	
	{
		s1 = v->uc->clip_id;
		if(!CLIP_PLAYING(v))
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Not playing a clip");
			return;
		}
	}

	if(!clip_exists(s1))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Selected clip %d does not exist", s1);
		return;
	}

	if(entry == clip_get_render_entry(s1) )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Destination entry cannot be the same as source entry.");
		return;
	}	

	clip_get_description( s1, prefix );
	if(!veejay_create_temp_file(prefix, tmp))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot create file %s",
			tmp);
		return;
	}
	veejay_msg(VEEJAY_MSG_DEBUG, "Filename [%s]", tmp );


	int fformat = _recorder_format;
	if(fformat==-1)
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Set a destination video format first");
		return; 
	}
	// wrong format
	if(! veejay_prep_el( v, s1 ))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot initialize render editlist");
		return;
	}

	int nf = clip_get_endFrame( s1 ) - clip_get_startFrame( s1 );
	nf = nf / clip_get_speed(s1);
	if( clip_get_looptype(s1) == 2 )
		nf *= 2;

	if( clip_init_encoder( s1, tmp, fformat, v->edit_list, nf) == 1)
	{
		video_playback_setup *s = v->settings;
		s->clip_record_id = v->uc->clip_id;
		s->render_list = entry;
		veejay_msg(VEEJAY_MSG_INFO, "Rendering clip to render list entry %d", 
			entry );
	}
	
}

void	vj_event_clip_move_render		( 	void *ptr,	const char format[],	va_list ap )
{

	int args[2];
	char *str = NULL;
	editlist **el;
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,str,format,ap);

	int sample_id = args[0];
	int entry = args[1];

	if(CLIP_PLAYING(v))
	{
		if(sample_id==0) sample_id = v->uc->clip_id;
		if(sample_id==-1) sample_id = clip_size()-1;
	}

	if(clip_exists(sample_id))
	{
		el = (editlist**) clip_get_user_data(sample_id);
		if(el==NULL)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Nothing rendered yet");
			return;
		}
		editlist *elentry = el[entry];
		if(elentry)
		{
			char *filename = strdup( elentry->video_file_list[0] );
			//vj_el_append_video_file( v->edit_list, filename);
			if( veejay_edit_addmovie(
				v,filename,0,0,v->edit_list->video_frames - 1))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Moved history entry %d to EDL", entry );

			}		
			vj_el_free( elentry );
		}
	}

}

void	vj_event_clip_sel_render		(	void *ptr,	const char format[],	va_list ap  )
{
	int args[2];
	int entry;
	int s1;
	char *str = NULL;
	editlist **el;
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,str,format,ap);
	
	s1 = args[0];
	entry = args[1];
	if(entry < 0 || entry > CLIP_MAX_RENDER)
		return;

	if(s1 == 0 )
	{
		s1 = v->uc->clip_id;
		if(!CLIP_PLAYING(v))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Not playing a clip");
			return;
		}
	}
	if(!clip_exists(s1))
		return;

	if(entry == 0 )
	{
		if(clip_set_render_entry(s1, 0 ) )
			veejay_msg(VEEJAY_MSG_INFO, "Selected main clip %d", s1 );  
		return;
	}

	// check if entry is playable
	el = (editlist**) clip_get_user_data( s1 );
	if( el == NULL )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Nothing rendered yet for sample %d", s1 );
		return;
	}

	if(el[entry])  
	{
		if(clip_set_render_entry( s1, entry ))
		{
				veejay_msg(VEEJAY_MSG_INFO, "Selected render entry %d of clip %d", entry, s1 );
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_INFO,"Render entry %d is empty", entry);
	}

}



void vj_event_clip_rec_start( void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	int changed = 0;
	int result = 0;
	char *str = NULL;
	char prefix[150];
	P_A(args,str,format,ap);

	if(!CLIP_PLAYING(v)) 
	{
		p_invalid_mode();
		return;
	}

	char tmp[255];
	bzero(tmp,255);
	bzero(prefix,150);
	clip_get_description(v->uc->clip_id, prefix );
	if(!veejay_create_temp_file(prefix, tmp))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot create file %s",
			tmp);
		return;
	}

	if( args[0] == 0 )
	{
		int n = clip_get_speed(v->uc->clip_id);
		if( n == 0) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip was paused, forcing normal speed");
			n = 1;
		}
		else
		{
			if (n < 0 ) n = n * -1;
		}
		args[0] = clip_get_longest(v->uc->clip_id);
		changed = 1;
	}

	int format_ = _recorder_format;
	if(format_==-1)
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Set a destination video format first");
		return; 
	}
	veejay_msg(VEEJAY_MSG_DEBUG, "Video frames to record: %ld", args[0]);

	if( clip_init_encoder( v->uc->clip_id, tmp, format_, v->edit_list, args[0]) == 1)
	{
		video_playback_setup *s = v->settings;
		s->clip_record_id = v->uc->clip_id;
		if(args[1])
			s->clip_record_switch = 1;
		result = 1;
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Unable to initialize clip recorder");
	}   

	if(changed)
	{
		veejay_set_clip(v,v->uc->clip_id);
	}


	if(result == 1)
	{
		veejay_msg(VEEJAY_MSG_INFO,
			"Recording editlist frames starting from %d (%d frames and %s)", 
			v->settings->current_frame_num,
			args[0], (args[1]==1? "autoplay" : "no autoplay"));
		v->settings->clip_record = 1;
		v->settings->clip_record_switch = args[1];
	}

}

void vj_event_clip_rec_stop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	
	if( CLIP_PLAYING(v)) 
	{
		video_playback_setup *s = v->settings;
		if( clip_stop_encoder( v->uc->clip_id ) == 1 ) 
		{
			char avi_file[255];
			int start = -1;
			int destination = v->edit_list->video_frames - 1;
			v->settings->clip_record = 0;
			if( clip_get_encoded_file(v->uc->clip_id, avi_file)!=0) return;
			
			clip_reset_encoder( v->uc->clip_id );

			if( veejay_edit_addmovie(
				v,avi_file,start,destination,destination))
			{
				int len = clip_get_encoded_frames(v->uc->clip_id) - 1;
				clip_info *skel = clip_skeleton_new(destination, destination+len);		
				if(clip_store(skel)==0) 
				{
					veejay_msg(VEEJAY_MSG_INFO, "Created new clip %d from file %s",
						skel->clip_id,avi_file);
					clip_set_looptype( skel->clip_id,1);
				}	
			}		
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot add videofile %s to EditList!",avi_file);
			}

			clip_reset_encoder( v->uc->clip_id);
			s->clip_record = 0;	
			s->clip_record_id = 0;
			if(s->clip_record_switch) 
			{
				veejay_set_clip( v, clip_size()-1);
				s->clip_record_switch = 0;
				veejay_msg(VEEJAY_MSG_INFO, "Switching to clip %d (recording)", clip_size()-1);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip recorder was never started for clip %d",v->uc->clip_id);
		}
		
	}
	else 
	{
		p_invalid_mode();
	}
}


void vj_event_clip_set_num_loops(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(args[0] == 0) args[0] = v->uc->clip_id;
	if(args[0] == -1) args[0] = clip_size()-1;

	if(clip_exists(args[0]))
	{

		if(	clip_set_loops(v->uc->clip_id, args[1]))
		{	veejay_msg(VEEJAY_MSG_INFO, "Setted %d no. of loops for clip %d",
				clip_get_loops(v->uc->clip_id),args[0]);
		}
		else	
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Cannot set %d loops for clip %d",args[1],args[0]);
		}

	}
	else
	{
		p_no_clip(args[0]);
	}
}


void vj_event_clip_rel_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[4];
	//video_playback_setup *s = v->settings;
	char *str = NULL;
	int s_start;
	int s_end;

	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v))
	{

		if(args[0] == 0)
		{
			args[0] = v->uc->clip_id;
		}
	if(args[0] == -1) args[0] = clip_size()-1;	
		if(!clip_exists(args[0]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip does not exist");
			return;
		}
		
		s_start = clip_get_startFrame(args[0]) + args[1];
		s_end = clip_get_endFrame(args[0]) + args[2];

		if(s_end > v->edit_list->video_frames-1) s_end = v->edit_list->video_frames - 1;

		if( s_start >= 1 && s_end <= (v->edit_list->video_frames-1) )
		{ 
			if	(clip_set_startframe(args[0],s_start) &&
				clip_set_endframe(args[0],s_end))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip update start %d end %d",
					s_start,s_end);
			}
	
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_INFO, "Invalid playmode");
	}

}

void vj_event_clip_set_start(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	int mf;
	video_playback_setup *s = v->settings;
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;
	
	mf = veejay_el_max_frames(v, args[0]);

	if( (args[1] >= s->min_frame_num) && (args[1] <= mf) && clip_exists(args[0])) 
	{
	  if( args[1] < clip_get_endFrame(args[0])) {
		if( clip_set_startframe(args[0],args[1] ) ) {
			veejay_msg(VEEJAY_MSG_INFO, "Clip starting frame updated to frame %d",
			  clip_get_startFrame(args[0]));
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Unable to update clip %d 's starting position to %d",args[0],args[1]);
		}
	  }
	  else 
	  {
		veejay_msg(VEEJAY_MSG_ERROR, "Clip %d 's starting position %d must be greater than ending position %d.",
			args[0],args[1], clip_get_endFrame(args[0]));
	  }
	}
	else
	{
		if(!clip_exists(args[0])) p_no_clip(args[0]) else veejay_msg(VEEJAY_MSG_ERROR, "Invalid position %d given",args[1]);
	}
}

void vj_event_clip_set_end(void *ptr, const char format[] , va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	int mf;
	video_playback_setup *s = v->settings;
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[1] == -1)
	{
		args[1] = v->edit_list->video_frames-1;
	}

	mf = veejay_el_max_frames( v, args[0]);

	if( (args[1] >= s->min_frame_num) && (args[1] <= mf) && (clip_exists(args[0])))
	{
		if( args[1] >= clip_get_startFrame(v->uc->clip_id)) {
	       		if(clip_set_endframe(args[0],args[1])) {
	   			veejay_msg(VEEJAY_MSG_INFO,"Clip ending frame updated to frame %d",
	        		clip_get_endFrame(args[0]));
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Unable to update clip %d 's ending position to %d",
					args[0],args[1]);
			}
	      	}
		else 
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip %d 's ending position %d must be greater than or equal to starting position %d.",
				args[0],args[1], clip_get_startFrame(v->uc->clip_id));
		}
	}
	else
	{
		if(!clip_exists(args[0])) p_no_clip(args[0]) else veejay_msg(VEEJAY_MSG_ERROR, "Invalid position %d given",args[1]);
	}
	
}

void vj_event_clip_del(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(CLIP_PLAYING(v)) 
	{
		if(v->uc->clip_id == args[0])
		{
			veejay_msg(VEEJAY_MSG_INFO,"Cannot delete clip while playing");
		}
		else
		{
			if(clip_del(args[0]))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Deleted clip %d", args[0]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Unable to delete clip %d",args[0]);
			}
		}	
	}
	else
	{
		p_invalid_mode();
	}
}

void vj_event_clip_copy(void *ptr, const char format[] , va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(CLIP_PLAYING(v))
	{
		if( args[0] == 0 ) args[0] = v->uc->clip_id;
		if( args[0] == -1) args[0] = clip_size()-1;
	}

	if( clip_exists(args[0] ))
	{
		int new_clip = clip_copy(args[0]);
		if(new_clip)
		{
			veejay_msg(VEEJAY_MSG_INFO, "Copied clip %d to %d",args[0],new_clip);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Failed to copy clip %d.",args[0]);
		}
	}
}

void vj_event_clip_clear_all(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if( !CLIP_PLAYING(v)) 
	{
		clip_del_all();
	}
	else
	{
		p_invalid_mode();
	}
} 



void vj_event_chain_enable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		clip_set_effect_status(v->uc->clip_id, 1);
	}
	if(STREAM_PLAYING(v))
	{
		vj_tag_set_effect_status(v->uc->clip_id, 1);
	}
	veejay_msg(VEEJAY_MSG_INFO, "Effect chain is enabled");
	
}


void vj_event_chain_disable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v) )
	{
		clip_set_effect_status(v->uc->clip_id, 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on Clip %d is disabled",v->uc->clip_id);
	}
	if(STREAM_PLAYING(v) )
	{
		vj_tag_set_effect_status(v->uc->clip_id, 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on Stream %d is enabled",v->uc->clip_id);
	}

}

void vj_event_clip_chain_enable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	int args[4];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(args[0] == 0)
	{
		args[0] = v->uc->clip_id;
	}
	
	if(CLIP_PLAYING(v) && clip_exists(args[0]))
	{
		clip_set_effect_status(args[0], 1);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on Clip %d is enabled",args[0]);
	}
	
}

void	vj_event_all_clips_chain_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(CLIP_PLAYING(v))
	{
		int i;
		for(i=0; i < clip_size()-1; i++)
		{
			clip_set_effect_status( i, args[0] );
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect Chain on all clips %s", (args[0]==0 ? "Disabled" : "Enabled"));
	}
	if(STREAM_PLAYING(v))
	{
		int i;
		for(i=0; i < vj_tag_size()-1; i++)
		{
			vj_tag_set_effect_status(i,args[0]);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect Chain on all streams %s", (args[0]==0 ? "Disabled" : "Enabled"));
	}
}


void vj_event_tag_chain_enable(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[4];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(STREAM_PLAYING(v) && vj_tag_exists(args[0]))
	{
		vj_tag_set_effect_status(args[0], 1);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is enabled",args[0]);
	}

}
void vj_event_tag_chain_disable(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(STREAM_PLAYING(v) && vj_tag_exists(args[0]))
	{
		vj_tag_set_effect_status(args[0], 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is disabled",args[0]);
	}

}

void vj_event_clip_chain_disable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(args[0] == 0)
	{
		args[0] = v->uc->clip_id;
	}
	
	if(CLIP_PLAYING(v) && clip_exists(args[0]))
	{
		clip_set_effect_status(args[0], 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is disabled",args[0]);
	}
	if(STREAM_PLAYING(v) && vj_tag_exists(args[0]))
	{
		vj_tag_set_effect_status(args[0], 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is disabled",args[0]);
	}
	
}


void vj_event_chain_toggle(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		int flag = clip_get_effect_status(v->uc->clip_id);
		if(flag == 0) 
		{
			clip_set_effect_status(v->uc->clip_id,1); 
		}
		else
		{
			clip_set_effect_status(v->uc->clip_id,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain is %s.", (clip_get_effect_status(v->uc->clip_id) ? "enabled" : "disabled"));
	}
	if(STREAM_PLAYING(v))
	{
		int flag = vj_tag_get_effect_status(v->uc->clip_id);
		if(flag == 0) 	
		{
			vj_tag_set_effect_status(v->uc->clip_id,1); 
		}
		else
		{
			vj_tag_set_effect_status(v->uc->clip_id,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain is %s.", (vj_tag_get_effect_status(v->uc->clip_id) ? "enabled" : "disabled"));
	}
}	

void vj_event_chain_entry_video_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		int c = clip_get_selected_entry(v->uc->clip_id);
		int flag = clip_get_chain_status(v->uc->clip_id,c);
		if(flag == 0)
		{
			clip_set_chain_status(v->uc->clip_id, c,1);	
		}
		else
		{	
			clip_set_chain_status(v->uc->clip_id, c,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Video on chain entry %d is %s", c,
			(flag==0 ? "Disabled" : "Enabled"));
	}
	if(STREAM_PLAYING(v))
	{
		int c = vj_tag_get_selected_entry(v->uc->clip_id);
		int flag = vj_tag_get_chain_status( v->uc->clip_id,c);
		if(flag == 0)
		{
			vj_tag_set_chain_status(v->uc->clip_id, c,1);	
		}
		else
		{	
			vj_tag_set_chain_status(v->uc->clip_id, c,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Video on chain entry %d is %s", c,
			(flag==0 ? "Disabled" : "Enabled"));

	}
}

void vj_event_chain_entry_enable_video(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];	
	char *s = NULL;
	P_A(args,s,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			if(clip_set_chain_status(args[0],args[1],1) != -1)	
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d: Video on chain entry %d is %s",args[0],args[1],
					( clip_get_chain_status(args[0],args[1]) == 1 ? "Enabled" : "Disabled"));
			}
		}
	}
	if(STREAM_PLAYING(v))
	{
		if(args[0] == 0)args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0])) 
		{
			if(vj_tag_set_chain_status(args[0],args[1],1)!=-1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Stream %d: Video on chain entry %d is %s",args[0],args[1],
					vj_tag_get_chain_status(args[0],args[1]) == 1 ? "Enabled" : "Disabled" );
			}
		}
	}
}
void vj_event_chain_entry_disable_video(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);

		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			if(clip_set_chain_status(args[0],args[1],0)!=-1)
			{	
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d: Video on chain entry %d is %s",args[0],args[1],
				( clip_get_chain_status(args[0],args[1])==1 ? "Enabled" : "Disabled"));
			}
		}
	}
	if(STREAM_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;	
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);

		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(vj_tag_exists(args[0]))
		{
			if(vj_tag_set_chain_status(args[0],args[1],0)!=-1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Stream %d: Video on chain entry %d is %s",args[0],args[1],
					vj_tag_get_chain_status(args[0],args[1]) == 1 ? "Enabled" : "Disabled" );
			}
		}
	}

}

void	vj_event_manual_chain_fade(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);

	if(args[0] == 0 && (CLIP_PLAYING(v) || STREAM_PLAYING(v)) )
	{
		args[0] = v->uc->clip_id;
	}

	if( args[1] < 0 || args[1] > 255)
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Invalid opacity range %d use [0-255] ", args[1]);
		return;
	}
	args[1] = 255 - args[1];

	if( CLIP_PLAYING(v) && clip_exists(args[0])) 
	{
		if( clip_set_manual_fader( args[0], args[1] ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Set chain opacity to %d",
				clip_get_fader_val( args[0] ));
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Error setting chain opacity of clip %d to %d", args[0],args[1]);
		}
	}
	if (STREAM_PLAYING(v) && vj_tag_exists(args[0])) 
	{
		if( vj_tag_set_manual_fader( args[0], args[1] ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Set chain opacity to %d", args[0]);
		}
	}

}

void vj_event_chain_fade_in(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(args[0] == 0 && (CLIP_PLAYING(v) || STREAM_PLAYING(v)) )
	{
		args[0] = v->uc->clip_id;
	}

	if( CLIP_PLAYING(v) && clip_exists(args[0])) 
	{
		if( clip_set_fader_active( args[0], args[1],-1 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Chain Fade In from clip to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
			if(clip_get_effect_status(args[0]==0))
			{
				clip_set_effect_status(args[0], -1);
			}
		}
	}
	if (STREAM_PLAYING(v) && vj_tag_exists(args[0])) 
	{
		if( vj_tag_set_fader_active( args[0], args[1],-1 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO,"Chain Fade In from stream to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
			if(vj_tag_get_effect_status(args[0]==0))
			{
				vj_tag_set_effect_status(args[0],-1);
			}
		}
	}

}

void vj_event_chain_fade_out(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(args[0] == 0 && (CLIP_PLAYING(v) || STREAM_PLAYING(v)) )
	{
		args[0] = v->uc->clip_id;
	}

	if( CLIP_PLAYING(v) && clip_exists(args[0])) 
	{
		if( clip_set_fader_active( args[0], args[1],1 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Chain Fade Out from clip to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
		}
	}
	if (STREAM_PLAYING(v) && vj_tag_exists(args[0])) 
	{
		if( vj_tag_set_fader_active( args[0], args[1],1 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO,"Chain Fade Out from stream to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
		}
	}
}



void vj_event_chain_clear(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; 
	P_A(args,str,format,ap);

	if(args[0] == 0 && (CLIP_PLAYING(v) || STREAM_PLAYING(v)) )
	{
		args[0] = v->uc->clip_id;
	}

	if( CLIP_PLAYING(v) && clip_exists(args[0])) 
	{
		int i;
		for(i=0; i < CLIP_MAX_EFFECTS;i++)
		{
			int effect = clip_get_effect_any(args[0],i);
			if(vj_effect_is_valid(effect))
			{
				clip_chain_remove(args[0],i);
				veejay_msg(VEEJAY_MSG_INFO,"Clip %d: Deleted effect %s from entry %d",
					args[0],vj_effect_get_description(effect), i);
			}
		}
		v->uc->chain_changed = 1;
	}
	if (STREAM_PLAYING(v) && vj_tag_exists(args[0])) 
	{
		int i;
		for(i=0; i < CLIP_MAX_EFFECTS;i++)
		{
			int effect = vj_tag_get_effect_any(args[0],i);
			if(vj_effect_is_valid(effect))
			{
				vj_tag_chain_remove(args[0],i);
				veejay_msg(VEEJAY_MSG_INFO,"Stream %d: Deleted effect %s from entry %d",	
					args[0],vj_effect_get_description(effect), i);
			}
		}
		v->uc->chain_changed = 1;
	}


}

void vj_event_chain_entry_del(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			int effect = clip_get_effect_any(args[0],args[1]);
			if( vj_effect_is_valid(effect)) 
			{
				clip_chain_remove(args[0],args[1]);
				v->uc->chain_changed = 1;
				veejay_msg(VEEJAY_MSG_INFO,"Clip %d: Deleted effect %s from entry %d",
					args[0],vj_effect_get_description(effect), args[1]);
			}
		}
	}

	if (STREAM_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(vj_tag_exists(args[0])) 		
		{
			int effect = vj_tag_get_effect_any(args[0],args[1]);
			if(vj_effect_is_valid(effect))
			{
				vj_tag_chain_remove(args[0],args[1]);
				v->uc->chain_changed = 1;
				veejay_msg(VEEJAY_MSG_INFO,"Stream %d: Deleted effect %s from entry %d",	
					args[0],vj_effect_get_description(effect), args[1]);
			}
		}
	}
}

void vj_event_chain_entry_set_defaults(void *ptr, const char format[], va_list ap) 
{

}

void vj_event_chain_entry_set(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);

		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			//int real_id = vj_effect_real_to_sequence(args[2]);
			if(clip_chain_add(args[0],args[1],args[2]) != -1) 
			{
				veejay_msg(VEEJAY_MSG_DEBUG, "Clip %d chain entry %d has effect %s",
					args[0],args[1],vj_effect_get_description(args[2]));
				v->uc->chain_changed = 1;
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set effect %d on clip %d chain %d",args[2],args[0],args[1]);
			}
		}
	}
	if( STREAM_PLAYING(v)) 
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = vj_tag_size()-1;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);	

		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(vj_tag_exists(args[0]))
		{
			if(vj_tag_set_effect(args[0],args[1], args[2]) != -1)
			{
			//	veejay_msg(VEEJAY_MSG_INFO, "Stream %d chain entry %d has effect %s",
			//		args[0],args[1],vj_effect_get_description(real_id));
				v->uc->chain_changed = 1;
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set effect %d on stream %d chain %d",args[2],args[0],args[1]);
			}
		}
	}
}

void vj_event_chain_entry_select(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	if( CLIP_PLAYING(v)  )
	{
		if(args[0] >= 0 && args[0] < CLIP_MAX_EFFECTS)
		{
			if( clip_set_selected_entry( v->uc->clip_id, args[0])) 
			{
			veejay_msg(VEEJAY_MSG_INFO,"Selected entry %d [%s]",
			  clip_get_selected_entry(v->uc->clip_id), 
			  vj_effect_get_description( 
				clip_get_effect_any(v->uc->clip_id,clip_get_selected_entry(v->uc->clip_id))));
			}
		}
	}
	if ( STREAM_PLAYING(v))
	{
		if(args[0] >= 0 && args[0] < CLIP_MAX_EFFECTS)
		{
			if( vj_tag_set_selected_entry(v->uc->clip_id,args[0]))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Selected entry %d [%s]",
			 	vj_tag_get_selected_entry(v->uc->clip_id),
				vj_effect_get_description( 
					vj_tag_get_effect_any(v->uc->clip_id,vj_tag_get_selected_entry(v->uc->clip_id))));
			}
		}	
	}
}

void vj_event_entry_up(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(CLIP_PLAYING(v) || STREAM_PLAYING(v))
	{
		int effect_id=-1;
		int c=-1;
		if(CLIP_PLAYING(v))
		{
			c = clip_get_selected_entry(v->uc->clip_id) + args[0];
			if(c >= CLIP_MAX_EFFECTS) c = 0;
			clip_set_selected_entry( v->uc->clip_id, c);
			effect_id = clip_get_effect_any(v->uc->clip_id, c );
		}
		if(STREAM_PLAYING(v))
		{
			c = vj_tag_get_selected_entry(v->uc->clip_id)+args[0];
			if( c>= CLIP_MAX_EFFECTS) c = 0;
			vj_tag_set_selected_entry(v->uc->clip_id,c);
			effect_id = vj_tag_get_effect_any(v->uc->clip_id,c);
		}

		veejay_msg(VEEJAY_MSG_INFO, "Entry %d has effect %s",
			c, vj_effect_get_description(effect_id));

	}
}
void vj_event_entry_down(void *ptr, const char format[] ,va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(CLIP_PLAYING(v) || STREAM_PLAYING(v)) 
	{
		int effect_id=-1;
		int c = -1;
		
		if(CLIP_PLAYING(v))
		{
			c = clip_get_selected_entry( v->uc->clip_id ) - args[0];
			if(c < 0) c = CLIP_MAX_EFFECTS-1;
			clip_set_selected_entry( v->uc->clip_id, c);
			effect_id = clip_get_effect_any(v->uc->clip_id, c );
		}
		if(STREAM_PLAYING(v))
		{
			c = vj_tag_get_selected_entry(v->uc->clip_id) - args[0];
			if(c<0) c= CLIP_MAX_EFFECTS-1;
			vj_tag_set_selected_entry(v->uc->clip_id,c);
			effect_id = vj_tag_get_effect_any(v->uc->clip_id,c);
		}
		veejay_msg(VEEJAY_MSG_INFO , "Entry %d has effect %s",
			c, vj_effect_get_description(effect_id));
	}
}

void vj_event_chain_entry_preset(void *ptr,const char format[], va_list ap)
{
	int args[16];
	veejay_t *v = (veejay_t*)ptr;
	memset(args,0,16); 
	//P_A16(args,format,ap);
	char *str = NULL;
	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v)) 
	{
	    int num_p = 0;
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			int real_id = args[2];
			int i;
			num_p   = vj_effect_get_num_params(real_id);
			
			veejay_msg(VEEJAY_MSG_DEBUG, "Effect %d , params %d known as %s",
				real_id, num_p, vj_effect_get_description(real_id));

			if(clip_chain_add( args[0],args[1],args[2])!=-1)
			{
				int args_offset = 3;
				
				veejay_msg(VEEJAY_MSG_DEBUG, "Clip %d Chain entry %d has effect %s with %d arguments",
					args[0],args[1],vj_effect_get_description(real_id),num_p);
				for(i=0; i < num_p; i++)
				{
					if(vj_effect_valid_value(real_id,i,args[(i+args_offset)]) )
					{

				 		if(clip_set_effect_arg(args[0],args[1],i,args[(i+args_offset)] )==-1)	
						{
							"Error setting argument %d value %d for %s",
							i,
							args[(i+args_offset)],
							vj_effect_get_description(real_id);
						}
					}
				}

				if ( vj_effect_get_extra_frame( real_id ) && args[num_p + args_offset])
				{
					int source = args[num_p+4];	
					int channel_id = args[num_p+3];
					int err = 1;
					if( (source != VJ_TAG_TYPE_NONE && vj_tag_exists(channel_id))|| (source == VJ_TAG_TYPE_NONE && clip_exists(channel_id)) )
					{
						err = 0;
					}
					if(	err == 0 && clip_set_chain_source( args[0],args[1], source ) &&
				       		clip_set_chain_channel( args[0],args[1], channel_id  ))
					{
					  veejay_msg(VEEJAY_MSG_INFO, "Updated mixing channel to %s %d",
						(source == VJ_TAG_TYPE_NONE ? "clip" : "stream" ),
						channel_id);
					}
					else
					{
					  veejay_msg(VEEJAY_MSG_ERROR, "updating mixing channel (channel %d is an invalid %s?)",
					   channel_id, (source == 0 ? "stream" : "clip" ));
					}
				}
			}
		}
	}
	if( STREAM_PLAYING(v)) 
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(vj_tag_exists(v->uc->clip_id)) 
		{
			int real_id = args[2];
			int num_p   = vj_effect_get_num_params(real_id);
			int i;
		
			if(vj_tag_set_effect(args[0],args[1], args[2]) != -1)
			{
				for(i=0; i < num_p; i++) 
				{
					if(vj_effect_valid_value(real_id, i, args[i+3]) )
					{
						if(vj_tag_set_effect_arg(args[0],args[1],i,args[i+3]) == -1)
						{
							veejay_msg(VEEJAY_MSG_ERROR, "setting argument %d value %d for  %s",
								i,
								args[i+3],
								vj_effect_get_description(real_id));
						}
					}
					else
					{
						veejay_msg(VEEJAY_MSG_ERROR, "Parameter %d value %d is invalid for effect %d (%d-%d)",
							i,args[(i+3)], real_id,
							vj_effect_get_min_limit(real_id,i),
							vj_effect_get_max_limit(real_id,i));
					}
				}
				v->uc->chain_changed = 1;
			}

			if( args[ num_p + 3] && vj_effect_get_extra_frame(real_id) )
			{
				int channel_id = args[num_p + 3];
				int source = args[ num_p + 4];
				int err = 1;

				if( (source != VJ_TAG_TYPE_NONE && vj_tag_exists(channel_id))|| (source == VJ_TAG_TYPE_NONE && clip_exists(channel_id)) )
				{
					err = 0;
				}

				if( err == 0 && vj_tag_set_chain_source( args[0],args[1], source ) &&
				    vj_tag_set_chain_channel( args[0],args[1], channel_id ))
				{
					veejay_msg(VEEJAY_MSG_INFO,"Updated mixing channel to %s %d",
						(source == VJ_TAG_TYPE_NONE ? "clip" : "stream"), channel_id  );
				}
				else
				{
					  veejay_msg(VEEJAY_MSG_ERROR, "updating mixing channel (channel %d is an invalid %s?)",
					   channel_id, (source == 0 ? "stream" : "clip" ));
				}
			}
		}
	}

}

void vj_event_chain_entry_src_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		int entry = clip_get_selected_entry(v->uc->clip_id);
		int src = clip_get_chain_source(v->uc->clip_id, entry);
		int cha = clip_get_chain_channel( v->uc->clip_id, entry );
		if(src == 0 ) // source is clip, toggle to stream
		{
			if(!vj_tag_exists(cha))
			{
				cha =vj_tag_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			veejay_msg(VEEJAY_MSG_DEBUG, "Switched from source Clip to Stream");
			src = vj_tag_get_type(cha);
		}
		else
		{
			src = 0; // source is stream, toggle to clip
			if(!clip_exists(cha))
			{
				cha = clip_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}
			}
			veejay_msg(VEEJAY_MSG_DEBUG, "Switched from source Stream to Clip");
		}
		clip_set_chain_source( v->uc->clip_id, entry, src );
		clip_set_chain_channel(v->uc->clip_id,entry,cha);
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses %s %d", entry,(src==VJ_TAG_TYPE_NONE ? "Clip":"Stream"), cha);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
	} 

	if(STREAM_PLAYING(v))
	{
		int entry = vj_tag_get_selected_entry(v->uc->clip_id);
		int src = vj_tag_get_chain_source(v->uc->clip_id, entry);
		int cha = vj_tag_get_chain_channel( v->uc->clip_id, entry );
		char description[100];

		if(src == VJ_TAG_TYPE_NONE ) 
		{
			if(!vj_tag_exists(cha))
			{
				cha = vj_tag_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			src = vj_tag_get_type(cha);
		}
		else
		{
			src = 0;
			if(!clip_exists(cha))
			{
				cha = clip_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}
			}
		}
		vj_tag_set_chain_source( v->uc->clip_id, entry, src );
		vj_tag_set_chain_channel(v->uc->clip_id,entry,cha);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

		vj_tag_get_descriptive(cha, description);
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses channel %d (%s)", entry, cha,description);
	} 
}

void vj_event_chain_entry_source(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[3];
	char *str = NULL;
	P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			int src = args[2];
			int c = clip_get_chain_channel(args[0],args[1]);
			if(src == VJ_TAG_TYPE_NONE)
			{
				if(!clip_exists(c))
				{
					c = clip_size()-1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a clip first\n");
						return;
					}
				}
			}
			else
			{
				if(!vj_tag_exists(c) )
				{
					c = vj_tag_size() - 1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a stream first (there are none)");
						return;
					}
					src = vj_tag_get_type(c);
				}
			}

			if(c > 0)
			{
			   clip_set_chain_channel(args[0],args[1], c);
			   clip_set_chain_source (args[0],args[1],src);

				veejay_msg(VEEJAY_MSG_INFO, "Mixing with source (%s %d)", 
					src == VJ_TAG_TYPE_NONE ? "clip" : "stream",c);
				if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

			}
				
		}
	}
	if(STREAM_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(vj_tag_exists(args[0]))
		{
			int src = args[2];
			int c = vj_tag_get_chain_channel(args[0],args[1]);

			if(src == VJ_TAG_TYPE_NONE)
			{
				if(!clip_exists(c))
				{
					c = clip_size()-1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a clip first\n");
						return;
					}
				}
			}
			else
			{
				if(!vj_tag_exists(c) )
				{
					c = vj_tag_size() - 1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a stream first (there are none)");
						return;
					}
					src = vj_tag_get_type(c);
				}
			}

			if(c > 0)
			{
			   vj_tag_set_chain_channel(args[0],args[1], c);
			   vj_tag_set_chain_source (args[0],args[1],src);
				veejay_msg(VEEJAY_MSG_INFO, "Mixing with source (%s %d)", 
					src==VJ_TAG_TYPE_NONE ? "clip" : "stream",c);
			if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

			}	
		}
	}
}

void vj_event_chain_entry_channel_dec(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	//DUMP_ARG(args);

	if(CLIP_PLAYING(v))
	{
		int entry = clip_get_selected_entry(v->uc->clip_id);
		int cha = clip_get_chain_channel(v->uc->clip_id,entry);
		int src = clip_get_chain_source(v->uc->clip_id,entry);

		if(src==VJ_TAG_TYPE_NONE)
		{	//decrease clip id
			if(cha <= 1)
			{
				cha = clip_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}		
			}
			else
			{
				cha = cha - args[0];
			}
		}
		else	
		{
			if( cha <= 1)
			{
				cha = vj_tag_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			else
			{
				cha = cha - args[0];
			}
			src = vj_tag_get_type( cha );
			clip_set_chain_source( v->uc->clip_id,entry,src);
		}
		clip_set_chain_channel( v->uc->clip_id, entry, cha );
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses %s %d",entry,
				(src==VJ_TAG_TYPE_NONE ? "Clip" : "Stream"),cha);
			 
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

	}
	if(STREAM_PLAYING(v))
	{
		int entry = vj_tag_get_selected_entry(v->uc->clip_id);
		int cha   = vj_tag_get_chain_channel(v->uc->clip_id,entry);
		int src   = vj_tag_get_chain_source(v->uc->clip_id,entry);
		char description[100];
		if(src==VJ_TAG_TYPE_NONE)
		{	//decrease clip id
			if(cha <= 1)
			{
				cha = clip_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}		
			}
			else
			{
				cha = cha - args[0];
			}
		}
		else	
		{
			if( cha <= 1)
			{
				cha = vj_tag_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			else
			{
				cha = cha - args[0];
			}
			src = vj_tag_get_type( cha );
			vj_tag_set_chain_source( v->uc->clip_id, entry, src);
		}
		vj_tag_set_chain_channel( v->uc->clip_id, entry, cha );
		vj_tag_get_descriptive( cha, description);

		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses Stream %d (%s)",entry,cha,description);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
 
	}

}

void vj_event_chain_entry_channel_inc(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	//DUMP_ARG(args);

	if(CLIP_PLAYING(v))
	{
		int entry = clip_get_selected_entry(v->uc->clip_id);
		int cha = clip_get_chain_channel(v->uc->clip_id,entry);
		int src = clip_get_chain_source(v->uc->clip_id,entry);
		if(src==VJ_TAG_TYPE_NONE)
		{
			int num_c = clip_size()-1;
			if(num_c <= 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
				return;
			}
			//decrease clip id
			if(cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
		}
		else	
		{
			int num_c = vj_tag_size()-1;
			if(num_c <=0 )
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");	
				return;
			}
			if( cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
			src = vj_tag_get_type( cha );
			clip_set_chain_source( v->uc->clip_id, entry,src );
		}
		clip_set_chain_channel( v->uc->clip_id, entry, cha );
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses %s %d",entry,
			(src==VJ_TAG_TYPE_NONE ? "Clip" : "Stream"),cha);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
	
			 
	}
	if(STREAM_PLAYING(v))
	{
		int entry = vj_tag_get_selected_entry(v->uc->clip_id);
		int cha   = vj_tag_get_chain_channel(v->uc->clip_id,entry);
		int src   = vj_tag_get_chain_source(v->uc->clip_id,entry);
		char description[100];
		if(src==VJ_TAG_TYPE_NONE)
		{
			int num_c = clip_size()-1;
			if(num_c <= 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
				return;
			}
			//decrease clip id
			if(cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
		}
		else	
		{
			int num_c = vj_tag_size()-1;
			if(num_c <=0 )
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");	
				return;
			}
			if( cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
			src = vj_tag_get_type( cha );
			vj_tag_set_chain_source( v->uc->clip_id, entry, src);
		}

		vj_tag_set_chain_channel( v->uc->clip_id, entry, cha );
		vj_tag_get_descriptive( cha, description);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses Stream %d (%s)",entry,
			vj_tag_get_chain_channel(v->uc->clip_id,entry),description);
	}
}

void vj_event_chain_entry_channel(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);
	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of bounds: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			int src = clip_get_chain_source( args[0],args[1]);
			int err = 1;
			if(src == VJ_TAG_TYPE_NONE && clip_exists(args[2]))
			{
				err = 0;
			}
			if(src != VJ_TAG_TYPE_NONE && vj_tag_exists(args[2]))
			{
				err = 0;
			}	
			if(err == 0 && clip_set_chain_channel(args[0],args[1], args[2])>= 0)	
			{
				veejay_msg(VEEJAY_MSG_INFO, "Selected input channel (%s %d)",
				  (src == VJ_TAG_TYPE_NONE ? "clip" : "stream"),args[2]);
				if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Invalid channel (%s %d) given",
					(src ==VJ_TAG_TYPE_NONE ? "clip" : "stream") , args[2]);
			}
		}
	}
	if(STREAM_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(vj_tag_exists(args[0]))
		{
			int src = vj_tag_get_chain_source(args[0],args[1]);
			int err = 1;
			if( src == VJ_TAG_TYPE_NONE && clip_exists( args[2]))
				err = 0;
			if( src != VJ_TAG_TYPE_NONE && vj_tag_exists( args[2] ))
				err = 0;

			if( err == 0 && vj_tag_set_chain_channel(args[0],args[1],args[2])>=0)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Selected input channel (%s %d)",
				(src==VJ_TAG_TYPE_NONE ? "clip" : "stream"), args[2]);
				if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Invalid channel (%s %d) given",
					(src ==VJ_TAG_TYPE_NONE ? "clip" : "stream") , args[2]);
			}
		}
	}
}

void vj_event_chain_entry_srccha(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[4];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			int source = args[2];
			int channel_id = args[3];
			int err = 1;
			if( source == VJ_TAG_TYPE_NONE && clip_exists(channel_id))
				err = 0;
			if( source != VJ_TAG_TYPE_NONE && vj_tag_exists(channel_id))
				err = 0;

	
			if(	err == 0 &&
				clip_set_chain_source(args[0],args[1],source)!=-1 &&
				clip_set_chain_channel(args[0],args[1],channel_id) != -1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Selected input channel (%s %d) to mix in",
					(source == VJ_TAG_TYPE_NONE ? "clip" : "stream") , channel_id);
				if(v->no_bezerk) veejay_set_clip(v,v->uc->clip_id);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Invalid channel (%s %d) given",
					(source ==VJ_TAG_TYPE_NONE ? "clip" : "stream") , args[2]);
			}
		}
	}
	if(STREAM_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

	 	if(vj_tag_exists(args[0])) 
		{
			int source = args[2];
			int channel_id = args[3];
			int err = 1;
			if( source == VJ_TAG_TYPE_NONE && clip_exists(channel_id))
				err = 0;
			if( source != VJ_TAG_TYPE_NONE && vj_tag_exists(channel_id))
				err = 0;

	
			if(	err == 0 &&
				vj_tag_set_chain_source(args[0],args[1],source)!=-1 &&
				vj_tag_set_chain_channel(args[0],args[1],channel_id) != -1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Selected input channel (%s %d) to mix in",
					(source == VJ_TAG_TYPE_NONE ? "clip" : "stream") , channel_id);
				if(v->no_bezerk) veejay_set_clip(v,v->uc->clip_id);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Invalid channel (%s %d) given",
					(source ==VJ_TAG_TYPE_NONE ? "clip" : "stream") , args[2]);
			}
		}	
	}

}


void vj_event_chain_arg_inc(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
		int c = clip_get_selected_entry(v->uc->clip_id);
		int effect = clip_get_effect_any(v->uc->clip_id, c);
		int val = clip_get_effect_arg(v->uc->clip_id,c,args[0]);
		if ( vj_effect_is_valid( effect  ) )
		{

			int tval = val + args[1];
			if( tval > vj_effect_get_max_limit( effect,args[0] ) )
				tval = vj_effect_get_min_limit( effect,args[0]);
			else
				if( tval < vj_effect_get_min_limit( effect,args[0] ) )
					tval = vj_effect_get_max_limit( effect,args[0] );
					
		   if( vj_effect_valid_value( effect, args[0],tval ) )
		   {
			if(clip_set_effect_arg( v->uc->clip_id, c,args[0],(val+args[1]))!=-1 )
			{
				veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d value %d",args[0],(val+args[1]));
			}
		   }
		   if(clip_set_effect_arg( v->uc->clip_id, c,args[0],tval) )
		   {
			veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d value %d",args[0],tval);
		   }
				
		}
	}
	if(STREAM_PLAYING(v)) 
	{
		int c = vj_tag_get_selected_entry(v->uc->clip_id);
		int effect = vj_tag_get_effect_any(v->uc->clip_id, c);
		int val = vj_tag_get_effect_arg(v->uc->clip_id, c, args[0]);

		int tval = val + args[1];

		if( tval > vj_effect_get_max_limit( effect,args[0] ))
			tval = vj_effect_get_min_limit( effect,args[0] );
		else
			if( tval < vj_effect_get_min_limit( effect,args[0] ))
				tval = vj_effect_get_max_limit( effect,args[0] );

	
		if(vj_tag_set_effect_arg(v->uc->clip_id, c, args[0], tval) )
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d value %d",args[0], tval );
		}
	}
}

void vj_event_chain_entry_set_arg_val(void *ptr, const char format[], va_list ap)
{
	int args[4];
	veejay_t *v = (veejay_t*)ptr;
	char *str = NULL; P_A(args,str,format,ap);
	
	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(clip_exists(args[0]))
		{
			int effect = clip_get_effect_any( args[0], args[1] );
			if( vj_effect_valid_value(effect,args[2],args[3]) )
			{
				if(clip_set_effect_arg( args[0], args[1], args[2], args[3])) {
				  veejay_msg(VEEJAY_MSG_INFO, "Set parameter %d to %d on Entry %d of Clip %d", args[2], args[3],args[1],args[0]);
				}
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR,"Tried to set invalid parameter value/type: %d %d for effect %d on entry %d",
		args[2],args[3],effect,args[1]);
			}
		} else { veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exist", args[0]); }	
	}
	if(STREAM_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(v_chi(args[1]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Chain index out of boundaries: %d", args[1]);
			return;
		}

		if(vj_tag_exists(args[0]))
		{
			int effect = vj_tag_get_effect_any(args[0],args[1] );
			if ( vj_effect_valid_value( effect,args[2],args[3] ) )
			{
				if(vj_tag_set_effect_arg(args[0],args[1],args[2],args[3])) {
					veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d to %d on Entry %d of Stream %d", args[2],args[3],args[2],args[1]);
				}
			}
			else {
			veejay_msg(VEEJAY_MSG_ERROR, "Tried to set invalid parameter value/type : %d %d",
				args[2],args[3]);
			}
		}
		else {
			veejay_msg(VEEJAY_MSG_ERROR,"Stream %d does not exist", args[0]);
		}
	}
}

void vj_event_el_cut(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if ( CLIP_PLAYING(v) || STREAM_PLAYING(v)) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot cut frames in this playback mode");
		return;
	}

	if( PLAIN_PLAYING(v)) 
	{
		if(clip_size()>0)
			veejay_msg(VEEJAY_MSG_WARNING, "Cutting frames from the Edit List to a buffer affects your clips");

		if(veejay_edit_cut( v, args[0], args[1] ))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Cut frames %d-%d into buffer",args[0],args[1]);
		}
	}
}

void vj_event_el_copy(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if ( CLIP_PLAYING(v) || STREAM_PLAYING(v)) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot copy frames in this playback mode");
		return;
	}

	if( PLAIN_PLAYING(v)) 
	{
		if(clip_size()>0)
			veejay_msg(VEEJAY_MSG_WARNING, "Copying frames to the Edit List affects your clips");

		if(veejay_edit_copy( v, args[0],args[1] )) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Copy frames %d-%d into buffer",args[0],args[1]);
		}
	}

}

void vj_event_el_del(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if ( CLIP_PLAYING(v) || STREAM_PLAYING(v)) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot delete frames in this playback mode");
		return;
	}

	if( PLAIN_PLAYING(v)) 
	{
		if(clip_size()>0)
			veejay_msg(VEEJAY_MSG_WARNING, "Deleting frames from the Edit List affects your clips!");
 
		if(veejay_edit_delete(v, args[0],args[1])) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Deleted frames %d-%d into buffer", args[0],args[1]);
		}
	}
}

void vj_event_el_crop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if ( CLIP_PLAYING(v) || STREAM_PLAYING(v)) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot delete frames in this playback mode");
		return;
	}

	if( PLAIN_PLAYING(v)) 
	{
		if(args[0] >= 1 && args[1] <= v->edit_list->video_frames-1 && args[1] >= 1 && args[1] > args[0] && args[1] <= 
			v->edit_list->video_frames-1) 
		{

			if(clip_size()>0)
				veejay_msg(VEEJAY_MSG_WARNING, "Deleting frames from the Edit List affects your clips!");

			if(veejay_edit_delete(v, 1, args[0]) && veejay_edit_delete(v, args[1], v->edit_list->video_frames-1) )
			{
				veejay_set_frame(v,1);
				veejay_msg(VEEJAY_MSG_INFO, "Cropped frames %d-%d", args[0],args[1]);
			}
		}
	}
}

void vj_event_el_paste_at(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	if ( CLIP_PLAYING(v) || STREAM_PLAYING(v)) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot paste frames in this playback mode");
		return;
	}


	if( PLAIN_PLAYING(v)) 
	{
		if(clip_size()>0)
			veejay_msg(VEEJAY_MSG_WARNING, "Pasting frames to the Edit List affects your clips!");


		if( args[0] >= 0 && args[0] <= v->edit_list->video_frames-1)
		{		
			if( veejay_edit_paste( v, args[0] ) ) 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Pasted buffer at frame %d",args[0]);
			}
		}
	}
}

void vj_event_el_save_editlist(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	char str[1024];
	//int *args = NULL;
 	int args[2] = {0,0};
	P_A(args,str,format,ap);
	if( veejay_save_all(v, str,args[0],args[1]) )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Saved EditList as %s",str);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Unable to save EditList as %s",str);
	}
}

void vj_event_el_load_editlist(void *ptr, const char format[], va_list ap)
{
	veejay_msg(VEEJAY_MSG_ERROR, "EditList: Load not implemented");
}

void vj_event_el_add_video(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int start = -1;
	int destination = v->edit_list->video_frames-1;
	char str[1024];
	int *args = NULL;
	P_A(args,str,format,ap);


	if ( veejay_edit_addmovie(v,str,start,destination,destination))
	{
		veejay_msg(VEEJAY_MSG_INFO, "Appended video file %s to EditList",str); 
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot append file %s to EditList",str);
	}
}

void vj_event_el_add_video_clip(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int start = -1;
	int destination = v->edit_list->video_frames-1;
	char str[1024];
	int *args = NULL;
	P_A(args,str,format,ap);

	if ( veejay_edit_addmovie(v,str,start,destination,destination))
	{
		int start_pos = destination;
		int end_pos = v->edit_list->video_frames-1;
		clip_info *skel = clip_skeleton_new(start_pos,end_pos);
		if(clip_store(skel) == 0)
		{
			veejay_msg(VEEJAY_MSG_INFO, "Appended video file %s to EditList as new clip %d",str,skel->clip_id); 
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Appended file %s to EditList",str);
	}
	
}

void vj_event_tag_del(void *ptr, const char format[] , va_list ap ) 
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	
	if(STREAM_PLAYING(v) && v->uc->clip_id == args[0])
	{
		veejay_msg(VEEJAY_MSG_INFO,"Cannot delete stream while playing");
	}
	else 
	{
		if(vj_tag_exists(args[0]))	
		{
			if(vj_tag_del(args[0]))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Deleted stream %d", args[0]);
			}
		}	
	}
}

void vj_event_tag_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);
	if(STREAM_PLAYING(v))
	{
		int active = vj_tag_get_active(v->uc->clip_id);
		vj_tag_set_active( v->uc->clip_id, !active);
		veejay_msg(VEEJAY_MSG_INFO, "Stream is %s", (vj_tag_get_active(v->uc->clip_id) ? "active" : "disabled"));
	}
}

void vj_event_tag_new_avformat(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	char str[255];
	int *args = NULL;
	P_A(args,str,format,ap);

	if( veejay_create_tag(v, VJ_TAG_TYPE_AVFORMAT, str, v->nstreams,0,0) != 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Unable to create new FFmpeg stream");
	}	
}
#ifdef SUPPORT_READ_DV2
void	vj_event_tag_new_dv1394(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);

	if(args[0] == -1) args[0] = 63;
	veejay_msg(VEEJAY_MSG_DEBUG, "Try channel %d", args[0]);
	if( veejay_create_tag(v, VJ_TAG_TYPE_DV1394, "/dev/dv1394", v->nstreams,0, args[0]) != 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Unable to create new DV1394 stream");
	}
}
#endif

#ifdef HAVE_V4L
void vj_event_tag_new_v4l(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	char *str = NULL;
	int args[2];
	char filename[255];
	P_A(args,str,format,ap);

	sprintf(filename, "video%d", args[0]);

	if( veejay_create_tag(v, VJ_TAG_TYPE_V4L, filename, v->nstreams,0,args[1]) != 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Unable to create new Video4Linux stream ");
	}	
}
#endif
void vj_event_tag_new_net(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;

	char str[255];
	int args[2];

	P_A(args,str,format,ap);
	if( veejay_create_tag(v, VJ_TAG_TYPE_NET, str, v->nstreams, 0,args[0]) != 0 )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Unable to create new Network stream");
	}
}

void vj_event_tag_new_mcast(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;

	char str[255];
	int args[2];

	P_A(args,str,format,ap);
	int id = veejay_create_tag(v, VJ_TAG_TYPE_MCAST, str, v->nstreams, 0,args[0]);
	if( id <= 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Unable to create new multicast stream");
	}

}



void vj_event_tag_new_color(void *ptr, const char format[], va_list ap)
{
	veejay_t *v= (veejay_t*) ptr;
	char *str=NULL;
	int args[4];
	P_A(args,str,format,ap);

	int i;
	for(i = 0 ; i < 3; i ++ )
		CLAMPVAL( args[i] );

	
	int id =  vj_tag_new( VJ_TAG_TYPE_COLOR, NULL, -1, v->edit_list,v->pixel_format, -1 );
	if(id > 0)
	{
		vj_tag_set_stream_color( id, args[0],args[1],args[2] );
	}	
}

void vj_event_tag_new_y4m(void *ptr, const char format[], va_list ap)
{
	veejay_t *v= (veejay_t*) ptr;
	char str[255];
	int *args = NULL;
	P_A(args,str,format,ap);
	if( veejay_create_tag(v, VJ_TAG_TYPE_YUV4MPEG, str, v->nstreams,0,0) != 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Unable to create new Yuv4mpeg stream");
	}
}
#ifdef HAVE_V4L
void vj_event_v4l_set_brightness(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0]==0) args[0] = v->uc->clip_id;
	if(args[0]==-1) args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && STREAM_PLAYING(v))
	{
		if(vj_tag_set_brightness(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set brightness to %d",args[1]);
		}
	}
	
}
// 159, 164 for white
void	vj_event_v4l_get_info(void *ptr, const char format[] , va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0]==0) args[0] = v->uc->clip_id;
	if(args[0]==-1) args[0] = vj_tag_size()-1;

	char send_msg[33];
	char message[30];
	bzero(send_msg, sizeof(send_msg));
	bzero(message,sizeof(message));

	if(vj_tag_exists(args[0]))
	{
		int values[5];
		memset(values,0,sizeof(values));
		if(vj_tag_get_v4l_properties( args[0], &values[0], &values[1], &values[2], &values[3],
				&values[4]))
		{
			sprintf(message, "%05d%05d%05d%05d%05d",
				values[0],values[1],values[2],values[3],values[4] );
		}
	}
	FORMAT_MSG(send_msg, message);
	SEND_MSG( v,send_msg );
}


void vj_event_v4l_set_contrast(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0]==0) args[0] = v->uc->clip_id;
	if(args[0]==-1)args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && STREAM_PLAYING(v))
	{
		if(vj_tag_set_contrast(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set contrast to %d",args[1]);
		}
	}

}


void vj_event_v4l_set_white(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0]==0) args[0] = v->uc->clip_id;
	if(args[0]==-1)args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && STREAM_PLAYING(v))
	{
		if(vj_tag_set_white(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set whiteness to %d",args[1]);
		}
	}

}
void vj_event_v4l_set_color(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) args[0] = v->uc->clip_id;
	if(args[0] == -1) args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && STREAM_PLAYING(v))
	{
		if(vj_tag_set_color(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set color to %d",args[1]);
		}
	}

}
void vj_event_v4l_set_hue(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) args[0] = v->uc->clip_id;
	if(args[0] == -1) args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && STREAM_PLAYING(v))
	{
		if(vj_tag_set_hue(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set hue to %d",args[1]);
		}
	}

}
#endif

void vj_event_tag_set_format(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char str[255]; 
	bzero(str,255);
	P_A(args,str,format,ap);

	if(v->settings->tag_record || v->settings->offline_record)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot change data format while recording to disk");
		return;
	}

	if(strncasecmp(str, "yv16",4) == 0 || strncasecmp(str,"y422",4)==0)
	{
		_recorder_format = ENCODER_YUV422;
		veejay_msg(VEEJAY_MSG_INFO, "Recorder writes in YUV 4:2:2 Planar");
		return;
	}

	if(strncasecmp(str,"mpeg4",5)==0 || strncasecmp(str,"divx",4)==0)
	{
		_recorder_format = ENCODER_MPEG4;
		veejay_msg(VEEJAY_MSG_INFO, "Recorder writes in MPEG4 format");
		return;
	}

	if(strncasecmp(str,"msmpeg4v3",9)==0 || strncasecmp(str,"div3",4)==0)
	{
		_recorder_format = ENCODER_DIVX;
		veejay_msg(VEEJAY_MSG_INFO,"Recorder writes in MSMPEG4v3 format");
		return;
	}
	if(strncasecmp(str,"dvvideo",7)==0||strncasecmp(str,"dvsd",4)==0)
	{
		_recorder_format = ENCODER_DVVIDEO;
		veejay_msg(VEEJAY_MSG_INFO,"Recorder writes in DVVIDEO format");
		return;
	}
	if(strncasecmp(str,"mjpeg",5)== 0 || strncasecmp(str,"mjpg",4)==0 ||
		strncasecmp(str, "jpeg",4)==0)
	{
		_recorder_format = ENCODER_MJPEG;
		veejay_msg(VEEJAY_MSG_INFO, "Recorder writes in MJPEG format");
		return;
	}
	if(strncasecmp(str,"i420",4)==0 || strncasecmp(str,"yv12",4)==0 )
	{
		_recorder_format = ENCODER_YUV420;
		veejay_msg(VEEJAY_MSG_INFO, "Recorder writes in uncompressed YV12/I420 (see swapping)");
		if(v->pixel_format == FMT_422)
		{
			veejay_msg(VEEJAY_MSG_WARNING, "Using 2x2 -> 1x1 and 1x1 -> 2x2 conversion");
		}
		return;
	}
	veejay_msg(VEEJAY_MSG_INFO, "Use one of these:");
	veejay_msg(VEEJAY_MSG_INFO, "mpeg4, div3, dvvideo , mjpeg , i420 or yv16");	

}

static void _vj_event_tag_record( veejay_t *v , int *args, char *str )
{
	if(!STREAM_PLAYING(v))
	{
		p_invalid_mode();
		return;
	}

	char tmp[255];
	char prefix[255];
	if(args[0] <= 0) 
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Number of frames to record must be > 0");
		return;
	}

	if(args[1] < 0 || args[1] > 1)
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Auto play is either on or off");
		return;
	}	

	char sourcename[255];	
	bzero(sourcename,255);
	vj_tag_get_description( v->uc->clip_id, sourcename );
	sprintf(prefix,"%s-%02d-", sourcename, v->uc->clip_id);
	//todo let user set base name
	if(! veejay_create_temp_file(prefix, tmp )) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot create temporary file %s", tmp);
		return;
	}

	int format = _recorder_format;
	if(_recorder_format == -1)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Set a destination format first");
		return;
	}

	if( vj_tag_init_encoder( v->uc->clip_id, tmp, format,		
			args[0]) != 1 ) 
	{
		veejay_msg(VEEJAY_MSG_INFO, "Error trying to start recording from stream %d", v->uc->clip_id);
		vj_tag_stop_encoder(v->uc->clip_id);
		v->settings->tag_record = 0;
		return;
	} 

	if(args[1]==0) 
		v->settings->tag_record_switch = 0;
	else
		v->settings->tag_record_switch = 1;

	v->settings->tag_record = 1;
}

void vj_event_tag_rec_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; 
	P_A(args,str,format,ap);

	_vj_event_tag_record( v, args, str );
}

void vj_event_tag_rec_stop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t *)ptr;
	video_playback_setup *s = v->settings;

	if( STREAM_PLAYING(v)  && v->settings->tag_record) 
	{
		int play_now = s->tag_record_switch;
		if(!vj_tag_stop_encoder( v->uc->clip_id))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Wasnt recording anyway");
			return;
		}
		
		char avi_file[255];
		int start = -1;
		int destination = v->edit_list->video_frames - 1;

		if( !vj_tag_get_encoded_file(v->uc->clip_id, avi_file)) 
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Dont know where I put the file?!");
			return;
		}	

		if( veejay_edit_addmovie(
			v,avi_file,start,destination,destination))
		{
			int len = vj_tag_get_encoded_frames(v->uc->clip_id) - 1;
			clip_info *skel = clip_skeleton_new(destination, destination+len);		
			if(clip_store(skel)==0) 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Created new clip %d from file %s",
					skel->clip_id,avi_file);
				clip_set_looptype( skel->clip_id,1);
			}
			veejay_msg(VEEJAY_MSG_INFO,"Added file %s (%d frames) to EditList",
				avi_file, len );	
		}		
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot add videofile %s to EditList!",avi_file);
		}

		veejay_msg(VEEJAY_MSG_ERROR, "Stopped recording from stream %d", v->uc->clip_id);
		vj_tag_reset_encoder( v->uc->clip_id);
		s->tag_record = 0;
		s->tag_record_switch = 0;

		if(play_now) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Playing clip %d now", clip_size()-1);
			veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_CLIP, clip_size()-1 );
		}
	}
	else
	{
		if(v->settings->offline_record)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Perhaps you want to stop recording from a non visible stream ? See VIMS id %d",
				VIMS_STREAM_OFFLINE_REC_STOP);
		}
		veejay_msg(VEEJAY_MSG_ERROR, "Not recording from visible stream");
	}
}

void vj_event_tag_rec_offline_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);

	if( v->settings->offline_record )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Already recording from a stream");
		return;
	}
 	if( v->settings->tag_record)
        {
		veejay_msg(VEEJAY_MSG_ERROR ,"Please stop the stream recorder first");
		return;
	}

	if( STREAM_PLAYING(v) && (args[0] == v->uc->clip_id) )
	{
		veejay_msg(VEEJAY_MSG_INFO,"Using stream recorder for stream  %d (is playing)",args[0]);
		_vj_event_tag_record(v, args+1, str);
		return;
	}


	if( vj_tag_exists(args[0]))
	{
		char tmp[255];
		
		int format = _recorder_format;
		char prefix[40];
		sprintf(prefix, "stream-%02d", args[0]);

		if(!veejay_create_temp_file(prefix, tmp ))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Error creating temporary file %s", tmp);
			return;
		}

		if(format==-1)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Set a destination format first");
			return;
		}
	
		if( vj_tag_init_encoder( args[0], tmp, format,		
			args[1]) ) 
		{
			video_playback_setup *s = v->settings;
			veejay_msg(VEEJAY_MSG_INFO, "(Offline) recording from stream %d", args[0]);
			s->offline_record = 1;
			s->offline_tag_id = args[0];
			s->offline_created_clip = args[2];
		} 
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "(Offline) error starting recording stream %d",args[0]);
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Stream %d does not exist",args[0]);
	}
}

void vj_event_tag_rec_offline_stop(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	video_playback_setup *s = v->settings;
	if(s->offline_record) 
	{
		if( vj_tag_stop_encoder( s->offline_tag_id ) == 0 )
		{
			char avi_file[255];
			int start = -1;
			int destination = v->edit_list->video_frames - 1;

			if( vj_tag_get_encoded_file(v->uc->clip_id, avi_file)!=0) return;
			

			if( veejay_edit_addmovie(
				v,avi_file,start,destination,destination))
			{
				int len = vj_tag_get_encoded_frames(v->uc->clip_id) - 1;
				clip_info *skel = clip_skeleton_new(destination, destination+len);		
				if(clip_store(skel)==0) 
				{
					veejay_msg(VEEJAY_MSG_INFO, "Created new clip %d from file %s",
						skel->clip_id,avi_file);
					clip_set_looptype( skel->clip_id,1);
				}	
			}		
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot add videofile %s to EditList!",avi_file);
			}

			vj_tag_reset_encoder( v->uc->clip_id);

			if(s->offline_created_clip) 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Playing new clip %d now ", clip_size()-1);
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP , clip_size()-1);
			}
		}
		s->offline_record = 0;
		s->offline_tag_id = 0;
	}
}


void vj_event_output_y4m_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	char str[1024];
	int *args = NULL;
	P_A( args,str,format,ap);
	if(v->stream_enabled==0)
	{
		int n=0;
		strncpy(v->stream_outname, str,strlen(str));
		n= vj_yuv_stream_start_write(v->output_stream,str,v->edit_list);
		if(n==1) 
		{
			int s = v->settings->current_playback_speed;
			veejay_msg(VEEJAY_MSG_DEBUG, "Pausing veejay");
			
			veejay_set_speed(v,0);
			if(vj_yuv_stream_open_pipe(v->output_stream,str,v->edit_list))
			{
				vj_yuv_stream_header_pipe(v->output_stream,v->edit_list);
				v->stream_enabled = 1;
			}
			veejay_msg(VEEJAY_MSG_DEBUG, "Resuming veejay");
			veejay_set_speed(v,s);
			
		}
		if(n==0)
		if( vj_yuv_stream_start_write(v->output_stream,str,v->edit_list)==0)
		{
			v->stream_enabled = 1;
			veejay_msg(VEEJAY_MSG_INFO, "Started YUV4MPEG streaming to [%s]", str);
		}
		if(n==-1)
		{
			veejay_msg(VEEJAY_MSG_INFO, "YUV4MPEG stream not started");
		}
	}	
}

void vj_event_output_y4m_stop(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(v->stream_enabled==1)
	{
		vj_yuv_stream_stop_write(v->output_stream);
		v->stream_enabled = 0;
		veejay_msg(VEEJAY_MSG_INFO , "Stopped YUV4MPEG streaming to %s", v->stream_outname);
	}
}

void vj_event_enable_audio(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(v->edit_list->has_audio)
	{
		if(v->audio != AUDIO_PLAY)
		{
			if( vj_perform_audio_start(v) )
			{
				v->audio = AUDIO_PLAY;
			}
			else 
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot start Jack ");
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Already playing audio");
		}
	}
	else 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Video has no audio");
	}
}

void vj_event_disable_audio(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	if(v->edit_list->has_audio)
	{
		if(v->audio == AUDIO_PLAY)
		{
			vj_perform_audio_stop(v);
			v->audio = NO_AUDIO;
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Not playing audio");
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Video has no audio");
	}
}


void vj_event_effect_inc(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int real_id;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);	
	if(!CLIP_PLAYING(v) && !STREAM_PLAYING(v))
	{
		p_invalid_mode();
		return;
	}
	v->uc->key_effect += args[0];
	if(v->uc->key_effect >= MAX_EFFECTS) v->uc->key_effect = 1;

	real_id = vj_effect_get_real_id(v->uc->key_effect);

	veejay_msg(VEEJAY_MSG_INFO, "Selected %s Effect %s (%d)", 
		(vj_effect_get_extra_frame(real_id)==1 ? "Video" : "Image"),
		vj_effect_get_description(real_id),
		real_id);
}

void vj_event_effect_dec(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int real_id;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(!CLIP_PLAYING(v) && !STREAM_PLAYING(v))
	{
		p_invalid_mode();
		return;
	}

	v->uc->key_effect -= args[0];
	if(v->uc->key_effect <= 0) v->uc->key_effect = MAX_EFFECTS-1;
	
	real_id = vj_effect_get_real_id(v->uc->key_effect);
	veejay_msg(VEEJAY_MSG_INFO, "Selected %s Effect %s (%d)",
		(vj_effect_get_extra_frame(real_id) == 1 ? "Video" : "Image"), 
		vj_effect_get_description(real_id),
		real_id);	
}
void vj_event_effect_add(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(CLIP_PLAYING(v)) 
	{	
		int c = clip_get_selected_entry(v->uc->clip_id);
		if ( clip_chain_add( v->uc->clip_id, c, 
				       vj_effect_get_real_id(v->uc->key_effect)) != 1)
		{
			int real_id = vj_effect_get_real_id(v->uc->key_effect);
			veejay_msg(VEEJAY_MSG_INFO,"Added Effect %s on chain entry %d",
				vj_effect_get_description(real_id),
				c
			);
			if(v->no_bezerk && vj_effect_get_extra_frame(real_id) ) veejay_set_clip(v,v->uc->clip_id);
			v->uc->chain_changed = 1;
		}
	}
	if(STREAM_PLAYING(v))
	{
		int c = vj_tag_get_selected_entry(v->uc->clip_id);
		if ( vj_tag_set_effect( v->uc->clip_id, c,
				vj_effect_get_real_id( v->uc->key_effect) ) != -1) 
		{
			int real_id = vj_effect_get_real_id(v->uc->key_effect);
			veejay_msg(VEEJAY_MSG_INFO,"Added Effect %s on chain entry %d",
				vj_effect_get_description(real_id),
				c
			);
			if(v->no_bezerk && vj_effect_get_extra_frame(real_id)) veejay_set_clip(v,v->uc->clip_id);
			v->uc->chain_changed = 1;
		}
	}

}

void vj_event_misc_start_rec_auto(void *ptr, const char format[], va_list ap)
{
 
}
void vj_event_misc_start_rec(void *ptr, const char format[], va_list ap)
{

}
void vj_event_misc_stop_rec(void *ptr, const char format[], va_list ap)
{

}

void vj_event_select_id(void *ptr, const char format[], va_list ap)
{
	veejay_t *v=  (veejay_t*)ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str, format, ap);
	if(!STREAM_PLAYING(v))
	{ 
		int clip_id = (v->uc->clip_key*12)-12 + args[0];
		if(clip_exists(clip_id))
		{
			veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_CLIP, clip_id);
			vj_event_print_clip_info(v,clip_id);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Selected clip %d does not exist",clip_id);
		}
	}	
	else
	{
		int clip_id = (v->uc->clip_key*12)-12 + args[0];
		if(vj_tag_exists(clip_id ))
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG ,clip_id);

		}
		else
		{
			veejay_msg(VEEJAY_MSG_INFO,"Selected stream %d does not exist",clip_id);
		}
	}

}



void	vj_event_plugin_command(void *ptr,	const char	format[],	va_list ap)
{
	int args[2];
	char str[1024];
	char *plugin_name;
	char *p = &str[0];
	char *d = &plugin_name[0];
	int oops = 19;
	P_A(args,str,format,ap);
	bzero(plugin_name,0);
	while ( *(p) != ':' && *(p) != '\0' && *(p) != ';' && oops != 0)
	{
		*(d++) = *(p++);
		oops--;
	}
	if(oops==0)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Plugin name too long");
		return;
	}
	*(d++) = '\0';
	*(p++);
	plugins_event( plugin_name, p ); 
}

void	vj_event_unload_plugin(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	char str[1024];
	P_A(args,str,format,ap);
	veejay_msg(VEEJAY_MSG_DEBUG, "Try to close plugin '%s'", str);
	
	plugins_free( str );
}

void	vj_event_load_plugin(void *ptr, const char format[], va_list ap)
{
	int args[2];
	char str[1024];
	P_A(args,str,format,ap);

	
	veejay_msg(VEEJAY_MSG_DEBUG, "Try to open plugin '%s' ", str);
	if(plugins_init( str ))
	{
		veejay_msg(VEEJAY_MSG_INFO, "Loaded plugin %s", str);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Unloaded plugin %s", str);
	}
}

void vj_event_select_bank(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v =(veejay_t*) ptr;
	int args[1];

	char *str = NULL; P_A(args,str,format,ap);
	if(args[0] >= 1 && args[0] <= 9)
	{
		veejay_msg(VEEJAY_MSG_INFO,"Selected bank %d (active clip range is now %d-%d)",args[0],
			(12 * args[0]) - 12 , (12 * args[0]));
		v->uc->clip_key = args[0];
	}
}

void vj_event_print_tag_info(veejay_t *v, int id) 
{
	int i, y, j, value;
	char description[100];
	char source[150];
	char title[150];
	vj_tag_get_descriptive(id,description);
	vj_tag_get_description(id, title);
	vj_tag_get_source_name(id, source);

	if(v->settings->tag_record)
		veejay_msg(VEEJAY_MSG_INFO, "Stream '%s' [%d]/[%d] [%s] %s recorded: %06ld frames ",
			title,id,vj_tag_size()-1,description,
		(vj_tag_get_active(id) ? "is active" : "is not active"),
		vj_tag_get_encoded_frames(id));
	else
	veejay_msg(VEEJAY_MSG_INFO,
		"Stream [%d]/[%d] [%s] %s ",
		id, vj_tag_size()-1, description,
		(vj_tag_get_active(id) == 1 ? "is active" : "is not active"));

	veejay_msg(VEEJAY_MSG_INFO,  "|-----------------------------------|");	
	for (i = 0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = vj_tag_get_effect_any(id, i);
		if (y != -1) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "%02d  [%d] [%s] %s (%s)",
				i,
				y,
				vj_tag_get_chain_status(id,i) ? "on" : "off", vj_effect_get_description(y),
				(vj_effect_get_subformat(y) == 1 ? "2x2" : "1x1")
			);

			for (j = 0; j < vj_effect_get_num_params(y); j++)
			{
				value = vj_tag_get_effect_arg(id, i, j);
				if (j == 0)
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, "          [%04d]", value);
				}
				else
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, " [%04d]",value);
				}
				
	    		}
	    		veejay_msg(VEEJAY_MSG_PRINT, "\n");

			if (vj_effect_get_extra_frame(y) == 1)
			{
				int source = vj_tag_get_chain_source(id, i);
				veejay_msg(VEEJAY_MSG_INFO, "     V %s [%d]",(source == VJ_TAG_TYPE_NONE ? "Clip" : "Stream"),
			    		vj_tag_get_chain_channel(id,i)
					);
				//veejay_msg(VEEJAY_MSG_INFO, "     A: %s",   vj_tag_get_chain_audio(id, i) ? "yes" : "no");
	    		}

	    		veejay_msg(VEEJAY_MSG_PRINT, "\n");
		}
    	}
	v->real_fps = -1;

}

void vj_event_create_effect_bundle(veejay_t * v, char *buf, int key_id, int key_mod )
{
	char blob[50 * CLIP_MAX_EFFECTS];
	char prefix[20];
	int i ,y,j;
	int num_cmd = 0;
	int id = v->uc->clip_id;
	int event_id = 0;
	int bunlen=0;
	bzero( prefix, 20);
	bzero( blob, (50 * CLIP_MAX_EFFECTS));
   
	if(!CLIP_PLAYING(v) && !STREAM_PLAYING(v)) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot take snapshot of Effect Chain");
		return;
	}

 	for (i = 0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = (CLIP_PLAYING(v) ? clip_get_effect_any(id, i) : vj_tag_get_effect_any(id,i) );
		if (y != -1)
		{
			num_cmd++;
		}
	}
	if(num_cmd < 0) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Effect Chain is empty." );           
		return;
	}

	for (i=0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = (CLIP_PLAYING(v) ? clip_get_effect_any(id, i) : vj_tag_get_effect_any(id,i) );
		if( y != -1)
		{
			//int entry = i;
			int effect_id = y;
			if(effect_id != -1)
			{
				char bundle[200];
				int np = vj_effect_get_num_params(y);
				bzero(bundle,200);
				sprintf(bundle, "%03d:0 %d %d", VIMS_CHAIN_ENTRY_SET_PRESET,i, effect_id );
		    		for (j = 0; j < np; j++)
				{
					char svalue[10];
					int value = (CLIP_PLAYING(v) ? clip_get_effect_arg(id, i, j) : vj_tag_get_effect_arg(id,i,j));
					if(value != -1)
					{
						if(j == (np-1))
							sprintf(svalue, " %d;", value);
						else 
							sprintf(svalue, " %d", value);
						strncat( bundle, svalue, strlen(svalue));
					}
				}
				strncpy( blob+bunlen, bundle,strlen(bundle));
				bunlen += strlen(bundle);
			}
		}
 	}
	sprintf(prefix, "BUN:%03d{", num_cmd);
	sprintf(buf, "%s%s}",prefix,blob);
	event_id = vj_event_suggest_bundle_id();

	if(event_id <= 0 )	
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot add more bundles");
		return;
	}

	vj_msg_bundle *m = vj_event_bundle_new( buf, event_id);
	if(!m)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Unable to create new Bundle");
		return;
	}
	if(!vj_event_bundle_store(m))
		veejay_msg(VEEJAY_MSG_ERROR, "Error storing Bundle %d", event_id);
}


void vj_event_print_clip_info(veejay_t *v, int id) 
{
	video_playback_setup *s = v->settings;
	int y, i, j;
	long value;
	char timecode[15];
	char curtime[15];
	char cliptitle[200];
	MPEG_timecode_t tc;
	int start = clip_get_startFrame( id );
	int end = clip_get_endFrame( id );
	int speed = clip_get_speed(id);
	int len = end - start;

	if(start == 0) len ++;
	bzero(cliptitle,200);	
	mpeg_timecode(&tc, len,
 		mpeg_framerate_code(mpeg_conform_framerate(v->edit_list->video_fps)),v->edit_list->video_fps);
	sprintf(timecode, "%2d:%2.2d:%2.2d:%2.2d", tc.h, tc.m, tc.s, tc.f);

	mpeg_timecode(&tc,  s->current_frame_num,
		mpeg_framerate_code(mpeg_conform_framerate
		   (v->edit_list->video_fps)),
	  	v->edit_list->video_fps);

	sprintf(curtime, "%2d:%2.2d:%2.2d:%2.2d", tc.h, tc.m, tc.s, tc.f);
	clip_get_description( id, cliptitle );
	veejay_msg(VEEJAY_MSG_PRINT, "\n");
	veejay_msg(VEEJAY_MSG_INFO, 
		"Clip '%s'[%4d]/[%4d]\t[duration: %s | %8d ]",
		cliptitle,id,clip_size()-1,timecode,len);

	if(clip_encoder_active(v->uc->clip_id))
	{
		veejay_msg(VEEJAY_MSG_INFO, "REC %09d\t[timecode: %s | %8ld ]",
			clip_get_frames_left(v->uc->clip_id),
			curtime,(long)v->settings->current_frame_num);

	}
	else
	{
		veejay_msg(VEEJAY_MSG_INFO, "                \t[timecode: %s | %8ld ]",
			curtime,(long)v->settings->current_frame_num);
	}
	veejay_msg(VEEJAY_MSG_INFO, 
		"[%09d] - [%09d] @ %4.2f (speed %d)",
		start,end, (float)speed * v->edit_list->video_fps,speed);
	veejay_msg(VEEJAY_MSG_INFO,
		"[%s looping]",
		(clip_get_looptype(id) ==
		2 ? "pingpong" : (clip_get_looptype(id)==1 ? "normal" : "no")  )
		);

	int first = 0;
    	for (i = 0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = clip_get_effect_any(id, i);
		if (y != -1)
		{
			if(!first)
			{
			 veejay_msg(VEEJAY_MSG_INFO, "\nI: E F F E C T  C H A I N\nI:");
			 veejay_msg(VEEJAY_MSG_INFO,"Entry|Effect ID|SW | Name");
				first = 1;
			}
			veejay_msg(VEEJAY_MSG_INFO, "%02d   |%03d      |%s| %s %s",
				i,
				y,
				clip_get_chain_status(id,i) ? "on " : "off", vj_effect_get_description(y),
				(vj_effect_get_subformat(y) == 1 ? "2x2" : "1x1")
			);

	    		for (j = 0; j < vj_effect_get_num_params(y); j++)
			{
				value = clip_get_effect_arg(id, i, j);
				if (j == 0)
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, "I:\t\t\tP%d=[%d]",j, value);
				}
				else
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, " P%d=[%d] ",j,value);
				}
			}
			veejay_msg(VEEJAY_MSG_PRINT, "\n");
	    		if (vj_effect_get_extra_frame(y) == 1)
			{
				int source = clip_get_chain_source(id, i);
						 
				veejay_msg(VEEJAY_MSG_PRINT, "I:\t\t\t Mixing with %s %d\n",(source == VJ_TAG_TYPE_NONE ? "clip" : "stream"),
			    		clip_get_chain_channel(id,i)
					);
	    		}
		}
    	}
	v->real_fps = -1;
	veejay_msg(VEEJAY_MSG_PRINT, "\n");
}

void vj_event_print_plain_info(void *ptr, int x)
{
	veejay_t *v = (veejay_t*) ptr;
	v->real_fps = -1;	
	if( PLAIN_PLAYING(v)) vj_el_print( v->edit_list );
}

void vj_event_print_info(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);
	if(args[0]==0)
	{
		args[0] = v->uc->clip_id;
	}

	vj_event_print_plain_info(v,args[0]);

	if( CLIP_PLAYING(v) && clip_exists(args[0])  )
	{
		vj_event_print_clip_info( v, args[0] );
	}
	if( STREAM_PLAYING(v) && vj_tag_exists(args[0]) )
	{
		vj_event_print_tag_info(v, args[0]) ;
	}
}

void	vj_event_send_tag_list			(	void *ptr,	const char format[],	va_list ap	)
{
	int args[1];
	int start_from_tag = 1;
	veejay_t *v = (veejay_t*)ptr;
	char *str = NULL; 
	P_A(args,str,format,ap);
	int i,n;
	bzero( _s_print_buf,SEND_BUF);
	sprintf(_s_print_buf, "%05d",0);

	if(args[0]>0) start_from_tag = args[0];

	n = vj_tag_size()-1;

	if (n >= 1 )
	{
		char line[300];
		bzero( _print_buf, SEND_BUF);

		for(i=start_from_tag; i <= n; i++)
		{
			if(vj_tag_exists(i))
			{	
				vj_tag *tag = vj_tag_get(i);
				char source_name[255];
				char cmd[300];
				bzero(source_name,200);bzero(cmd,255);
				bzero(line,300);
				vj_tag_get_description( i, source_name );
				sprintf(line,"%05d%02d%03d%03d%03d%03d%03d%s",
					i,
					vj_tag_get_type(i),
					tag->color_r,
					tag->color_g,
					tag->color_b,
					tag->opacity, 
					strlen(source_name),
					source_name
				);
				sprintf(cmd, "%03d%s",strlen(line),line);
				APPEND_MSG(_print_buf,cmd); 
			}
		}
		sprintf(_s_print_buf, "%05d%s",strlen(_print_buf),_print_buf);
	}
	SEND_MSG(v,_s_print_buf);
}



void	vj_event_send_clip_list		(	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	int start_from_clip = 1;
	char cmd[300];
	char *str = NULL;
	int i,n;
	P_A(args,str,format,ap);

	if(args[0]>0) start_from_clip = args[0];

	bzero( _s_print_buf,SEND_BUF);
	sprintf(_s_print_buf, "%05d", 0);

	n = clip_size()-1;
	if( n >= 1 )
	{
		char line[308];
		bzero(_print_buf, SEND_BUF);
		for(i=start_from_clip; i <= n; i++)
		{
			if(clip_exists(i))
			{	
				char description[CLIP_MAX_DESCR_LEN];
				int end_frame = clip_get_endFrame(i);
				int start_frame = clip_get_startFrame(i);
				bzero(cmd, 300);

				/* format of clip:
				 	00000 : id
				    000000000 : start    
                                    000000000 : end
                                    xxx: str  : description
				*/
				clip_get_description( i, description );
				
				sprintf(cmd,"%05d%09d%09d%03d%s",
					i,
					start_frame,	
					end_frame,
					strlen(description),
					description
				);
				FORMAT_MSG(line,cmd);
				APPEND_MSG(_print_buf,line); 
			}

		}
		sprintf(_s_print_buf, "%05d%s", strlen(_print_buf),_print_buf);
	}
	SEND_MSG(v, _s_print_buf);
}

void	vj_event_send_chain_entry		( 	void *ptr,	const char format[],	va_list ap	)
{
	char fline[255];
	char line[255];
	int args[2];
	char *str = NULL;
	int error = 1;
	veejay_t *v = (veejay_t*)ptr;
	P_A(args,str,format,ap);

	bzero(fline,255);
	sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);

	if(args[0] == 0) 
		args[0] = v->uc->clip_id;

	if(CLIP_PLAYING(v))
	{
		if(args[1]==-1) args[1] = clip_get_selected_entry(args[0]);
		int effect_id = clip_get_effect_any(args[0], args[1]);
		
		if(effect_id > 0)
		{
			int is_video = vj_effect_get_extra_frame(effect_id);
			int params[CLIP_MAX_PARAMETERS];
			int p;
			int video_on = clip_get_chain_status(args[0],args[1]);
			int audio_on = 0;
			//int audio_on = clip_get_chain_audio(args[0],args[1]);
			int num_params = vj_effect_get_num_params(effect_id);
			for(p = 0 ; p < num_params; p++)
			{
				params[p] = clip_get_effect_arg(args[0],args[1],p);
			}
			for(p = num_params; p < CLIP_MAX_PARAMETERS; p++)
			{
				params[p] = 0;
			}

			sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				effect_id,
				is_video,
				num_params,
				params[0],
				params[1],	
				params[2],	
				params[3],
				params[4],		
				params[5],
				params[6],
				params[7],
				params[8],
				video_on,
				audio_on,
				clip_get_chain_source(args[0],args[1]),
				clip_get_chain_channel(args[0],args[1]) 
			);				
			error = 0;
		}
	}
	
	if(STREAM_PLAYING(v))
	{
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(args[0]);
 		int effect_id = vj_tag_get_effect_any(args[0], args[1]);

		if(effect_id > 0)
		{
			int is_video = vj_effect_get_extra_frame(effect_id);
			int params[CLIP_MAX_PARAMETERS];
			int p;
			int num_params = vj_effect_get_num_params(effect_id);
			int video_on = vj_tag_get_chain_status(args[0], args[1]);
			for(p = 0 ; p < num_params; p++)
			{
				params[p] = vj_tag_get_effect_arg(args[0],args[1],p);
			}
			for(p = num_params; p < CLIP_MAX_PARAMETERS;p++)
			{
				params[p] = 0;
			}

			sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				effect_id,
				is_video,
				num_params,
				params[0],
				params[1],	
				params[2],	
				params[3],
				params[4],		
				params[5],
				params[6],
				params[7],
				params[8],
				video_on,	
				0,
				vj_tag_get_chain_source(args[0],args[1]),
				vj_tag_get_chain_channel(args[0],args[1])
			);				
			error = 0;
		}
	}


	FORMAT_MSG(fline,line);
	SEND_MSG(v, fline);
}

void	vj_event_send_chain_list		( 	void *ptr,	const char format[],	va_list ap	)
{
	int i;
	char line[18];
	int args[1];
	char *str = NULL;
	veejay_t *v = (veejay_t*)ptr;
	P_A(args,str,format,ap);

	if(args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}

	bzero( _s_print_buf,SEND_BUF);
	bzero( _print_buf, SEND_BUF);

	sprintf( _s_print_buf, "%03d",0 );

	if(CLIP_PLAYING(v))
	{
		if(args[0] == -1) args[0] = clip_size()-1;

		for(i=0; i < CLIP_MAX_EFFECTS; i++)
		{
			int effect_id = clip_get_effect_any(args[0], i);
			if(effect_id > 0)
			{
				int is_video = vj_effect_get_extra_frame(effect_id);
				int using_effect = clip_get_chain_status(args[0], i);
				int using_audio = 0;
				//int using_audio = clip_get_chain_audio(args[0],i);
				sprintf(line,"%02d%03d%1d%1d%1d",
					i,
					effect_id,
					is_video,
					(using_effect <= 0  ? 0 : 1 ),
					(using_audio  <= 0  ? 0 : 1 )
				);
						
				APPEND_MSG(_print_buf,line);
			}
		}
		sprintf(_s_print_buf, "%03d%s",strlen(_print_buf), _print_buf);

	}
	if(STREAM_PLAYING(v))
	{
		if(args[0] == -1) args[0] = vj_tag_size()-1;

		for(i=0; i < CLIP_MAX_EFFECTS; i++) 
		{
			int effect_id = vj_tag_get_effect_any(args[0], i);
			if(effect_id > 0)
			{
				int is_video = vj_effect_get_extra_frame(effect_id);
				int using_effect = vj_tag_get_chain_status(args[0],i);
				sprintf(line, "%02d%03d%1d%1d%1d",
					i,
					effect_id,
					is_video,
					(using_effect <= 0  ? 0 : 1 ),
					0
				);
				APPEND_MSG(_print_buf, line);
			}
		}
		sprintf(_s_print_buf, "%03d%s",strlen( _print_buf ), _print_buf);

	}
	SEND_MSG(v, _s_print_buf);
}

void	vj_event_send_clip_history_list	(	void *ptr,	const char format[],	va_list ap	)
{
	int args[2];
	char *s = NULL;
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,s,format,ap);

	if(args[0] == 0)
	{
		if(CLIP_PLAYING(v))
			args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	bzero( _s_print_buf,SEND_BUF);
	bzero( _print_buf, SEND_BUF);

	sprintf( _s_print_buf, "%03d", 0 );

	if(clip_exists(args[0]))
	{
		int entry = 0;
		int id = args[0];
		clip_info *clip = clip_get( id );
		char hisline[25];
		
		for( entry = 0; entry < CLIP_MAX_RENDER; entry ++ )
		{	
			editlist **el = (editlist**) clip_get_user_data( id );
			bzero(hisline,25);
			if(el && el[entry])
				sprintf(hisline,"%02d%010d%010d",
					entry, (int)clip->first_frame[entry],
					(int) clip->last_frame[entry] );
			else
				sprintf(hisline,"%02d%010d%010d", entry, 0, 0 );
			strncat( _print_buf, hisline, strlen(hisline ));
		}
		sprintf( _s_print_buf, "%03d%s", strlen( _print_buf ), _print_buf );
	}
	SEND_MSG(v, _s_print_buf);
}

void 	vj_event_send_video_information		( 	void *ptr,	const char format[],	va_list ap	)
{
	/* send video properties */
	char info_msg[255];
	bzero( _s_print_buf,SEND_BUF);


	veejay_t *v = (veejay_t*)ptr;
	bzero(info_msg,100);
	snprintf(info_msg,sizeof(info_msg)-1, "%04d %04d %01d %c %02.3f %1d %04d %06ld %02d %03ld %08ld",
		v->edit_list->video_width,
		v->edit_list->video_height,
		v->edit_list->video_inter,
		v->edit_list->video_norm,
		v->edit_list->video_fps,  
		v->edit_list->has_audio,
		v->edit_list->audio_bits,
		v->edit_list->audio_rate,
		v->edit_list->audio_chans,
		v->edit_list->num_video_files,
		v->edit_list->video_frames
		);	
	sprintf( _s_print_buf, "%03d%s",strlen(info_msg), info_msg);

	SEND_MSG(v,_s_print_buf);
}

void 	vj_event_send_editlist			(	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*) ptr;
	editlist *el = v->edit_list;
	
	if( el->num_video_files <= 0 )
	{
		SEND_MSG( v, "00000000");
		return;
	}

	bzero( _s_print_buf, SEND_BUF );
	int b = 0;
	int nf = 0;
	char *msg = (char*) vj_el_write_line_ascii( v->edit_list, &b,&nf );
	sprintf( _s_print_buf, "%06d%s", b, msg );
	free(msg);

	SEND_MSG( v, _s_print_buf );
}

void	vj_event_send_devices			(	void *ptr,	const char format[],	va_list ap	)
{
	char str[255];
	struct dirent **namelist;
	int n_dev = 0;
	int n;
	char device_list[512];
	char useable_devices[2];
	int *args = NULL;
	veejay_t *v = (veejay_t*)ptr;
	P_A(args,str,format,ap);
	bzero(device_list,512);

	n = scandir(str,&namelist,0,alphasort);
	if( n<= 0)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "No device information in [%s]",str);
		SEND_MSG(v,"0000");
		return;
	}

		
	while(n--)
	{
		if( strncmp(namelist[n]->d_name, "video", 4)==0)
		{
			FILE *fd;
			char filename[300];
			sprintf(filename,"%s%s",str,namelist[n]->d_name);
			fd = fopen( filename, "r");
			if(fd)
			{
				fclose(fd);
			}
		}
	}
	sprintf(useable_devices,"%02d", n_dev);

	APPEND_MSG( device_list, useable_devices );
	SEND_MSG(v,device_list);

}

void	vj_event_send_frame				( 	void *ptr, const char format[], va_list ap )
{
	veejay_t *v = (veejay_t*) ptr;

	// send frame (peer to peer) on demand
	vj_perform_send_primary_frame_s( v,0 );
}


void	vj_event_mcast_start				(	void *ptr,	const char format[],	va_list ap )
{
	veejay_t *v = (veejay_t*) ptr;
	if(!v->settings->use_vims_mcast)
		veejay_msg(VEEJAY_MSG_ERROR, "start veejay in multicast mode (see -V commandline option)");	
	else
	{
		v->settings->mcast_frame_sender = 1;
		veejay_msg(VEEJAY_MSG_INFO, "Veejay started mcast frame sender");
	}	
}


void	vj_event_mcast_stop				(	void *ptr,	const char format[],	va_list ap )
{
	veejay_t *v = (veejay_t*) ptr;
	if(!v->settings->use_vims_mcast)
		veejay_msg(VEEJAY_MSG_ERROR, "start veejay in multicast mode (see -V commandline option)");	
	else
	{
		v->settings->mcast_frame_sender = 0;
		veejay_msg(VEEJAY_MSG_INFO, "Veejay stopped mcast frame sender");
	}	
}

void	vj_event_send_effect_list		(	void *ptr,	const char format[],	va_list ap	)
{
	int i;
	veejay_t *v = (veejay_t*)ptr;
	char line[300];   
	char fline[300];
	bzero( _print_buf, SEND_BUF);
	bzero( _s_print_buf,SEND_BUF);

	for(i=1; i < MAX_EFFECTS; i++)
	{
		int effect_id = vj_effect_get_real_id(i);
		bzero(line, 300);
		if(effect_id > 0 && vj_effect_get_summary(i,line)==1)
		{
			sprintf(fline, "%03d%s",strlen(line),line);
			strcat( _print_buf, fline );

		}
		else
		{
			fprintf(stderr, "no matching effect for %d\n",i);	
			fprintf(stderr, "get summary returns with %d",
				vj_effect_get_summary(i,line));
		}
	}	
	sprintf( _s_print_buf, "%05d%s",strlen(_print_buf), _print_buf);

	SEND_MSG(v,_s_print_buf);
}



int vj_event_load_bundles(char *bundle_file)
{
	FILE *fd;
	char *event_name, *event_msg;
	char buf[65535];
	int event_id=0;
	if(!bundle_file) return -1;
	fd = fopen(bundle_file, "r");
	bzero(buf,65535);
	if(!fd) return -1;
	while(fgets(buf,4096,fd))
	{
		buf[strlen(buf)-1] = 0;
		event_name = strtok(buf, "|");
		event_msg = strtok(NULL, "|");
		if(event_msg!=NULL && event_name!=NULL) {
			//veejay_msg(VEEJAY_MSG_INFO, "Event: %s , Msg [%s]",event_name,event_msg);
			event_id = atoi( event_name );
			if(event_id && event_msg)
			{
				vj_msg_bundle *m = vj_event_bundle_new( event_msg, event_id );
				if(m != NULL) 
				{
					if( vj_event_bundle_store(m) ) 
					{
						veejay_msg(VEEJAY_MSG_INFO, "(VIMS) Registered a bundle as event %03d",event_id);
					}
				}
			}
		}
	}
	fclose(fd);
	return 1;
}

void vj_event_do_bundled_msg(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char s[1024];	
	vj_msg_bundle *m;
	P_A( args, s , format, ap);
	//veejay_msg(VEEJAY_MSG_INFO, "Parsing message bundle as event");
	m = vj_event_bundle_get(args[0]);
	if(m)
	{
		vj_event_parse_bundle( v, m->bundle );
	}	
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Requested event %d does not exist. ",args[0]);
	}
}

#ifdef HAVE_SDL
void vj_event_attach_detach_key(void *ptr, const char format[], va_list ap)
{
	int args[4] = { 0,0,0,0 };
	char value[100];
	bzero(value,100);
	int mode = 0;
	

	P_A( args, value, format ,ap );

	if( args[0] < 0 || args[0] > VIMS_MAX )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "invalid event identifier specified");
		return;
	}


	if( args[1] <= 0 || args[1] >= SDLK_LAST)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid key identifier %d (range is 1 - %d)", args[1], SDLK_LAST);
		return;
	}
	if( args[2] < 0 || args[2] > VIMS_MOD_SHIFT )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid key modifier (3=shift,2=ctrl,1=alt, 0=none)");
		return;
	}

	if(args[0] == 0 )
		mode = 1;
	else
		mode = 2; // assign key symbol / key modifier


	veejay_msg(VEEJAY_MSG_DEBUG,
	   "%s , event_id = %d, symbol = %d, modifier = %d, arguments = %s",
		__FUNCTION__, args[0], args[1], args[2], value );

	if( mode == 1 )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Detached key %d + %d from event %d", 
			args[1],args[2],keyboard_events[(args[2]*SDLK_LAST)+args[1]].vims.list_id);
		vj_event_unregister_keyb_event( args[1],args[2] );
	}
	if( mode == 2 )
	{
		if(vj_event_register_keyb_event( args[0], args[1], args[2], value ))
		{
		 veejay_msg(VEEJAY_MSG_DEBUG, "Trigger VIMS %d with %d,%d (%s)",
			args[0],args[1],args[2], value );
		}
}	}
#endif

void vj_event_bundled_msg_del(void *ptr, const char format[], va_list ap)
{
	
	int args[1];	
	char *s = NULL;
	P_A(args,s,format,ap);
	if ( vj_event_bundle_del( args[0] ) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO,"Bundle %d deleted from event system",args[0]);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Bundle is %d is not known",args[0]);
	}
}




void vj_event_bundled_msg_add(void *ptr, const char format[], va_list ap)
{
	
	int args[2] = {0,0};
	char s[1024];
	bzero(s, 1024);
	P_A(args,s,format,ap);

	if(args[0] == 0)
	{
		args[0] = vj_event_suggest_bundle_id();
		veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) suggested new Event id %d", args[0]);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) requested to add/replace %d", args[0]);
	}

	if(args[0] < VIMS_BUNDLE_START|| args[0] > VIMS_BUNDLE_END )
	{
		// invalid bundle
		veejay_msg(VEEJAY_MSG_ERROR, "Customized events range from %d-%d", VIMS_BUNDLE_START, VIMS_BUNDLE_END);
		return;
	}
	// allocate new
	veejay_strrep( s, '_', ' ');
	vj_msg_bundle *m = vj_event_bundle_new(s, args[0]);
	if(!m)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error adding bundle ?!");
		return;
	}

	// bye existing bundle
	if( vj_event_bundle_exists(args[0]))
	{
		veejay_msg(VEEJAY_MSG_DEBUG,"(VIMS) Bundle exists - replace ");
		vj_event_bundle_del( args[0] );
	}

	if( vj_event_bundle_store(m)) 
	{
		veejay_msg(VEEJAY_MSG_INFO, "(VIMS) Registered Bundle %d in VIMS",args[0]);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) Error in Bundle %d '%s'",args[0],s );
	}
}

void	vj_event_set_stream_color(void *ptr, const char format[], va_list ap)
{
	int args[4];
	char *s = NULL;
	P_A(args,s,format,ap);
	veejay_t *v = (veejay_t*) ptr;
	
	if(STREAM_PLAYING(v))
	{
		if(args[0] == 0 ) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = vj_tag_size()-1;
	}
	// allow changing of color while playing plain/sample
	if(vj_tag_exists(args[0]) &&
		vj_tag_get_type(args[0]) == VJ_TAG_TYPE_COLOR )
	{
		CLAMPVAL( args[1] );
		CLAMPVAL( args[2] );
		CLAMPVAL( args[3] );	
		vj_tag_set_stream_color(args[0],args[1],args[2],args[3]);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Stream %d does not exist",
			args[0]);
	}
}

#ifdef HAVE_JPEG
void vj_event_screenshot(void *ptr, const char format[], va_list ap)
{
	int *args = NULL;
	char s[1024];
	bzero(s,1024);

	P_A(args,s,format,ap);

	veejay_t *v = (veejay_t*)ptr;
    	v->uc->hackme = 1;
	if( strncasecmp(s, "(NULL)", 6) == 0  )
		v->uc->filename = NULL;
	else
		v->uc->filename = strdup( s );
	
}

void		vj_event_quick_bundle( void *ptr, const char format[], va_list ap)
{
	vj_event_commit_bundle( (veejay_t*) ptr,0,0);
}

#endif
