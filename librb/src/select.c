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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: select.c 26092 2008-09-19 15:13:52Z androsyn $
 */
#define FD_SETSIZE 65535
#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>

#if defined(HAVE_SELECT) || defined(_WIN32)

#ifdef _WIN32
#define MY_FD_SET(x, y) FD_SET((SOCKET)x, y)
#define MY_FD_CLR(x, y) FD_CLR((SOCKET)x, y)
#else
#define MY_FD_SET(x, y) FD_SET(x, y)
#define MY_FD_CLR(x, y) FD_CLR(x, y)
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
/*
 * Note that this is only a single list - multiple lists is kinda pointless
 * under select because the list size is a function of the highest FD :-)
 *   -- adrian
 */

static fd_set select_readfds;
static fd_set select_writefds;

/*
 * You know, I'd rather have these local to rb_select but for some
 * reason my gcc decides that I can't modify them at all..
 *   -- adrian
 */
static fd_set tmpreadfds;
static fd_set tmpwritefds;

static int rb_maxfd = -1;
static void select_update_selectfds(rb_fde_t *F, short event, PF * handler);

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

/*
 * set and clear entries in the select array ..
 */
static void
select_update_selectfds(rb_fde_t *F, short event, PF * handler)
{
	/* Update the read / write set */
	if(event & RB_SELECT_READ)
	{
		if(handler)
		{
			MY_FD_SET(F->fd, &select_readfds);
			F->pflags |= RB_SELECT_READ;
		}
		else
		{
			MY_FD_CLR(F->fd, &select_readfds);
			F->pflags &= ~RB_SELECT_READ;
		}
	}

	if(event & RB_SELECT_WRITE)
	{
		if(handler)
		{
			MY_FD_SET(F->fd, &select_writefds);
			F->pflags |= RB_SELECT_WRITE;
		}
		else
		{
			MY_FD_CLR(F->fd, &select_writefds);
			F->pflags &= ~RB_SELECT_WRITE;
		}
	}

	if(F->pflags & (RB_SELECT_READ | RB_SELECT_WRITE))
	{
		if(F->fd > rb_maxfd)
		{
			rb_maxfd = F->fd;
		}
	}
	else if(F->fd <= rb_maxfd)
	{
		while(rb_maxfd >= 0 && !FD_ISSET(rb_maxfd, &select_readfds)
		      && !FD_ISSET(rb_maxfd, &select_writefds))
			rb_maxfd--;
	}
}


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Public functions */

int
rb_setup_fd_select(rb_fde_t *F)
{
	return 0;
}


/*
 * rb_init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
extern int rb_maxconnections;
int
rb_init_netio_select(void)
{
	if(rb_maxconnections > FD_SETSIZE)
		rb_maxconnections = FD_SETSIZE;	/* override this */
	FD_ZERO(&select_readfds);
	FD_ZERO(&select_writefds);
	return 0;
}

/*
 * rb_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
rb_setselect_select(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	lrb_assert(IsFDOpen(F));

	if(type & RB_SELECT_READ)
	{
		F->read_handler = handler;
		F->read_data = client_data;
		select_update_selectfds(F, RB_SELECT_READ, handler);
	}
	if(type & RB_SELECT_WRITE)
	{
		F->write_handler = handler;
		F->write_data = client_data;
		select_update_selectfds(F, RB_SELECT_WRITE, handler);
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
 * Do IO events
 */

int
rb_select_select(long delay)
{
	int num;
	int fd;
	PF *hdl;
	rb_fde_t *F;
	struct timeval to;

	/* Copy over the read/write sets so we don't have to rebuild em */
	memcpy(&tmpreadfds, &select_readfds, sizeof(fd_set));
	memcpy(&tmpwritefds, &select_writefds, sizeof(fd_set));

	for(;;)
	{
		to.tv_sec = 0;
		to.tv_usec = delay * 1000;
		num = select(rb_maxfd + 1, &tmpreadfds, &tmpwritefds, NULL, &to);
		if(num >= 0)
			break;
		if(rb_ignore_errno(errno))
			continue;
		rb_set_time();
		/* error! */
		return -1;
		/* NOTREACHED */
	}
	rb_set_time();

	if(num == 0)
		return 0;

	/* XXX we *could* optimise by falling out after doing num fds ... */
	for(fd = 0; fd < rb_maxfd + 1; fd++)
	{
		F = rb_find_fd(fd);
		if(F == NULL)
			continue;
		if(FD_ISSET(fd, &tmpreadfds))
		{
			hdl = F->read_handler;
			F->read_handler = NULL;
			if(hdl)
				hdl(F, F->read_data);
		}

		if(!IsFDOpen(F))
			continue;	/* Read handler closed us..go on */

		if(FD_ISSET(fd, &tmpwritefds))
		{
			hdl = F->write_handler;
			F->write_handler = NULL;
			if(hdl)
				hdl(F, F->write_data);
		}

		if(F->read_handler == NULL)
			select_update_selectfds(F, RB_SELECT_READ, NULL);
		if(F->write_handler == NULL)
			select_update_selectfds(F, RB_SELECT_WRITE, NULL);
	}
	return 0;
}

#else /* select not supported..what sort of garbage is this? */
int
rb_init_netio_select(void)
{
	return ENOSYS;
}

void
rb_setselect_select(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	errno = ENOSYS;
	return;
}

int
rb_select_select(long delay)
{
	errno = ENOSYS;
	return -1;
}

int
rb_setup_fd_select(rb_fde_t *F)
{
	errno = ENOSYS;
	return -1;
}

#endif
