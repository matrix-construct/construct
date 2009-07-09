/*
 *  ircd-ratbox: A slightly useful ircd.
 *  commio.c: Network/file related functions
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
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
 *  $Id: commio.c 26254 2008-12-10 04:04:38Z androsyn $
 */
#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <commio-ssl.h>
#include <event-int.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#define HAVE_SSL 1

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif


struct timeout_data
{
	rb_fde_t *F;
	rb_dlink_node node;
	time_t timeout;
	PF *timeout_handler;
	void *timeout_data;
};

rb_dlink_list *rb_fd_table;
static rb_bh *fd_heap;

static rb_dlink_list timeout_list;
static rb_dlink_list closed_list;

static struct ev_entry *rb_timeout_ev;


static const char *rb_err_str[] = { "Comm OK", "Error during bind()",
	"Error during DNS lookup", "connect timeout",
	"Error during connect()",
	"Comm Error"
};

/* Highest FD and number of open FDs .. */
static int number_fd = 0;
int rb_maxconnections = 0;

static PF rb_connect_timeout;
static PF rb_connect_tryconnect;
#ifdef RB_IPV6
static void mangle_mapped_sockaddr(struct sockaddr *in);
#endif

#ifndef HAVE_SOCKETPAIR
static int rb_inet_socketpair(int d, int type, int protocol, int sv[2]);
static int rb_inet_socketpair_udp(rb_fde_t **newF1, rb_fde_t **newF2);
#endif

static inline rb_fde_t *
add_fd(int fd)
{
	rb_fde_t *F = rb_find_fd(fd);

	/* look up to see if we have it already */
	if(F != NULL)
		return F;

	F = rb_bh_alloc(fd_heap);
	F->fd = fd;
	rb_dlinkAdd(F, &F->node, &rb_fd_table[rb_hash_fd(fd)]);
	return (F);
}

static inline void
remove_fd(rb_fde_t *F)
{
	if(F == NULL || !IsFDOpen(F))
		return;

	rb_dlinkMoveNode(&F->node, &rb_fd_table[rb_hash_fd(F->fd)], &closed_list);
}

static void
free_fds(void)
{
	rb_fde_t *F;
	rb_dlink_node *ptr, *next;
	RB_DLINK_FOREACH_SAFE(ptr, next, closed_list.head)
	{
		F = ptr->data;
		rb_dlinkDelete(ptr, &closed_list);
		rb_bh_free(fd_heap, F);
	}
}

/* 32bit solaris is kinda slow and stdio only supports fds < 256
 * so we got to do this crap below.
 * (BTW Fuck you Sun, I hate your guts and I hope you go bankrupt soon)
 */

#if defined (__SVR4) && defined (__sun)
static void
rb_fd_hack(int *fd)
{
	int newfd;
	if(*fd > 256 || *fd < 0)
		return;
	if((newfd = fcntl(*fd, F_DUPFD, 256)) != -1)
	{
		close(*fd);
		*fd = newfd;
	}
	return;
}
#else
#define rb_fd_hack(fd)
#endif


/* close_all_connections() can be used *before* the system come up! */

static void
rb_close_all(void)
{
#ifndef _WIN32
	int i;

	/* XXX someone tell me why we care about 4 fd's ? */
	/* XXX btw, fd 3 is used for profiler ! */
	for(i = 3; i < rb_maxconnections; ++i)
	{
		close(i);
	}
#endif
}

/*
 * get_sockerr - get the error value from the socket or the current errno
 *
 * Get the *real* error from the socket (well try to anyway..).
 * This may only work when SO_DEBUG is enabled but its worth the
 * gamble anyway.
 */
int
rb_get_sockerr(rb_fde_t *F)
{
	int errtmp;
	int err = 0;
	rb_socklen_t len = sizeof(err);

	if(!(F->type & RB_FD_SOCKET))
		return errno;

	rb_get_errno();
	errtmp = errno;

#ifdef SO_ERROR
	if(F != NULL
	   && !getsockopt(rb_get_fd(F), SOL_SOCKET, SO_ERROR, (char *)&err, (rb_socklen_t *) & len))
	{
		if(err)
			errtmp = err;
	}
	errno = errtmp;
#endif
	return errtmp;
}

/*
 * rb_getmaxconnect - return the max number of connections allowed
 */
int
rb_getmaxconnect(void)
{
	return (rb_maxconnections);
}

/*
 * set_sock_buffers - set send and receive buffers for socket
 * 
 * inputs	- fd file descriptor
 * 		- size to set
 * output       - returns true (1) if successful, false (0) otherwise
 * side effects -
 */
int
rb_set_buffers(rb_fde_t *F, int size)
{
	if(F == NULL)
		return 0;
	if(setsockopt
	   (F->fd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size))
	   || setsockopt(F->fd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size)))
		return 0;
	return 1;
}

/*
 * set_non_blocking - Set the client connection into non-blocking mode. 
 *
 * inputs	- fd to set into non blocking mode
 * output	- 1 if successful 0 if not
 * side effects - use POSIX compliant non blocking and
 *                be done with it.
 */
int
rb_set_nb(rb_fde_t *F)
{
	int nonb = 0;
	int res;
	int fd;
	if(F == NULL)
		return 0;
	fd = F->fd;

	if((res = rb_setup_fd(F)))
		return res;
#ifdef O_NONBLOCK
	nonb |= O_NONBLOCK;
	res = fcntl(fd, F_GETFL, 0);
	if(-1 == res || fcntl(fd, F_SETFL, res | nonb) == -1)
		return 0;
#else
	nonb = 1;
	res = 0;
	if(ioctl(fd, FIONBIO, (char *)&nonb) == -1)
		return 0;
#endif

	return 1;
}

/*
 * rb_settimeout() - set the socket timeout
 *
 * Set the timeout for the fd
 */
void
rb_settimeout(rb_fde_t *F, time_t timeout, PF * callback, void *cbdata)
{
	struct timeout_data *td;

	if(F == NULL)
		return;

	lrb_assert(IsFDOpen(F));
	td = F->timeout;
	if(callback == NULL)	/* user wants to remove */
	{
		if(td == NULL)
			return;
		rb_dlinkDelete(&td->node, &timeout_list);
		rb_free(td);
		F->timeout = NULL;
		if(rb_dlink_list_length(&timeout_list) == 0)
		{
			rb_event_delete(rb_timeout_ev);
			rb_timeout_ev = NULL;
		}
		return;
	}

	if(F->timeout == NULL)
		td = F->timeout = rb_malloc(sizeof(struct timeout_data));

	td->F = F;
	td->timeout = rb_current_time() + timeout;
	td->timeout_handler = callback;
	td->timeout_data = cbdata;
	rb_dlinkAdd(td, &td->node, &timeout_list);
	if(rb_timeout_ev == NULL)
	{
		rb_timeout_ev = rb_event_add("rb_checktimeouts", rb_checktimeouts, NULL, 5);
	}
}

