/*
 *  ircd-ratbox: A slightly useful ircd.
 *  sigio.c: Linux Realtime SIGIO compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2002 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2002 ircd-ratbox development team
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
 *  $Id: sigio.c 26092 2008-09-19 15:13:52Z androsyn $
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1		/* Needed for F_SETSIG */
#endif

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <event-int.h>
#include <fcntl.h>		/* Yes this needs to be before the ifdef */

#if defined(HAVE_SYS_POLL_H) && (HAVE_POLL) && (F_SETSIG)
#define USING_SIGIO
#endif


#ifdef USING_SIGIO

#include <signal.h>
#include <sys/poll.h>

#if defined(USE_TIMER_CREATE)
#define SIGIO_SCHED_EVENT 1
#endif

#define RTSIGIO SIGRTMIN
#define RTSIGTIM (SIGRTMIN+1)


struct _pollfd_list
{
	struct pollfd *pollfds;
	int maxindex;		/* highest FD number */
	int allocated;
};

typedef struct _pollfd_list pollfd_list_t;

pollfd_list_t pollfd_list;
static int can_do_event = 0;
static int sigio_is_screwed = 0;	/* We overflowed our sigio queue */
static sigset_t our_sigset;


/*
 * rb_init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
int
rb_init_netio_sigio(void)
{
	int fd;
	pollfd_list.pollfds = rb_malloc(rb_getmaxconnect() * (sizeof(struct pollfd)));
	pollfd_list.allocated = rb_getmaxconnect();
	for(fd = 0; fd < rb_getmaxconnect(); fd++)
	{
		pollfd_list.pollfds[fd].fd = -1;
	}

	pollfd_list.maxindex = 0;

	sigio_is_screwed = 1;	/* Start off with poll first.. */

	sigemptyset(&our_sigset);
	sigaddset(&our_sigset, RTSIGIO);
	sigaddset(&our_sigset, SIGIO);
#ifdef SIGIO_SCHED_EVENT
	sigaddset(&our_sigset, RTSIGTIM);
#endif
	sigprocmask(SIG_BLOCK, &our_sigset, NULL);
	return 0;
}

static inline void
resize_pollarray(int fd)
{
	if(rb_unlikely(fd >= pollfd_list.allocated))
	{
		int x, old_value = pollfd_list.allocated;
		pollfd_list.allocated += 1024;
		pollfd_list.pollfds =
			rb_realloc(pollfd_list.pollfds,
				   pollfd_list.allocated * (sizeof(struct pollfd)));
		memset(&pollfd_list.pollfds[old_value + 1], 0, sizeof(struct pollfd) * 1024);
		for(x = old_value + 1; x < pollfd_list.allocated; x++)
		{
			pollfd_list.pollfds[x].fd = -1;
		}
	}
}


/*
 * void setup_sigio_fd(int fd)
 * 
 * Input: File descriptor
 * Output: None
 * Side Effect: Sets the FD up for SIGIO
 */

int
rb_setup_fd_sigio(rb_fde_t *F)
{
	int flags = 0;
	int fd = F->fd;
	flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1)
		return 0;
	/* if set async, clear it so we can reset it in the kernel :/ */
	if(flags & O_ASYNC)
	{
		flags &= ~O_ASYNC;
		fcntl(fd, F_SETFL, flags);
	}

	flags |= O_ASYNC | O_NONBLOCK;

	if(fcntl(fd, F_SETFL, flags) == -1)
		return 0;

	if(fcntl(fd, F_SETSIG, RTSIGIO) == -1)
		return 0;
	if(fcntl(fd, F_SETOWN, getpid()) == -1)
		return 0;

	return 1;
}

