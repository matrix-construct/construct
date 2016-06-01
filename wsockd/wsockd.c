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
#include "sha1.h"

#define MAXPASSFD 4
#ifndef READBUF_SIZE
#define READBUF_SIZE 16384
#endif

#define WEBSOCKET_SERVER_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCKET_ANSWER_STRING_1 "HTTP/1.1 101 Switching Protocols\r\nAccess-Control-Allow-Origin: *\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
#define WEBSOCKET_ANSWER_STRING_2 "\r\n\r\n"

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
	rawbuf_head_t *modbuf_in;

	buf_head_t plainbuf_out;
	buf_head_t plainbuf_in;

	uint32_t id;

	rb_fde_t *mod_fd;
	rb_fde_t *plain_fd;
	uint64_t mod_out;
	uint64_t mod_in;
	uint64_t plain_in;
	uint64_t plain_out;
	uint8_t flags;

	char client_key[37];		/* maximum 36 bytes + nul */
} conn_t;

#define WEBSOCKET_OPCODE_TEXT_FRAME          1

#define WEBSOCKET_MASK_LENGTH 4

#define WEBSOCKET_MAX_UNEXTENDED_PAYLOAD_DATA_LENGTH 125

typedef struct {
	uint8_t opcode_rsv_fin; // opcode: 4, rsv1: 1, rsv2: 1, rsv3: 1, fin: 1
	uint8_t payload_length_mask; // payload_length: 7, mask: 1
} ws_frame_hdr_t;

typedef struct {
	ws_frame_hdr_t header;
	uint8_t payload_data[WEBSOCKET_MAX_UNEXTENDED_PAYLOAD_DATA_LENGTH];
} ws_frame_payload_t;

typedef struct {
	ws_frame_hdr_t header;
} ws_frame_t;

typedef struct {
	ws_frame_hdr_t header;
	uint16_t payload_length_extended;
} ws_frame_ext_t;

typedef struct {
	ws_frame_hdr_t header;
	uint64_t payload_length_extended;
} ws_frame_ext2_t;

static inline void
ws_frame_set_opcode(ws_frame_hdr_t *header, int opcode)
{
	header->opcode_rsv_fin &= ~0xF;
	header->opcode_rsv_fin |= opcode & 0xF;
}

static inline void
ws_frame_set_fin(ws_frame_hdr_t *header, int fin)
{
	header->opcode_rsv_fin &= ~(0x1 << 7);
	header->opcode_rsv_fin |= (fin << 7) & (0x1 << 7);
}

static void close_conn(conn_t * conn, int wait_plain, const char *fmt, ...);
static void conn_mod_read_cb(rb_fde_t *fd, void *data);
static void conn_plain_read_cb(rb_fde_t *fd, void *data);
static void conn_plain_process_recvq(conn_t *conn);

#define FLAG_CORK	0x01
#define FLAG_DEAD	0x02
#define FLAG_WSOCK	0x04
#define FLAG_KEYED	0x08

#define IsCork(x) ((x)->flags & FLAG_CORK)
#define IsDead(x) ((x)->flags & FLAG_DEAD)
#define IsKeyed(x) ((x)->flags & FLAG_KEYED)

#define SetCork(x) ((x)->flags |= FLAG_CORK)
#define SetDead(x) ((x)->flags |= FLAG_DEAD)
#define SetWS(x)   ((x)->flags |= FLAG_WSOCK)
#define SetKeyed(x) ((x)->flags |= FLAG_KEYED)

#define ClearCork(x) ((x)->flags &= ~FLAG_CORK)

#define NO_WAIT 0x0
#define WAIT_PLAIN 0x1

#define CONN_HASH_SIZE 2000
#define connid_hash(x)	(&connid_hash_table[(x % CONN_HASH_SIZE)])

static const char *remote_closed = "Remote host closed the connection";

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

static void
conn_add_id_hash(conn_t * conn, uint32_t id)
{
	conn->id = id;
	rb_dlinkAdd(conn, &conn->node, connid_hash(id));
}

