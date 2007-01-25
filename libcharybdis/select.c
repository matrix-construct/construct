/*
 *  ircd-ratbox: A slightly useful ircd.
 *  select.c: select() compatible network routines.
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
 *  $Id: select.c 390 2005-12-07 18:46:56Z nenolod $
 */

#include "config.h"

#include "libcharybdis.h"

#if HARD_FDLIMIT_ >= FD_SETSIZE
#error HARD_FDLIMIT_ must be less than FD_SETSIZE(try using poll instead of select)
#endif
/*
 * Note that this is only a single list - multiple lists is kinda pointless
 * under select because the list size is a function of the highest FD :-)
 *   -- adrian
 */

fd_set select_readfds;
fd_set select_writefds;

/*
 * You know, I'd rather have these local to comm_select but for some
 * reason my gcc decides that I can't modify them at all..
 *   -- adrian
 */
fd_set tmpreadfds;
fd_set tmpwritefds;

static void select_update_selectfds(int fd, short event, PF * handler);

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * set and clear entries in the select array ..
 */
static void
select_update_selectfds(int fd, short event, PF * handler)
{
	/* Update the read / write set */
	if(event & COMM_SELECT_READ)
	{
		if(handler)
			FD_SET(fd, &select_readfds);
		else
			FD_CLR(fd, &select_readfds);
	}
	if(event & COMM_SELECT_WRITE)
	{
		if(handler)
			FD_SET(fd, &select_writefds);
		else
			FD_CLR(fd, &select_writefds);
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
	FD_ZERO(&select_readfds);
	FD_ZERO(&select_writefds);
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
		select_update_selectfds(fd, COMM_SELECT_READ, handler);
	}
	if(type & COMM_SELECT_WRITE)
	{
		F->write_handler = handler;
		F->write_data = client_data;
		select_update_selectfds(fd, COMM_SELECT_WRITE, handler);
	}
	if(timeout)
		F->timeout = CurrentTime + (timeout / 1000);
}

/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 */

/*
 * comm_select
 *
 * Do IO events
 */

int
comm_select(unsigned long delay)
{
	int num;
	int fd;
	PF *hdl;
	fde_t *F;
	struct timeval to;

	/* Copy over the read/write sets so we don't have to rebuild em */
	memcpy(&tmpreadfds, &select_readfds, sizeof(fd_set));
	memcpy(&tmpwritefds, &select_writefds, sizeof(fd_set));

	for (;;)
	{
		to.tv_sec = 0;
		to.tv_usec = delay * 1000;
		num = select(highest_fd + 1, &tmpreadfds, &tmpwritefds, NULL, &to);
		if(num >= 0)
			break;
		if(ignoreErrno(errno))
			continue;
		set_time();
		/* error! */
		return -1;
		/* NOTREACHED */
	}
	set_time();

	if(num == 0)
		return 0;

	/* XXX we *could* optimise by falling out after doing num fds ... */
	for (fd = 0; fd < highest_fd + 1; fd++)
	{
		F = &fd_table[fd];

		if(FD_ISSET(fd, &tmpreadfds))
		{
			hdl = F->read_handler;
			F->read_handler = NULL;
			if(hdl)
				hdl(fd, F->read_data);
		}

		if(F->flags.open == 0)
			continue;	/* Read handler closed us..go on */

		if(FD_ISSET(fd, &tmpwritefds))
		{
			hdl = F->write_handler;
			F->write_handler = NULL;
			if(hdl)
				hdl(fd, F->write_data);
		}

		if(F->read_handler == NULL)
			select_update_selectfds(fd, COMM_SELECT_READ, NULL);
		if(F->write_handler == NULL)
			select_update_selectfds(fd, COMM_SELECT_WRITE, NULL);
	}
	return 0;
}

