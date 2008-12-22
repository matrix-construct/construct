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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: epoll.c 26294 2008-12-13 03:01:19Z androsyn $
 */
#define _GNU_SOURCE 1

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <event-int.h>
#if defined(HAVE_EPOLL_CTL) && (HAVE_SYS_EPOLL_H)
#define USING_EPOLL
#include <fcntl.h>
#include <sys/epoll.h>

#if defined(HAVE_SIGNALFD) && (HAVE_SYS_SIGNALFD_H) && (USE_TIMER_CREATE) && (HAVE_SYS_UIO_H)
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/uio.h>
#define EPOLL_SCHED_EVENT 1
#endif

#if defined(USE_TIMERFD_CREATE)
#include <sys/timerfd.h>
#endif

#define RTSIGNAL SIGRTMIN
struct epoll_info
{
	int ep;
	struct epoll_event *pfd;
	int pfd_size;
};

static struct epoll_info *ep_info;
static int can_do_event;
static int can_do_timerfd;

/*
 * rb_init_netio
 *
 * This is a needed exported function which will be called to initialise
 * the network loop code.
 */
int
rb_init_netio_epoll(void)
{
	can_do_event = 0;	/* shut up gcc */
	can_do_timerfd = 0;
	ep_info = rb_malloc(sizeof(struct epoll_info));
	ep_info->pfd_size = getdtablesize();
	ep_info->ep = epoll_create(ep_info->pfd_size);
	if(ep_info->ep < 0)
	{
		return -1;
	}
	rb_open(ep_info->ep, RB_FD_UNKNOWN, "epoll file descriptor");
	ep_info->pfd = rb_malloc(sizeof(struct epoll_event) * ep_info->pfd_size);

	return 0;
}

int
rb_setup_fd_epoll(rb_fde_t *F)
{
	return 0;
}


/*
 * rb_setselect
 *
 * This is a needed exported function which will be called to register
 * and deregister interest in a pending IO state for a given FD.
 */
void
rb_setselect_epoll(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	struct epoll_event ep_event;
	int old_flags = F->pflags;
	int op = -1;

	lrb_assert(IsFDOpen(F));

	/* Update the list, even though we're not using it .. */
	if(type & RB_SELECT_READ)
	{
		if(handler != NULL)
			F->pflags |= EPOLLIN;
		else
			F->pflags &= ~EPOLLIN;
		F->read_handler = handler;
		F->read_data = client_data;
	}

	if(type & RB_SELECT_WRITE)
	{
		if(handler != NULL)
			F->pflags |= EPOLLOUT;
		else
			F->pflags &= ~EPOLLOUT;
		F->write_handler = handler;
		F->write_data = client_data;
	}

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

	if(op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD)
		ep_event.events |= EPOLLET;

	if(epoll_ctl(ep_info->ep, op, F->fd, &ep_event) != 0)
	{
		rb_lib_log("rb_setselect_epoll(): epoll_ctl failed: %s", strerror(errno));
		abort();
	}


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
rb_select_epoll(long delay)
{
	int num, i, flags, old_flags, op;
	struct epoll_event ep_event;
	int o_errno;
	void *data;

	num = epoll_wait(ep_info->ep, ep_info->pfd, ep_info->pfd_size, delay);

	/* save errno as rb_set_time() will likely clobber it */
	o_errno = errno;
	rb_set_time();
	errno = o_errno;

	if(num < 0 && !rb_ignore_errno(o_errno))
		return RB_ERROR;

	if(num <= 0)
		return RB_OK;

	for(i = 0; i < num; i++)
	{
		PF *hdl;
		rb_fde_t *F = ep_info->pfd[i].data.ptr;
		old_flags = F->pflags;
		if(ep_info->pfd[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR))
		{
			hdl = F->read_handler;
			data = F->read_data;
			F->read_handler = NULL;
			F->read_data = NULL;
			if(hdl)
			{
				hdl(F, data);
			}
		}

		if(!IsFDOpen(F))
			continue;
		if(ep_info->pfd[i].events & (EPOLLOUT | EPOLLHUP | EPOLLERR))
		{
			hdl = F->write_handler;
			data = F->write_data;
			F->write_handler = NULL;
			F->write_data = NULL;

			if(hdl)
			{
				hdl(F, data);
			}
		}

		if(!IsFDOpen(F))
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
			if(op == EPOLL_CTL_MOD || op == EPOLL_CTL_ADD)
				ep_event.events |= EPOLLET;

			if(epoll_ctl(ep_info->ep, op, F->fd, &ep_event) != 0)
			{
				rb_lib_log("rb_select_epoll(): epoll_ctl failed: %s",
					   strerror(errno));
			}
		}

	}
	return RB_OK;
}

