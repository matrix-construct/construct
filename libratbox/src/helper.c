/*
 *  ircd-ratbox: A slightly useful ircd
 *  helper.c: Starts and deals with ircd helpers
 *  
 *  Copyright (C) 2006 Aaron Sethman <androsyn@ratbox.org>
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
 *  $Id: helper.c 26092 2008-09-19 15:13:52Z androsyn $
 */
#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>

struct _rb_helper
{
	char *path;
	buf_head_t sendq;
	buf_head_t recvq;
	rb_fde_t *ifd;
	rb_fde_t *ofd;
	pid_t pid;
	int fork_count;
	rb_helper_cb *read_cb;
	rb_helper_cb *error_cb;
};


/* setup all the stuff a new child needs */
rb_helper *
rb_helper_child(rb_helper_cb * read_cb, rb_helper_cb * error_cb, log_cb * ilog,
		restart_cb * irestart, die_cb * idie, int maxcon, size_t lb_heap_size,
		size_t dh_size, size_t fd_heap_size)
{
	rb_helper *helper;
	int maxfd, x = 0;
	int ifd, ofd;
	char *tifd, *tofd, *tmaxfd;

	tifd = getenv("IFD");
	tofd = getenv("OFD");
	tmaxfd = getenv("MAXFD");

	if(tifd == NULL || tofd == NULL || tmaxfd == NULL)
		return NULL;

	helper = rb_malloc(sizeof(rb_helper));
	ifd = (int)strtol(tifd, NULL, 10);
	ofd = (int)strtol(tofd, NULL, 10);
	maxfd = (int)strtol(tmaxfd, NULL, 10);

#ifndef _WIN32
	for(x = 0; x < maxfd; x++)
	{
		if(x != ifd && x != ofd)
			close(x);
	}
	x = open("/dev/null", O_RDWR);
	if(ifd != 0 && ofd != 0)
		dup2(x, 0);
	if(ifd != 1 && ofd != 1)
		dup2(x, 1);
	if(ifd != 2 && ofd != 2)
		dup2(x, 2);
	if(x > 2)		/* don't undo what we just did */
		close(x);
#else
	x = 0;			/* shut gcc up */
#endif

	rb_lib_init(ilog, irestart, idie, 0, maxfd, dh_size, fd_heap_size);
	rb_linebuf_init(lb_heap_size);
	rb_linebuf_newbuf(&helper->sendq);
	rb_linebuf_newbuf(&helper->recvq);

	helper->ifd = rb_open(ifd, RB_FD_PIPE, "incoming connection");
	helper->ofd = rb_open(ofd, RB_FD_PIPE, "outgoing connection");
	rb_set_nb(helper->ifd);
	rb_set_nb(helper->ofd);

	helper->read_cb = read_cb;
	helper->error_cb = error_cb;
	return helper;
}

/*
 * start_fork_helper
 * starts a new ircd helper
 * note that this function doesn't start doing reading..thats the job of the caller
 */

