/*
 *  wsockd.c: charybdis websockets helper
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2007 ircd-ratbox development team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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
 */

#include "stdinc.h"

#define MAXPASSFD 4
#ifndef READBUF_SIZE
#define READBUF_SIZE 16384
#endif

static void setup_signals(void);
static pid_t ppid;

static inline uint32_t
buf_to_uint32(uint8_t *buf)
{
	uint32_t x;
	memcpy(&x, buf, sizeof(x));
	return x;
}

static inline void
uint32_to_buf(uint8_t *buf, uint32_t x)
{
	memcpy(buf, &x, sizeof(x));
	return;
}

typedef struct _mod_ctl_buf
{
	rb_dlink_node node;
	uint8_t *buf;
	size_t buflen;
	rb_fde_t *F[MAXPASSFD];
	int nfds;
} mod_ctl_buf_t;

typedef struct _mod_ctl
{
	rb_dlink_node node;
	int cli_count;
	rb_fde_t *F;
	rb_fde_t *F_pipe;
	rb_dlink_list readq;
	rb_dlink_list writeq;
} mod_ctl_t;

static mod_ctl_t *mod_ctl;

typedef struct _conn
{
	rb_dlink_node node;
	mod_ctl_t *ctl;
	rawbuf_head_t *modbuf_out;
	rawbuf_head_t *plainbuf_out;

	uint32_t id;

	rb_fde_t *mod_fd;
	rb_fde_t *plain_fd;
	uint64_t mod_out;
	uint64_t mod_in;
	uint64_t plain_in;
	uint64_t plain_out;
	uint8_t flags;
} conn_t;

#define FLAG_CORK	0x01
#define FLAG_DEAD	0x02
#define FLAG_WSOCK	0x04

#define IsCork(x) ((x)->flags & FLAG_CORK)
#define IsDead(x) ((x)->flags & FLAG_DEAD)
#define IsWS(x)   ((x)->flags & FLAG_WSOCK)

#define SetCork(x) ((x)->flags |= FLAG_CORK)
#define SetDead(x) ((x)->flags |= FLAG_DEAD)
#define SetWS(x)   ((x)->flags |= FLAG_WSOCK)

#define ClearCork(x) ((x)->flags &= ~FLAG_CORK)
#define ClearDead(x) ((x)->flags &= ~FLAG_DEAD)
#define ClearWS(x)   ((x)->flags &= ~FLAG_WSOCK)

#define NO_WAIT 0x0
#define WAIT_PLAIN 0x1

#define HASH_WALK_SAFE(i, max, ptr, next, table) for(i = 0; i < max; i++) { RB_DLINK_FOREACH_SAFE(ptr, next, table[i].head)
#define HASH_WALK_END }
#define CONN_HASH_SIZE 2000
#define connid_hash(x)	(&connid_hash_table[(x % CONN_HASH_SIZE)])

static rb_dlink_list connid_hash_table[CONN_HASH_SIZE];
static rb_dlink_list dead_list;

static void conn_plain_read_shutdown_cb(rb_fde_t *fd, void *data);

#ifndef _WIN32
static void
dummy_handler(int sig)
{
	return;
}
#endif

static void
setup_signals()
{
#ifndef _WIN32
	struct sigaction act;

	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGPIPE);
	sigaddset(&act.sa_mask, SIGALRM);
#ifdef SIGTRAP
	sigaddset(&act.sa_mask, SIGTRAP);
#endif

#ifdef SIGWINCH
	sigaddset(&act.sa_mask, SIGWINCH);
	sigaction(SIGWINCH, &act, 0);
#endif
	sigaction(SIGPIPE, &act, 0);
#ifdef SIGTRAP
	sigaction(SIGTRAP, &act, 0);
#endif

	act.sa_handler = dummy_handler;
	sigaction(SIGALRM, &act, 0);
#endif
}