#ifdef EPOLL_SCHED_EVENT
int
rb_epoll_supports_event(void)
{
	/* try to detect at runtime if everything we need actually works */
	timer_t timer;
	struct sigevent ev;
	int fd;
	sigset_t set;

	if(can_do_event == 1)
		return 1;
	if(can_do_event == -1)
		return 0;

#ifdef USE_TIMERFD_CREATE
	if((fd = timerfd_create(CLOCK_REALTIME, 0)) >= 0)
	{
		close(fd);
		can_do_event = 1;
		can_do_timerfd = 1;
		return 1;
	}
#endif

	ev.sigev_signo = SIGVTALRM;
	ev.sigev_notify = SIGEV_SIGNAL;
	if(timer_create(CLOCK_REALTIME, &ev, &timer) != 0)
	{
		can_do_event = -1;
		return 0;
	}
	timer_delete(timer);
	sigemptyset(&set);
	fd = signalfd(-1, &set, 0);
	if(fd < 0)
	{
		can_do_event = -1;
		return 0;
	}
	close(fd);
	can_do_event = 1;
	return 1;
}


/* bleh..work around a glibc header bug on 32bit systems */
struct our_signalfd_siginfo
{
	uint32_t signo;
	int32_t err;
	int32_t code;
	uint32_t pid;
	uint32_t uid;
	int32_t fd;
	uint32_t tid;
	uint32_t band;
	uint32_t overrun;
	uint32_t trapno;
	int32_t status;
	int32_t svint;
	uint64_t svptr;
	uint64_t utime;
	uint64_t stime;
	uint64_t addr;
	uint8_t pad[48];
};


#define SIGFDIOV_COUNT 16
static void
signalfd_handler(rb_fde_t *F, void *data)
{
	static struct our_signalfd_siginfo fdsig[SIGFDIOV_COUNT];
	static struct iovec iov[SIGFDIOV_COUNT];
	struct ev_entry *ev;
	int ret, x;

	for(x = 0; x < SIGFDIOV_COUNT; x++)
	{
		iov[x].iov_base = &fdsig[x];
		iov[x].iov_len = sizeof(struct our_signalfd_siginfo);
	}

	while(1)
	{
		ret = readv(rb_get_fd(F), iov, SIGFDIOV_COUNT);

		if(ret == 0 || (ret < 0 && !rb_ignore_errno(errno)))
		{
			rb_close(F);
			rb_epoll_init_event();
			return;
		}

		if(ret < 0)
		{
			rb_setselect(F, RB_SELECT_READ, signalfd_handler, NULL);
			return;
		}
		for(x = 0; x < ret / (int)sizeof(struct our_signalfd_siginfo); x++)
		{
#if __WORDSIZE == 32 && defined(__sparc__)
			uint32_t *q = (uint32_t *)&fdsig[x].svptr;
			ev = (struct ev_entry *)q[0];
#else
			ev = (struct ev_entry *)(uintptr_t)(fdsig[x].svptr);

#endif
			if(ev == NULL)
				continue;
			rb_run_event(ev);
		}
	}
}

void
rb_epoll_init_event(void)
{

	sigset_t ss;
	rb_fde_t *F;
	int sfd;
	rb_epoll_supports_event();
	if(!can_do_timerfd)
	{
		sigemptyset(&ss);
		sigaddset(&ss, RTSIGNAL);
		sigprocmask(SIG_BLOCK, &ss, 0);
		sigemptyset(&ss);
		sigaddset(&ss, RTSIGNAL);
		sfd = signalfd(-1, &ss, 0);
		if(sfd == -1)
		{
			can_do_event = -1;
			return;
		}
		F = rb_open(sfd, RB_FD_UNKNOWN, "signalfd");
		rb_set_nb(F);
		signalfd_handler(F, NULL);
	}
}

