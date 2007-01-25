/*
 *  ircd-ratbox: A slightly useful ircd.
 *  kqueue.c: FreeBSD kqueue compatible network routines.
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
 *  $Id: kqueue.c 398 2005-12-12 18:12:46Z nenolod $
 */

#include "stdinc.h"
#include <sys/event.h>

#include "libcharybdis.h"

#define KE_LENGTH MAX_CLIENTS

/* jlemon goofed up and didn't add EV_SET until fbsd 4.3 */

#ifndef EV_SET
#define EV_SET(kevp, a, b, c, d, e, f) do {     \
        (kevp)->ident = (a);                    \
        (kevp)->filter = (b);                   \
        (kevp)->flags = (c);                    \
        (kevp)->fflags = (d);                   \
        (kevp)->data = (e);                     \
        (kevp)->udata = (f);                    \
} while(0)
#endif

static void kq_update_events(fde_t *, short, PF *);
static int kq;
static struct timespec zero_timespec;

static struct kevent *kqlst;	/* kevent buffer */
static int kqmax;		/* max structs to buffer */
static int kqoff;		/* offset into the buffer */


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* Private functions */

void
kq_update_events(fde_t * F, short filter, PF * handler)
{
	PF *cur_handler;
	int kep_flags;

	switch (filter)
	{
	case EVFILT_READ:
		cur_handler = F->read_handler;
		break;
	case EVFILT_WRITE:
		cur_handler = F->write_handler;
		break;
	default:
		/* XXX bad! -- adrian */
		return;
		break;
	}

	if((cur_handler == NULL && handler != NULL) || (cur_handler != NULL && handler == NULL))
	{
		struct kevent *kep;

		kep = kqlst + kqoff;

		if(handler != NULL)
		{
			if(filter == EVFILT_WRITE)
				kep_flags = (EV_ADD | EV_ENABLE | EV_ONESHOT);
			else
				kep_flags = (EV_ADD | EV_ENABLE);
		} 
		else		
		{
			/* lets definately not poll stuff that isn't real --
			 * some kqueue implementations hate doing this... and
			 * it's intended to delete AND disable at the same time.
			 *
			 * don't believe me? read kevent(4). --nenolod
			 */
			kep_flags = (EV_DELETE | EV_DISABLE);
		}

		EV_SET(kep, (uintptr_t) F->fd, filter, kep_flags, 0, 0, (void *) F);

		if(kqoff == kqmax)
		{
			int ret;

			ret = kevent(kq, kqlst, kqoff, NULL, 0, &zero_timespec);
			/* jdc -- someone needs to do error checking... */
			if(ret == -1)
			{
				libcharybdis_log("kq_update_events(): kevent(): %s", strerror(errno));
				return;
			}
			kqoff = 0;
		}
		else
		{
			kqoff++;
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
	kq = kqueue();
	if(kq < 0)
	{
		libcharybdis_log("init_netio: Couldn't open kqueue fd!\n");
		exit(115);	/* Whee! */
	}
	kqmax = getdtablesize();
	kqlst = MyMalloc(sizeof(struct kevent) * kqmax);
	zero_timespec.tv_sec = 0;
	zero_timespec.tv_nsec = 0;
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
		kq_update_events(F, EVFILT_READ, handler);
		F->read_handler = handler;
		F->read_data = client_data;
	}
	if(type & COMM_SELECT_WRITE)
	{
		kq_update_events(F, EVFILT_WRITE, handler);
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
	static struct kevent ke[KE_LENGTH];
	struct timespec poll_time;

	/*
	 * remember we are doing NANOseconds here, not micro/milli. God knows
	 * why jlemon used a timespec, but hey, he wrote the interface, not I
	 *   -- Adrian
	 */

	poll_time.tv_sec = delay / 1000;

	poll_time.tv_nsec = (delay % 1000) * 1000000;

	for (;;)
	{
		num = kevent(kq, kqlst, kqoff, ke, KE_LENGTH, &poll_time);
		kqoff = 0;

		if(num >= 0)
			break;

		if(ignoreErrno(errno))
			break;

		set_time();

		return COMM_ERROR;

		/* NOTREACHED */
	}

	set_time();

	if(num == 0)
		return COMM_OK;	/* No error.. */

	for (i = 0; i < num; i++)
	{
		int fd = (int) ke[i].ident;
		PF *hdl = NULL;
		fde_t *F = &fd_table[fd];

		if(ke[i].flags & EV_ERROR)
		{
			errno = (int) ke[i].data;
			/* XXX error == bad! -- adrian */
			continue;	/* XXX! */
		}

		switch (ke[i].filter)
		{

		case EVFILT_READ:

			if((hdl = F->read_handler) != NULL)
			{
				F->read_handler = NULL;
				hdl(fd, F->read_data);
			}

			break;

		case EVFILT_WRITE:

			if((hdl = F->write_handler) != NULL)
			{
				F->write_handler = NULL;
				hdl(fd, F->write_data);
			}
			break;

		default:
			/* Bad! -- adrian */
			break;
		}
	}
	return COMM_OK;
}

