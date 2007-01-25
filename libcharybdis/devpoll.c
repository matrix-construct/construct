/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_bsd_devpoll.c: /dev/poll compatible network routines.
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
 *  $Id: devpoll.c 390 2005-12-07 18:46:56Z nenolod $
 */

#include "config.h"

#include "stdinc.h"
#include <sys/devpoll.h>

#include "libcharybdis.h"

#define POLL_LENGTH	HARD_FDLIMIT


static void devpoll_update_events(int, short, PF *);
static int dpfd;
static short fdmask[POLL_LENGTH];
static void devpoll_update_events(int, short, PF *);
static void devpoll_write_update(int, int);

/* #define NOTYET 1 */

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * Write an update to the devpoll filter.
 * See, we end up having to do a seperate (?) remove before we do an
 * add of a new polltype, so we have to have this function seperate from
 * the others.
 */
static void
devpoll_write_update(int fd, int events)
{
	struct pollfd pollfds[1];	/* Just to be careful */
	int retval;

	/* Build the pollfd entry */
	pollfds[0].revents = 0;
	pollfds[0].fd = fd;
	pollfds[0].events = events;

	/* Write the thing to our poll fd */
	retval = write(dpfd, &pollfds[0], sizeof(struct pollfd));
	if(retval != sizeof(struct pollfd))
		libcharybdis_log("devpoll_write_update: dpfd write failed %d: %s", errno, strerror(errno));
	/* Done! */
}

void
devpoll_update_events(int fd, short filter, PF * handler)
{
	int update_required = 0;
	int cur_mask = fdmask[fd];
	PF *cur_handler;
	fdmask[fd] = 0;
	switch (filter)
	{
	case COMM_SELECT_READ:
		cur_handler = fd_table[fd].read_handler;
		if(handler)
			fdmask[fd] |= POLLRDNORM;
		else
			fdmask[fd] &= ~POLLRDNORM;
		if(fd_table[fd].write_handler)
			fdmask[fd] |= POLLWRNORM;
		break;
	case COMM_SELECT_WRITE:
		cur_handler = fd_table[fd].write_handler;
		if(handler)
			fdmask[fd] |= POLLWRNORM;
		else
			fdmask[fd] &= ~POLLWRNORM;
		if(fd_table[fd].read_handler)
			fdmask[fd] |= POLLRDNORM;
		break;
	default:
#ifdef NOTYET
		libcharybdis_log("devpoll_update_events called with unknown filter: %hd", filter);
#endif
		return;
		break;
	}

	if(cur_handler == NULL && handler != NULL)
		update_required++;
	else if(cur_handler != NULL && handler == NULL)
		update_required++;
	if(cur_mask != fdmask[fd])
		update_required++;
	if(update_required)
	{
		/*
		 * Ok, we can call devpoll_write_update() here now to re-build the
		 * fd struct. If we end up with nothing on this fd, it won't write
		 * anything.
		 */
		if(fdmask[fd])
		{
			devpoll_write_update(fd, POLLREMOVE);
			devpoll_write_update(fd, fdmask[fd]);
		}
		else
			devpoll_write_update(fd, POLLREMOVE);
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
	memset(&fdmask, 0, sizeof(fdmask));
	dpfd = open("/dev/poll", O_RDWR);
	if(dpfd < 0)
	{
		fprintf(stderr,
		     "init_netio: Couldn't open /dev/poll - %d: %s\n",
		     errno, strerror(errno));
		exit(115);	/* Whee! */
	}
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

	/* Update the list, even though we're not using it .. */
	F->list = list;

	if(type & COMM_SELECT_READ)
	{
		devpoll_update_events(fd, COMM_SELECT_READ, handler);
		F->read_handler = handler;
		F->read_data = client_data;
	}
	if(type & COMM_SELECT_WRITE)
	{
		devpoll_update_events(fd, COMM_SELECT_WRITE, handler);
		F->write_handler = handler;
		F->write_data = client_data;
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
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * comm_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */

int
comm_select(unsigned long delay)
{
	int num, i;
	struct pollfd pollfds[POLL_LENGTH];
	struct dvpoll dopoll;

	do
	{
		for (;;)
		{
			dopoll.dp_timeout = delay;
			dopoll.dp_nfds = POLL_LENGTH;
			dopoll.dp_fds = &pollfds[0];
			num = ioctl(dpfd, DP_POLL, &dopoll);
			if(num >= 0)
				break;
			if(ignoreErrno(errno))
				break;
			set_time();
			return COMM_ERROR;
		}

		set_time();
		if(num == 0)
			continue;

		for (i = 0; i < num; i++)
		{
			int fd = dopoll.dp_fds[i].fd;
			PF *hdl = NULL;
			fde_t *F = &fd_table[fd];
			if((dopoll.dp_fds[i].
			    revents & (POLLRDNORM | POLLIN | POLLHUP |
				       POLLERR))
			   && (dopoll.dp_fds[i].events & (POLLRDNORM | POLLIN)))
			{
				if((hdl = F->read_handler) != NULL)
				{
					F->read_handler = NULL;
					hdl(fd, F->read_data);
					/*
					 * this call used to be with a NULL pointer, BUT
					 * in the devpoll case we only want to update the
					 * poll set *if* the handler changes state (active ->
					 * NULL or vice versa.)
					 */
					devpoll_update_events(fd,
							      COMM_SELECT_READ, F->read_handler);
				}
				else
					libcharybdis_log("comm_select: Unhandled read event: fdmask: %x",
					     fdmask[fd]);
			}

			if(F->flags.open == 0)
				continue;	/* Read handler closed us..go on to do something more useful */
			if((dopoll.dp_fds[i].
			    revents & (POLLWRNORM | POLLOUT | POLLHUP |
				       POLLERR))
			   && (dopoll.dp_fds[i].events & (POLLWRNORM | POLLOUT)))
			{
				if((hdl = F->write_handler) != NULL)
				{
					F->write_handler = NULL;
					hdl(fd, F->write_data);
					/* See above similar code in the read case */
					devpoll_update_events(fd,
							      COMM_SELECT_WRITE, F->write_handler);
				}
				else
					libcharybdis_log("comm_select: Unhandled write event: fdmask: %x",
					     fdmask[fd]);

			}
			if(dopoll.dp_fds[i].revents & POLLNVAL)
			{
				libcharybdis_log("revents was Invalid for %d", fd);
			}
		}
		return COMM_OK;
	}
	while (0);
	/* XXX Get here, we broke! */
	return 0;
}