/*
 * rb_checktimeouts() - check the socket timeouts
 *
 * All this routine does is call the given callback/cbdata, without closing
 * down the file descriptor. When close handlers have been implemented,
 * this will happen.
 */
void
rb_checktimeouts(void *notused)
{
	rb_dlink_node *ptr, *next;
	struct timeout_data *td;
	rb_fde_t *F;
	PF *hdl;
	void *data;

	RB_DLINK_FOREACH_SAFE(ptr, next, timeout_list.head)
	{
		td = ptr->data;
		F = td->F;
		if(F == NULL || !IsFDOpen(F))
			continue;

		if(td->timeout < rb_current_time())
		{
			hdl = td->timeout_handler;
			data = td->timeout_data;
			rb_dlinkDelete(&td->node, &timeout_list);
			F->timeout = NULL;
			rb_free(td);
			hdl(F, data);
		}
	}
}

static void
rb_accept_tryaccept(rb_fde_t *F, void *data)
{
	struct rb_sockaddr_storage st;
	rb_fde_t *new_F;
	rb_socklen_t addrlen = sizeof(st);
	int new_fd;

	while(1)
	{
		new_fd = accept(F->fd, (struct sockaddr *)&st, &addrlen);
		rb_get_errno();
		if(new_fd < 0)
		{
			rb_setselect(F, RB_SELECT_ACCEPT, rb_accept_tryaccept, NULL);
			return;
		}

		rb_fd_hack(&new_fd);

		new_F = rb_open(new_fd, RB_FD_SOCKET, "Incoming Connection");

		if(new_F == NULL)
		{
			rb_lib_log
				("rb_accept: new_F == NULL on incoming connection. Closing new_fd == %d\n",
				 new_fd);
			close(new_fd);
			continue;
		}

		if(rb_unlikely(!rb_set_nb(new_F)))
		{
			rb_get_errno();
			rb_lib_log("rb_accept: Couldn't set FD %d non blocking!", new_F->fd);
			rb_close(new_F);
		}

#ifdef RB_IPV6
		mangle_mapped_sockaddr((struct sockaddr *)&st);
#endif

		if(F->accept->precb != NULL)
		{
			if(!F->accept->precb(new_F, (struct sockaddr *)&st, addrlen, F->accept->data))	/* pre-callback decided to drop it */
				continue;
		}
#ifdef HAVE_SSL
		if(F->type & RB_FD_SSL)
		{
			rb_ssl_accept_setup(F, new_F, (struct sockaddr *)&st, addrlen);
		}
		else
#endif /* HAVE_SSL */
		{
			F->accept->callback(new_F, RB_OK, (struct sockaddr *)&st, addrlen,
					    F->accept->data);
		}
	}

}

/* try to accept a TCP connection */
void
rb_accept_tcp(rb_fde_t *F, ACPRE * precb, ACCB * callback, void *data)
{
	if(F == NULL)
		return;
	lrb_assert(callback);

	F->accept = rb_malloc(sizeof(struct acceptdata));
	F->accept->callback = callback;
	F->accept->data = data;
	F->accept->precb = precb;
	rb_accept_tryaccept(F, NULL);
}

/*
 * void rb_connect_tcp(int fd, struct sockaddr *dest,
 *                       struct sockaddr *clocal, int socklen,
 *                       CNCB *callback, void *data, int timeout)
 * Input: An fd to connect with, a host and port to connect to,
 *        a local sockaddr to connect from + length(or NULL to use the
 *        default), a callback, the data to pass into the callback, the
 *        address family.
 * Output: None.
 * Side-effects: A non-blocking connection to the host is started, and
 *               if necessary, set up for selection. The callback given
 *               may be called now, or it may be called later.
 */
void
rb_connect_tcp(rb_fde_t *F, struct sockaddr *dest,
	       struct sockaddr *clocal, int socklen, CNCB * callback, void *data, int timeout)
{
	if(F == NULL)
		return;

	lrb_assert(callback);
	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;

	memcpy(&F->connect->hostaddr, dest, sizeof(F->connect->hostaddr));

	/* Note that we're using a passed sockaddr here. This is because
	 * generally you'll be bind()ing to a sockaddr grabbed from
	 * getsockname(), so this makes things easier.
	 * XXX If NULL is passed as local, we should later on bind() to the
	 * virtual host IP, for completeness.
	 *   -- adrian
	 */
	if((clocal != NULL) && (bind(F->fd, clocal, socklen) < 0))
	{
		/* Failure, call the callback with RB_ERR_BIND */
		rb_connect_callback(F, RB_ERR_BIND);
		/* ... and quit */
		return;
	}

	/* We have a valid IP, so we just call tryconnect */
	/* Make sure we actually set the timeout here .. */
	rb_settimeout(F, timeout, rb_connect_timeout, NULL);
	rb_connect_tryconnect(F, NULL);
}


/*
 * rb_connect_callback() - call the callback, and continue with life
 */
void
rb_connect_callback(rb_fde_t *F, int status)
{
	CNCB *hdl;
	void *data;
	int errtmp = errno;	/* save errno as rb_settimeout clobbers it sometimes */

	/* This check is gross..but probably necessary */
	if(F == NULL || F->connect == NULL || F->connect->callback == NULL)
		return;
	/* Clear the connect flag + handler */
	hdl = F->connect->callback;
	data = F->connect->data;
	F->connect->callback = NULL;


	/* Clear the timeout handler */
	rb_settimeout(F, 0, NULL, NULL);
	errno = errtmp;
	/* Call the handler */
	hdl(F, status, data);
}


/*
 * rb_connect_timeout() - this gets called when the socket connection
 * times out. This *only* can be called once connect() is initially
 * called ..
 */
static void
rb_connect_timeout(rb_fde_t *F, void *notused)
{
	/* error! */
	rb_connect_callback(F, RB_ERR_TIMEOUT);
}

/* static void rb_connect_tryconnect(int fd, void *notused)
 * Input: The fd, the handler data(unused).
 * Output: None.
 * Side-effects: Try and connect with pending connect data for the FD. If
 *               we succeed or get a fatal error, call the callback.
 *               Otherwise, it is still blocking or something, so register
 *               to select for a write event on this FD.
 */
