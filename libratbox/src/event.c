/*
 *  ircd-ratbox: A slightly useful ircd.
 *  event.c: Event functions.
 *
 *  Copyright (C) 1998-2000 Regents of the University of California
 *  Copyright (C) 2001-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  Code borrowed from the squid web cache by Adrian Chadd.
 *  Original header:
 *
 *  DEBUG: section 41   Event Processing
 *  AUTHOR: Henrik Nordstrom
 *
 *  SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 *  ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by the
 *  National Science Foundation.  Squid is Copyrighted (C) 1998 by
 *  the Regents of the University of California.  Please see the
 *  COPYRIGHT file for full details.  Squid incorporates software
 *  developed and/or copyrighted by other sources.  Please see the
 *  CREDITS file for full details.
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
 *  $Id: event.c 26272 2008-12-10 05:55:10Z androsyn $
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <event-int.h>

#define EV_NAME_LEN 33
static char last_event_ran[EV_NAME_LEN];
static rb_dlink_list event_list;

static time_t event_time_min = -1;

/*
 * struct ev_entry * 
 * rb_event_find(EVH *func, void *arg)
 *
 * Input: Event function and the argument passed to it
 * Output: Index to the slow in the event_table
 * Side Effects: None
 */
static struct ev_entry *
rb_event_find(EVH * func, void *arg)
{
	rb_dlink_node *ptr;
	struct ev_entry *ev;
	RB_DLINK_FOREACH(ptr, event_list.head)
	{
		ev = ptr->data;
		if((ev->func == func) && (ev->arg == arg))
			return ev;
	}

	return NULL;
}

/*
 * struct ev_entry * 
 * rb_event_add(const char *name, EVH *func, void *arg, time_t when)
 *
 * Input: Name of event, function to call, arguments to pass, and frequency
 *	  of the event.
 * Output: None
 * Side Effects: Adds the event to the event list.
 */
struct ev_entry *
rb_event_add(const char *name, EVH * func, void *arg, time_t when)
{
	struct ev_entry *ev;
	ev = rb_malloc(sizeof(struct ev_entry));
	ev->func = func;
	ev->name = rb_strndup(name, EV_NAME_LEN);
	ev->arg = arg;
	ev->when = rb_current_time() + when;
	ev->next = when;
	ev->frequency = when;

	if((ev->when < event_time_min) || (event_time_min == -1))
	{
		event_time_min = ev->when;
	}
	rb_dlinkAdd(ev, &ev->node, &event_list);
	rb_io_sched_event(ev, when);
	return ev;
}

struct ev_entry *
rb_event_addonce(const char *name, EVH * func, void *arg, time_t when)
{
	struct ev_entry *ev;
	ev = rb_malloc(sizeof(struct ev_entry));
	ev->func = func;
	ev->name = rb_strndup(name, EV_NAME_LEN);
	ev->arg = arg;
	ev->when = rb_current_time() + when;
	ev->next = when;
	ev->frequency = 0;

	if((ev->when < event_time_min) || (event_time_min == -1))
		event_time_min = ev->when;

	rb_dlinkAdd(ev, &ev->node, &event_list);
	rb_io_sched_event(ev, when);
	return ev;
}

/*
 * void rb_event_delete(struct ev_entry *ev)
 *
 * Input: pointer to ev_entry for the event
 * Output: None
 * Side Effects: Removes the event from the event list
 */
void
rb_event_delete(struct ev_entry *ev)
{
	if(ev == NULL)
		return;

	rb_dlinkDelete(&ev->node, &event_list);
	rb_io_unsched_event(ev);
	rb_free(ev->name);
	rb_free(ev);
}

/*
 * void rb_event_find_delete(EVH *func, void *arg)
 *
 * Input: pointer to func and data
 * Output: None
 * Side Effects: Removes the event from the event list
 */
void
rb_event_find_delete(EVH * func, void *arg)
{
	rb_event_delete(rb_event_find(func, arg));
}

/* 
 * struct ev_entry *
 * rb_event_addish(const char *name, EVH *func, void *arg, time_t delta_isa)
 *
 * Input: Name of event, function to call, arguments to pass, and frequency
 *	  of the event.
 * Output: None
 * Side Effects: Adds the event to the event list within +- 1/3 of the
 *	         specified frequency.
 */
