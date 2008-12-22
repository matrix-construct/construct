/*
 *  ircd-ratbox: A slightly useful ircd.
 *  ports.c: Solaris ports compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002-2004,2008 ircd-ratbox development team
 *  Copyright (C) 2005 Edward Brocklesby.
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
 *  $Id: ports.c 26286 2008-12-10 23:28:53Z androsyn $
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <event-int.h>
#if defined(HAVE_PORT_H) && (HAVE_PORT_CREATE)

#include <port.h>

#define PE_LENGTH	128

static int pe;
static struct timespec zero_timespec;

static port_event_t *pelst;	/* port buffer */
static int pemax;		/* max structs to buffer */

int
rb_setup_fd_ports(rb_fde_t *F)
{
	return 0;
}

/*
 * rb_init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */

int
rb_init_netio_ports(void)
{
	if((pe = port_create()) < 0)
	{
		return errno;
	}
	pemax = getdtablesize();
	pelst = rb_malloc(sizeof(port_event_t) * pemax);
	zero_timespec.tv_sec = 0;
	zero_timespec.tv_nsec = 0;
	rb_set_time();
	return 0;
}

/*
 * rb_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
rb_setselect_ports(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	lrb_assert(IsFDOpen(F));
	int old_flags = F->pflags;

	if(type & RB_SELECT_READ)
	{
		F->read_handler = handler;
		F->read_data = client_data;
	}
	if(type & RB_SELECT_WRITE)
	{
		F->write_handler = handler;
		F->write_data = client_data;
	}
	F->pflags = 0;

	if(F->read_handler != NULL)
		F->pflags = POLLIN;
	if(F->write_handler != NULL)
		F->pflags |= POLLOUT;

	if(old_flags == 0 && F->pflags == 0)
		return;
	else if(F->pflags <= 0)
	{
		port_dissociate(pe, PORT_SOURCE_FD, F->fd);
		return;
	}
	
	port_associate(pe, PORT_SOURCE_FD, F->fd, F->pflags, F);
	
}

/*
 * rb_select
 *
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * rb_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */

int
rb_select_ports(long delay)
{
	int i, fd;
	int nget = 1;
	struct timespec poll_time;
	struct timespec *p = NULL;
	struct ev_entry *ev;

	if(delay >= 0)
	{
		poll_time.tv_sec = delay / 1000;
		poll_time.tv_nsec = (delay % 1000) * 1000000;
		p = &poll_time;
	}


	i = port_getn(pe, pelst, pemax, &nget, p);
	rb_set_time();

	if(i == -1)
		return RB_OK;

	for(i = 0; i < nget; i++)
	{
		if(pelst[i].portev_source == PORT_SOURCE_FD)
		{
			fd = pelst[i].portev_object;
			PF *hdl = NULL;
			rb_fde_t *F = pelst[i].portev_user;
			if((pelst[i].portev_events & (POLLIN | POLLHUP | POLLERR)) && (hdl = F->read_handler))
			{
				F->read_handler = NULL;
				hdl(F, F->read_data);
			}
			if((pelst[i].portev_events & (POLLOUT | POLLHUP | POLLERR)) && (hdl = F->write_handler))
			{
				F->write_handler = NULL;
				hdl(F, F->write_data);
			}
		} else if(pelst[i].portev_source == PORT_SOURCE_TIMER)
		{
			ev = (struct ev_entry *)pelst[i].portev_user;
			rb_run_event(ev);
		}
	}
	return RB_OK;
}

int
rb_ports_supports_event(void)
{
	return 1;
};

void
rb_ports_init_event(void)
{
	return;
}

int
rb_ports_sched_event(struct ev_entry *event, int when)
{
	timer_t *id;
	struct sigevent ev;
	port_notify_t not;
	struct itimerspec ts;

	event->comm_ptr = rb_malloc(sizeof(timer_t));
	id = event->comm_ptr;
				
	memset(&ev, 0, sizeof(ev));
	ev.sigev_notify = SIGEV_PORT;
	ev.sigev_value.sival_ptr = &not;
	
	memset(&not, 0, sizeof(not));	
	not.portnfy_port = pe;
	not.portnfy_user = event;
	
	if(timer_create(CLOCK_REALTIME, &ev, id) < 0)
	{
		rb_lib_log("timer_create: %s\n", strerror(errno));
		return 0;
	}
	
	memset(&ts, 0, sizeof(ts));
	ts.it_value.tv_sec = when;
	ts.it_value.tv_nsec = 0; 
	if(event->frequency != 0)
		ts.it_interval = ts.it_value;

	if(timer_settime(*id, 0, &ts, NULL) < 0)
	{
		rb_lib_log("timer_settime: %s\n", strerror(errno));
		return 0;
	}
	return 1;					
}

void
rb_ports_unsched_event(struct ev_entry *event)
{
	timer_delete(*((timer_t *) event->comm_ptr));
	rb_free(event->comm_ptr);
	event->comm_ptr = NULL;
}
#else /* ports not supported */

int
rb_ports_supports_event(void)
{
	errno = ENOSYS;
	return 0;
}

void
rb_ports_init_event(void)
{
	return;
}
 
int
rb_ports_sched_event(struct ev_entry *event, int when)
{
	errno = ENOSYS;
	return -1;
}
 
void
rb_ports_unsched_event(struct ev_entry *event)
{
	return;
}

int
rb_init_netio_ports(void)
{
	return ENOSYS;
}

void
rb_setselect_ports(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	errno = ENOSYS;
	return;
}

int
rb_select_ports(long delay)
{
	errno = ENOSYS;
	return -1;
}

int
rb_setup_fd_ports(rb_fde_t *F)
{
	errno = ENOSYS;
	return -1;
}


#endif
