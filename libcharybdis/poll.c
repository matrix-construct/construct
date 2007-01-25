/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_bsd_poll.c: POSIX poll() compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: poll.c 390 2005-12-07 18:46:56Z nenolod $
 */

#include "config.h"
#include "stdinc.h"
#include <sys/poll.h>

#include "libcharybdis.h"

/* I hate linux -- adrian */
#ifndef POLLRDNORM
#define POLLRDNORM POLLIN
#endif
#ifndef POLLWRNORM
#define POLLWRNORM POLLOUT
#endif

struct _pollfd_list
{
	struct pollfd pollfds[MAXCONNECTIONS];
	int maxindex;		/* highest FD number */
};

typedef struct _pollfd_list pollfd_list_t;

pollfd_list_t pollfd_list;
static void poll_update_pollfds(int, short, PF *);
static unsigned long last_count = 0; 
static unsigned long empty_count = 0;
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * find a spare slot in the fd list. We can optimise this out later!
 *   -- adrian
 */
static inline int
poll_findslot(void)
{
	int i;
	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		if(pollfd_list.pollfds[i].fd == -1)
		{
			/* MATCH!!#$*&$ */
			return i;
		}
	}
	s_assert(1 == 0);
	/* NOTREACHED */
	return -1;
}

/*
 * set and clear entries in the pollfds[] array.
 */
static void
poll_update_pollfds(int fd, short event, PF * handler)
{
	fde_t *F = &fd_table[fd];
	int comm_index;

	if(F->comm_index < 0)
	{
		F->comm_index = poll_findslot();
	}
	comm_index = F->comm_index;

	/* Update the events */
	if(handler)
	{
		F->list = FDLIST_IDLECLIENT;
		pollfd_list.pollfds[comm_index].events |= event;
		pollfd_list.pollfds[comm_index].fd = fd;
		/* update maxindex here */
		if(comm_index > pollfd_list.maxindex)
			pollfd_list.maxindex = comm_index;
	}
	else
	{
		if(comm_index >= 0)
		{
			pollfd_list.pollfds[comm_index].events &= ~event;
			if(pollfd_list.pollfds[comm_index].events == 0)
			{
				pollfd_list.pollfds[comm_index].fd = -1;
				pollfd_list.pollfds[comm_index].revents = 0;
				F->comm_index = -1;
				F->list = FDLIST_NONE;

				/* update pollfd_list.maxindex here */
				if(comm_index == pollfd_list.maxindex)
					while (pollfd_list.maxindex >= 0 &&
					       pollfd_list.pollfds[pollfd_list.maxindex].fd == -1)
						pollfd_list.maxindex--;
			}
		}
	}
}


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Public functions */


/*
 * init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
void
init_netio(void)
{
	int fd;

	for (fd = 0; fd < MAXCONNECTIONS; fd++)
	{
		pollfd_list.pollfds[fd].fd = -1;
	}
	pollfd_list.maxindex = 0;
}

/*
 * comm_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
comm_setselect(int fd, fdlist_t list, unsigned int type, PF * handler,
	       void *client_data, time_t timeout)
{
	fde_t *F = &fd_table[fd];
	s_assert(fd >= 0);
	s_assert(F->flags.open);

	if(type & COMM_SELECT_READ)
	{
		F->read_handler = handler;
		F->read_data = client_data;
		poll_update_pollfds(fd, POLLRDNORM, handler);
	}
	if(type & COMM_SELECT_WRITE)
	{
		F->write_handler = handler;
		F->write_data = client_data;
		poll_update_pollfds(fd, POLLWRNORM, handler);
	}
	if(timeout)
		F->timeout = CurrentTime + (timeout / 1000);
}

static void
irc_sleep(unsigned long useconds)
{     
#ifdef HAVE_NANOSLEEP    
        struct timespec t;
        t.tv_sec = useconds / (unsigned long) 1000000;
        t.tv_nsec = (useconds % (unsigned long) 1000000) * 1000;
        nanosleep(&t, (struct timespec *) NULL);
#else    
        struct timeval t;        
        t.tv_sec = 0;    
        t.tv_usec = useconds;
        select(0, NULL, NULL, NULL, &t);
#endif
        return;
}

/* int comm_select_fdlist(unsigned long delay)
 * Input: The maximum time to delay.
 * Output: Returns -1 on error, 0 on success.
 * Side-effects: Deregisters future interest in IO and calls the handlers
 *               if an event occurs for an FD.
 * Comments: Check all connections for new connections and input data
 * that is to be processed. Also check for connections with data queued
 * and whether we can write it out.
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * comm_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */
int
comm_select(unsigned long delay)
{
	int num;
	int fd;
	int ci;
	unsigned long ndelay;
	PF *hdl;

	if(last_count > 0)
	{
		empty_count = 0;
		ndelay = 0;
	}
	else {
		ndelay = ++empty_count * 15000 ;
		if(ndelay > delay * 1000)
			ndelay = delay * 1000;	
	}
	
	for (;;)
	{
		/* XXX kill that +1 later ! -- adrian */
		if(ndelay > 0)
			irc_sleep(ndelay); 
		last_count = num = poll(pollfd_list.pollfds, pollfd_list.maxindex + 1, 0);
		if(num >= 0)
			break;
		if(ignoreErrno(errno))
			continue;
		/* error! */
		set_time();
		return -1;
		/* NOTREACHED */
	}

	/* update current time again, eww.. */
	set_time();

	if(num == 0)
		return 0;
	/* XXX we *could* optimise by falling out after doing num fds ... */
	for (ci = 0; ci < pollfd_list.maxindex + 1; ci++)
	{
		fde_t *F;
		int revents;
		if(((revents = pollfd_list.pollfds[ci].revents) == 0) ||
		   (pollfd_list.pollfds[ci].fd) == -1)
			continue;
		fd = pollfd_list.pollfds[ci].fd;
		F = &fd_table[fd];
		if(revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
		{
			hdl = F->read_handler;
			F->read_handler = NULL;
			poll_update_pollfds(fd, POLLRDNORM, NULL);
			if(hdl)
				hdl(fd, F->read_data);
		}
		if(revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
		{
			hdl = F->write_handler;
			F->write_handler = NULL;
			poll_update_pollfds(fd, POLLWRNORM, NULL);
			if(hdl)
				hdl(fd, F->write_data);
		}
	}
	return 0;
}