static void
free_conn(conn_t * conn)
{
	rb_linebuf_donebuf(&conn->plainbuf_in);
	rb_linebuf_donebuf(&conn->plainbuf_out);

	rb_free_rawbuffer(conn->modbuf_in);
	rb_free_rawbuffer(conn->modbuf_out);

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
conn_plain_write_sendq(rb_fde_t *fd, void *data)
{
	conn_t *conn = data;
	int retlen;

	if(IsDead(conn))
		return;

	while((retlen = rb_linebuf_flush(fd, &conn->plainbuf_out)) > 0)
		conn->plain_out += retlen;

	if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
	{
		close_conn(data, NO_WAIT, NULL);
		return;
	}

	if(rb_linebuf_alloclen(&conn->plainbuf_out) > 0)
		rb_setselect(conn->plain_fd, RB_SELECT_WRITE, conn_plain_write_sendq, conn);
	else
		rb_setselect(conn->plain_fd, RB_SELECT_WRITE, NULL, NULL);
}

static void
conn_mod_write_sendq(rb_fde_t *fd, void *data)
{
	conn_t *conn = data;
	const char *err;
	int retlen;

	if(IsDead(conn))
		return;

	while((retlen = rb_rawbuf_flush(conn->modbuf_out, fd)) > 0)
		conn->mod_out += retlen;

	if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
	{
		if(retlen == 0)
			close_conn(conn, WAIT_PLAIN, "%s", remote_closed);
		err = strerror(errno);
		close_conn(conn, WAIT_PLAIN, "Write error: %s", err);
		return;
	}

	if(rb_rawbuf_length(conn->modbuf_out) > 0)
		rb_setselect(conn->mod_fd, RB_SELECT_WRITE, conn_mod_write_sendq, conn);
	else
		rb_setselect(conn->mod_fd, RB_SELECT_WRITE, NULL, NULL);

	if(IsCork(conn) && rb_rawbuf_length(conn->modbuf_out) == 0)
	{
		ClearCork(conn);
		conn_plain_read_cb(conn->plain_fd, conn);
	}
}

static void
conn_mod_write(conn_t * conn, void *data, size_t len)
{
	if(IsDead(conn))	/* no point in queueing to a dead man */
		return;
	rb_rawbuf_append(conn->modbuf_out, data, len);
}

static void
conn_mod_write_short_frame(conn_t * conn, void *data, int len)
{
	ws_frame_hdr_t hdr;

	ws_frame_set_opcode(&hdr, WEBSOCKET_OPCODE_TEXT_FRAME);
	ws_frame_set_fin(&hdr, 1);
	hdr.payload_length_mask = (len + 2) & 0x7f;

	conn_mod_write(conn, &hdr, sizeof(hdr));
	conn_mod_write(conn, data, len);
	conn_mod_write(conn, "\r\n", 2);
}

static void
conn_mod_write_long_frame(conn_t * conn, void *data, int len)
{
	ws_frame_ext_t hdr;

	ws_frame_set_opcode(&hdr.header, WEBSOCKET_OPCODE_TEXT_FRAME);
	ws_frame_set_fin(&hdr.header, 1);
	hdr.header.payload_length_mask = 126;
	hdr.payload_length_extended = htons(len + 2);

	conn_mod_write(conn, &hdr, sizeof(hdr));
	conn_mod_write(conn, data, len);
	conn_mod_write(conn, "\r\n", 2);
}

static void
conn_mod_write_frame(conn_t *conn, void *data, int len)
{
	if(IsDead(conn))	/* no point in queueing to a dead man */
		return;

	if (len < 123)
	{
		conn_mod_write_short_frame(conn, data, len);
		return;
	}

	conn_mod_write_long_frame(conn, data, len);
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

	if (IsKeyed(conn))
		conn_plain_process_recvq(conn);

	rb_rawbuf_flush(conn->modbuf_out, conn->mod_fd);
	rb_linebuf_flush(conn->plain_fd, &conn->plainbuf_out);
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
	conn->mod_fd = mod_fd;
	conn->plain_fd = plain_fd;
	conn->id = -1;
	rb_set_nb(mod_fd);
	rb_set_nb(plain_fd);

	rb_linebuf_newbuf(&conn->plainbuf_in);
	rb_linebuf_newbuf(&conn->plainbuf_out);

	conn->modbuf_in = rb_new_rawbuffer();
	conn->modbuf_out = rb_new_rawbuffer();

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
ws_frame_unmask(char *msg, int length, uint8_t maskval[WEBSOCKET_MASK_LENGTH])
{
	int i;

	for (i = 0; i < length; i++)
		msg[i] = msg[i] ^ maskval[i % 4];
}

static void
conn_mod_process_frame(conn_t *conn, ws_frame_hdr_t *hdr, int masked)
{
	char msg[WEBSOCKET_MAX_UNEXTENDED_PAYLOAD_DATA_LENGTH];
	uint8_t maskval[WEBSOCKET_MASK_LENGTH];
	int dolen;

	/* if we're masked, we get to collect the masking key for this frame */
	if (masked)
	{
		dolen = rb_rawbuf_get(conn->modbuf_in, maskval, sizeof(maskval));
		if (!dolen)
		{
			close_conn(conn, WAIT_PLAIN, "websocket error: fault unpacking unmask key");
			return;
		}
	}

	dolen = rb_rawbuf_get(conn->modbuf_in, msg, hdr->payload_length_mask);
	if (!dolen)
	{
		close_conn(conn, WAIT_PLAIN, "websocket error: fault unpacking message");
		return;
	}

	if (masked)
		ws_frame_unmask(msg, dolen, maskval);

	rb_linebuf_parse(&conn->plainbuf_out, msg, dolen, 1);
}

static void
conn_mod_process_large(conn_t *conn, ws_frame_hdr_t *hdr, int masked)
{
	char msg[READBUF_SIZE];
	uint16_t msglen;
	uint8_t maskval[WEBSOCKET_MASK_LENGTH];
	int dolen;

	memset(msg, 0, sizeof msg);

	dolen = rb_rawbuf_get(conn->modbuf_in, &msglen, sizeof(msglen));
	if (!dolen)
	{
		close_conn(conn, WAIT_PLAIN, "websocket error: fault unpacking message size");
		return;
	}

	msglen = ntohs(msglen);

	if (masked)
	{
		dolen = rb_rawbuf_get(conn->modbuf_in, maskval, sizeof(maskval));
		if (!dolen)
		{
			close_conn(conn, WAIT_PLAIN, "websocket error: fault unpacking unmask key");
			return;
		}
	}

	dolen = rb_rawbuf_get(conn->modbuf_in, msg, msglen);
	if (!dolen)
	{
		close_conn(conn, WAIT_PLAIN, "websocket error: fault unpacking message");
		return;
	}

	if (masked)
		ws_frame_unmask(msg, dolen, maskval);

	rb_linebuf_parse(&conn->plainbuf_out, msg, dolen, 1);
}

static void
conn_mod_process_huge(conn_t *conn, ws_frame_hdr_t *hdr, int masked)
{
	/* XXX implement me */
}

static void
conn_mod_process(conn_t *conn)
{
	ws_frame_hdr_t hdr;

	while (1)
	{
		int masked;
		int dolen = rb_rawbuf_get(conn->modbuf_in, &hdr, sizeof(hdr));
		if (dolen != sizeof(hdr))
			break;

		masked = (hdr.payload_length_mask >> 7) == 1;

		hdr.payload_length_mask &= 0x7f;
		switch (hdr.payload_length_mask)
		{
		case 126:
			conn_mod_process_large(conn, &hdr, masked);
			break;
		case 127:
			conn_mod_process_huge(conn, &hdr, masked);
			break;
		default:
			conn_mod_process_frame(conn, &hdr, masked);
			break;
		}
	}

	conn_plain_write_sendq(conn->plain_fd, conn);
}

static void
conn_mod_handshake_process(conn_t *conn)
{
	char inbuf[READBUF_SIZE];

	memset(inbuf, 0, sizeof inbuf);

	while (1)
	{
		char *p = NULL;

		int dolen = rb_rawbuf_get(conn->modbuf_in, inbuf, sizeof inbuf);
		if (!dolen)
			break;

		if ((p = rb_strcasestr(inbuf, "Sec-WebSocket-Key:")) != NULL)
		{
			char *start, *end;

			start = p + strlen("Sec-WebSocket-Key:");

			for (; start < (inbuf + READBUF_SIZE) && *start; start++)
			{
				if (*start != ' ' && *start != '\t')
					break;
			}

			for (end = start; end < (inbuf + READBUF_SIZE) && *end; end++)
			{
				if (*end == '\r' || *end == '\n')
				{
					*end = '\0';
					break;
				}
			}

			rb_strlcpy(conn->client_key, start, sizeof(conn->client_key));
			SetKeyed(conn);
		}
	}

	if (IsKeyed(conn))
	{
		SHA1 sha1;
		uint8_t digest[SHA1_DIGEST_LENGTH];
		char *resp;

		sha1_init(&sha1);
		sha1_update(&sha1, (uint8_t *) conn->client_key, strlen(conn->client_key));
		sha1_update(&sha1, (uint8_t *) WEBSOCKET_SERVER_KEY, strlen(WEBSOCKET_SERVER_KEY));
		sha1_final(&sha1, digest);

		resp = (char *) rb_base64_encode(digest, SHA1_DIGEST_LENGTH);

		conn_mod_write(conn, WEBSOCKET_ANSWER_STRING_1, strlen(WEBSOCKET_ANSWER_STRING_1));
		conn_mod_write(conn, resp, strlen(resp));
		conn_mod_write(conn, WEBSOCKET_ANSWER_STRING_2, strlen(WEBSOCKET_ANSWER_STRING_2));

		rb_free(resp);
	}

	conn_mod_write_sendq(conn->mod_fd, conn);
}

static void
conn_mod_read_cb(rb_fde_t *fd, void *data)
{
	char inbuf[READBUF_SIZE];

	memset(inbuf, 0, sizeof inbuf);

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

		length = rb_read(fd, inbuf, sizeof(inbuf));

		if (length < 0)
		{
			if (rb_ignore_errno(errno))
			{
				rb_setselect(fd, RB_SELECT_READ, conn_mod_read_cb, conn);
				conn_plain_write_sendq(conn->plain_fd, conn);
			}
			else
				close_conn(conn, NO_WAIT, "Connection closed");

			return;
		}
		else if (length == 0)
		{
			close_conn(conn, NO_WAIT, "Connection closed");
			return;
		}

		rb_rawbuf_append(conn->modbuf_in, inbuf, length);
		if (!IsKeyed(conn))
			conn_mod_handshake_process(conn);
		else
			conn_mod_process(conn);

		if ((size_t) length < sizeof(inbuf))
		{
			rb_setselect(fd, RB_SELECT_READ, conn_mod_read_cb, conn);
			return;
		}
	}
}

static bool
plain_check_cork(conn_t * conn)
{
	if(rb_rawbuf_length(conn->modbuf_out) >= 4096)
	{
		/* if we have over 4k pending outbound, don't read until
		 * we've cleared the queue */
		SetCork(conn);
		rb_setselect(conn->plain_fd, RB_SELECT_READ, NULL, NULL);
		/* try to write */
		if (IsKeyed(conn))
			conn_mod_write_sendq(conn->mod_fd, conn);
		return true;
	}

	return false;
}

static void
conn_plain_process_recvq(conn_t *conn)
{
	char inbuf[READBUF_SIZE];

	memset(inbuf, 0, sizeof inbuf);

	while (1)
	{
		int dolen = rb_linebuf_get(&conn->plainbuf_in, inbuf, sizeof inbuf, LINEBUF_COMPLETE, LINEBUF_PARSED);
		if (!dolen)
			break;

		conn_mod_write_frame(conn, inbuf, dolen);
	}

	if (IsKeyed(conn))
		conn_mod_write_sendq(conn->mod_fd, conn);
}

static void
conn_plain_read_cb(rb_fde_t *fd, void *data)
{
	char inbuf[READBUF_SIZE];

	memset(inbuf, 0, sizeof inbuf);

	conn_t *conn = data;
	int length = 0;
	if(conn == NULL)
		return;

	if(IsDead(conn))
		return;

	if(plain_check_cork(conn))
		return;

	while(1)
	{
		if(IsDead(conn))
			return;

		length = rb_read(conn->plain_fd, inbuf, sizeof(inbuf));

		if(length == 0 || (length < 0 && !rb_ignore_errno(errno)))
		{
			close_conn(conn, NO_WAIT, NULL);
			return;
		}

		if(length < 0)
		{
			rb_setselect(conn->plain_fd, RB_SELECT_READ, conn_plain_read_cb, conn);
			if (IsKeyed(conn))
				conn_plain_process_recvq(conn);
			return;
		}
		conn->plain_in += length;

		(void) rb_linebuf_parse(&conn->plainbuf_in, inbuf, length, 0);

		if(IsDead(conn))
			return;
		if(plain_check_cork(conn))
			return;
	}
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

	conn_mod_read_cb(conn->mod_fd, conn);
	conn_plain_read_cb(conn->plain_fd, conn);
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
	rb_linebuf_init(4096);
	rb_init_rawbuffers(4096);

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
