/* veejay - Linux VeeJay
 *           (C) 2002-2004 Niels Elburg <nelburg@looze.net> 
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


#ifndef VIMS_H
#define VIMS_H

/*
 * VIMS selectors
 */

enum {
	VIMS_BUNDLE_START	=	500,
	VIMS_BUNDLE_END		=	599,	
	VIMS_MAX		=	702,
};

enum {
/* query information */
	VIMS_EFFECT_LIST			=	401,
	VIMS_EDITLIST_LIST			=	402,
	VIMS_BUNDLE_LIST			=	403,
	VIMS_STREAM_DEVICES			=	406,
	VIMS_SAMPLE_LIST			=	408,
	VIMS_CHAIN_GET_ENTRY			=	410,
	VIMS_VIMS_LIST				=	411,
	VIMS_LOG				=	412,
	VIMS_SAMPLE_INFO			=	413,
/* general controls */
	VIMS_FULLSCREEN				=	301,
	VIMS_QUIT				=	600,
	VIMS_RECORD_DATAFORMAT			=	302,
	VIMS_AUDIO_SET_ACTIVE			=	306,
	VIMS_REC_AUTO_START			=	320,	
	VIMS_REC_STOP				=	321,
	VIMS_REC_START				=	322,
	VIMS_BEZERK				=	324,
	VIMS_DEBUG_LEVEL			=	325,
	VIMS_RESIZE_SCREEN			=	326,
	VIMS_SCREENSHOT				=	330,
	VIMS_RGB24_IMAGE			=	333,

/* video controls */
	VIMS_SAMPLE_PLAY_FORWARD			=	10,
	VIMS_SAMPLE_PLAY_BACKWARD		=	11,
	VIMS_SAMPLE_PLAY_STOP			=	12,
	VIMS_SAMPLE_SKIP_FRAME			=	13,
	VIMS_SAMPLE_PREV_FRAME			=	14,
	VIMS_SAMPLE_SKIP_SECOND			=	15,
	VIMS_SAMPLE_PREV_SECOND			=	16,
	VIMS_SAMPLE_GOTO_START			=	17,
	VIMS_SAMPLE_GOTO_END			=	18,
	VIMS_SAMPLE_SET_FRAME			=	19,
	VIMS_SAMPLE_SET_SPEED			=	20,
	VIMS_SAMPLE_SET_SLOW			=	21,
/* editlist commands */

	VIMS_SAMPLE_NEW				=	100,
	VIMS_SAMPLE_SELECT			=	101,
	VIMS_SAMPLE_DEL				=	120,
	VIMS_SAMPLE_SET_PROPERTIES		=	103,
	VIMS_SAMPLE_LOAD			=	125,
	VIMS_SAMPLE_SAVE			=	126,
	VIMS_SAMPLE_DEL_ALL			=	121,
	VIMS_SAMPLE_COPY			=	127,
	VIMS_SAMPLE_REC_START			=	130,
	VIMS_SAMPLE_REC_STOP			=	131,
	VIMS_SAMPLE_CHAIN_ACTIVE		=	112,
	VIMS_SAMPLE_SET_VOLUME			=	132,
	VIMS_SAMPLE_EDL_PASTE_AT			=	50,
	VIMS_SAMPLE_EDL_COPY			=	51,
	VIMS_SAMPLE_EDL_DEL			=	52,
	VIMS_SAMPLE_EDL_CROP			=	53,
	VIMS_SAMPLE_EDL_CUT			=	54,
	VIMS_SAMPLE_EDL_ADD			=	55,
	VIMS_SAMPLE_EDL_SAVE			=	58,
	VIMS_SAMPLE_EDL_LOAD			=	59,
	VIMS_SAMPLE_EDL_EXPORT			=	56,
	
	VIMS_SAMPLE_CHAIN_SET_ACTIVE			=	353,
	VIMS_SAMPLE_CHAIN_CLEAR			=	355,
	VIMS_SAMPLE_CHAIN_LIST				=	358,
	VIMS_SAMPLE_CHAIN_SET_ENTRY			=	359,
	VIMS_SAMPLE_CHAIN_ENTRY_SET_FX			=	360,
	VIMS_SAMPLE_CHAIN_ENTRY_SET_PRESET		=	361,
	VIMS_SAMPLE_CHAIN_ENTRY_SET_ACTIVE		=	363,
	VIMS_SAMPLE_CHAIN_ENTRY_SET_VALUE		=	366,
	VIMS_SAMPLE_CHAIN_ENTRY_SET_INPUT		=	367,
	VIMS_SAMPLE_CHAIN_ENTRY_CLEAR			=	369,
	VIMS_SAMPLE_CHAIN_ENTRY_SET_ALPHA		=	370,

	VIMS_SAMPLE_CHAIN_ENTRY_SET_STATE		=	377,

	VIMS_GET_FRAME				=	42,

	VIMS_SET_PORT				=	650,
	VIMS_GET_PORT				=	649,
	
};
#endif
