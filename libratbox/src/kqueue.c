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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: kqueue.c 26092 2008-09-19 15:13:52Z androsyn $
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <event-int.h>

#if defined(HAVE_SYS_EVENT_H) && (HAVE_KEVENT)

#include <sys/event.h>

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

#ifdef EVFILT_TIMER
#define KQUEUE_SCHED_EVENT
#endif


static void kq_update_events(rb_fde_t *, short, PF *);
static int kq;
static struct timespec zero_timespec;

static struct kevent *kqlst;	/* kevent buffer */
static struct kevent *kqout;	/* kevent output buffer */
static int kqmax;		/* max structs to buffer */
static int kqoff;		/* offset into the buffer */


int
rb_setup_fd_kqueue(rb_fde_t *F)
{
	return 0;
}

static void
kq_update_events(rb_fde_t *F, short filter, PF * handler)
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
			kep_flags = EV_ADD | EV_ONESHOT;
		}
		else
		{
			kep_flags = EV_DELETE;
		}

		EV_SET(kep, (uintptr_t)F->fd, filter, kep_flags, 0, 0, (void *)F);

		if(++kqoff == kqmax)
		{
			int ret, i;

			/* Add them one at a time, because there may be
			 * already closed fds in it. The kernel will try
			 * to report invalid fds in the output; if there
			 * is no space, it silently stops processing the
			 * array at that point. We cannot give output space
			 * because that would also return events we cannot
			 * process at this point.
			 */
			for(i = 0; i < kqoff; i++)
			{
				ret = kevent(kq, kqlst + i, 1, NULL, 0, &zero_timespec);
				/* jdc -- someone needs to do error checking... */
				/* EBADF is normal here -- jilles */
				if(ret == -1 && errno != EBADF)
					rb_lib_log("kq_update_events(): kevent(): %s",
						   strerror(errno));
			}
			kqoff = 0;
		}
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
rb_init_netio_kqueue(void)
{
	kq = kqueue();
	if(kq < 0)
	{
		return errno;
	}
	kqmax = getdtablesize();
	kqlst = rb_malloc(sizeof(struct kevent) * kqmax);
	kqout = rb_malloc(sizeof(struct kevent) * kqmax);
	rb_open(kq, RB_FD_UNKNOWN, "kqueue fd");
	zero_timespec.tv_sec = 0;
	zero_timespec.tv_nsec = 0;

	return 0;
}

/*
 * rb_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
rb_setselect_kqueue(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	lrb_assert(IsFDOpen(F));

	if(type & RB_SELECT_READ)
	{
		kq_update_events(F, EVFILT_READ, handler);
		F->read_handler = handler;
		F->read_data = client_data;
	}
	if(type & RB_SELECT_WRITE)
	{
		kq_update_events(F, EVFILT_WRITE, handler);
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
rb_select_kqueue(long delay)
{
	int num, i;
	struct timespec poll_time;
	struct timespec *pt;
	rb_fde_t *F;


	if(delay < 0)
	{
		pt = NULL;
	}
	else
	{
		pt = &poll_time;
		poll_time.tv_sec = delay / 1000;
		poll_time.tv_nsec = (delay % 1000) * 1000000;
	}

	for(;;)
	{
		num = kevent(kq, kqlst, kqoff, kqout, kqmax, pt);
		kqoff = 0;

		if(num >= 0)
			break;

		if(rb_ignore_errno(errno))
			break;

		rb_set_time();

		return RB_ERROR;

		/* NOTREACHED */
	}

	rb_set_time();

	if(num == 0)
		return RB_OK;	/* No error.. */

	for(i = 0; i < num; i++)
	{
		PF *hdl = NULL;

		if(kqout[i].flags & EV_ERROR)
		{
			errno = kqout[i].data;
			/* XXX error == bad! -- adrian */
			continue;	/* XXX! */
		}

		switch (kqout[i].filter)
		{

		case EVFILT_READ:
			F = kqout[i].udata;
			if((hdl = F->read_handler) != NULL)
			{
				F->read_handler = NULL;
				hdl(F, F->read_data);
			}

			break;

		case EVFILT_WRITE:
			F = kqout[i].udata;
			if((hdl = F->write_handler) != NULL)
			{
				F->write_handler = NULL;
				hdl(F, F->write_data);
			}
			break;
#if defined(EVFILT_TIMER)
		case EVFILT_TIMER:
			rb_run_event(kqout[i].udata);
			break;
#endif
		default:
			/* Bad! -- adrian */
			break;
		}
	}
	return RB_OK;
}

#if defined(KQUEUE_SCHED_EVENT)
static int can_do_event = 0;
int
rb_kqueue_supports_event(void)
{
	struct kevent kv;
	struct timespec ts;
	int xkq;

	if(can_do_event == 1)
		return 1;
	if(can_do_event == -1)
		return 0;

	xkq = kqueue();
	ts.tv_sec = 0;
	ts.tv_nsec = 1000;


	EV_SET(&kv, (uintptr_t)0x0, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, 1, 0);
	if(kevent(xkq, &kv, 1, NULL, 0, NULL) < 0)
	{
		can_do_event = -1;
		close(xkq);
		return 0;
	}
	close(xkq);
	can_do_event = 1;
	return 1;
}

int
rb_kqueue_sched_event(struct ev_entry *event, int when)
{
	struct kevent kev;
	int kep_flags;

	kep_flags = EV_ADD;
	if(event->frequency == 0)
		kep_flags |= EV_ONESHOT;
	EV_SET(&kev, (uintptr_t)event, EVFILT_TIMER, kep_flags, 0, when * 1000, event);
	if(kevent(kq, &kev, 1, NULL, 0, NULL) < 0)
		return 0;
	return 1;
}

void
rb_kqueue_unsched_event(struct ev_entry *event)
{
	struct kevent kev;
	EV_SET(&kev, (uintptr_t)event, EVFILT_TIMER, EV_DELETE, 0, 0, event);
	kevent(kq, &kev, 1, NULL, 0, NULL);
}

void
rb_kqueue_init_event(void)
{
	return;
}
#endif /* KQUEUE_SCHED_EVENT */

#else /* kqueue not supported */
int
rb_init_netio_kqueue(void)
{
	errno = ENOSYS;
	return -1;
}

void
rb_setselect_kqueue(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	errno = ENOSYS;
	return;
}

int
rb_select_kqueue(long delay)
{
	errno = ENOSYS;
	return -1;
}

int
rb_setup_fd_kqueue(rb_fde_t *F)
{
	errno = ENOSYS;
	return -1;
}

#endif

#if !defined(HAVE_KEVENT) || !defined(KQUEUE_SCHED_EVENT)
void
rb_kqueue_init_event(void)
{
	return;
}

int
rb_kqueue_sched_event(struct ev_entry *event, int when)
{
	errno = ENOSYS;
	return -1;
}

void
rb_kqueue_unsched_event(struct ev_entry *event)
{
	return;
}

int
rb_kqueue_supports_event(void)
{
	errno = ENOSYS;
	return 0;
}
#endif /* !HAVE_KEVENT || !KQUEUE_SCHED_EVENT */