rb_helper *
rb_helper_start(const char *name, const char *fullpath, rb_helper_cb * read_cb,
		rb_helper_cb * error_cb)
{
	rb_helper *helper;
	const char *parv[2];
	char buf[128];
	char fx[16], fy[16];
	rb_fde_t *in_f[2];
	rb_fde_t *out_f[2];
	pid_t pid;

	if(access(fullpath, X_OK) == -1)
		return NULL;

	helper = rb_malloc(sizeof(rb_helper));

	rb_snprintf(buf, sizeof(buf), "%s helper - read", name);
	if(rb_pipe(&in_f[0], &in_f[1], buf) < 0)
	{
		rb_free(helper);
		return NULL;
	}
	rb_snprintf(buf, sizeof(buf), "%s helper - write", name);
	if(rb_pipe(&out_f[0], &out_f[1], buf) < 0)
	{
		rb_free(helper);
		return NULL;
	}

	rb_snprintf(fx, sizeof(fx), "%d", rb_get_fd(in_f[1]));
	rb_snprintf(fy, sizeof(fy), "%d", rb_get_fd(out_f[0]));

	rb_set_nb(in_f[0]);
	rb_set_nb(in_f[1]);
	rb_set_nb(out_f[0]);
	rb_set_nb(out_f[1]);

	rb_setenv("IFD", fy, 1);
	rb_setenv("OFD", fx, 1);
	rb_setenv("MAXFD", "256", 1);

	rb_snprintf(buf, sizeof(buf), "-ircd %s daemon", name);
	parv[0] = buf;
	parv[1] = NULL;

#ifdef _WIN32
	SetHandleInformation((HANDLE) rb_get_fd(in_f[1]), HANDLE_FLAG_INHERIT, 1);
	SetHandleInformation((HANDLE) rb_get_fd(out_f[0]), HANDLE_FLAG_INHERIT, 1);
#endif

	pid = rb_spawn_process(fullpath, (const char **)parv);

	if(pid == -1)
	{
		rb_close(in_f[0]);
		rb_close(in_f[1]);
		rb_close(out_f[0]);
		rb_close(out_f[1]);
		rb_free(helper);
		return NULL;
	}

	rb_close(in_f[1]);
	rb_close(out_f[0]);

	rb_linebuf_newbuf(&helper->sendq);
	rb_linebuf_newbuf(&helper->recvq);

	helper->ifd = in_f[0];
	helper->ofd = out_f[1];
	helper->read_cb = read_cb;
	helper->error_cb = error_cb;
	helper->fork_count = 0;
	helper->pid = pid;

	return helper;
}


void
rb_helper_restart(rb_helper *helper)
{
	helper->error_cb(helper);
}


static void
rb_helper_write_sendq(rb_fde_t *F, void *helper_ptr)
{
	rb_helper *helper = helper_ptr;
	int retlen;

	if(rb_linebuf_len(&helper->sendq) > 0)
	{
		while((retlen = rb_linebuf_flush(F, &helper->sendq)) > 0)
			;;
		if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
		{
			rb_helper_restart(helper);
			return;
		}
	}

	if(rb_linebuf_len(&helper->sendq) > 0)
		rb_setselect(helper->ofd, RB_SELECT_WRITE, rb_helper_write_sendq, helper);
}

void
rb_helper_write_queue(rb_helper *helper, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	rb_linebuf_putmsg(&helper->sendq, format, &ap, NULL);
	va_end(ap);
}

void
rb_helper_write_flush(rb_helper *helper)
{
	rb_helper_write_sendq(helper->ofd, helper);
}


void
rb_helper_write(rb_helper *helper, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	rb_linebuf_putmsg(&helper->sendq, format, &ap, NULL);
	va_end(ap);
	rb_helper_write_flush(helper);
}

static void
rb_helper_read_cb(rb_fde_t *F, void *data)
{
	rb_helper *helper = (rb_helper *)data;
	static char buf[32768];
	int length;
	if(helper == NULL)
		return;

	while((length = rb_read(helper->ifd, buf, sizeof(buf))) > 0)
	{
		rb_linebuf_parse(&helper->recvq, buf, length, 0);
		helper->read_cb(helper);
	}

	if(length == 0 || (length < 0 && !rb_ignore_errno(errno)))
	{
		rb_helper_restart(helper);
		return;
	}

	rb_setselect(helper->ifd, RB_SELECT_READ, rb_helper_read_cb, helper);
}

void
rb_helper_run(rb_helper *helper)
{
	if(helper == NULL)
		return;
	rb_helper_read_cb(helper->ifd, helper);
}


void
rb_helper_close(rb_helper *helper)
{
	if(helper == NULL)
		return;
	rb_kill(helper->pid, SIGKILL);
	rb_close(helper->ifd);
	rb_close(helper->ofd);
	rb_free(helper);
}

int
rb_helper_read(rb_helper *helper, void *buf, size_t bufsize)
{
	return rb_linebuf_get(&helper->recvq, buf, bufsize, LINEBUF_COMPLETE, LINEBUF_PARSED);
}

void
rb_helper_loop(rb_helper *helper, long delay)
{
	rb_helper_run(helper);
	while(1)
	{
		rb_lib_loop(delay);
	}
}