static int
maxconn(void)
{
#if defined(RLIMIT_NOFILE) && defined(HAVE_SYS_RESOURCE_H)
	struct rlimit limit;

	if(!getrlimit(RLIMIT_NOFILE, &limit))
	{
		return limit.rlim_cur;
	}
#endif /* RLIMIT_FD_MAX */
	return MAXCONNECTIONS;
}

static conn_t *
conn_find_by_id(uint32_t id)
{
	rb_dlink_node *ptr;
	conn_t *conn;

	RB_DLINK_FOREACH(ptr, (connid_hash(id))->head)
	{
		conn = ptr->data;
		if(conn->id == id && !IsDead(conn))
			return conn;
	}
	return NULL;
}

static void
conn_add_id_hash(conn_t * conn, uint32_t id)
{
	conn->id = id;
	rb_dlinkAdd(conn, &conn->node, connid_hash(id));
}

static void
free_conn(conn_t * conn)
{
	rb_free_rawbuffer(conn->modbuf_out);
	rb_free_rawbuffer(conn->plainbuf_out);
	rb_free(conn);
}

static void
clean_dead_conns(void *unused)
{
	conn_t *conn;
	rb_dlink_node *ptr, *next;

	RB_DLINK_FOREACH_SAFE(ptr, next, dead_list.head)
	{
		conn = ptr->data;
		free_conn(conn);
	}

	dead_list.tail = dead_list.head = NULL;
}

static void
mod_write_ctl(rb_fde_t *F, void *data)
{
	mod_ctl_t *ctl = data;
	mod_ctl_buf_t *ctl_buf;
	rb_dlink_node *ptr, *next;
	int retlen, x;

	RB_DLINK_FOREACH_SAFE(ptr, next, ctl->writeq.head)
	{
		ctl_buf = ptr->data;
		retlen = rb_send_fd_buf(ctl->F, ctl_buf->F, ctl_buf->nfds, ctl_buf->buf,
					ctl_buf->buflen, ppid);
		if(retlen > 0)
		{
			rb_dlinkDelete(ptr, &ctl->writeq);
			for(x = 0; x < ctl_buf->nfds; x++)
				rb_close(ctl_buf->F[x]);
			rb_free(ctl_buf->buf);
			rb_free(ctl_buf);

		}
		if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
			exit(0);

	}
	if(rb_dlink_list_length(&ctl->writeq) > 0)
		rb_setselect(ctl->F, RB_SELECT_WRITE, mod_write_ctl, ctl);
}

static void
mod_cmd_write_queue(mod_ctl_t * ctl, const void *data, size_t len)
{
	mod_ctl_buf_t *ctl_buf;
	ctl_buf = rb_malloc(sizeof(mod_ctl_buf_t));
	ctl_buf->buf = rb_malloc(len);
	ctl_buf->buflen = len;
	memcpy(ctl_buf->buf, data, len);
	ctl_buf->nfds = 0;
	rb_dlinkAddTail(ctl_buf, &ctl_buf->node, &ctl->writeq);
	mod_write_ctl(ctl->F, ctl);
}

static void
close_conn(conn_t * conn, int wait_plain, const char *fmt, ...)
{
	va_list ap;
	char reason[128];	/* must always be under 250 bytes */
	uint8_t buf[256];
	int len;
	if(IsDead(conn))
		return;

	rb_rawbuf_flush(conn->modbuf_out, conn->mod_fd);
	rb_rawbuf_flush(conn->plainbuf_out, conn->plain_fd);
	rb_close(conn->mod_fd);
	SetDead(conn);

	rb_dlinkDelete(&conn->node, connid_hash(conn->id));

	if(!wait_plain || fmt == NULL)
	{
		rb_close(conn->plain_fd);
		rb_dlinkAdd(conn, &conn->node, &dead_list);
		return;
	}

	rb_setselect(conn->plain_fd, RB_SELECT_READ, conn_plain_read_shutdown_cb, conn);
	rb_setselect(conn->plain_fd, RB_SELECT_WRITE, NULL, NULL);

	va_start(ap, fmt);
	vsnprintf(reason, sizeof(reason), fmt, ap);
	va_end(ap);

	buf[0] = 'D';
	uint32_to_buf(&buf[1], conn->id);
	rb_strlcpy((char *) &buf[5], reason, sizeof(buf) - 5);
	len = (strlen(reason) + 1) + 5;
	mod_cmd_write_queue(conn->ctl, buf, len);
}

