/************************************************************************
 *   IRC - Internet Relay Chat, servlink/io.c
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id: io.c 1285 2006-05-05 15:03:53Z nenolod $
 */

#include "setup.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#include "servlink.h"
#include "io.h"
#include "control.h"

static int check_error(int, int, int);

static const char *
fd_name(int fd)
{
	if(fd == CONTROL.fd)
		return "control";
	if(fd == LOCAL.fd)
		return "data";
	if(fd == REMOTE.fd)
		return "network";

	/* uh oh... */
	return "unknown";
}

#if defined( HAVE_LIBZ )
static unsigned char tmp_buf[BUFLEN];
static unsigned char tmp2_buf[BUFLEN];
#endif

static unsigned char ctrl_buf[256] = "";
static unsigned int ctrl_len = 0;
static unsigned int ctrl_ofs = 0;

void
io_loop(int nfds)
{
	fd_set rfds;
	fd_set wfds;
	int i, ret;

	/* loop forever */
	for (;;)
	{
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		for (i = 0; i < 3; i++)
		{
			if(fds[i].read_cb)
				FD_SET(fds[i].fd, &rfds);
			if(fds[i].write_cb)
				FD_SET(fds[i].fd, &wfds);
		}

		/* we have <3 fds ever, so I don't think select is too painful */
		ret = select(nfds, &rfds, &wfds, NULL, NULL);

		if(ret < 0)
		{
			check_error(ret, IO_SELECT, -1);	/* exit on fatal errors */
		}
		else if(ret > 0)
		{
			/* call any callbacks */
			for (i = 0; i < 3; i++)
			{
				if(FD_ISSET(fds[i].fd, &rfds) && fds[i].read_cb)
					(*fds[i].read_cb) ();
				if(FD_ISSET(fds[i].fd, &wfds) && fds[i].write_cb)
					(*fds[i].write_cb) ();
			}
		}
	}
}

void
send_data_blocking(int fd, unsigned char *data, int datalen)
{
	int ret;
	fd_set wfds;

	while (1)
	{
		ret = write(fd, data, datalen);

		if(ret == datalen)
			return;
		else if(ret > 0)
		{
			data += ret;
			datalen -= ret;
		}

		ret = check_error(ret, IO_WRITE, fd);

		FD_ZERO(&wfds);
		FD_SET(fd, &wfds);

		/* sleep until we can write to the fd */
		while (1)
		{
			ret = select(fd + 1, NULL, &wfds, NULL, NULL);

			if(ret > 0)	/* break out so we can write */
				break;

			if(ret < 0)	/* error ? */
				check_error(ret, IO_SELECT, fd);	/* exit on fatal errors */

			/* loop on non-fatal errors */
		}
	}
}

/*
 * process_sendq:
 * 
 * used before CMD_INIT to pass contents of SendQ from ircd
 * to servlink.  This data must _not_ be encrypted/compressed.
 */
void
process_sendq(struct ctrl_command *cmd)
{
	send_data_blocking(REMOTE.fd, cmd->data, cmd->datalen);
}

/*
 * process_recvq:
 *
 * used before CMD_INIT to pass contents of RecvQ from ircd
 * to servlink.  This data must be decrypted/decopmressed before
 * sending back to the ircd.
 */
void
process_recvq(struct ctrl_command *cmd)
{
	int ret;
	unsigned char *buf;
	int blen;
	unsigned char *data = cmd->data;
	unsigned int datalen = cmd->datalen;

	buf = data;
	blen = datalen;
	ret = -1;
	if(datalen > READLEN)
		send_error("Error processing INJECT_RECVQ - buffer too long (%d > %d)",
			   datalen, READLEN);

#ifdef HAVE_LIBZ
	if(in_state.zip)
	{
		/* decompress data */
		in_state.zip_state.z_stream.next_in = buf;
		in_state.zip_state.z_stream.avail_in = blen;
		in_state.zip_state.z_stream.next_out = tmp2_buf;
		in_state.zip_state.z_stream.avail_out = BUFLEN;

		buf = tmp2_buf;
		while (in_state.zip_state.z_stream.avail_in)
		{
			if((ret = inflate(&in_state.zip_state.z_stream, Z_NO_FLUSH)) != Z_OK)
			{
				if(!strncmp("ERROR ", (char *)in_state.zip_state.z_stream.next_in, 6))
					send_error("Received uncompressed ERROR");
				else
					send_error("Inflate failed: %s", zError(ret));
			}
			blen = BUFLEN - in_state.zip_state.z_stream.avail_out;

			if(in_state.zip_state.z_stream.avail_in)
			{
				send_data_blocking(LOCAL.fd, buf, blen);
				blen = 0;
				in_state.zip_state.z_stream.next_out = buf;
				in_state.zip_state.z_stream.avail_out = BUFLEN;
			}
		}

		if(!blen)
			return;
	}
#endif

	send_data_blocking(LOCAL.fd, buf, blen);
}