/*
 * rb_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
rb_setselect_sigio(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	if(F == NULL)
		return;

	if(type & RB_SELECT_READ)
	{
		F->read_handler = handler;
		F->read_data = client_data;
		if(handler != NULL)
			F->pflags |= POLLRDNORM;
		else
			F->pflags &= ~POLLRDNORM;
	}
	if(type & RB_SELECT_WRITE)
	{
		F->write_handler = handler;
		F->write_data = client_data;
		if(handler != NULL)
			F->pflags |= POLLWRNORM;
		else
			F->pflags &= ~POLLWRNORM;
	}

	resize_pollarray(F->fd);

	if(F->pflags <= 0)
	{
		pollfd_list.pollfds[F->fd].events = 0;
		pollfd_list.pollfds[F->fd].fd = -1;
		if(F->fd == pollfd_list.maxindex)
		{
			while(pollfd_list.maxindex >= 0
			      && pollfd_list.pollfds[pollfd_list.maxindex].fd == -1)
				pollfd_list.maxindex--;
		}
	}
	else
	{
		pollfd_list.pollfds[F->fd].events = F->pflags;
		pollfd_list.pollfds[F->fd].fd = F->fd;
		if(F->fd > pollfd_list.maxindex)
			pollfd_list.maxindex = F->fd;
	}

}



/* int rb_select(long delay)
 * Input: The maximum time to delay.
 * Output: Returns -1 on error, 0 on success.
 * Side-effects: Deregisters future interest in IO and calls the handlers
 *               if an event occurs for an FD.
 * Comments: Check all connections for new connections and input data
 * that is to be processed. Also check for connections with data queued
 * and whether we can write it out.
 * Called to do the new-style IO, courtesy of squid (like most of this
 * new IO code). This routine handles the stuff we've hidden in
 * rb_setselect and fd_table[] and calls callbacks for IO ready
 * events.
 */
