/*
 *  ircd-ratbox: A slightly useful ircd.
 *  commio.h: A header for the network subsystem.
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
 *  $Id: rb_commio.h 26092 2008-09-19 15:13:52Z androsyn $
 */

#ifndef RB_LIB_H
# error "Do not use commio.h directly"
#endif

#ifndef INCLUDED_commio_h
#define INCLUDED_commio_h


struct sockaddr;
struct _fde;
typedef struct _fde rb_fde_t;

/* Callback for completed IO events */
typedef void PF(rb_fde_t *, void *);

/* Callback for completed connections */
/* int fd, int status, void * */
typedef void CNCB(rb_fde_t *, int, void *);
/* callback for fd table dumps */
typedef void DUMPCB(int, const char *desc, void *);
/* callback for accept callbacks */
typedef void ACCB(rb_fde_t *, int status, struct sockaddr *addr, rb_socklen_t len, void *);
/* callback for pre-accept callback */
typedef int ACPRE(rb_fde_t *, struct sockaddr *addr, rb_socklen_t len, void *);

enum
{
	RB_OK,
	RB_ERR_BIND,
	RB_ERR_DNS,
	RB_ERR_TIMEOUT,
	RB_ERR_CONNECT,
	RB_ERROR,
	RB_ERROR_SSL,
	RB_ERR_MAX
};

#define RB_FD_NONE		0x01
#define RB_FD_FILE		0x02
#define RB_FD_SOCKET		0x04
#ifndef _WIN32
#define RB_FD_PIPE		0x08
#else
#define RB_FD_PIPE		RB_FD_SOCKET
#endif
#define	RB_FD_LISTEN		0x10
#define RB_FD_SSL		0x20
#define RB_FD_UNKNOWN		0x40

#define RB_RW_IO_ERROR		-1	/* System call error */
#define RB_RW_SSL_ERROR		-2	/* SSL Error */
#define RB_RW_SSL_NEED_READ	-3	/* SSL Needs read */
#define RB_RW_SSL_NEED_WRITE	-4	/* SSL Needs write */


struct rb_iovec
{
	void *iov_base;
	size_t iov_len;
};


void rb_fdlist_init(int closeall, int maxfds, size_t heapsize);

rb_fde_t *rb_open(int, uint8_t, const char *);
void rb_close(rb_fde_t *);
void rb_dump_fd(DUMPCB *, void *xdata);
void rb_note(rb_fde_t *, const char *);

/* Type of IO */
#define	RB_SELECT_READ		0x1
#define	RB_SELECT_WRITE		0x2

#define RB_SELECT_ACCEPT		RB_SELECT_READ
#define RB_SELECT_CONNECT		RB_SELECT_WRITE

#define RB_SSL_CERTFP_LEN 20

int rb_set_nb(rb_fde_t *);
int rb_set_buffers(rb_fde_t *, int);

int rb_get_sockerr(rb_fde_t *);

void rb_settimeout(rb_fde_t *, time_t, PF *, void *);
void rb_checktimeouts(void *);
void rb_connect_tcp(rb_fde_t *, struct sockaddr *, struct sockaddr *, int, CNCB *, void *, int);
void rb_connect_tcp_ssl(rb_fde_t *, struct sockaddr *, struct sockaddr *, int, CNCB *, void *, int);
int rb_connect_sockaddr(rb_fde_t *, struct sockaddr *addr, int len);

const char *rb_errstr(int status);
rb_fde_t *rb_socket(int family, int sock_type, int proto, const char *note);
int rb_socketpair(int family, int sock_type, int proto, rb_fde_t **F1, rb_fde_t **F2,
		  const char *note);

void rb_accept_tcp(rb_fde_t *, ACPRE * precb, ACCB * callback, void *data);
ssize_t rb_write(rb_fde_t *, const void *buf, int count);
ssize_t rb_writev(rb_fde_t *, struct rb_iovec *vector, int count);

ssize_t rb_read(rb_fde_t *, void *buf, int count);
int rb_pipe(rb_fde_t **, rb_fde_t **, const char *desc);

int rb_setup_ssl_server(const char *cert, const char *keyfile, const char *dhfile);
int rb_ssl_listen(rb_fde_t *, int backlog);
int rb_listen(rb_fde_t *, int backlog);

const char *rb_inet_ntop(int af, const void *src, char *dst, unsigned int size);
int rb_inet_pton(int af, const char *src, void *dst);
const char *rb_inet_ntop_sock(struct sockaddr *src, char *dst, unsigned int size);
int rb_inet_pton_sock(const char *src, struct sockaddr *dst);
int rb_getmaxconnect(void);
int rb_ignore_errno(int);

/* Generic wrappers */
void rb_setselect(rb_fde_t *, unsigned int type, PF * handler, void *client_data);
void rb_init_netio(void);
int rb_select(unsigned long);
int rb_fd_ssl(rb_fde_t *F);
int rb_get_fd(rb_fde_t *F);
const char *rb_get_ssl_strerror(rb_fde_t *F);
int rb_get_ssl_certfp(rb_fde_t *F, uint8_t certfp[RB_SSL_CERTFP_LEN]);

rb_fde_t *rb_get_fde(int fd);

int rb_send_fd_buf(rb_fde_t *xF, rb_fde_t **F, int count, void *data, size_t datasize, pid_t pid);
int rb_recv_fd_buf(rb_fde_t *F, void *data, size_t datasize, rb_fde_t **xF, int count);

void rb_set_type(rb_fde_t *F, uint8_t type);
uint8_t rb_get_type(rb_fde_t *F);

const char *rb_get_iotype(void);

typedef enum
{
	RB_PRNG_EGD,
	RB_PRNG_FILE,
#ifdef _WIN32
	RB_PRNGWIN32,
#endif
	RB_PRNG_DEFAULT,
} prng_seed_t;

int rb_init_prng(const char *path, prng_seed_t seed_type);
int rb_get_random(void *buf, size_t len);
int rb_get_pseudo_random(void *buf, size_t len);
void rb_ssl_start_accepted(rb_fde_t *new_F, ACCB * cb, void *data, int timeout);
void rb_ssl_start_connected(rb_fde_t *F, CNCB * callback, void *data, int timeout);
int rb_supports_ssl(void);

unsigned int rb_ssl_handshake_count(rb_fde_t *F);
void rb_ssl_clear_handshake_count(rb_fde_t *F);


int rb_pass_fd_to_process(rb_fde_t *, pid_t, rb_fde_t *);
rb_fde_t *rb_recv_fd(rb_fde_t *);

#endif /* INCLUDED_commio_h */
