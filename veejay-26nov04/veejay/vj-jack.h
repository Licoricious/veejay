#ifndef VJ_JACK_H
#define VJ_JACK_H

#include <config.h>
#ifdef HAVE_JACK
#include <veejay/vj-el.h>

int vj_jack_init(editlist *el);

int vj_jack_update_buffer( uint8_t *buff, int bps, int num_channels, int buf_len);

int vj_jack_stop();

int vj_jack_start();

int vj_jack_pause();

int vj_jack_resume();

int vj_jack_continue(int speed);
#endif
#endif