void
send_zipstats(struct ctrl_command *unused)
{
#ifdef HAVE_LIBZ
	int i = 0;
	int ret;
	u_int32_t len;
	if(!in_state.active || !out_state.active)
		send_error("Error processing CMD_ZIPSTATS - link is not active!");
	if(!in_state.zip || !out_state.zip)
		send_error("Error processing CMD_ZIPSTATS - link is not compressed!");

	ctrl_buf[i++] = RPL_ZIPSTATS;
	ctrl_buf[i++] = 0;
	ctrl_buf[i++] = 16;

	len = (u_int32_t) in_state.zip_state.z_stream.total_out;
	ctrl_buf[i++] = ((len >> 24) & 0xFF);
	ctrl_buf[i++] = ((len >> 16) & 0xFF);
	ctrl_buf[i++] = ((len >> 8) & 0xFF);
	ctrl_buf[i++] = ((len) & 0xFF);

	len = (u_int32_t) in_state.zip_state.z_stream.total_in;
	ctrl_buf[i++] = ((len >> 24) & 0xFF);
	ctrl_buf[i++] = ((len >> 16) & 0xFF);
	ctrl_buf[i++] = ((len >> 8) & 0xFF);
	ctrl_buf[i++] = ((len) & 0xFF);

	len = (u_int32_t) out_state.zip_state.z_stream.total_in;
	ctrl_buf[i++] = ((len >> 24) & 0xFF);
	ctrl_buf[i++] = ((len >> 16) & 0xFF);
	ctrl_buf[i++] = ((len >> 8) & 0xFF);
	ctrl_buf[i++] = ((len) & 0xFF);

	len = (u_int32_t) out_state.zip_state.z_stream.total_out;
	ctrl_buf[i++] = ((len >> 24) & 0xFF);
	ctrl_buf[i++] = ((len >> 16) & 0xFF);
	ctrl_buf[i++] = ((len >> 8) & 0xFF);
	ctrl_buf[i++] = ((len) & 0xFF);

	in_state.zip_state.z_stream.total_in = 0;
	in_state.zip_state.z_stream.total_out = 0;
	out_state.zip_state.z_stream.total_in = 0;
	out_state.zip_state.z_stream.total_out = 0;

	ret = check_error(write(CONTROL.fd, ctrl_buf, i), IO_WRITE, CONTROL.fd);
	if(ret < i)
	{
		/* write incomplete, register write cb */
		CONTROL.write_cb = write_ctrl;
		/*  deregister read_cb */
		CONTROL.read_cb = NULL;
		ctrl_ofs = ret;
		ctrl_len = i - ret;
		return;
	}
#else
	send_error("can't send_zipstats -- no zlib support!");
#endif
}

/* send_error
 *   - we ran into some problem, make a last ditch effort to 
 *     flush the control fd sendq, then (blocking) send an
 *     error message over the control fd.
 */
void
send_error(const char *message, ...)
{
	va_list args;
	static int sending_error = 0;
	struct linger linger_opt = { 1, 30 };	/* wait 30 seconds */
	int len;

	if(sending_error)
		exit(1);	/* we did _try_ */

	sending_error = 1;

	if(ctrl_len)		/* attempt to flush any data we have... */
	{
		send_data_blocking(CONTROL.fd, (ctrl_buf + ctrl_ofs), ctrl_len);
	}

	/* prepare the message, in in_buf, since we won't be using it again.. */
	in_state.buf[0] = RPL_ERROR;
	in_state.buf[1] = 0;
	in_state.buf[2] = 0;

	va_start(args, message);
	len = vsprintf((char *) in_state.buf + 3, message, args);
	va_end(args);

	in_state.buf[3 + len++] = '\0';
	in_state.buf[1] = len >> 8;
	in_state.buf[2] = len & 0xFF;
	len += 3;

	send_data_blocking(CONTROL.fd, in_state.buf, len);

	/* XXX - is this portable?
	 *       this obviously will fail on a non socket.. */
	setsockopt(CONTROL.fd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(struct linger));

	/* well, we've tried... */
	exit(1);		/* now abort */
}

