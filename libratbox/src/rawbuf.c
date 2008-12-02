/*
 *  ircd-ratbox:  A slight useful ircd
 *  rawbuf.c: raw buffer (non-line oriented buffering)
 *  
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2007 ircd-ratbox development team
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
 *  $Id$
 */
#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#define RAWBUF_SIZE 1024

struct _rawbuf
{
	rb_dlink_node node;
	uint8_t data[RAWBUF_SIZE];
	int len;
	uint8_t flushing;
};

struct _rawbuf_head
{
	rb_dlink_list list;
	int len;
	int written;
};

static rb_bh *rawbuf_heap;


static rawbuf_t *
rb_rawbuf_alloc(void)
{
	rawbuf_t *t;
	t = rb_bh_alloc(rawbuf_heap);
	return t;
}

static rawbuf_t *
rb_rawbuf_newbuf(rawbuf_head_t * rb)
{
	rawbuf_t *buf;
	buf = rb_rawbuf_alloc();
	rb_dlinkAddTail(buf, &buf->node, &rb->list);
	return buf;
}

static void
rb_rawbuf_done(rawbuf_head_t * rb, rawbuf_t * buf)
{
	rawbuf_t *ptr = buf;
	rb_dlinkDelete(&buf->node, &rb->list);
	rb_bh_free(rawbuf_heap, ptr);
}

static int
rb_rawbuf_flush_writev(rawbuf_head_t * rb, rb_fde_t *F)
{
	rb_dlink_node *ptr, *next;
	rawbuf_t *buf;
	int x = 0, y = 0;
	int xret, retval;
	struct rb_iovec vec[RB_UIO_MAXIOV];
	memset(vec, 0, sizeof(vec));

	if(rb->list.head == NULL)
	{
		errno = EAGAIN;
		return -1;
	}

	RB_DLINK_FOREACH(ptr, rb->list.head)
	{
		if(x >= RB_UIO_MAXIOV)
			break;

		buf = ptr->data;
		if(buf->flushing)
		{
			vec[x].iov_base = buf->data + rb->written;
			vec[x++].iov_len = buf->len - rb->written;
			continue;
		}
		vec[x].iov_base = buf->data;
		vec[x++].iov_len = buf->len;

	}

	if(x == 0)
	{
		errno = EAGAIN;
		return -1;
	}
	xret = retval = rb_writev(F, vec, x);
	if(retval <= 0)
		return retval;

	RB_DLINK_FOREACH_SAFE(ptr, next, rb->list.head)
	{
		buf = ptr->data;
		if(y++ >= x)
			break;
		if(buf->flushing)
		{
			if(xret >= buf->len - rb->written)
			{
				xret -= buf->len - rb->written;
				rb->len -= buf->len - rb->written;
				rb_rawbuf_done(rb, buf);
				continue;
			}
		}

		if(xret >= buf->len)
		{
			xret -= buf->len;
			rb->len -= buf->len;
			rb_rawbuf_done(rb, buf);
		}
		else
		{
			buf->flushing = 1;
			rb->written = xret;
			rb->len -= xret;
			break;
		}

	}
	return retval;
}

int
rb_rawbuf_flush(rawbuf_head_t * rb, rb_fde_t *F)
{
	rawbuf_t *buf;
	int retval;
	if(rb->list.head == NULL)
	{
		errno = EAGAIN;
		return -1;
	}

	if(!rb_fd_ssl(F))
		return rb_rawbuf_flush_writev(rb, F);

	buf = rb->list.head->data;
	if(!buf->flushing)
	{
		buf->flushing = 1;
		rb->written = 0;
	}

	retval = rb_write(F, buf->data + rb->written, buf->len - rb->written);
	if(retval <= 0)
		return retval;

	rb->written += retval;
	if(rb->written == buf->len)
	{
		rb->written = 0;
		rb_dlinkDelete(&buf->node, &rb->list);
		rb_bh_free(rawbuf_heap, buf);
	}
	rb->len -= retval;
	lrb_assert(rb->len >= 0);
	return retval;
}


void
rb_rawbuf_append(rawbuf_head_t * rb, void *data, int len)
{
	rawbuf_t *buf = NULL;
	int clen;
	void *ptr;
	if(rb->list.tail != NULL)
		buf = rb->list.tail->data;

	if(buf != NULL && buf->len < RAWBUF_SIZE && !buf->flushing)
	{
		buf = (rawbuf_t *) rb->list.tail->data;
		clen = RAWBUF_SIZE - buf->len;
		ptr = (void *)(buf->data + buf->len);
		if(len < clen)
			clen = len;

		memcpy(ptr, data, clen);
		buf->len += clen;
		rb->len += clen;
		len -= clen;
		if(len == 0)
			return;
		data = (char *)data + clen;

	}

	while(len > 0)
	{
		buf = rb_rawbuf_newbuf(rb);

		if(len >= RAWBUF_SIZE)
			clen = RAWBUF_SIZE;
		else
			clen = len;

		memcpy(buf->data, data, clen);
		buf->len += clen;
		len -= clen;
		data = (char *)data + clen;
		rb->len += clen;
	}
}


int
rb_rawbuf_get(rawbuf_head_t * rb, void *data, int len)
{
	rawbuf_t *buf;
	int cpylen;
	void *ptr;
	if(rb->list.head == NULL)
		return 0;

	buf = rb->list.head->data;

	if(buf->flushing)
		ptr = (void *)(buf->data + rb->written);
	else
		ptr = buf->data;

	if(len > buf->len)
		cpylen = buf->len;
	else
		cpylen = len;

	memcpy(data, ptr, cpylen);

	if(cpylen == buf->len)
	{
		rb->written = 0;
		rb_rawbuf_done(rb, buf);
		rb->len -= len;
		return cpylen;
	}

	buf->flushing = 1;
	buf->len -= cpylen;
	rb->len -= cpylen;
	rb->written += cpylen;
	return cpylen;
}

int
rb_rawbuf_length(rawbuf_head_t * rb)
{
	if(rb_dlink_list_length(&rb->list) == 0 && rb->len != 0)
		lrb_assert(1 == 0);
	return rb->len;
}

rawbuf_head_t *
rb_new_rawbuffer(void)
{
	return rb_malloc(sizeof(rawbuf_head_t));

}

void
rb_free_rawbuffer(rawbuf_head_t * rb)
{
	rb_dlink_node *ptr, *next;
	RB_DLINK_FOREACH_SAFE(ptr, next, rb->list.head)
	{
		rb_rawbuf_done(rb, ptr->data);
	}
	rb_free(rb);
}


void
rb_init_rawbuffers(int heap_size)
{
	if(rawbuf_heap == NULL)
		rawbuf_heap = rb_bh_create(sizeof(rawbuf_t), heap_size, "librb_rawbuf_heap");
}