static conn_t *
make_conn(mod_ctl_t * ctl, rb_fde_t *mod_fd, rb_fde_t *plain_fd)
{
	conn_t *conn = rb_malloc(sizeof(conn_t));
	conn->ctl = ctl;
	conn->modbuf_out = rb_new_rawbuffer();
	conn->plainbuf_out = rb_new_rawbuffer();
	conn->mod_fd = mod_fd;
	conn->plain_fd = plain_fd;
	conn->id = -1;
	rb_set_nb(mod_fd);
	rb_set_nb(plain_fd);
	return conn;
}

static void
cleanup_bad_message(mod_ctl_t * ctl, mod_ctl_buf_t * ctlb)
{
	int i;

	/* XXX should log this somehow */
	for (i = 0; i < ctlb->nfds; i++)
		rb_close(ctlb->F[i]);
}

static void
conn_mod_handshake_cb(rb_fde_t *fd, void *data)
{
	char inbuf[READBUF_SIZE];
	conn_t *conn = data;
	int length = 0;
	if (conn == NULL)
		return;

	if (IsDead(conn))
		return;

	while (1)
	{
		if (IsDead(conn))
			return;

		length = rb_read(conn->plain_fd, inbuf, sizeof(inbuf));
		if (length == 0 || (length < 0 && !rb_ignore_errno(errno)))
		{
			close_conn(conn, NO_WAIT, "Connection closed");
			return;
		}
	}
}

static void
conn_mod_read_cb(rb_fde_t *fd, void *data)
{
}

static void
conn_plain_read_cb(rb_fde_t *fd, void *data)
{
}

static void
conn_plain_read_shutdown_cb(rb_fde_t *fd, void *data)
{
	char inbuf[READBUF_SIZE];
	conn_t *conn = data;
	int length = 0;

	if(conn == NULL)
		return;

	while(1)
	{
		length = rb_read(conn->plain_fd, inbuf, sizeof(inbuf));

		if(length == 0 || (length < 0 && !rb_ignore_errno(errno)))
		{
			rb_close(conn->plain_fd);
			rb_dlinkAdd(conn, &conn->node, &dead_list);
			return;
		}

		if(length < 0)
		{
			rb_setselect(conn->plain_fd, RB_SELECT_READ, conn_plain_read_shutdown_cb, conn);
			return;
		}
	}
}

static void
wsock_process(mod_ctl_t * ctl, mod_ctl_buf_t * ctlb)
{
	conn_t *conn;
	uint32_t id;

	conn = make_conn(ctl, ctlb->F[0], ctlb->F[1]);

	id = buf_to_uint32(&ctlb->buf[1]);
	conn_add_id_hash(conn, id);
	SetWS(conn);

	if(rb_get_type(conn->mod_fd) & RB_FD_UNKNOWN)
		rb_set_type(conn->mod_fd, RB_FD_SOCKET);

	if(rb_get_type(conn->plain_fd) == RB_FD_UNKNOWN)
		rb_set_type(conn->plain_fd, RB_FD_SOCKET);

	conn_mod_handshake_cb(conn->mod_fd, conn);
}

static void
mod_process_cmd_recv(mod_ctl_t * ctl)
{
	rb_dlink_node *ptr, *next;
	mod_ctl_buf_t *ctl_buf;

	RB_DLINK_FOREACH_SAFE(ptr, next, ctl->readq.head)
	{
		ctl_buf = ptr->data;

		switch (*ctl_buf->buf)
		{
		case 'A':
			{
				if (ctl_buf->nfds != 2 || ctl_buf->buflen != 5)
				{
					cleanup_bad_message(ctl, ctl_buf);
					break;
				}
				wsock_process(ctl, ctl_buf);
				break;
			}
		default:
			break;
			/* Log unknown commands */
		}
		rb_dlinkDelete(ptr, &ctl->readq);
		rb_free(ctl_buf->buf);
		rb_free(ctl_buf);
	}

}