static void
rb_connect_tryconnect(rb_fde_t *F, void *notused)
{
	int retval;

	if(F == NULL || F->connect == NULL || F->connect->callback == NULL)
		return;
	/* Try the connect() */
	retval = connect(F->fd,
			 (struct sockaddr *)&F->connect->hostaddr,
			 GET_SS_LEN(&F->connect->hostaddr));
	/* Error? */
	if(retval < 0)
	{
		/*
		 * If we get EISCONN, then we've already connect()ed the socket,
		 * which is a good thing.
		 *   -- adrian
		 */
		rb_get_errno();
		if(errno == EISCONN)
			rb_connect_callback(F, RB_OK);
		else if(rb_ignore_errno(errno))
			/* Ignore error? Reschedule */
			rb_setselect(F, RB_SELECT_CONNECT, rb_connect_tryconnect, NULL);
		else
			/* Error? Fail with RB_ERR_CONNECT */
			rb_connect_callback(F, RB_ERR_CONNECT);
		return;
	}
	/* If we get here, we've suceeded, so call with RB_OK */
	rb_connect_callback(F, RB_OK);
}


int
rb_connect_sockaddr(rb_fde_t *F, struct sockaddr *addr, int len)
{
	if(F == NULL)
		return 0;

	memcpy(addr, &F->connect->hostaddr, len);
	return 1;
}

/*
 * rb_error_str() - return an error string for the given error condition
 */
const char *
rb_errstr(int error)
{
	if(error < 0 || error >= RB_ERR_MAX)
		return "Invalid error number!";
	return rb_err_str[error];
}


