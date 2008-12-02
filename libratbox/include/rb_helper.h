/*
 *  ircd-ratbox: A slightly useful ircd
 *  helper.h: Starts and deals with ircd helpers
 *  
 *  Copyright (C) 2006 Aaron Sethman <androsyn@ratbox.org>
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
 *  $Id: rb_helper.h 26092 2008-09-19 15:13:52Z androsyn $
 */

#ifndef RB_LIB_H
# error "Do not use helper.h directly"
#endif

#ifndef INCLUDED_helper_h
#define INCLUDED_helper_h

struct _rb_helper;
typedef struct _rb_helper rb_helper;

typedef void rb_helper_cb(rb_helper *);



rb_helper *rb_helper_start(const char *name, const char *fullpath, rb_helper_cb * read_cb,
			   rb_helper_cb * error_cb);

rb_helper *rb_helper_child(rb_helper_cb * read_cb, rb_helper_cb * error_cb,
			   log_cb * ilog, restart_cb * irestart, die_cb * idie,
			   int maxcon, size_t lb_heap_size, size_t dh_size, size_t fd_heap_size);

void rb_helper_restart(rb_helper *helper);
#ifdef __GNUC__
void
rb_helper_write(rb_helper *helper, const char *format, ...)
__attribute((format(printf, 2, 3)));
     void rb_helper_write_queue(rb_helper *helper, const char *format, ...)
	__attribute((format(printf, 2, 3)));
#else
void rb_helper_write(rb_helper *helper, const char *format, ...);
void rb_helper_write_queue(rb_helper *helper, const char *format, ...);
#endif
void rb_helper_write_flush(rb_helper *helper);

void rb_helper_run(rb_helper *helper);
void rb_helper_close(rb_helper *helper);
int rb_helper_read(rb_helper *helper, void *buf, size_t bufsize);
void rb_helper_loop(rb_helper *helper, long delay);
#endif
