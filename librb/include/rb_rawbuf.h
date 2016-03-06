/*
 *  ircd-ratbox: A slightly useful ircd.
 *  rawbuf.h: A header for rawbuf.c
 *
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2007 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id$
 */

#ifndef RB_LIB_H
# error "Do not use rawbuf.h directly"
#endif

#ifndef INCLUDED_RAWBUF_H__
#define INCLUDED_RAWBUF_H__



typedef struct _rawbuf rawbuf_t;
typedef struct _rawbuf_head rawbuf_head_t;

void rb_init_rawbuffers(int heapsize);
void rb_free_rawbuffer(rawbuf_head_t *);
rawbuf_head_t *rb_new_rawbuffer(void);
int rb_rawbuf_get(rawbuf_head_t *, void *data, int len);
void rb_rawbuf_append(rawbuf_head_t *, void *data, int len);
int rb_rawbuf_flush(rawbuf_head_t *, rb_fde_t *F);
int rb_rawbuf_length(rawbuf_head_t * rb);

#endif