int
rb_socketpair(int family, int sock_type, int proto, rb_fde_t **F1, rb_fde_t **F2, const char *note)
{
	int nfd[2];
	if(number_fd >= rb_maxconnections)
	{
		errno = ENFILE;
		return -1;
	}

#ifdef HAVE_SOCKETPAIR
	if(socketpair(family, sock_type, proto, nfd))
#else
	if(sock_type == SOCK_DGRAM)
	{
		return rb_inet_socketpair_udp(F1, F2);
	}

	if(rb_inet_socketpair(AF_INET, sock_type, proto, nfd))
#endif
		return -1;

	rb_fd_hack(&nfd[0]);
	rb_fd_hack(&nfd[1]);

	*F1 = rb_open(nfd[0], RB_FD_SOCKET, note);
	*F2 = rb_open(nfd[1], RB_FD_SOCKET, note);

	if(*F1 == NULL)
	{
		if(*F2 != NULL)
			rb_close(*F2);
		return -1;
	}

	if(*F2 == NULL)
	{
		rb_close(*F1);
		return -1;
	}

	/* Set the socket non-blocking, and other wonderful bits */
	if(rb_unlikely(!rb_set_nb(*F1)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", nfd[0], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}

	if(rb_unlikely(!rb_set_nb(*F2)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", nfd[1], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}

	return 0;
}


int
rb_pipe(rb_fde_t **F1, rb_fde_t **F2, const char *desc)
{
#ifndef _WIN32
	int fd[2];
	if(number_fd >= rb_maxconnections)
	{
		errno = ENFILE;
		return -1;
	}
	if(pipe(fd) == -1)
		return -1;
	rb_fd_hack(&fd[0]);
	rb_fd_hack(&fd[1]);
	*F1 = rb_open(fd[0], RB_FD_PIPE, desc);
	*F2 = rb_open(fd[1], RB_FD_PIPE, desc);

	if(rb_unlikely(!rb_set_nb(*F1)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", fd[0], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}

	if(rb_unlikely(!rb_set_nb(*F2)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", fd[1], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}


	return 0;
#else
	/* Its not a pipe..but its selectable.  I'll take dirty hacks
	 * for $500 Alex.
	 */
	return rb_socketpair(AF_INET, SOCK_STREAM, 0, F1, F2, desc);
#endif
}

/*
 * rb_socket() - open a socket
 *
 * This is a highly highly cut down version of squid's rb_open() which
 * for the most part emulates socket(), *EXCEPT* it fails if we're about
 * to run out of file descriptors.
 */
rb_fde_t *
rb_socket(int family, int sock_type, int proto, const char *note)
{
	rb_fde_t *F;
	int fd;
	/* First, make sure we aren't going to run out of file descriptors */
	if(rb_unlikely(number_fd >= rb_maxconnections))
	{
		errno = ENFILE;
		return NULL;
	}

	/*
	 * Next, we try to open the socket. We *should* drop the reserved FD
	 * limit if/when we get an error, but we can deal with that later.
	 * XXX !!! -- adrian
	 */
	fd = socket(family, sock_type, proto);
	rb_fd_hack(&fd);
	if(rb_unlikely(fd < 0))
		return NULL;	/* errno will be passed through, yay.. */

#if defined(RB_IPV6) && defined(IPV6_V6ONLY)
	/* 
	 * Make sure we can take both IPv4 and IPv6 connections
	 * on an AF_INET6 socket
	 */
	if(family == AF_INET6)
	{
		int off = 1;
		if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) == -1)
		{
			rb_lib_log("rb_socket: Could not set IPV6_V6ONLY option to 1 on FD %d: %s",
				   fd, strerror(errno));
			close(fd);
			return NULL;
		}
	}
#endif

	F = rb_open(fd, RB_FD_SOCKET, note);
	if(F == NULL)
	{
		rb_lib_log("rb_socket: rb_open returns NULL on FD %d: %s, closing fd", fd,
			   strerror(errno));
		close(fd);
		return NULL;
	}
	/* Set the socket non-blocking, and other wonderful bits */
	if(rb_unlikely(!rb_set_nb(F)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", fd, strerror(errno));
		rb_close(F);
		return NULL;
	}

	return F;
}

/*
 * If a sockaddr_storage is AF_INET6 but is a mapped IPv4
 * socket manged the sockaddr.
 */
#ifdef RB_IPV6
static void
mangle_mapped_sockaddr(struct sockaddr *in)
{
	struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)in;

	if(in->sa_family == AF_INET)
		return;

	if(in->sa_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr))
	{
		struct sockaddr_in in4;
		memset(&in4, 0, sizeof(struct sockaddr_in));
		in4.sin_family = AF_INET;
		in4.sin_port = in6->sin6_port;
		in4.sin_addr.s_addr = ((uint32_t *)&in6->sin6_addr)[3];
		memcpy(in, &in4, sizeof(struct sockaddr_in));
	}
	return;
}
#endif

/*
 * rb_listen() - listen on a port
 */
int
rb_listen(rb_fde_t *F, int backlog)
{
	F->type = RB_FD_SOCKET | RB_FD_LISTEN;
	/* Currently just a simple wrapper for the sake of being complete */
	return listen(F->fd, backlog);
}

void
rb_fdlist_init(int closeall, int maxfds, size_t heapsize)
{
	static int initialized = 0;
#ifdef _WIN32
	WSADATA wsaData;
	int err;
	int vers = MAKEWORD(2, 0);

	err = WSAStartup(vers, &wsaData);
	if(err != 0)
	{
		rb_lib_die("WSAStartup failed");
	}

#endif
	if(!initialized)
	{
		rb_maxconnections = maxfds;
		if(closeall)
			rb_close_all();
		/* Since we're doing this once .. */
		initialized = 1;
	}
	fd_heap = rb_bh_create(sizeof(rb_fde_t), heapsize, "librb_fd_heap");

}


/* Called to open a given filedescriptor */
rb_fde_t *
rb_open(int fd, uint8_t type, const char *desc)
{
	rb_fde_t *F;
	lrb_assert(fd >= 0);

	F = add_fd(fd);

	lrb_assert(!IsFDOpen(F));
	if(rb_unlikely(IsFDOpen(F)))
	{
		const char *fdesc;
		if(F != NULL && F->desc != NULL)
			fdesc = F->desc;
		else
			fdesc = "NULL";
		rb_lib_log("Trying to rb_open an already open FD: %d desc: %s", fd, fdesc);
		return NULL;
	}
	F->fd = fd;
	F->type = type;
	SetFDOpen(F);

	if(desc != NULL)
		F->desc = rb_strndup(desc, FD_DESC_SZ);
	number_fd++;
	return F;
}


/* Called to close a given filedescriptor */
void
rb_close(rb_fde_t *F)
{
	int type, fd;

	if(F == NULL)
		return;

	fd = F->fd;
	type = F->type;
	lrb_assert(IsFDOpen(F));

	lrb_assert(!(type & RB_FD_FILE));
	if(rb_unlikely(type & RB_FD_FILE))
	{
		lrb_assert(F->read_handler == NULL);
		lrb_assert(F->write_handler == NULL);
	}
	rb_setselect(F, RB_SELECT_WRITE | RB_SELECT_READ, NULL, NULL);
	rb_settimeout(F, 0, NULL, NULL);
	rb_free(F->accept);
	rb_free(F->connect);
	rb_free(F->desc);
#ifdef HAVE_SSL
	if(type & RB_FD_SSL)
	{
		rb_ssl_shutdown(F);
	}
#endif /* HAVE_SSL */
	if(IsFDOpen(F))
	{
		remove_fd(F);
		ClearFDOpen(F);
	}

	number_fd--;

#ifdef _WIN32
	if(type & (RB_FD_SOCKET | RB_FD_PIPE))
	{
		closesocket(fd);
		return;
	}
	else
#endif
		close(fd);
}


/*
 * rb_dump_fd() - dump the list of active filedescriptors
 */
void
rb_dump_fd(DUMPCB * cb, void *data)
{
	static const char *empty = "";
	rb_dlink_node *ptr;
	rb_dlink_list *bucket;
	rb_fde_t *F;
	unsigned int i;

	for(i = 0; i < RB_FD_HASH_SIZE; i++)
	{
		bucket = &rb_fd_table[i];

		if(rb_dlink_list_length(bucket) <= 0)
			continue;

		RB_DLINK_FOREACH(ptr, bucket->head)
		{
			F = ptr->data;
			if(F == NULL || !IsFDOpen(F))
				continue;

			cb(F->fd, F->desc ? F->desc : empty, data);
		}
	}
}

/*
 * rb_note() - set the fd note
 *
 * Note: must be careful not to overflow rb_fd_table[fd].desc when
 *       calling.
 */
void
rb_note(rb_fde_t *F, const char *string)
{
	if(F == NULL)
		return;

	rb_free(F->desc);
	F->desc = rb_strndup(string, FD_DESC_SZ);
}

void
rb_set_type(rb_fde_t *F, uint8_t type)
{
	/* if the caller is calling this, lets assume they have a clue */
	F->type = type;
	return;
}

uint8_t
rb_get_type(rb_fde_t *F)
{
	return F->type;
}

int
rb_fd_ssl(rb_fde_t *F)
{
	if(F == NULL)
		return 0;
	if(F->type & RB_FD_SSL)
		return 1;
	return 0;
}

int
rb_get_fd(rb_fde_t *F)
{
	if(F == NULL)
		return -1;
	return (F->fd);
}

rb_fde_t *
rb_get_fde(int fd)
{
	return rb_find_fd(fd);
}

ssize_t
rb_read(rb_fde_t *F, void *buf, int count)
{
	ssize_t ret;
	if(F == NULL)
		return 0;

	/* This needs to be *before* RB_FD_SOCKET otherwise you'll process 
	 * an SSL socket as a regular socket 
	 */
#ifdef HAVE_SSL
	if(F->type & RB_FD_SSL)
	{
		return rb_ssl_read(F, buf, count);
	}
#endif
	if(F->type & RB_FD_SOCKET)
	{
		ret = recv(F->fd, buf, count, 0);
		if(ret < 0)
		{
			rb_get_errno();
		}
		return ret;
	}


	/* default case */
	return read(F->fd, buf, count);
}


ssize_t
rb_write(rb_fde_t *F, const void *buf, int count)
{
	ssize_t ret;
	if(F == NULL)
		return 0;

#ifdef HAVE_SSL
	if(F->type & RB_FD_SSL)
	{
		return rb_ssl_write(F, buf, count);
	}
#endif
	if(F->type & RB_FD_SOCKET)
	{
		ret = send(F->fd, buf, count, MSG_NOSIGNAL);
		if(ret < 0)
		{
			rb_get_errno();
		}
		return ret;
	}

	return write(F->fd, buf, count);
}

#if defined(HAVE_SSL) || defined(WIN32) || !defined(HAVE_WRITEV)
static ssize_t
rb_fake_writev(rb_fde_t *F, const struct rb_iovec *vp, size_t vpcount)
{
	ssize_t count = 0;

	while(vpcount-- > 0)
	{
		ssize_t written = rb_write(F, vp->iov_base, vp->iov_len);

		if(written <= 0)
		{
			if(count > 0)
				return count;
			else
				return written;
		}
		count += written;
		vp++;
	}
	return (count);
}
#endif

#if defined(WIN32) || !defined(HAVE_WRITEV)
ssize_t
rb_writev(rb_fde_t *F, struct rb_iovec * vecount, int count)
{
	return rb_fake_writev(F, vecount, count);
}

#else
ssize_t
rb_writev(rb_fde_t *F, struct rb_iovec * vector, int count)
{
	if(F == NULL)
	{
		errno = EBADF;
		return -1;
	}
#ifdef HAVE_SSL
	if(F->type & RB_FD_SSL)
	{
		return rb_fake_writev(F, vector, count);
	}
#endif /* HAVE_SSL */
#ifdef HAVE_SENDMSG
	if(F->type & RB_FD_SOCKET)
	{
		struct msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = (struct iovec *)vector;
		msg.msg_iovlen = count;
		return sendmsg(F->fd, &msg, MSG_NOSIGNAL);
	}
#endif /* HAVE_SENDMSG */
	return writev(F->fd, (struct iovec *)vector, count);

}
#endif


/* 
 * From: Thomas Helvey <tomh@inxpress.net>
 */
static const char *IpQuadTab[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
	"20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
	"30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
	"40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
	"50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
	"60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
	"70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
	"80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
	"90", "91", "92", "93", "94", "95", "96", "97", "98", "99",
	"100", "101", "102", "103", "104", "105", "106", "107", "108", "109",
	"110", "111", "112", "113", "114", "115", "116", "117", "118", "119",
	"120", "121", "122", "123", "124", "125", "126", "127", "128", "129",
	"130", "131", "132", "133", "134", "135", "136", "137", "138", "139",
	"140", "141", "142", "143", "144", "145", "146", "147", "148", "149",
	"150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
	"160", "161", "162", "163", "164", "165", "166", "167", "168", "169",
	"170", "171", "172", "173", "174", "175", "176", "177", "178", "179",
	"180", "181", "182", "183", "184", "185", "186", "187", "188", "189",
	"190", "191", "192", "193", "194", "195", "196", "197", "198", "199",
	"200", "201", "202", "203", "204", "205", "206", "207", "208", "209",
	"210", "211", "212", "213", "214", "215", "216", "217", "218", "219",
	"220", "221", "222", "223", "224", "225", "226", "227", "228", "229",
	"230", "231", "232", "233", "234", "235", "236", "237", "238", "239",
	"240", "241", "242", "243", "244", "245", "246", "247", "248", "249",
	"250", "251", "252", "253", "254", "255"
};

/*
 * inetntoa - in_addr to string
 *      changed name to remove collision possibility and
 *      so behaviour is guaranteed to take a pointer arg.
 *      -avalon 23/11/92
 *  inet_ntoa --  returned the dotted notation of a given
 *      internet number
 *      argv 11/90).
 *  inet_ntoa --  its broken on some Ultrix/Dynix too. -avalon
 */

static const char *
inetntoa(const char *in)
{
	static char buf[16];
	char *bufptr = buf;
	const unsigned char *a = (const unsigned char *)in;
	const char *n;

	n = IpQuadTab[*a++];
	while(*n)
		*bufptr++ = *n++;
	*bufptr++ = '.';
	n = IpQuadTab[*a++];
	while(*n)
		*bufptr++ = *n++;
	*bufptr++ = '.';
	n = IpQuadTab[*a++];
	while(*n)
		*bufptr++ = *n++;
	*bufptr++ = '.';
	n = IpQuadTab[*a];
	while(*n)
		*bufptr++ = *n++;
	*bufptr = '\0';
	return buf;
}


/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#define SPRINTF(x) ((size_t)rb_sprintf x)

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static const char *inet_ntop4(const unsigned char *src, char *dst, unsigned int size);
#ifdef RB_IPV6
static const char *inet_ntop6(const unsigned char *src, char *dst, unsigned int size);
#endif

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a unsigned char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop4(const unsigned char *src, char *dst, unsigned int size)
{
	if(size < 16)
		return NULL;
	return strcpy(dst, inetntoa((const char *)src));
}

/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
#ifdef RB_IPV6
static const char *
inet_ntop6(const unsigned char *src, char *dst, unsigned int size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct
	{
		int base, len;
	}
	best, cur;
	unsigned int words[IN6ADDRSZ / INT16SZ];
	int i;

	/*
	 * Preprocess:
	 *      Copy the input (bytewise) array into a wordwise array.
	 *      Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, '\0', sizeof words);
	for(i = 0; i < IN6ADDRSZ; i += 2)
		words[i / 2] = (src[i] << 8) | src[i + 1];
	best.base = -1;
	best.len = 0;
	cur.base = -1;
	cur.len = 0;
	for(i = 0; i < (IN6ADDRSZ / INT16SZ); i++)
	{
		if(words[i] == 0)
		{
			if(cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		}
		else
		{
			if(cur.base != -1)
			{
				if(best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if(cur.base != -1)
	{
		if(best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if(best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for(i = 0; i < (IN6ADDRSZ / INT16SZ); i++)
	{
		/* Are we inside the best run of 0x00's? */
		if(best.base != -1 && i >= best.base && i < (best.base + best.len))
		{
			if(i == best.base)
			{
				if(i == 0)
					*tp++ = '0';
				*tp++ = ':';
			}
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if(i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if(i == 6 && best.base == 0 &&
		   (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
		{
			if(!inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		tp += SPRINTF((tp, "%x", words[i]));
	}
	/* Was it a trailing run of 0x00's? */
	if(best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */

	if((unsigned int)(tp - tmp) > size)
	{
		return (NULL);
	}
	return strcpy(dst, tmp);
}
#endif

int
rb_inet_pton_sock(const char *src, struct sockaddr *dst)
{
	if(rb_inet_pton(AF_INET, src, &((struct sockaddr_in *)dst)->sin_addr))
	{
		((struct sockaddr_in *)dst)->sin_port = 0;
		((struct sockaddr_in *)dst)->sin_family = AF_INET;
		SET_SS_LEN(dst, sizeof(struct sockaddr_in));
		return 1;
	}
#ifdef RB_IPV6
	else if(rb_inet_pton(AF_INET6, src, &((struct sockaddr_in6 *)dst)->sin6_addr))
	{
		((struct sockaddr_in6 *)dst)->sin6_port = 0;
		((struct sockaddr_in6 *)dst)->sin6_family = AF_INET6;
		SET_SS_LEN(dst, sizeof(struct sockaddr_in6));
		return 1;
	}
#endif
	return 0;
}

const char *
rb_inet_ntop_sock(struct sockaddr *src, char *dst, unsigned int size)
{
	switch (src->sa_family)
	{
	case AF_INET:
		return (rb_inet_ntop(AF_INET, &((struct sockaddr_in *)src)->sin_addr, dst, size));
		break;
#ifdef RB_IPV6
	case AF_INET6:
		return (rb_inet_ntop
			(AF_INET6, &((struct sockaddr_in6 *)src)->sin6_addr, dst, size));
		break;
#endif
	default:
		return NULL;
		break;
	}
}

/* char *
 * rb_inet_ntop(af, src, dst, size)
 *	convert a network format address to presentation format.
 * return:
 *	pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *	Paul Vixie, 1996.
 */
const char *
rb_inet_ntop(int af, const void *src, char *dst, unsigned int size)
{
	switch (af)
	{
	case AF_INET:
		return (inet_ntop4(src, dst, size));
#ifdef RB_IPV6
	case AF_INET6:
		if(IN6_IS_ADDR_V4MAPPED((const struct in6_addr *)src) ||
		   IN6_IS_ADDR_V4COMPAT((const struct in6_addr *)src))
			return (inet_ntop4
				((const unsigned char *)&((const struct in6_addr *)src)->
				 s6_addr[12], dst, size));
		else
			return (inet_ntop6(src, dst, size));


#endif
	default:
		return (NULL);
	}
	/* NOTREACHED */
}

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

/* int
 * rb_inet_pton(af, src, dst)
 *	convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * return:
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *	Paul Vixie, 1996.
 */

/* int
 * inet_pton4(src, dst)
 *	like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *	1 if `src' is a valid dotted quad, else 0.
 * notice:
 *	does not touch `dst' unless it's returning 1.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, unsigned char *dst)
{
	int saw_digit, octets, ch;
	unsigned char tmp[INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while((ch = *src++) != '\0')
	{

		if(ch >= '0' && ch <= '9')
		{
			unsigned int new = *tp * 10 + (ch - '0');

			if(new > 255)
				return (0);
			*tp = new;
			if(!saw_digit)
			{
				if(++octets > 4)
					return (0);
				saw_digit = 1;
			}
		}
		else if(ch == '.' && saw_digit)
		{
			if(octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		}
		else
			return (0);
	}
	if(octets < 4)
		return (0);
	memcpy(dst, tmp, INADDRSZ);
	return (1);
}

#ifdef RB_IPV6
/* int
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 * credit:
 *	inspired by Mark Andrews.
 * author:
 *	Paul Vixie, 1996.
 */

static int
inet_pton6(const char *src, unsigned char *dst)
{
	static const char xdigits[] = "0123456789abcdef";
	unsigned char tmp[IN6ADDRSZ], *tp, *endp, *colonp;
	const char *curtok;
	int ch, saw_xdigit;
	unsigned int val;

	tp = memset(tmp, '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if(*src == ':')
		if(*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while((ch = tolower(*src++)) != '\0')
	{
		const char *pch;

		pch = strchr(xdigits, ch);
		if(pch != NULL)
		{
			val <<= 4;
			val |= (pch - xdigits);
			if(val > 0xffff)
				return (0);
			saw_xdigit = 1;
			continue;
		}
		if(ch == ':')
		{
			curtok = src;
			if(!saw_xdigit)
			{
				if(colonp)
					return (0);
				colonp = tp;
				continue;
			}
			else if(*src == '\0')
			{
				return (0);
			}
			if(tp + INT16SZ > endp)
				return (0);
			*tp++ = (unsigned char)(val >> 8) & 0xff;
			*tp++ = (unsigned char)val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if(*src != '\0' && ch == '.')
		{
			if(((tp + INADDRSZ) <= endp) && inet_pton4(curtok, tp) > 0)
			{
				tp += INADDRSZ;
				saw_xdigit = 0;
				break;	/* '\0' was seen by inet_pton4(). */
			}
		}
		else
			continue;
		return (0);
	}
	if(saw_xdigit)
	{
		if(tp + INT16SZ > endp)
			return (0);
		*tp++ = (unsigned char)(val >> 8) & 0xff;
		*tp++ = (unsigned char)val & 0xff;
	}
	if(colonp != NULL)
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		if(tp == endp)
			return (0);
		for(i = 1; i <= n; i++)
		{
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if(tp != endp)
		return (0);
	memcpy(dst, tmp, IN6ADDRSZ);
	return (1);
}
#endif
int
rb_inet_pton(int af, const char *src, void *dst)
{
	switch (af)
	{
	case AF_INET:
		return (inet_pton4(src, dst));
#ifdef RB_IPV6
	case AF_INET6:
		/* Somebody might have passed as an IPv4 address this is sick but it works */
		if(inet_pton4(src, dst))
		{
			char tmp[HOSTIPLEN];
			rb_sprintf(tmp, "::ffff:%s", src);
			return (inet_pton6(tmp, dst));
		}
		else
			return (inet_pton6(src, dst));
#endif
	default:
		return (-1);
	}
	/* NOTREACHED */
}


#ifndef HAVE_SOCKETPAIR

/* mostly based on perl's emulation of socketpair udp */
static int
rb_inet_socketpair_udp(rb_fde_t **newF1, rb_fde_t **newF2)
{
	struct sockaddr_in addr[2];
	rb_socklen_t size = sizeof(struct sockaddr_in);
	rb_fde_t *F[2];
	unsigned int fd[2];
	int i, got;
	unsigned short port;
	struct timeval wait = { 0, 100000 };
	int max;
	fd_set rset;
	struct sockaddr_in readfrom;
	unsigned short buf[2];
	int o_errno;

	memset(&addr, 0, sizeof(addr));

	for(i = 0; i < 2; i++)
	{
		F[i] = rb_socket(AF_INET, SOCK_DGRAM, 0, "udp socketpair");
		if(F[i] == NULL)
			goto failed;
		addr[i].sin_family = AF_INET;
		addr[i].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr[i].sin_port = 0;
		if(bind(rb_get_fd(F[i]), (struct sockaddr *)&addr[i], sizeof(struct sockaddr_in)))
			goto failed;
		fd[i] = rb_get_fd(F[i]);
	}

	for(i = 0; i < 2; i++)
	{
		if(getsockname(fd[i], (struct sockaddr *)&addr[i], &size))
			goto failed;
		if(size != sizeof(struct sockaddr_in))
			goto failed;
		if(connect(fd[!i], (struct sockaddr *)&addr[i], sizeof(struct sockaddr_in)) == -1)
			goto failed;
	}

	for(i = 0; i < 2; i++)
	{
		port = addr[i].sin_port;
		got = rb_write(F[i], &port, sizeof(port));
		if(got != sizeof(port))
		{
			if(got == -1)
				goto failed;
			goto abort_failed;
		}
	}

	max = fd[1] > fd[0] ? fd[1] : fd[0];
	FD_ZERO(&rset);
	FD_SET(fd[0], &rset);
	FD_SET(fd[1], &rset);
	got = select(max + 1, &rset, NULL, NULL, &wait);
	if(got != 2 || !FD_ISSET(fd[0], &rset) || !FD_ISSET(fd[1], &rset))
	{
		if(got == -1)
			goto failed;
		goto abort_failed;
	}

	for(i = 0; i < 2; i++)
	{
#ifdef MSG_DONTWAIT
		int flag = MSG_DONTWAIT
#else
		int flag = 0;
#endif
		got = recvfrom(rb_get_fd(F[i]), (char *)&buf, sizeof(buf), flag,
			       (struct sockaddr *)&readfrom, &size);
		if(got == -1)
			goto failed;
		if(got != sizeof(port)
		   || size != sizeof(struct sockaddr_in)
		   || buf[0] != (unsigned short)addr[!i].sin_port
		   || readfrom.sin_family != addr[!i].sin_family
		   || readfrom.sin_addr.s_addr != addr[!i].sin_addr.s_addr
		   || readfrom.sin_port != addr[!i].sin_port)
			goto abort_failed;
	}

	*newF1 = F[0];
	*newF2 = F[1];
	return 0;

#ifdef _WIN32
#define	ECONNABORTED WSAECONNABORTED
#endif

      abort_failed:
	rb_get_errno();
	errno = ECONNABORTED;
      failed:
	if(errno != ECONNABORTED)
		rb_get_errno();
	o_errno = errno;
	if(F[0] != NULL)
		rb_close(F[0]);
	if(F[1] != NULL)
		rb_close(F[1]);
	errno = o_errno;
	return -1;
}


int
rb_inet_socketpair(int family, int type, int protocol, int fd[2])
{
	int listener = -1;
	int connector = -1;
	int acceptor = -1;
	struct sockaddr_in listen_addr;
	struct sockaddr_in connect_addr;
	rb_socklen_t size;

	if(protocol || family != AF_INET)
	{
		errno = EAFNOSUPPORT;
		return -1;
	}
	if(!fd)
	{
		errno = EINVAL;
		return -1;
	}

	listener = socket(AF_INET, type, 0);
	if(listener == -1)
		return -1;
	memset(&listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	listen_addr.sin_port = 0;	/* kernel choses port.  */
	if(bind(listener, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) == -1)
		goto tidy_up_and_fail;
	if(listen(listener, 1) == -1)
		goto tidy_up_and_fail;

	connector = socket(AF_INET, type, 0);
	if(connector == -1)
		goto tidy_up_and_fail;
	/* We want to find out the port number to connect to.  */
	size = sizeof(connect_addr);
	if(getsockname(listener, (struct sockaddr *)&connect_addr, &size) == -1)
		goto tidy_up_and_fail;
	if(size != sizeof(connect_addr))
		goto abort_tidy_up_and_fail;
	if(connect(connector, (struct sockaddr *)&connect_addr, sizeof(connect_addr)) == -1)
		goto tidy_up_and_fail;

	size = sizeof(listen_addr);
	acceptor = accept(listener, (struct sockaddr *)&listen_addr, &size);
	if(acceptor == -1)
		goto tidy_up_and_fail;
	if(size != sizeof(listen_addr))
		goto abort_tidy_up_and_fail;
	close(listener);
	/* Now check we are talking to ourself by matching port and host on the
	   two sockets.  */
	if(getsockname(connector, (struct sockaddr *)&connect_addr, &size) == -1)
		goto tidy_up_and_fail;
	if(size != sizeof(connect_addr)
	   || listen_addr.sin_family != connect_addr.sin_family
	   || listen_addr.sin_addr.s_addr != connect_addr.sin_addr.s_addr
	   || listen_addr.sin_port != connect_addr.sin_port)
	{
		goto abort_tidy_up_and_fail;
	}
	fd[0] = connector;
	fd[1] = acceptor;
	return 0;

      abort_tidy_up_and_fail:
	errno = EINVAL;		/* I hope this is portable and appropriate.  */

      tidy_up_and_fail:
	{
		int save_errno = errno;
		if(listener != -1)
			close(listener);
		if(connector != -1)
			close(connector);
		if(acceptor != -1)
			close(acceptor);
		errno = save_errno;
		return -1;
	}
}

#endif


static void (*setselect_handler) (rb_fde_t *, unsigned int, PF *, void *);
static int (*select_handler) (long);
static int (*setup_fd_handler) (rb_fde_t *);
static int (*io_sched_event) (struct ev_entry *, int);
static void (*io_unsched_event) (struct ev_entry *);
static int (*io_supports_event) (void);
static void (*io_init_event) (void);
static char iotype[25];

const char *
rb_get_iotype(void)
{
	return iotype;
}

static int
rb_unsupported_event(void)
{
	return 0;
}

static int
try_kqueue(void)
{
	if(!rb_init_netio_kqueue())
	{
		setselect_handler = rb_setselect_kqueue;
		select_handler = rb_select_kqueue;
		setup_fd_handler = rb_setup_fd_kqueue;
		io_sched_event = rb_kqueue_sched_event;
		io_unsched_event = rb_kqueue_unsched_event;
		io_init_event = rb_kqueue_init_event;
		io_supports_event = rb_kqueue_supports_event;
		rb_strlcpy(iotype, "kqueue", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_epoll(void)
{
	if(!rb_init_netio_epoll())
	{
		setselect_handler = rb_setselect_epoll;
		select_handler = rb_select_epoll;
		setup_fd_handler = rb_setup_fd_epoll;
		io_sched_event = rb_epoll_sched_event;
		io_unsched_event = rb_epoll_unsched_event;
		io_supports_event = rb_epoll_supports_event;
		io_init_event = rb_epoll_init_event;
		rb_strlcpy(iotype, "epoll", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_ports(void)
{
	if(!rb_init_netio_ports())
	{
		setselect_handler = rb_setselect_ports;
		select_handler = rb_select_ports;
		setup_fd_handler = rb_setup_fd_ports;
		io_sched_event = rb_ports_sched_event;
		io_unsched_event = rb_ports_unsched_event;
		io_init_event =  rb_ports_init_event;
		io_supports_event = rb_ports_supports_event;
		rb_strlcpy(iotype, "ports", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_devpoll(void)
{
	if(!rb_init_netio_devpoll())
	{
		setselect_handler = rb_setselect_devpoll;
		select_handler = rb_select_devpoll;
		setup_fd_handler = rb_setup_fd_devpoll;
		io_sched_event = NULL;
		io_unsched_event = NULL;
		io_init_event = NULL;
		io_supports_event = rb_unsupported_event;
		rb_strlcpy(iotype, "devpoll", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_sigio(void)
{
	if(!rb_init_netio_sigio())
	{
		setselect_handler = rb_setselect_sigio;
		select_handler = rb_select_sigio;
		setup_fd_handler = rb_setup_fd_sigio;
		io_sched_event = rb_sigio_sched_event;
		io_unsched_event = rb_sigio_unsched_event;
		io_supports_event = rb_sigio_supports_event;
		io_init_event = rb_sigio_init_event;

		rb_strlcpy(iotype, "sigio", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_poll(void)
{
	if(!rb_init_netio_poll())
	{
		setselect_handler = rb_setselect_poll;
		select_handler = rb_select_poll;
		setup_fd_handler = rb_setup_fd_poll;
		io_sched_event = NULL;
		io_unsched_event = NULL;
		io_init_event = NULL;
		io_supports_event = rb_unsupported_event;
		rb_strlcpy(iotype, "poll", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_win32(void)
{
	if(!rb_init_netio_win32())
	{
		setselect_handler = rb_setselect_win32;
		select_handler = rb_select_win32;
		setup_fd_handler = rb_setup_fd_win32;
		io_sched_event = NULL;
		io_unsched_event = NULL;
		io_init_event = NULL;
		io_supports_event = rb_unsupported_event;
		rb_strlcpy(iotype, "win32", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_select(void)
{
	if(!rb_init_netio_select())
	{
		setselect_handler = rb_setselect_select;
		select_handler = rb_select_select;
		setup_fd_handler = rb_setup_fd_select;
		io_sched_event = NULL;
		io_unsched_event = NULL;
		io_init_event = NULL;
		io_supports_event = rb_unsupported_event;
		rb_strlcpy(iotype, "select", sizeof(iotype));
		return 0;
	}
	return -1;
}


int
rb_io_sched_event(struct ev_entry *ev, int when)
{
	if(ev == NULL || io_supports_event == NULL || io_sched_event == NULL
	   || !io_supports_event())
		return 0;
	return io_sched_event(ev, when);
}

void
rb_io_unsched_event(struct ev_entry *ev)
{
	if(ev == NULL || io_supports_event == NULL || io_unsched_event == NULL
	   || !io_supports_event())
		return;
	io_unsched_event(ev);
}

int
rb_io_supports_event(void)
{
	if(io_supports_event == NULL)
		return 0;
	return io_supports_event();
}

void
rb_io_init_event(void)
{
	io_init_event();
	rb_event_io_register_all();
}

void
rb_init_netio(void)
{
	char *ioenv = getenv("LIBRB_USE_IOTYPE");
	rb_fd_table = rb_malloc(RB_FD_HASH_SIZE * sizeof(rb_dlink_list));
	rb_init_ssl();

	if(ioenv != NULL)
	{
		if(!strcmp("epoll", ioenv))
		{
			if(!try_epoll())
				return;
		}
		else if(!strcmp("kqueue", ioenv))
		{
			if(!try_kqueue())
				return;
		}
		else if(!strcmp("ports", ioenv))
		{
			if(!try_ports())
				return;
		}
		else if(!strcmp("poll", ioenv))
		{
			if(!try_poll())
				return;
		}
		else if(!strcmp("devpoll", ioenv))
		{
			if(!try_devpoll())
				return;
		}
		else if(!strcmp("sigio", ioenv))
		{
			if(!try_sigio())
				return;
		}
		else if(!strcmp("select", ioenv))
		{
			if(!try_select())
				return;
		}
		if(!strcmp("win32", ioenv))
		{
			if(!try_win32())
				return;
		}

	}

	if(!try_kqueue())
		return;
	if(!try_epoll())
		return;
	if(!try_ports())
		return;
	if(!try_devpoll())
		return;
	if(!try_sigio())
		return;
	if(!try_poll())
		return;
	if(!try_win32())
		return;
	if(!try_select())
		return;

	rb_lib_log("rb_init_netio: Could not find any io handlers...giving up");

	abort();
}

void
rb_setselect(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	setselect_handler(F, type, handler, client_data);
}

int
rb_select(unsigned long timeout)
{
	int ret = select_handler(timeout);
	free_fds();
	return ret;
}

int
rb_setup_fd(rb_fde_t *F)
{
	return setup_fd_handler(F);
}



int
rb_ignore_errno(int error)
{
	switch (error)
	{
#ifdef EINPROGRESS
	case EINPROGRESS:
#endif
#if defined EWOULDBLOCK
	case EWOULDBLOCK:
#endif
#if defined(EAGAIN) && (EWOULDBLOCK != EAGAIN)
	case EAGAIN:
#endif
#ifdef EINTR
	case EINTR:
#endif
#ifdef ERESTART
	case ERESTART:
#endif
#ifdef ENOBUFS
	case ENOBUFS:
#endif
		return 1;
	default:
		break;
	}
	return 0;
}


#if defined(HAVE_SENDMSG) && !defined(WIN32)
int
rb_recv_fd_buf(rb_fde_t *F, void *data, size_t datasize, rb_fde_t **xF, int nfds)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	struct stat st;
	uint8_t stype = RB_FD_UNKNOWN;
	const char *desc;
	int fd, len, x, rfds;

	int control_len = CMSG_SPACE(sizeof(int) * nfds);

	iov[0].iov_base = data;
	iov[0].iov_len = datasize;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;
	cmsg = alloca(control_len);
	msg.msg_control = cmsg;
	msg.msg_controllen = control_len;

	if((len = recvmsg(rb_get_fd(F), &msg, 0)) <= 0)
		return len;

	if(msg.msg_controllen > 0 && msg.msg_control != NULL
	   && (cmsg = CMSG_FIRSTHDR(&msg)) != NULL)
	{
		rfds = ((unsigned char *)cmsg + cmsg->cmsg_len - CMSG_DATA(cmsg)) / sizeof(int);

		for(x = 0; x < nfds && x < rfds; x++)
		{
			fd = ((int *)CMSG_DATA(cmsg))[x];
			stype = RB_FD_UNKNOWN;
			desc = "remote unknown";
			if(!fstat(fd, &st))
			{
				if(S_ISSOCK(st.st_mode))
				{
					stype = RB_FD_SOCKET;
					desc = "remote socket";
				}
				else if(S_ISFIFO(st.st_mode))
				{
					stype = RB_FD_PIPE;
					desc = "remote pipe";
				}
				else if(S_ISREG(st.st_mode))
				{
					stype = RB_FD_FILE;
					desc = "remote file";
				}
			}
			xF[x] = rb_open(fd, stype, desc);
		}
	}
	else
		*xF = NULL;
	return len;
}


int
rb_send_fd_buf(rb_fde_t *xF, rb_fde_t **F, int count, void *data, size_t datasize, pid_t pid)
{
	int n;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	char empty = '0';
	char *buf;

	memset(&msg, 0, sizeof(&msg));
	if(datasize == 0)
	{
		iov[0].iov_base = &empty;
		iov[0].iov_len = 1;
	}
	else
	{
		iov[0].iov_base = data;
		iov[0].iov_len = datasize;
	}
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_flags = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	if(count > 0)
	{
		int i;
		int len = CMSG_SPACE(sizeof(int) * count);
		buf = alloca(len);

		msg.msg_control = buf;
		msg.msg_controllen = len;
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int) * count);

		for(i = 0; i < count; i++)
		{
			((int *)CMSG_DATA(cmsg))[i] = rb_get_fd(F[i]);
		}
		msg.msg_controllen = cmsg->cmsg_len;
	}
	n = sendmsg(rb_get_fd(xF), &msg, MSG_NOSIGNAL);
	return n;
}
#else
#ifndef _WIN32
int
rb_recv_fd_buf(rb_fde_t *F, void *data, size_t datasize, rb_fde_t **xF, int nfds)
{
	errno = ENOSYS;
	return -1;
}

int
rb_send_fd_buf(rb_fde_t *xF, rb_fde_t **F, int count, void *data, size_t datasize, pid_t pid)
{
	errno = ENOSYS;
	return -1;
}
#endif
#endif