/* read_ctrl
 *      called when a command is waiting on the control pipe
 */
void
read_ctrl(void)
{
	int ret;
	unsigned char tmp[2];
	unsigned char *len;
	struct command_def *cdef;
	static struct ctrl_command cmd = { 0, 0, 0, 0, NULL };

	if(cmd.command == 0)	/* we don't have a command yet */
	{
		cmd.gotdatalen = 0;
		cmd.datalen = 0;
		cmd.readdata = 0;
		cmd.data = NULL;

		/* read the command */
		if(!(ret = check_error(read(CONTROL.fd, tmp, 1), IO_READ, CONTROL.fd)))
			return;

		cmd.command = tmp[0];
	}

	for (cdef = command_table; cdef->commandid; cdef++)
	{
		if((int)cdef->commandid == cmd.command)
			break;
	}

	if(!cdef->commandid)
	{
		send_error("Unsupported command (servlink/ircd out of sync?): %d", cmd.command);
		/* NOTREACHED */
	}

	/* read datalen for commands including data */
	if(cdef->flags & COMMAND_FLAG_DATA)
	{
		if(cmd.gotdatalen < 2)
		{
			len = tmp;
			if(!(ret = check_error(read(CONTROL.fd, len,
						    (2 - cmd.gotdatalen)), IO_READ, CONTROL.fd)))
				return;

			if(cmd.gotdatalen == 0)
			{
				cmd.datalen = len[0] << 8;
				cmd.gotdatalen++;
				ret--;
				len++;
			}
			if(ret && (cmd.gotdatalen == 1))
			{
				cmd.datalen |= len[0];
				cmd.gotdatalen++;
				if(cmd.datalen > 0)
					cmd.data = calloc(cmd.datalen, 1);
			}
		}
	}

	if(cmd.readdata < cmd.datalen)	/* try to get any remaining data */
	{
		if(!(ret = check_error(read(CONTROL.fd,
					    (cmd.data + cmd.readdata),
					    cmd.datalen - cmd.readdata), IO_READ, CONTROL.fd)))
			return;

		cmd.readdata += ret;
		if(cmd.readdata < cmd.datalen)
			return;
	}

	/* we now have the command and any data */
	(*cdef->handler) (&cmd);

	if(cmd.datalen > 0)
		free(cmd.data);
	cmd.command = 0;
}

void
write_ctrl(void)
{
	int ret;

	assert(ctrl_len);

	if(!(ret = check_error(write(CONTROL.fd, (ctrl_buf + ctrl_ofs),
				     ctrl_len), IO_WRITE, CONTROL.fd)))
		return;		/* no data waiting */

	ctrl_len -= ret;

	if(!ctrl_len)
	{
		/* write completed, de-register write cb */
		CONTROL.write_cb = NULL;
		/* reregister read_cb */
		CONTROL.read_cb = read_ctrl;
		ctrl_ofs = 0;
	}
	else
		ctrl_ofs += ret;
}

void
read_data(void)
{
	int ret, ret2;
	unsigned char *buf = out_state.buf;
	int blen;
	ret2 = -1;
	assert(!out_state.len);

#if defined(HAVE_LIBZ) 
	if(out_state.zip || out_state.crypt)
		buf = tmp_buf;
#endif

	while ((ret = check_error(read(LOCAL.fd, buf, READLEN), IO_READ, LOCAL.fd)))
	{
		blen = ret;
#ifdef HAVE_LIBZ
		if(out_state.zip)
		{
			out_state.zip_state.z_stream.next_in = buf;
			out_state.zip_state.z_stream.avail_in = ret;

			buf = out_state.buf;
			out_state.zip_state.z_stream.next_out = buf;
			out_state.zip_state.z_stream.avail_out = BUFLEN;
			if(!(ret2 = deflate(&out_state.zip_state.z_stream,
					    Z_PARTIAL_FLUSH)) == Z_OK)
				send_error("error compressing outgoing data - deflate returned: %s",
					   zError(ret2));

			if(!out_state.zip_state.z_stream.avail_out)
				send_error("error compressing outgoing data - avail_out == 0");
			if(out_state.zip_state.z_stream.avail_in)
				send_error("error compressing outgoing data - avail_in != 0");

			blen = BUFLEN - out_state.zip_state.z_stream.avail_out;
		}
#endif


		ret = check_error(write(REMOTE.fd, out_state.buf, blen), IO_WRITE, REMOTE.fd);
		if(ret < blen)
		{
			/* write incomplete, register write cb */
			REMOTE.write_cb = write_net;
			/*  deregister read_cb */
			LOCAL.read_cb = NULL;
			out_state.ofs = ret;
			out_state.len = blen - ret;
			return;
		}
#if defined(HAVE_LIBZ) 
		if(out_state.zip)
			buf = tmp_buf;
#endif
	}

}