static void
mod_read_ctl(rb_fde_t *F, void *data)
{
	mod_ctl_buf_t *ctl_buf;
	mod_ctl_t *ctl = data;
	int retlen;
	int i;

	do
	{
		ctl_buf = rb_malloc(sizeof(mod_ctl_buf_t));
		ctl_buf->buf = rb_malloc(READBUF_SIZE);
		ctl_buf->buflen = READBUF_SIZE;
		retlen = rb_recv_fd_buf(ctl->F, ctl_buf->buf, ctl_buf->buflen, ctl_buf->F,
					MAXPASSFD);
		if(retlen <= 0)
		{
			rb_free(ctl_buf->buf);
			rb_free(ctl_buf);
		}
		else
		{
			ctl_buf->buflen = retlen;
			rb_dlinkAddTail(ctl_buf, &ctl_buf->node, &ctl->readq);
			for (i = 0; i < MAXPASSFD && ctl_buf->F[i] != NULL; i++)
				;
			ctl_buf->nfds = i;
		}
	}
	while(retlen > 0);

	if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
		exit(0);

	mod_process_cmd_recv(ctl);
	rb_setselect(ctl->F, RB_SELECT_READ, mod_read_ctl, ctl);
}

static void
read_pipe_ctl(rb_fde_t *F, void *data)
{
	char inbuf[READBUF_SIZE];
	int retlen;
	while((retlen = rb_read(F, inbuf, sizeof(inbuf))) > 0)
	{
		;;		/* we don't do anything with the pipe really, just care if the other process dies.. */
	}
	if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
		exit(0);
	rb_setselect(F, RB_SELECT_READ, read_pipe_ctl, NULL);
}

int
main(int argc, char **argv)
{
	const char *s_ctlfd, *s_pipe, *s_pid;
	int ctlfd, pipefd, x, maxfd;
	maxfd = maxconn();

	s_ctlfd = getenv("CTL_FD");
	s_pipe = getenv("CTL_PIPE");
	s_pid = getenv("CTL_PPID");

	if(s_ctlfd == NULL || s_pipe == NULL || s_pid == NULL)
	{
		fprintf(stderr,
			"This is the charybdis wsockd for internal ircd use.\n");
		fprintf(stderr,
			"You aren't supposed to run me directly. Exiting.\n");
		exit(1);
	}

	ctlfd = atoi(s_ctlfd);
	pipefd = atoi(s_pipe);
	ppid = atoi(s_pid);
	x = 0;
#ifndef _WIN32
	for(x = 0; x < maxfd; x++)
	{
		if(x != ctlfd && x != pipefd && x > 2)
			close(x);
	}
	x = open("/dev/null", O_RDWR);

	if(x >= 0)
	{
		if(ctlfd != 0 && pipefd != 0)
			dup2(x, 0);
		if(ctlfd != 1 && pipefd != 1)
			dup2(x, 1);
		if(ctlfd != 2 && pipefd != 2)
			dup2(x, 2);
		if(x > 2)
			close(x);
	}
#endif
	setup_signals();
	rb_lib_init(NULL, NULL, NULL, 0, maxfd, 1024, 4096);
	rb_init_rawbuffers(1024);

	mod_ctl = rb_malloc(sizeof(mod_ctl_t));
	mod_ctl->F = rb_open(ctlfd, RB_FD_SOCKET, "ircd control socket");
	mod_ctl->F_pipe = rb_open(pipefd, RB_FD_PIPE, "ircd pipe");
	rb_set_nb(mod_ctl->F);
	rb_set_nb(mod_ctl->F_pipe);
	rb_event_addish("clean_dead_conns", clean_dead_conns, NULL, 10);
	read_pipe_ctl(mod_ctl->F_pipe, NULL);
	mod_read_ctl(mod_ctl->F, mod_ctl);

	rb_lib_loop(0);
	return 0;
}
