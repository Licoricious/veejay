#ifndef VIMS_H
#define VIMS_H
enum {
	NET_RECORD_DATAFORMAT			=	10,
	NET_SET_PLAIN_MODE			= 	1,
	NET_INIT_GUI_SCREEN			=	11,
	NET_SWITCH_CLIP_TAG			=	13,
	NET_RESIZE_SDL_SCREEN			=	12,
	NET_SET_PLAY_MODE			=	2,
	NET_EFFECT_LIST				=	15,
	NET_EDITLIST_LIST			=	201,
	NET_SET_MODE_AND_GO			=	3,
	NET_EDITLIST_PASTE_AT			=	20,
	NET_EDITLIST_COPY			=	21,
	NET_EDITLIST_DEL			=	22,
	NET_EDITLIST_CROP			=	23,
	NET_EDITLIST_CUT			=	24,
	NET_EDITLIST_ADD			=	25,
	NET_EDITLIST_ADD_CLIP			=	26,
	NET_EDITLIST_SAVE			=	28,
	NET_EDITLIST_LOAD			=	29,
	NET_BUNDLE				=	50,
	NET_DEL_BUNDLE				=	51,
	NET_ADD_BUNDLE				=	52,
	NET_BUNDLE_ATTACH_KEY			=	53,
	NET_SCREENSHOT				=	54,
	NET_BUNDLE_FILE				=	55,
	NET_BUNDLE_SAVE				=	56,
	NET_VIDEO_INFORMATION			=	202,
	NET_AUDIO_ENABLE			=	4,
	NET_AUDIO_DISABLE			=	5,
	NET_VIDEO_PLAY_FORWARD			=	80,
	NET_VIDEO_PLAY_BACKWARD			=	81,
	NET_VIDEO_PLAY_STOP			=	82,
	NET_VIDEO_SKIP_FRAME			=	83,
	NET_VIDEO_PREV_FRAME			=	84,
	NET_VIDEO_SKIP_SECOND			=	85,
	NET_VIDEO_PREV_SECOND			=	86,
	NET_VIDEO_GOTO_START			=	87,
	NET_VIDEO_GOTO_END			=	88,
	NET_VIDEO_SET_FRAME			=	89,
	NET_VIDEO_SET_SPEED			=	90,
	NET_VIDEO_SET_SLOW			=	91,
	NET_SET_CLIP_START			=	92,
	NET_SET_CLIP_END			=	93,
	NET_CLIP_NEW				=	99,
	NET_CLIP_SELECT				=	100,
	NET_CLIP_DEL				=	101,
	NET_CLIP_SET_LOOPTYPE			=	102,
	NET_CLIP_SET_DESCRIPTION		=	103,
	NET_CLIP_SET_SPEED			=	104,
	NET_CLIP_SET_START			=	105,
	NET_CLIP_SET_END			=	106,
	NET_CLIP_SET_DUP			=	107,
	NET_CLIP_SET_FREEZE_MODE		=	109,
	NET_CLIP_SET_FREEZE_PFRAMES		=	110,
	NET_CLIP_SET_FREEZE_NFRAMES		=	111,
	NET_CLIP_SET_AUDIO_VOL			=	112,
	NET_CLIP_SET_MARKER_START		=	113,
	NET_CLIP_SET_MARKER_END			= 	114,
	NET_CLIP_SET_MARKER			=	115,
	NET_CLIP_CLEAR_MARKER			=	116,
	NET_CLIP_CLEAR_FREEZE			=	117,
	NET_CLIP_HISTORY_SET_ENTRY		=	118,
	NET_CLIP_HISTORY_ENTRY_AS_NEW		=	119,
	NET_CLIP_HISTORY_CLEAR_ENTRY		=	120,
	NET_CLIP_HISTORY_LOCK_ENTRY		=	121,
	NET_CLIP_HISTORY_UNLOCK_ENTRY		=	122,
	NET_CLIP_HISTORY_PLAY_ENTRY		=	123,
	NET_CLIP_LOAD_CLIPLIST			=	124,
	NET_CLIP_SAVE_CLIPLIST			=	125,
	NET_CLIP_HISTORY_LIST			=	126,
	NET_CLIP_DEL_ALL			=	135,
	NET_CLIP_COPY				=	139,
	NET_CLIP_LIST				=	205,
	NET_SET_MARKER_START			=	127,
	NET_SET_MARKER_END			= 	128,
	NET_CLIP_REC_START			=	130,
	NET_CLIP_REC_STOP			=	131,
	NET_CLIP_CHAIN_ENABLE			=	132,
	NET_CLIP_CHAIN_DISABLE			=	133,
    NET_CLIP_ADD_WAVE			=	136,
	NET_CLIP_DEL_WAVE			=	137,
	NET_CLIP_UPDATE				=	134,
	NET_TAG_SELECT				=	140,
	NET_TAG_ACTIVATE			=	141,
	NET_TAG_DEACTIVATE			=	142,
	NET_TAG_DELETE				=	143,
	NET_TAG_NEW_V4L				=	144,
	NET_TAG_NEW_VLOOP_BY_NAME		=	145,
	NET_TAG_NEW_VLOOP_BY_ID			=	146,
	NET_TAG_NEW_Y4M				=	147,
	NET_TAG_NEW_RAW				=	148,
	NET_TAG_NEW_AVFORMAT			=	149,
	NET_TAG_OFFLINE_REC_START		=	150,
	NET_TAG_OFFLINE_REC_STOP		=	151,
	NET_TAG_REC_START			=	152,
	NET_TAG_REC_STOP			=	153,
	NET_TAG_LOAD_TAGLIST			=	154,
	NET_TAG_SAVE_TAGLIST			=	155,
	NET_TAG_LIST				=	206,
	NET_TAG_DEVICES				=	207,
	NET_TAG_CHAIN_ENABLE			=	156,
	NET_TAG_CHAIN_DISABLE			=	157,
	NET_TAG_SET_BRIGHTNESS			=	160,
	NET_TAG_SET_CONTRAST			=	161,
	NET_TAG_SET_HUE				=	162,
	NET_TAG_SET_COLOR			=	163,
	NET_TAG_NEW_NET				=	165,
	NET_CHAIN_CHANNEL_INC			=	170,
	NET_CHAIN_CHANNEL_DEC			=	171,
	NET_CHAIN_GET_ENTRY			=	208,
	NET_CHAIN_TOGGLE_ALL			=	172,
	NET_CHAIN_COPY_TO_BUF			=	173,
	NET_CHAIN_PASTE_AS_NEW			=	174,
	NET_CHAIN_ENABLE			=	175,
	NET_CHAIN_DISABLE			=	176,
	NET_CHAIN_CLEAR				=	177,
	NET_CHAIN_FADE_IN			=	178,
	NET_CHAIN_FADE_OUT			=	179,
	NET_CHAIN_LIST				=	209,
	NET_CHAIN_SET_ENTRY			=	180,
	NET_CHAIN_ENTRY_SET_EFFECT		=	181,
	NET_CHAIN_ENTRY_SET_PRESET		=	182,
	NET_CHAIN_ENTRY_SET_ARG_VAL		=	183,
	NET_CHAIN_ENTRY_SET_VIDEO_ON		=	184,
	NET_CHAIN_ENTRY_SET_VIDEO_OFF		=	185,
	NET_CHAIN_ENTRY_SET_AUDIO_ON		=	186,
	NET_CHAIN_ENTRY_SET_AUDIO_OFF		=	187,
	NET_CHAIN_ENTRY_SET_AUDIO_VOL		=	188,
	NET_CHAIN_ENTRY_SET_DEFAULTS		=	189,
	NET_CHAIN_ENTRY_SET_CHANNEL		=	190,
	NET_CHAIN_ENTRY_SET_SOURCE		=	191,
	NET_CHAIN_ENTRY_SET_SOURCE_CHANNEL 	=	192,
	NET_CHAIN_ENTRY_CLEAR			=	193,
	NET_CHAIN_ENTRY_SET_AUTOMATIC		=	194,
	NET_CHAIN_ENTRY_DEL_AUTOMATIC		=	195,
	NET_CHAIN_ENTRY_ENABLE_AUTOMATIC	=	196,
	NET_CHAIN_ENTRY_DISABLE_AUTOMATIC	=	197,
	NET_CHAIN_MANUAL_FADE				=	198,
	NET_EFFECT_SET_BG			=	200,
	NET_OUTPUT_VLOOPBACK_START		=	70,
	NET_SET_VOLUME				=	220,
	NET_FULLSCREEN				=	222,
	NET_OUTPUT_Y4M_START			=	71,
	NET_OUTPUT_Y4M_STOP			=	72,
	NET_OUTPUT_RAW_START			=	73,
	NET_OUTPUT_RAW_STOP			=	74,
	NET_OUTPUT_VLOOPBACK_STARTN		=	75,
	NET_OUTPUT_VLOOPBACK_STOP		=	76,
	NET_GET_FRAME				=	225,
	NET_SAMPLE_MODE			=	250,
	NET_BEZERK				=	251,
	NET_DEBUG_LEVEL				=	252,
	NET_SHM_OPEN				=	253,
	NET_SUSPEND				=	254,
	NET_QUIT				=	255,
	NET_LOAD_PLUGIN				=	249,
	NET_UNLOAD_PLUGIN			=	248,
	NET_CMD_PLUGIN				=	245,
};
#endif