int
rb_select_sigio(long delay)
{
	int num = 0;
	int revents = 0;
	int sig;
	int fd;
	int ci;
	PF *hdl;
	rb_fde_t *F;
	void *data;
	struct siginfo si;

	struct timespec timeout;
	if(rb_sigio_supports_event() || delay >= 0)
	{
		timeout.tv_sec = (delay / 1000);
		timeout.tv_nsec = (delay % 1000) * 1000000;
	}

	for(;;)
	{
		if(!sigio_is_screwed)
		{
			if(can_do_event || delay < 0)
			{
				sig = sigwaitinfo(&our_sigset, &si);
			}
			else
				sig = sigtimedwait(&our_sigset, &si, &timeout);

			if(sig > 0)
			{

				if(sig == SIGIO)
				{
					rb_lib_log
						("Kernel RT Signal queue overflowed.  Is ulimit -i too small(or perhaps /proc/sys/kernel/rtsig-max on old kernels)");
					sigio_is_screwed = 1;
					break;
				}
#ifdef SIGIO_SCHED_EVENT
				if(sig == RTSIGTIM && can_do_event)
				{
					struct ev_entry *ev = (struct ev_entry *)si.si_ptr;
					if(ev == NULL)
						continue;
					rb_run_event(ev);
					continue;
				}
#endif
				fd = si.si_fd;
				pollfd_list.pollfds[fd].revents |= si.si_band;
				revents = pollfd_list.pollfds[fd].revents;
				num++;
				F = rb_find_fd(fd);
				if(F == NULL)
					continue;

				if(revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
				{
					hdl = F->read_handler;
					data = F->read_data;
					F->read_handler = NULL;
					F->read_data = NULL;
					if(hdl)
						hdl(F, data);
				}

				if(revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
				{
					hdl = F->write_handler;
					data = F->write_data;
					F->write_handler = NULL;
					F->write_data = NULL;
					if(hdl)
						hdl(F, data);
				}
			}
			else
				break;

		}
		else
			break;
	}

	if(!sigio_is_screwed)	/* We don't need to proceed */
	{
		rb_set_time();
		return 0;
	}

	signal(RTSIGIO, SIG_IGN);
	signal(RTSIGIO, SIG_DFL);
	sigio_is_screwed = 0;


	num = poll(pollfd_list.pollfds, pollfd_list.maxindex + 1, delay);
	rb_set_time();
	if(num < 0)
	{
		if(!rb_ignore_errno(errno))
			return RB_OK;
		else
			return RB_ERROR;
	}
	if(num == 0)
		return RB_OK;

	/* XXX we *could* optimise by falling out after doing num fds ... */
	for(ci = 0; ci < pollfd_list.maxindex + 1; ci++)
	{
		if(((revents = pollfd_list.pollfds[ci].revents) == 0)
		   || (pollfd_list.pollfds[ci].fd) == -1)
			continue;
		fd = pollfd_list.pollfds[ci].fd;
		F = rb_find_fd(fd);
		if(F == NULL)
			continue;
		if(revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
		{
			hdl = F->read_handler;
			data = F->read_data;
			F->read_handler = NULL;
			F->read_data = NULL;
			if(hdl)
				hdl(F, data);
		}

		if(IsFDOpen(F) && (revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR)))
		{
			hdl = F->write_handler;
			data = F->write_data;
			F->write_handler = NULL;
			F->write_data = NULL;
			if(hdl)
				hdl(F, data);
		}
		if(F->read_handler == NULL)
			rb_setselect_sigio(F, RB_SELECT_READ, NULL, NULL);
		if(F->write_handler == NULL)
			rb_setselect_sigio(F, RB_SELECT_WRITE, NULL, NULL);

	}

	return 0;
}

#if defined(SIGIO_SCHED_EVENT)
void
rb_sigio_init_event(void)
{
	rb_sigio_supports_event();
}


int
rb_sigio_supports_event(void)
{
	timer_t timer;
	struct sigevent ev;
	if(can_do_event == 1)
		return 1;
	if(can_do_event == -1)
		return 0;

	ev.sigev_signo = SIGVTALRM;
	ev.sigev_notify = SIGEV_SIGNAL;
	if(timer_create(CLOCK_REALTIME, &ev, &timer) != 0)
	{
		can_do_event = -1;
		return 0;
	}
	timer_delete(timer);
	can_do_event = 1;
	return 1;
}

int
rb_sigio_sched_event(struct ev_entry *event, int when)
{
	timer_t *id;
	struct sigevent ev;
	struct itimerspec ts;
	if(can_do_event <= 0)
		return 0;

	memset(&ev, 0, sizeof(&ev));
	event->comm_ptr = rb_malloc(sizeof(timer_t));
	id = event->comm_ptr;
	ev.sigev_notify = SIGEV_SIGNAL;
	ev.sigev_signo = RTSIGTIM;
	ev.sigev_value.sival_ptr = event;

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
rb_sigio_unsched_event(struct ev_entry *event)
{
	if(can_do_event <= 0)
		return;
	timer_delete(*((timer_t *) event->comm_ptr));
	rb_free(event->comm_ptr);
	event->comm_ptr = NULL;
}
#endif /* SIGIO_SCHED_EVENT */

#else

int
rb_init_netio_sigio(void)
{
	return ENOSYS;
}

void
rb_setselect_sigio(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	errno = ENOSYS;
	return;
}

int
rb_select_sigio(long delay)
{
	errno = ENOSYS;
	return -1;
}

int
rb_setup_fd_sigio(rb_fde_t *F)
{
	errno = ENOSYS;
	return -1;
}

#endif

#if !defined(USING_SIGIO) || !defined(SIGIO_SCHED_EVENT)
void
rb_sigio_init_event(void)
{
	return;
}

int
rb_sigio_sched_event(struct ev_entry *event, int when)
{
	errno = ENOSYS;
	return -1;
}

void
rb_sigio_unsched_event(struct ev_entry *event)
{
	return;
}

int
rb_sigio_supports_event(void)
{
	errno = ENOSYS;
	return 0;
}
#endif /* !USING_SIGIO || !SIGIO_SCHED_EVENT */