static int
rb_epoll_sched_event_signalfd(struct ev_entry *event, int when)
{
	timer_t *id;
	struct sigevent ev;
	struct itimerspec ts;

	memset(&ev, 0, sizeof(&ev));
	event->comm_ptr = rb_malloc(sizeof(timer_t));
	id = event->comm_ptr;
	ev.sigev_notify = SIGEV_SIGNAL;
	ev.sigev_signo = RTSIGNAL;
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

#ifdef USE_TIMERFD_CREATE
static void
rb_read_timerfd(rb_fde_t *F, void *data)
{
	struct ev_entry *event = (struct ev_entry *)data;
	int retlen;
	uint64_t count;

	if(event == NULL)
	{
		rb_close(F);
		return;
	}

	retlen = rb_read(F, &count, sizeof(count));

	if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
	{
		rb_close(F);
		rb_lib_log("rb_read_timerfd: timerfd[%s] closed on error: %s", event->name,
			   strerror(errno));
		return;
	}
	rb_setselect(F, RB_SELECT_READ, rb_read_timerfd, event);
	rb_run_event(event);
}


static int
rb_epoll_sched_event_timerfd(struct ev_entry *event, int when)
{
	struct itimerspec ts;
	static char buf[FD_DESC_SZ + 8];
	int fd;
	rb_fde_t *F;

	if((fd = timerfd_create(CLOCK_REALTIME, 0)) < 0)
	{
		rb_lib_log("timerfd_create: %s\n", strerror(errno));
		return 0;
	}

	memset(&ts, 0, sizeof(ts));
	ts.it_value.tv_sec = when;
	ts.it_value.tv_nsec = 0;
	if(event->frequency != 0)
		ts.it_interval = ts.it_value;

	if(timerfd_settime(fd, 0, &ts, NULL) < 0)
	{
		rb_lib_log("timerfd_settime: %s\n", strerror(errno));
		close(fd);
		return 0;
	}
	rb_snprintf(buf, sizeof(buf), "timerfd: %s", event->name);
	F = rb_open(fd, RB_FD_UNKNOWN, buf);
	rb_set_nb(F);
	event->comm_ptr = F;
	rb_setselect(F, RB_SELECT_READ, rb_read_timerfd, event);
	return 1;
}
#endif



int
rb_epoll_sched_event(struct ev_entry *event, int when)
{
#ifdef USE_TIMERFD_CREATE
	if(can_do_timerfd)
	{
		return rb_epoll_sched_event_timerfd(event, when);
	}
#endif
	return rb_epoll_sched_event_signalfd(event, when);
}

void
rb_epoll_unsched_event(struct ev_entry *event)
{
#ifdef USE_TIMERFD_CREATE
	if(can_do_timerfd)
	{
		rb_close((rb_fde_t *)event->comm_ptr);
		event->comm_ptr = NULL;
		return;
	}
#endif
	timer_delete(*((timer_t *) event->comm_ptr));
	rb_free(event->comm_ptr);
	event->comm_ptr = NULL;
}
#endif /* EPOLL_SCHED_EVENT */

#else /* epoll not supported here */
int
rb_init_netio_epoll(void)
{
	return ENOSYS;
}

void
rb_setselect_epoll(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	errno = ENOSYS;
	return;
}

int
rb_select_epoll(long delay)
{
	errno = ENOSYS;
	return -1;
}

int
rb_setup_fd_epoll(rb_fde_t *F)
{
	errno = ENOSYS;
	return -1;
}


#endif

#if !defined(USING_EPOLL) || !defined(EPOLL_SCHED_EVENT)
void
rb_epoll_init_event(void)
{
	return;
}

int
rb_epoll_sched_event(struct ev_entry *event, int when)
{
	errno = ENOSYS;
	return -1;
}

void
rb_epoll_unsched_event(struct ev_entry *event)
{
	return;
}

int
rb_epoll_supports_event(void)
{
	errno = ENOSYS;
	return 0;
}
#endif /* !USING_EPOLL || !EPOLL_SCHED_EVENT */