void
write_net(void)
{
	int ret;

	assert(out_state.len);

	if(!(ret = check_error(write(REMOTE.fd,
				     (out_state.buf + out_state.ofs),
				     out_state.len), IO_WRITE, REMOTE.fd)))
		return;		/* no data waiting */

	out_state.len -= ret;

	if(!out_state.len)
	{
		/* write completed, de-register write cb */
		REMOTE.write_cb = NULL;
		/* reregister read_cb */
		LOCAL.read_cb = read_data;
		out_state.ofs = 0;
	}
	else
		out_state.ofs += ret;
}

void
read_net(void)
{
	int ret;
	int ret2;
	unsigned char *buf = in_state.buf;
	int blen;
	ret2 = -1;
	assert(!in_state.len);

#if defined(HAVE_LIBZ)
	if(in_state.zip)
		buf = tmp_buf;
#endif

	while ((ret = check_error(read(REMOTE.fd, buf, READLEN), IO_READ, REMOTE.fd)))
	{
		blen = ret;
#ifdef HAVE_LIBZ
		if(in_state.zip)
		{
			/* decompress data */
			in_state.zip_state.z_stream.next_in = buf;
			in_state.zip_state.z_stream.avail_in = ret;
			in_state.zip_state.z_stream.next_out = in_state.buf;
			in_state.zip_state.z_stream.avail_out = BUFLEN;

			while (in_state.zip_state.z_stream.avail_in)
			{
				if((ret2 = inflate(&in_state.zip_state.z_stream,
						   Z_NO_FLUSH)) != Z_OK)
				{
					if(!strncmp("ERROR ", (char *)buf, 6))
						send_error("Received uncompressed ERROR");
					send_error("Inflate failed: %s", zError(ret2));
				}
				blen = BUFLEN - in_state.zip_state.z_stream.avail_out;

				if(in_state.zip_state.z_stream.avail_in)
				{
					if(blen)
					{
						send_data_blocking(LOCAL.fd, in_state.buf, blen);
						blen = 0;
					}

					in_state.zip_state.z_stream.next_out = in_state.buf;
					in_state.zip_state.z_stream.avail_out = BUFLEN;
				}
			}

			if(!blen)
				return;	/* that didn't generate any decompressed input.. */
		}
#endif

		ret = check_error(write(LOCAL.fd, in_state.buf, blen), IO_WRITE, LOCAL.fd);

		if(ret < blen)
		{
			in_state.ofs = ret;
			in_state.len = blen - ret;
			/* write incomplete, register write cb */
			LOCAL.write_cb = write_data;
			/* deregister read_cb */
			REMOTE.read_cb = NULL;
			return;
		}
#if defined(HAVE_LIBZ)
		if(in_state.zip)
			buf = tmp_buf;
#endif
	}
}

void
write_data(void)
{
	int ret;

	assert(in_state.len);

	if(!(ret = check_error(write(LOCAL.fd,
				     (in_state.buf + in_state.ofs),
				     in_state.len), IO_WRITE, LOCAL.fd)))
		return;

	in_state.len -= ret;

	if(!in_state.len)
	{
		/* write completed, de-register write cb */
		LOCAL.write_cb = NULL;
		/* reregister read_cb */
		REMOTE.read_cb = read_net;
		in_state.ofs = 0;
	}
	else
		in_state.ofs += ret;
}

int
check_error(int ret, int io, int fd)
{
	if(ret > 0)		/* no error */
		return ret;
	if(ret == 0)		/* EOF */
	{
		send_error("%s failed on %s: EOF", IO_TYPE(io), FD_NAME(fd));
		exit(1);	/* NOTREACHED */
	}

	/* ret == -1.. */
	switch (errno)
	{
	case EINPROGRESS:
	case EWOULDBLOCK:
#if EAGAIN != EWOULDBLOCK
	case EAGAIN:
#endif
	case EALREADY:
	case EINTR:
#ifdef ERESTART
	case ERESTART:
#endif
		/* non-fatal error, 0 bytes read */
		return 0;
	}

	/* fatal error */
	send_error("%s failed on %s: %s", IO_TYPE(io), FD_NAME(fd), strerror(errno));
	exit(1);		/* NOTREACHED */
}
