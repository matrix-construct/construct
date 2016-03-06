/*
 *  ircd-ratbox: A slightly useful ircd.
 *  commio-int.h: A header for the network subsystem.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2007 ircd-ratbox development team
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
 *  $Id: commio.h 24059 2007-07-24 17:25:41Z androsyn $
 */

#ifndef _COMMIO_INT_H
#define _COMMIO_INT_H 1

#define RB_FD_HASH_BITS 12
#define RB_FD_HASH_SIZE (1UL << RB_FD_HASH_BITS)
#define RB_FD_HASH_MASK (RB_FD_HASH_SIZE-1)

#define FD_DESC_SZ 128		/* hostlen + comment */


#ifdef _WIN32
#define rb_get_errno() do { errno = WSAGetLastError(); WSASetLastError(errno); } while(0)
#else
#define rb_get_errno()
#endif

#define rb_hash_fd(x) ((x ^ (x >> RB_FD_HASH_BITS) ^ (x >> (RB_FD_HASH_BITS * 2))) & RB_FD_HASH_MASK)

#ifdef HAVE_WRITEV
#ifndef UIO_MAXIOV
# if defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__)
			/* FreeBSD 4.7 defines it in sys/uio.h only if _KERNEL is specified */
#  define RB_UIO_MAXIOV 1024
# elif defined(__sgi)
			/* IRIX 6.5 has sysconf(_SC_IOV_MAX) which might return 512 or bigger */
#  define RB_UIO_MAXIOV 512
# elif defined(__sun)
			/* Solaris (and SunOS?) defines IOV_MAX instead */
#  ifndef IOV_MAX
#   define RB_UIO_MAXIOV 16
#  else
#   define RB_UIO_MAXIOV IOV_MAX
#  endif

# elif defined(IOV_MAX)
#  define RB_UIO_MAXIOV IOV_MAX
# else
#  define RB_UIO_MAXIOV 16
# endif
#else
#define RB_UIO_MAXIOV UIO_MAXIOV
#endif
#else
#define RB_UIO_MAXIOV 16
#endif
struct conndata
{
	/* We don't need the host here ? */
	struct rb_sockaddr_storage S;
	struct rb_sockaddr_storage hostaddr;
	time_t t;
	CNCB *callback;
	void *data;
	/* We'd also add the retry count here when we get to that -- adrian */
};

struct acceptdata
{
	struct rb_sockaddr_storage S;
	rb_socklen_t addrlen;
	ACCB *callback;
	ACPRE *precb;
	void *data;
};

/* Only have open flags for now, could be more later */
#define FLAG_OPEN	0x1
#define IsFDOpen(F)	(F->flags & FLAG_OPEN)
#define SetFDOpen(F)	(F->flags |= FLAG_OPEN)
#define ClearFDOpen(F)	(F->flags &= ~FLAG_OPEN)


struct _fde
{
	/* New-school stuff, again pretty much ripped from squid */
	/*
	 * Yes, this gives us only one pending read and one pending write per
	 * filedescriptor. Think though: when do you think we'll need more?
	 */
	rb_dlink_node node;
	int fd;			/* So we can use the rb_fde_t as a callback ptr */
	uint8_t flags;
	uint8_t type;
	int pflags;
	char *desc;
	PF *read_handler;
	void *read_data;
	PF *write_handler;
	void *write_data;
	struct timeout_data *timeout;
	struct conndata *connect;
	struct acceptdata *accept;
	void *ssl;
	unsigned int handshake_count;
	unsigned long ssl_errno;
};

typedef void (*comm_event_cb_t) (void *);

#ifdef USE_TIMER_CREATE
typedef struct timer_data
{
	timer_t td_timer_id;
	comm_event_cb_t td_cb;
	void *td_udata;
	int td_repeat;
} *comm_event_id;
#endif

extern rb_dlink_list *rb_fd_table;

static inline rb_fde_t *
rb_find_fd(int fd)
{
	rb_dlink_list *hlist;
	rb_dlink_node *ptr;

	if(rb_unlikely(fd < 0))
		return NULL;

	hlist = &rb_fd_table[rb_hash_fd(fd)];

	if(hlist->head == NULL)
		return NULL;

	RB_DLINK_FOREACH(ptr, hlist->head)
	{
		rb_fde_t *F = ptr->data;
		if(F->fd == fd)
			return F;
	}
	return NULL;
}


int rb_setup_fd(rb_fde_t *F);
void rb_connect_callback(rb_fde_t *F, int status);


int rb_io_sched_event(struct ev_entry *ev, int when);
void rb_io_unsched_event(struct ev_entry *ev);
int rb_io_supports_event(void);
void rb_io_init_event(void);

/* epoll versions */
void rb_setselect_epoll(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_epoll(void);
int rb_select_epoll(long);
int rb_setup_fd_epoll(rb_fde_t *F);

void rb_epoll_init_event(void);
int rb_epoll_sched_event(struct ev_entry *event, int when);
void rb_epoll_unsched_event(struct ev_entry *event);
int rb_epoll_supports_event(void);


/* poll versions */
void rb_setselect_poll(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_poll(void);
int rb_select_poll(long);
int rb_setup_fd_poll(rb_fde_t *F);

/* devpoll versions */
void rb_setselect_devpoll(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_devpoll(void);
int rb_select_devpoll(long);
int rb_setup_fd_devpoll(rb_fde_t *F);

/* sigio versions */
void rb_setselect_sigio(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_sigio(void);
int rb_select_sigio(long);
int rb_setup_fd_sigio(rb_fde_t *F);

void rb_sigio_init_event(void);
int rb_sigio_sched_event(struct ev_entry *event, int when);
void rb_sigio_unsched_event(struct ev_entry *event);
int rb_sigio_supports_event(void);


/* ports versions */
void rb_setselect_ports(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_ports(void);
int rb_select_ports(long);
int rb_setup_fd_ports(rb_fde_t *F);

void rb_ports_init_event(void);
int rb_ports_sched_event(struct ev_entry *event, int when);
void rb_ports_unsched_event(struct ev_entry *event);
int rb_ports_supports_event(void);


/* kqueue versions */
void rb_setselect_kqueue(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_kqueue(void);
int rb_select_kqueue(long);
int rb_setup_fd_kqueue(rb_fde_t *F);

void rb_kqueue_init_event(void);
int rb_kqueue_sched_event(struct ev_entry *event, int when);
void rb_kqueue_unsched_event(struct ev_entry *event);
int rb_kqueue_supports_event(void);


/* select versions */
void rb_setselect_select(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_select(void);
int rb_select_select(long);
int rb_setup_fd_select(rb_fde_t *F);

/* win32 versions */
void rb_setselect_win32(rb_fde_t *F, unsigned int type, PF * handler, void *client_data);
int rb_init_netio_win32(void);
int rb_select_win32(long);
int rb_setup_fd_win32(rb_fde_t *F);
#endif

