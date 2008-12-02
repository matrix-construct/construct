/*
 *  ircd-ratbox: A slightly useful ircd.
 *  event.h: The ircd event header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  $Id: rb_event.h 26092 2008-09-19 15:13:52Z androsyn $
 */

#ifndef RB_LIB_H
# error "Do not use event.h directly"
#endif

#ifndef INCLUDED_event_h
#define INCLUDED_event_h

struct ev_entry;
typedef void EVH(void *);

struct ev_entry *rb_event_add(const char *name, EVH * func, void *arg, time_t when);
struct ev_entry *rb_event_addonce(const char *name, EVH * func, void *arg, time_t when);
struct ev_entry *rb_event_addish(const char *name, EVH * func, void *arg, time_t delta_ish);
void rb_event_run(void);
void rb_event_init(void);
void rb_event_delete(struct ev_entry *);
void rb_event_find_delete(EVH * func, void *);
void rb_event_update(struct ev_entry *, time_t freq);
void rb_set_back_events(time_t);
void rb_dump_events(void (*func) (char *, void *), void *ptr);
void rb_run_event(struct ev_entry *);
time_t rb_event_next(void);

#endif /* INCLUDED_event_h */
