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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: devpoll.c 26254 2008-12-10 04:04:38Z androsyn $
 */
#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <fcntl.h>

#if defined(HAVE_DEVPOLL) && (HAVE_SYS_DEVPOLL_H)
#include <sys/devpoll.h>

static int dpfd;
static int maxfd;
static short *fdmask;
static void devpoll_update_events(rb_fde_t *, short, PF *);
static void devpoll_write_update(int, int);


int
rb_setup_fd_devpoll(rb_fde_t *F)
{
	return 0;
}


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
		rb_lib_log("devpoll_write_update: dpfd write failed %d: %s", errno,
			   strerror(errno));
	/* Done! */
}

static void
devpoll_update_events(rb_fde_t *F, short filter, PF * handler)
{
	int update_required = 0;
	int fd = rb_get_fd(F);
	int cur_mask = fdmask[fd];
	PF *cur_handler;
	fdmask[fd] = 0;
	switch (filter)
	{
	case RB_SELECT_READ:
		cur_handler = F->read_handler;
		if(handler)
			fdmask[fd] |= POLLRDNORM;
		else
			fdmask[fd] &= ~POLLRDNORM;
		if(F->write_handler)
			fdmask[fd] |= POLLWRNORM;
		break;
	case RB_SELECT_WRITE:
		cur_handler = F->write_handler;
		if(handler)
			fdmask[fd] |= POLLWRNORM;
		else
			fdmask[fd] &= ~POLLWRNORM;
		if(F->read_handler)
			fdmask[fd] |= POLLRDNORM;
		break;
	default:
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
 * rb_init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
int
rb_init_netio_devpoll(void)
{
	dpfd = open("/dev/poll", O_RDWR);
	if(dpfd < 0)
	{
		return errno;
	}
	maxfd = getdtablesize() - 2;	/* This makes more sense than HARD_FDLIMIT */
	fdmask = rb_malloc(sizeof(fdmask) * maxfd + 1);
	rb_open(dpfd, RB_FD_UNKNOWN, "/dev/poll file descriptor");
	return 0;
}

/*
 * rb_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
rb_setselect_devpoll(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	lrb_assert(IsFDOpen(F));

	if(type & RB_SELECT_READ)
	{
		devpoll_update_events(F, RB_SELECT_READ, handler);
		F->read_handler = handler;
		F->read_data = client_data;
	}
	if(type & RB_SELECT_WRITE)
	{
		devpoll_update_events(F, RB_SELECT_WRITE, handler);
		F->write_handler = handler;
		F->write_data = client_data;
	}
}

/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 */

/*
 * rb_select
 *
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * rb_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */

int
rb_select_devpoll(long delay)
{
	int num, i;
	struct pollfd pollfds[maxfd];
	struct dvpoll dopoll;

	do
	{
		for(;;)
		{
			dopoll.dp_timeout = delay;
			dopoll.dp_nfds = maxfd;
			dopoll.dp_fds = &pollfds[0];
			num = ioctl(dpfd, DP_POLL, &dopoll);
			if(num >= 0)
				break;
			if(rb_ignore_errno(errno))
				break;
			rb_set_time();
			return RB_ERROR;
		}

		rb_set_time();
		if(num == 0)
			continue;

		for(i = 0; i < num; i++)
		{
			int fd = dopoll.dp_fds[i].fd;
			PF *hdl = NULL;
			rb_fde_t *F = rb_find_fd(fd);
			if((dopoll.dp_fds[i].revents & (POLLRDNORM | POLLIN | POLLHUP |
							POLLERR))
			   && (dopoll.dp_fds[i].events & (POLLRDNORM | POLLIN)))
			{
				if((hdl = F->read_handler) != NULL)
				{
					F->read_handler = NULL;
					hdl(F, F->read_data);
					/*
					 * this call used to be with a NULL pointer, BUT
					 * in the devpoll case we only want to update the
					 * poll set *if* the handler changes state (active ->
					 * NULL or vice versa.)
					 */
					devpoll_update_events(F, RB_SELECT_READ, F->read_handler);
				}
			}

			if(!IsFDOpen(F))
				continue;	/* Read handler closed us..go on to do something more useful */
			if((dopoll.dp_fds[i].revents & (POLLWRNORM | POLLOUT | POLLHUP |
							POLLERR))
			   && (dopoll.dp_fds[i].events & (POLLWRNORM | POLLOUT)))
			{
				if((hdl = F->write_handler) != NULL)
				{
					F->write_handler = NULL;
					hdl(F, F->write_data);
					/* See above similar code in the read case */
					devpoll_update_events(F,
							      RB_SELECT_WRITE, F->write_handler);
				}

			}
		}
		return RB_OK;
	}
	while(0);
	/* XXX Get here, we broke! */
	return 0;
}

#else /* /dev/poll not supported */
int
rb_init_netio_devpoll(void)
{
	return ENOSYS;
}

void
rb_setselect_devpoll(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	errno = ENOSYS;
	return;
}

int
rb_select_devpoll(long delay)
{
	errno = ENOSYS;
	return -1;
}

int
rb_setup_fd_devpoll(rb_fde_t *F)
{
	errno = ENOSYS;
	return -1;
}
#endif