struct ev_entry *
rb_event_addish(const char *name, EVH * func, void *arg, time_t delta_ish)
{
	if(delta_ish >= 3.0)
	{
		const time_t two_third = (2 * delta_ish) / 3;
		delta_ish = two_third + ((rand() % 1000) * two_third) / 1000;
		/*
		 * XXX I hate the above magic, I don't even know if its right.
		 * Grr. -- adrian
		 */
	}
	return rb_event_add(name, func, arg, delta_ish);
}


void
rb_run_event(struct ev_entry *ev)
{
	rb_strlcpy(last_event_ran, ev->name, sizeof(last_event_ran));
	ev->func(ev->arg);
	if(!ev->frequency)
	{
		rb_io_unsched_event(ev);
		rb_dlinkDelete(&ev->node, &event_list);
		rb_free(ev);
		return;
	}
	ev->when = rb_current_time() + ev->frequency;
	if((ev->when < event_time_min) || (event_time_min == -1))
		event_time_min = ev->when;
}

/*
 * void rb_event_run(void)
 *
 * Input: None
 * Output: None
 * Side Effects: Runs pending events in the event list
 */
void
rb_event_run(void)
{
	rb_dlink_node *ptr, *next;
	struct ev_entry *ev;

	if(rb_io_supports_event())
		return;

	event_time_min = -1;
	RB_DLINK_FOREACH_SAFE(ptr, next, event_list.head)
	{
		ev = ptr->data;
		if(ev->when <= rb_current_time())
		{
			rb_strlcpy(last_event_ran, ev->name, sizeof(last_event_ran));
			ev->func(ev->arg);

			/* event is scheduled more than once */
			if(ev->frequency)
			{
				ev->when = rb_current_time() + ev->frequency;
				if((ev->when < event_time_min) || (event_time_min == -1))
					event_time_min = ev->when;
			}
			else
			{
				rb_dlinkDelete(&ev->node, &event_list);
				rb_free(ev);
			}
		}
		else
		{
			if((ev->when < event_time_min) || (event_time_min == -1))
				event_time_min = ev->when;
		}
	}
}

void
rb_event_io_register_all(void)
{
	rb_dlink_node *ptr;
	struct ev_entry *ev;

	if(!rb_io_supports_event())
		return;

	RB_DLINK_FOREACH(ptr, event_list.head)
	{
		ev = ptr->data;
		rb_io_sched_event(ev, ev->next);
	}
}

/*
 * void rb_event_init(void)
 *
 * Input: None
 * Output: None
 * Side Effects: Initializes the event system. 
 */
void
rb_event_init(void)
{
	rb_strlcpy(last_event_ran, "NONE", sizeof(last_event_ran));
}

void
rb_dump_events(void (*func) (char *, void *), void *ptr)
{
	int len;
	char buf[512];
	rb_dlink_node *dptr;
	struct ev_entry *ev;
	len = sizeof(buf);

	rb_snprintf(buf, len, "Last event to run: %s", last_event_ran);
	func(buf, ptr);

	rb_strlcpy(buf, "Operation                    Next Execution", len);
	func(buf, ptr);

	RB_DLINK_FOREACH(dptr, event_list.head)
	{
		ev = dptr->data;
		rb_snprintf(buf, len, "%-28s %-4ld seconds", ev->name,
			    ev->when - (long)rb_current_time());
		func(buf, ptr);
	}
}

/* 
 * void rb_set_back_events(time_t by)
 * Input: Time to set back events by.
 * Output: None.
 * Side-effects: Sets back all events by "by" seconds.
 */
void
rb_set_back_events(time_t by)
{
	rb_dlink_node *ptr;
	struct ev_entry *ev;
	RB_DLINK_FOREACH(ptr, event_list.head)
	{
		ev = ptr->data;
		if(ev->when > by)
			ev->when -= by;
		else
			ev->when = 0;
	}
}

void
rb_event_update(struct ev_entry *ev, time_t freq)
{
	if(ev == NULL)
		return;

	ev->frequency = freq;

	/* update when its scheduled to run if its higher
	 * than the new frequency
	 */
	if((rb_current_time() + freq) < ev->when)
		ev->when = rb_current_time() + freq;
	return;
}

time_t
rb_event_next(void)
{
	return event_time_min;
}
