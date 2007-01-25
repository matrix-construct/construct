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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: event.c 498 2006-01-15 16:40:33Z jilles $
 */

/*
 * How its used:
 *
 * Should be pretty self-explanatory. Events are added to the static
 * array event_table with a frequency time telling eventRun how often
 * to execute it.
 */

#include "stdinc.h"
#include "config.h"

#include "ircd.h"
#include "event.h"
#include "client.h"
#include "send.h"
#include "memory.h"
#include "s_log.h"
#include "numeric.h"

static const char *last_event_ran = NULL;
struct ev_entry event_table[MAX_EVENTS];
static time_t event_time_min = -1;

/*
 * void eventAdd(const char *name, EVH *func, void *arg, time_t when)
 *
 * Input: Name of event, function to call, arguments to pass, and frequency
 *	  of the event.
 * Output: None
 * Side Effects: Adds the event to the event list.
 */
void
eventAdd(const char *name, EVH * func, void *arg, time_t when)
{
	int i;

	/* find first inactive index */
	for (i = 0; i < MAX_EVENTS; i++)
	{
		if(event_table[i].active == 0)
		{
			event_table[i].func = func;
			event_table[i].name = name;
			event_table[i].arg = arg;
			event_table[i].when = CurrentTime + when;
			event_table[i].frequency = when;
			event_table[i].active = 1;

			if((event_table[i].when < event_time_min) || (event_time_min == -1))
				event_time_min = event_table[i].when;

			return;
		}
	}

	/* erk! couldnt add to event table */
	sendto_realops_snomask(SNO_DEBUG, L_ALL, "Unable to add event [%s] to event table", name);

}

void
eventAddOnce(const char *name, EVH *func, void *arg, time_t when)
{
	int i;

	/* find first inactive index */
	for (i = 0; i < MAX_EVENTS; i++)
	{
		if(event_table[i].active == 0)
		{
			event_table[i].func = func;
			event_table[i].name = name;
			event_table[i].arg = arg;
			event_table[i].when = CurrentTime + when;
			event_table[i].frequency = 0;
			event_table[i].active = 1;

			if ((event_table[i].when < event_time_min) || (event_time_min == -1))
				event_time_min = event_table[i].when;

			return;
		}
	}

	/* erk! couldnt add to event table */
	sendto_realops_snomask(SNO_DEBUG, L_ALL,
			     "Unable to add event [%s] to event table", name);
}

/*
 * void eventDelete(EVH *func, void *arg)
 *
 * Input: Function handler, argument that was passed.
 * Output: None
 * Side Effects: Removes the event from the event list
 */
void
eventDelete(EVH * func, void *arg)
{
	int i;

	i = eventFind(func, arg);

	if(i == -1)
		return;

	event_table[i].name = NULL;
	event_table[i].func = NULL;
	event_table[i].arg = NULL;
	event_table[i].active = 0;
}

/* 
 * void eventAddIsh(const char *name, EVH *func, void *arg, time_t delta_isa)
 *
 * Input: Name of event, function to call, arguments to pass, and frequency
 *	  of the event.
 * Output: None
 * Side Effects: Adds the event to the event list within +- 1/3 of the
 *	         specified frequency.
 */
void
eventAddIsh(const char *name, EVH * func, void *arg, time_t delta_ish)
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
	eventAdd(name, func, arg, delta_ish);
}

/*
 * void eventRun(void)
 *
 * Input: None
 * Output: None
 * Side Effects: Runs pending events in the event list
 */
void
eventRun(void)
{
	int i;

	for (i = 0; i < MAX_EVENTS; i++)
	{
		if(event_table[i].active && (event_table[i].when <= CurrentTime))
		{
			last_event_ran = event_table[i].name;
			event_table[i].func(event_table[i].arg);
			event_time_min = -1;

			/* event is scheduled more than once */
			if(event_table[i].frequency)
				event_table[i].when = CurrentTime + event_table[i].frequency;
			else
			{
				event_table[i].name = NULL;
				event_table[i].func = NULL;
				event_table[i].arg = NULL;
				event_table[i].active = 0;
			}
		}
	}
}


/*
 * time_t eventNextTime(void)
 * 
 * Input: None
 * Output: Specifies the next time eventRun() should be run
 * Side Effects: None
 */
time_t
eventNextTime(void)
{
	int i;

	if(event_time_min == -1)
	{
		for (i = 0; i < MAX_EVENTS; i++)
		{
			if(event_table[i].active &&
			   ((event_table[i].when < event_time_min) || (event_time_min == -1)))
				event_time_min = event_table[i].when;
		}
	}

	return event_time_min;
}

/*
 * void eventInit(void)
 *
 * Input: None
 * Output: None
 * Side Effects: Initializes the event system. 
 */
void
eventInit(void)
{
	last_event_ran = NULL;
	memset((void *) event_table, 0, sizeof(event_table));
}

/*
 * int eventFind(EVH *func, void *arg)
 *
 * Input: Event function and the argument passed to it
 * Output: Index to the slow in the event_table
 * Side Effects: None
 */
int
eventFind(EVH * func, void *arg)
{
	int i;

	for (i = 0; i < MAX_EVENTS; i++)
	{
		if((event_table[i].func == func) &&
		   (event_table[i].arg == arg) && event_table[i].active)
			return i;
	}

	return -1;
}

/* 
 * void show_events(struct Client *source_p)
 *
 * Input: Client requesting the event
 * Output: List of events
 * Side Effects: None
 */
void
show_events(struct Client *source_p)
{
	int i;

	if(last_event_ran)
		sendto_one_numeric(source_p, RPL_STATSDEBUG, 
				   "E :Last event to run: %s",
				   last_event_ran);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "E :Operation                    Next Execution");

	for (i = 0; i < MAX_EVENTS; i++)
	{
		if(event_table[i].active)
		{
			sendto_one_numeric(source_p, RPL_STATSDEBUG, 
					   "E :%-28s %-4d seconds",
					   event_table[i].name, 
					   (int)(event_table[i].when - CurrentTime));
		}
	}
}

/* 
 * void set_back_events(time_t by)
 * Input: Time to set back events by.
 * Output: None.
 * Side-effects: Sets back all events by "by" seconds.
 */
void
set_back_events(time_t by)
{
	int i;

	for (i = 0; i < MAX_EVENTS; i++)
	{
		if(event_table[i].when > by)
			event_table[i].when -= by;
		else
			event_table[i].when = 0;
	}
}

void
eventUpdate(const char *name, time_t freq)
{
	int i;

	for(i = 0; i < MAX_EVENTS; i++)
	{
		if(event_table[i].active && 
		   !irccmp(event_table[i].name, name))
		{
			event_table[i].frequency = freq;

			/* update when its scheduled to run if its higher
			 * than the new frequency
			 */
			if((CurrentTime + freq) < event_table[i].when)
				event_table[i].when = CurrentTime + freq;

			return;
		}
	}
}


