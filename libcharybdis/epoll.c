/*
 *  ircd-ratbox: A slightly useful ircd.
 *  epoll.c: Linux epoll compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2002 Aaron Sethman <androsyn@ratbox.org>
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
 *  $Id: epoll.c 390 2005-12-07 18:46:56Z nenolod $
 */

#include "config.h"
#include "stdinc.h"
#include <sys/epoll.h>

#include "libcharybdis.h"

static int ep;			/* epoll file descriptor */
static struct epoll_event *pfd;
static int pfd_size;


#ifndef HAVE_EPOLL_CTL /* bah..glibc doesn't support epoll yet.. */
#include <sys/epoll.h>
#include <sys/syscall.h>

_syscall1(int, epoll_create, int, maxfds);
_syscall4(int, epoll_ctl, int, epfd, int, op, int, fd, struct epoll_event *, events);
_syscall4(int, epoll_wait, int, epfd, struct epoll_event *, pevents,
		 int, maxevents, int, timeout);

#endif /* HAVE_EPOLL_CTL */


/*
 * init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
void
init_netio(void)
{
	pfd_size = getdtablesize();
	ep = epoll_create(pfd_size);
	pfd = MyMalloc(sizeof(struct epoll_event) * pfd_size);
	if(ep < 0)
	{
		fprintf(stderr, "init_netio: Couldn't open epoll fd!\n");
		exit(115);	/* Whee! */
	}
	comm_note(ep, "epoll file descriptor");
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
	struct epoll_event ep_event;
	fde_t *F = &fd_table[fd];
	int old_flags = F->pflags;
	int op = -1;
	
	s_assert(fd >= 0);
	s_assert(F->flags.open);
	
	/* Update the list, even though we're not using it .. */
	F->list = list;
	if(type & COMM_SELECT_READ)
	{
		if(handler != NULL)
			F->pflags |= EPOLLIN;
		else
			F->pflags &= ~EPOLLIN;
		F->read_handler = handler;
		F->read_data = client_data;
	}

	if(type & COMM_SELECT_WRITE)
	{
		if(handler != NULL)
			F->pflags |= EPOLLOUT;
		else
			F->pflags &= ~EPOLLOUT;
		F->write_handler = handler;
		F->write_data = client_data;
	}

	if(timeout)
		F->timeout = CurrentTime + (timeout / 1000);

	if(old_flags == 0 && F->pflags == 0)
		return;
	else if(F->pflags <= 0)
		op = EPOLL_CTL_DEL;
	else if(old_flags == 0 && F->pflags > 0)
		op = EPOLL_CTL_ADD;
	else if(F->pflags != old_flags)
		op = EPOLL_CTL_MOD;

	if(op == -1)
		return;


	ep_event.events = F->pflags;
	ep_event.data.ptr = F;

	if(epoll_ctl(ep, op, fd, &ep_event) != 0)
	{
		libcharybdis_log("comm_setselect(): epoll_ctl failed: %s", strerror(errno));
		abort();
	}


}

/*
 * comm_select
 *
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * comm_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */

int
comm_select(unsigned long delay)
{
	int num, i, flags, old_flags, op;
	struct epoll_event ep_event;
	void *data;

	num = epoll_wait(ep, pfd, pfd_size, delay);
	set_time();
	if(num < 0 && !ignoreErrno(errno))
	{
		return COMM_ERROR;
	}

	if(num == 0)
		return COMM_OK;
	for (i = 0; i < num; i++)
	{
		PF *hdl;
		fde_t *F = pfd[i].data.ptr;
		old_flags = F->pflags;
		if(pfd[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR))
		{
			hdl = F->read_handler;
			data = F->read_data;
			F->read_handler = NULL;
			F->read_data = NULL;
			if(hdl) {
				hdl(F->fd, data);
			}
			else
				libcharybdis_log("epoll.c: NULL read handler called");

		}


		if(F->flags.open == 0)
			continue;
		if(pfd[i].events & (EPOLLOUT | EPOLLHUP | EPOLLERR))
		{
			hdl = F->write_handler;
			data = F->write_data;
			F->write_handler = NULL;
			F->write_data = NULL;

			if(hdl) {
				hdl(F->fd, data);
			}
			else
				libcharybdis_log("epoll.c: NULL write handler called");
		}
		
		if(F->flags.open == 0)
			continue;		
		
		flags = 0;
		
		if(F->read_handler != NULL)
			flags |= EPOLLIN;
		if(F->write_handler != NULL)
			flags |= EPOLLOUT;
		
		if(old_flags != flags)
		{
			if(flags == 0)
				op = EPOLL_CTL_DEL;			
			else
				op = EPOLL_CTL_MOD;
			F->pflags = ep_event.events = flags;
			ep_event.data.ptr = F;
			if(epoll_ctl(ep, op, F->fd, &ep_event) != 0)
			{
				libcharybdis_log("comm_setselect(): epoll_ctl failed: %s", strerror(errno));
			}
		}
					
	}
	return COMM_OK;
}

