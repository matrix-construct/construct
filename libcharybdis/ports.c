/*
 *  charybdis: A slightly useful ircd.
 *  ports.c: Solaris ports compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005 Edward Brocklesby.
 *  Copyright (C) 2005 William Pitcock and Jilles Tjoelker
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
 *  $Id$
 */

#include "stdinc.h"
#include <port.h>
#include <time.h>

#include "libcharybdis.h"

#define PE_LENGTH	128

static void pe_update_events(fde_t *, short, PF *);
static int pe;
static struct timespec zero_timespec;

static port_event_t *pelst;	/* port buffer */
static int pemax;		/* max structs to buffer */

int 
ircd_setup_fd(int fd)
{
        return 0;
}
        

void
pe_update_events(fde_t * F, short filter, PF * handler)
{
	PF *cur_handler = NULL;

	if (filter == POLLRDNORM)
		cur_handler = F->read_handler;
	else if (filter == POLLWRNORM)
		cur_handler = F->write_handler;

	if (!cur_handler && handler)
		port_associate(pe, PORT_SOURCE_FD, F->fd, filter, F);
	else if (cur_handler && !handler)
		port_dissociate(pe, PORT_SOURCE_FD, F->fd);
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
	if((pe = port_create()) < 0) {
		libcharybdis_log("init_netio: Couldn't open port fd!\n");
		exit(115);	/* Whee! */
	}
	pemax = getdtablesize();
	pelst = MyMalloc(sizeof(port_event_t) * pemax);
	zero_timespec.tv_sec = 0;
	zero_timespec.tv_nsec = 0;
}

/*
 * ircd_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
ircd_setselect(int fd, fdlist_t list, unsigned int type, PF * handler,
	       void *client_data)
{
	fde_t *F = &fd_table[fd];
	s_assert(fd >= 0);
	s_assert(F->flags.open);

	/* Update the list, even though we're not using it .. */
	F->list = list;

	if(type & IRCD_SELECT_READ) {
		pe_update_events(F, POLLRDNORM, handler);
		F->read_handler = handler;
		F->read_data = client_data;
	}
	if(type & IRCD_SELECT_WRITE) {
		pe_update_events(F, POLLWRNORM, handler);
		F->write_handler = handler;
		F->write_data = client_data;
	}
}

/*
 * ircd_select
 *
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * ircd_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */

int
ircd_select(unsigned long delay)
{
	int	 	 i, fd;
	uint	 	 nget = 1;
	struct	timespec 	 poll_time;
	struct	timer_data	*tdata;

	poll_time.tv_sec = delay / 1000;
	poll_time.tv_nsec = (delay % 1000) * 1000000;

	i = port_getn(pe, pelst, pemax, &nget, &poll_time);
	ircd_set_time();

	if (i == -1)
		return COMM_ERROR;

	for (i = 0; i < nget; i++) {
		switch(pelst[i].portev_source) {
		case PORT_SOURCE_FD:
			fd = pelst[i].portev_object;
			PF *hdl = NULL;
			fde_t *F = &fd_table[fd];

			if ((pelst[i].portev_events & POLLRDNORM) && (hdl = F->read_handler)) {
				F->read_handler = NULL;
				hdl(fd, F->read_data);
			}
			if ((pelst[i].portev_events & POLLWRNORM) && (hdl = F->write_handler)) {
				F->write_handler = NULL;
				hdl(fd, F->write_data);
			}
			break;

		case PORT_SOURCE_TIMER:
			tdata = pelst[i].portev_user;
			tdata->td_cb(tdata->td_udata);

			if (!tdata->td_repeat)
				free(tdata);

			break;
		}
	}
	return COMM_OK;
}
